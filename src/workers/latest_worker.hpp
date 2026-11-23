// latest_worker.hpp - one background thread that runs "latest wins" jobs.
//
// Replaces the hand-written RenderWorker / GeometryWorker / DecodeWorker /
// SegmentWorker, which all implemented the same protocol:
//   * every submit() and invalidate() bumps a generation number;
//   * only the newest request can win - a newer submit replaces a job that is
//     still waiting, and a running job polls the number (Cancel) to abandon
//     itself when it has been superseded;
//   * a finished value is parked under a lock and re-checked against the
//     newest generation, so a result that was invalidated while it finished
//     is never delivered;
//   * the UI thread is woken by notify() OUTSIDE the lock;
//   * shutdown() joins the thread.
//
// Thread model: everything public is called from ONE thread (the
// UI thread), with one exception: waitForLatest() may also be called from a
// helper thread (outstanding_ is atomic for that reason; counter_ is not).
// The worker thread, started lazily by the first submit(), only runs the job
// function and touches the parked slot. setNotify() may be called at any time:
// the worker takes its own copy of the callback under the lock and calls that
// copy outside it.
//
// No liteui / editor dependencies: the header is tested with plain ints.
#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>

// What a running job gets to see of the generation protocol. `latest` is the
// raw atomic so existing cancellable routines (renderWith, rotateArbitrary,
// segmentObjectWand) keep their (const std::atomic<uint64_t>* latest, uint64_t
// gen) parameters unchanged: pass c.latest, c.gen.
struct WorkerCancel {
  const std::atomic<uint64_t> *latest = nullptr;
  uint64_t gen = 0;
  // True once a newer submit()/invalidate()/shutdown() has superseded this job.
  bool cancelled() const { return latest->load() != gen; }
};

template <class Job, class Value> class LatestWorker {
public:
  using JobType = Job;
  using Result = Value;
  using Cancel = WorkerCancel;
  using Exception = std::exception_ptr;
  // Runs on the worker thread. Return nullopt to say "abandoned" (the result
  // is dropped, nothing is parked, nothing is notified). Jobs that cannot be
  // cancelled simply always return a value (a failure value, if need be).
  using JobFn = std::function<std::optional<Value>(Job &&, Cancel)>;

  enum class WaitStatus { completed, cancelled, failed };

  struct WaitResult {
    WaitStatus status;
    std::optional<Value> value;
    std::exception_ptr error;
  };

  explicit LatestWorker(JobFn fn) : fn_(std::move(fn)) {}
  LatestWorker(const LatestWorker &) = delete;
  LatestWorker &operator=(const LatestWorker &) = delete;

  ~LatestWorker() {
    // Makes a running job bail at its next poll; thread_ (declared last, so
    // destroyed first) then requests stop and joins.
    latest_.store(kShutdown);
  }

  // Called from the worker thread each time a result has been parked. May be
  // set or replaced at any time (see the thread model above). The old callback
  // is destroyed after the lock is released.
  void setNotify(std::function<void()> fn) {
    std::lock_guard lk(m_);
    notify_.swap(fn);
  }

  // Stops the worker and joins it; afterwards notify_ will never run again.
  // Blocks until a running job that ignores Cancel has finished.
  void shutdown() {
    latest_.store(kShutdown);
    outstanding_ = false; // nothing will be delivered any more
    {
      std::lock_guard lk(m_);
    }
    doneCv_.notify_all();
    if (thread_.joinable()) {
      thread_.request_stop(); // also wakes a worker sleeping for a job
      thread_.join();
    }
  }

  // Queues `job`, superseding any earlier one.
  void submit(Job job) {
    if (!thread_.joinable())
      thread_ = std::jthread([this](std::stop_token st) { run(st); });
    const uint64_t gen = ++counter_;
    latest_.store(gen);
    outstanding_ = true;
    {
      std::lock_guard lk(m_);
      pending_.emplace(Pending{gen, std::move(job)});
      done_.reset();
      abandonedGen_.reset();
    }
    workCv_.notify_one();
    doneCv_.notify_all();
  }

  // Forgets whatever is waiting or running; its result will never be taken.
  void invalidate() {
    latest_.store(++counter_);
    outstanding_ = false;
    {
      std::lock_guard lk(m_);
      pending_.reset();
      done_.reset();
      abandonedGen_.reset();
    }
    doneCv_.notify_all();
  }

  // True from submit() until its result has been taken or invalidate().
  bool outstanding() const { return outstanding_; }

  // Non-blocking: the finished result of the newest request, if it is ready.
  std::optional<Value> collect() {
    if (!outstanding_)
      return std::nullopt;
    std::unique_lock lk(m_);

    if (abandonedGen_ && *abandonedGen_ == latest_.load()) {
      abandonedGen_.reset();
      outstanding_ = false;
      return std::nullopt;
    }

    if (done_ && done_->gen != latest_.load())
      done_.reset();

    if (!done_)
      return std::nullopt;

    // Consume the request before reporting an error.
    if (done_->error) {
      auto error = done_->error;

      done_.reset();
      outstanding_ = false;

      lk.unlock();

      std::rethrow_exception(error);
    }

    Value v = std::move(*done_->value);

    done_.reset();
    outstanding_ = false;

    return v;
  }

  // Waits for the request that is latest when this function is called.
  //
  // Returns:
  //   completed: the request produced a value.
  //   cancelled: the request was superseded, invalidated, shut down,
  //              or abandoned by returning nullopt.
  //   failed:    the job threw an exception; inspect result.error.
  //
  // If a newer submit() occurs while waiting, this call returns cancelled.
  // It does NOT switch to waiting for the newer request.
  //
  // With nothing outstanding (never submitted, invalidated, shut down, or the
  // result already taken) there is nothing to wait for: returns cancelled at
  // once instead of blocking on a generation that can never finish.
  WaitResult waitForLatest() {
    if (!outstanding_)
      return {WaitStatus::cancelled, std::nullopt, nullptr};
    std::unique_lock lk(m_);

    const uint64_t target = latest_.load();

    doneCv_.wait(lk, [this, target] {
      return latest_.load() != target ||
             (abandonedGen_ && *abandonedGen_ == target) ||
             (done_ && done_->gen == target);
    });

    // A newer submit(), invalidate(), or shutdown() superseded this request.
    if (latest_.load() != target) {
      return {WaitStatus::cancelled, std::nullopt, nullptr};
    }

    // The job returned nullopt without producing a result.
    if (abandonedGen_ && *abandonedGen_ == target) {
      outstanding_ = false;
      abandonedGen_.reset();

      return {WaitStatus::cancelled, std::nullopt, nullptr};
    }

    // The job threw an exception.
    if (done_->error) {
      auto error = done_->error;

      done_.reset();
      outstanding_ = false;

      return {WaitStatus::failed, std::nullopt, error};
    }

    // Successful result.
    Value v = std::move(*done_->value);

    done_.reset();
    outstanding_ = false;

    return {WaitStatus::completed, std::move(v), nullptr};
  }

private:
  // Wakes the UI. The callback is copied under the lock (setNotify may run at
  // any time) and called outside it, so a slow or re-entrant callback never
  // blocks submit() / collect() / waitForLatest(). Copying a std::function can
  // throw, so it sits inside the try with the call. A failing notification
  // must not terminate the worker. Never call this with m_ held.
  void wake() {
    try {
      std::function<void()> fn;
      {
        std::lock_guard lk(m_);
        fn = notify_;
      }
      if (fn)
        fn();
    } catch (...) {
    }
  }

  struct Pending {
    uint64_t gen;
    Job job;
  };
  struct Done {
    uint64_t gen;
    std::optional<Value> value;
    std::exception_ptr error;
  };
  static constexpr uint64_t kShutdown = ~uint64_t{0};

  void run(std::stop_token st) {
    for (;;) {
      std::optional<Pending> cur;
      {
        std::unique_lock lk(m_);
        if (!workCv_.wait(lk, st, [this] { return pending_.has_value(); }))
          return; // stop requested and nothing waiting
        cur.emplace(std::move(*pending_));
        pending_.reset();
      }
      const uint64_t gen = cur->gen;
      if (latest_.load() != gen)
        continue; // superseded while queued: don't even start
      std::optional<Value> v;
      std::exception_ptr error;

      try {
        v = fn_(std::move(cur->job), Cancel{&latest_, gen});
      } catch (...) {
        error = std::current_exception();
      }

      cur.reset();

      // No value and no exception means the job was abandoned.
      if (!v && !error) {
        {
          std::lock_guard lk(m_);
          if (latest_.load() == gen)
            abandonedGen_ = gen;
        }
        doneCv_.notify_all();
        continue;
      }
      {
        std::lock_guard lk(m_);

        // Discard stale success or failure.
        if (latest_.load() != gen)
          continue;

        if (error)
          done_.emplace(Done{gen, std::nullopt, error});
        else
          done_.emplace(Done{gen, std::move(v), nullptr});
      }
      doneCv_.notify_all();
      wake();
    }
  }

  JobFn fn_;
  std::mutex m_;                       // guards pending_, done_, notify_
  std::condition_variable_any workCv_; // "a job is waiting" (stop-aware)
  std::condition_variable doneCv_;     // "a result is parked"
  std::optional<Pending> pending_;
  std::optional<Done> done_;
  std::optional<uint64_t>
      abandonedGen_; // Set when the current generation's job returns nullopt.
  std::function<void()> notify_;    // see setNotify; guarded by m_
  std::atomic<uint64_t> latest_{0}; // newest generation; read by the worker
  uint64_t counter_ = 0;            // UI thread only
  std::atomic<bool> outstanding_{false}; // written by the UI thread (and by
                                         // waitForLatest), readable anywhere
  std::jthread thread_;             // keep last: joined first on destruction
};