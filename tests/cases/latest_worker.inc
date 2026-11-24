// cases/latest_worker.inc - included from literoom_test.cpp: no main(), no check.hpp, no
// #define LITEUI_IMPLEMENTATION. Helpers and TEST_CASEs live in namespace latest_worker_cases
// so they cannot collide with other cases.
//
// Gates and latches instead of sleeps; every wait has a timeout so a broken
// worker fails the test instead of hanging it. Catch2 assertions are not
// thread-safe, so they only ever run on the main thread; the worker
// thread only records into atomics / mutex-guarded state.

#include "workers/latest_worker.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <thread>
#include <vector>
#include <stdexcept>

namespace latest_worker_cases
{
using namespace std::chrono_literals;
static constexpr auto kWait = 5s;

// Counts events; waitFor(n) blocks until at least n have happened.
class Latch
{
public:
  void signal()
  {
    {
      std::lock_guard lk(m_);
      ++n_;
    }
    cv_.notify_all();
  }
  bool waitFor(int n, std::chrono::milliseconds t = std::chrono::duration_cast<std::chrono::milliseconds>(kWait))
  {
    std::unique_lock lk(m_);
    return cv_.wait_for(lk, t, [&] { return n_ >= n; });
  }
  int count()
  {
    std::lock_guard lk(m_);
    return n_;
  }

private:
  std::mutex m_;
  std::condition_variable cv_;
  int n_ = 0;
};

// A door the job waits at until the test opens it.
class Gate
{
public:
  void open()
  {
    {
      std::lock_guard lk(m_);
      open_ = true;
    }
    cv_.notify_all();
  }
  bool wait()
  {
    std::unique_lock lk(m_);
    return cv_.wait_for(lk, kWait, [&] { return open_; });
  }

private:
  std::mutex m_;
  std::condition_variable cv_;
  bool open_ = false;
};

// Gives the worker time to run code that has no observable signal (e.g. the
// bookkeeping after a job returns). Only used to let a NEGATIVE check bite: too
// short a grace can hide a bug (a false pass), but can never fail a correct
// worker. The one exception is a test that needs a helper thread to be
// blocked inside waitForLatest() before the test goes on: there a late helper
// can fail a correct worker, so such a test passes a longer grace.
static void grace(std::chrono::milliseconds d = 100ms) { std::this_thread::sleep_for(d); }

// Records which job ids actually started, in order.
class Log
{
public:
  void add(int v)
  {
    std::lock_guard lk(m_);
    v_.push_back(v);
  }
  std::vector<int> get()
  {
    std::lock_guard lk(m_);
    return v_;
  }

private:
  std::mutex m_;
  std::vector<int> v_;
};

using IntWorker = LatestWorker<int, int>;

// Spins until `c` is cancelled (or the timeout hits). True = it was cancelled.
static bool waitCancelled(const WorkerCancel &c)
{
  const auto end = std::chrono::steady_clock::now() + kWait;
  while (!c.cancelled())
  {
    if (std::chrono::steady_clock::now() > end)
      return false;
    std::this_thread::yield();
  }
  return true;
}

// ------------------------------------------------------------------ basics
TEST_CASE("latest_worker: basic", "[latest_worker][threaded]")
{
  Latch notified;
  IntWorker w([](int &&j, WorkerCancel) -> std::optional<int>
              { return j * 2; });
  w.setNotify([&] { notified.signal(); });
  CHECK(!w.outstanding());
  CHECK(!w.collect().has_value()); // nothing submitted
  w.submit(21);
  CHECK(w.outstanding());
  CHECK(notified.waitFor(1));
  auto r = w.collect();
  CHECK(r.has_value());
  CHECK((r && *r == 42));
  CHECK(!w.outstanding());
  CHECK(!w.collect().has_value()); // delivered once
  w.shutdown();
  CHECK(notified.count() == 1);
}

// --------------------------------------------- newest wins, queued job skipped
TEST_CASE("latest_worker: newestWinsAndQueuedNeverStarts", "[latest_worker][threaded]")
{
  Latch started, notified;
  Gate gate;
  Log log;
  IntWorker w([&](int &&j, WorkerCancel) -> std::optional<int>
              {
                log.add(j);
                started.signal();
                if (j == 1)
                  gate.wait(); // ignores cancellation on purpose
                return j * 10; });
  w.setNotify([&] { notified.signal(); });
  w.submit(1);
  CHECK(started.waitFor(1)); // job 1 is running, held at the gate
  w.submit(2);               // queued...
  w.submit(3);               // ...and replaced before it ever starts
  CHECK(!w.collect().has_value());
  gate.open();
  CHECK(notified.waitFor(1));
  auto r = w.collect();
  CHECK((r && *r == 30));      // job 1's result (10) never arrives
  w.shutdown();              // joins: the worker is now fully idle
  CHECK(notified.count() == 1);
  const std::vector<int> expect{1, 3};
  CHECK(log.get() == expect); // 2 never started
}

// ------------------------------------------- a running job sees the cancel flag
TEST_CASE("latest_worker: cancelOnSubmit", "[latest_worker][threaded]")
{
  Latch started, notified;
  std::atomic<int> sawCancel{0};
  IntWorker w([&](int &&j, WorkerCancel c) -> std::optional<int>
              {
                started.signal();
                if (j == 1)
                {
                  if (waitCancelled(c))
                    ++sawCancel;
                  if (c.cancelled())
                    return std::nullopt; // abandoned
                }
                return j; });
  w.setNotify([&] { notified.signal(); });
  w.submit(1);
  CHECK(started.waitFor(1));
  w.submit(2); // supersedes the running job
  CHECK(notified.waitFor(1));
  auto r = w.collect();
  CHECK((r && *r == 2));
  w.shutdown();
  CHECK(sawCancel == 1);
  CHECK(notified.count() == 1); // the abandoned job produced no notify
}

TEST_CASE("latest_worker: cancelOnInvalidate", "[latest_worker][threaded]")
{
  Latch started, finished, notified;
  std::atomic<int> sawCancel{0};
  IntWorker w([&](int &&, WorkerCancel c) -> std::optional<int>
              {
                started.signal();
                if (waitCancelled(c))
                  ++sawCancel;
                finished.signal();
                return std::nullopt; });
  w.setNotify([&] { notified.signal(); });
  w.submit(1);
  CHECK(started.waitFor(1));
  w.invalidate();
  CHECK(!w.outstanding());
  CHECK(finished.waitFor(1));
  CHECK(!w.collect().has_value());
  w.shutdown();
  CHECK(sawCancel == 1);
  CHECK(notified.count() == 0); // abandoned: no result, no notify
}

// ------------------------- invalidate drops pending work and a running result
TEST_CASE("latest_worker: invalidateDropsPending", "[latest_worker][threaded]")
{
  Latch started, notified;
  Gate gate;
  Log log;
  IntWorker w([&](int &&j, WorkerCancel) -> std::optional<int>
              {
                log.add(j);
                started.signal();
                if (j == 1)
                  gate.wait();
                return j; });
  w.setNotify([&] { notified.signal(); });
  w.submit(1);
  CHECK(started.waitFor(1));
  w.submit(2); // pending behind the running job 1
  w.invalidate(); // drops the pending one as well
  gate.open();
  grace();     // let the worker finish job 1 (and look for more work)
  w.submit(3); // a sentinel: runs after job 1 has been dealt with
  CHECK(notified.waitFor(1));
  auto r = w.collect();
  CHECK((r && *r == 3));
  w.shutdown();
  CHECK(notified.count() == 1); // job 1's finished-but-invalidated result: no notify
  const std::vector<int> expect{1, 3};
  CHECK(log.get() == expect);
}

// A result parked but not yet collected is thrown away by a newer submit() or
// by invalidate(): waitForLatest must then wait for the NEW job.
TEST_CASE("latest_worker: parkedResultDiscarded", "[latest_worker][threaded]")
{
  {
    Latch started, notified;
    Gate gate;
    IntWorker w([&](int &&j, WorkerCancel) -> std::optional<int>
                {
                  started.signal();
                  if (j == 2)
                    gate.wait();
                  return j; });
    w.setNotify([&] { notified.signal(); });
    w.submit(1);
    CHECK(notified.waitFor(1)); // 1's result is parked, not collected
    w.submit(2);                // must discard it
    CHECK(started.waitFor(2));  // job 2 is running, held at the gate
    // waitForLatest blocks, so it runs on a helper thread (the test thread
    // opens the gate meanwhile). It must wait for job 2, not hand over job 1.
    using R = IntWorker::WaitResult;
    R got{IntWorker::WaitStatus::failed, std::nullopt, nullptr};
    std::thread t([&] { got = w.waitForLatest(); });
    grace();
    gate.open();
    t.join();
    CHECK(got.status == IntWorker::WaitStatus::completed);
    CHECK((got.value && *got.value == 2));
    w.shutdown();
  }
  {
    Latch started, notified;
    Gate gate;
    IntWorker w([&](int &&j, WorkerCancel) -> std::optional<int>
                {
                  started.signal();
                  if (j == 2)
                    gate.wait();
                  return j; });
    w.setNotify([&] { notified.signal(); });
    w.submit(1);
    CHECK(notified.waitFor(1)); // parked
    w.invalidate();             // discards it, even though nothing else follows
    CHECK(!w.outstanding());
    w.submit(2);
    CHECK(started.waitFor(2));
    gate.open();
    auto r = w.waitForLatest();
    CHECK(r.status == IntWorker::WaitStatus::completed);
    CHECK((r.value && *r.value == 2));
    w.shutdown();
  }
}

// nullopt means "abandoned": nothing is parked, nothing is notified.
TEST_CASE("latest_worker: nulloptIsAbandoned", "[latest_worker][threaded]")
{
  Latch finished, notified;
  IntWorker w([&](int &&j, WorkerCancel) -> std::optional<int>
              {
                finished.signal();
                if (j == 1)
                  return std::nullopt; // gives up although nobody superseded it
                return j; });
  w.setNotify([&] { notified.signal(); });
  w.submit(1);
  CHECK(finished.waitFor(1));
  grace(); // job 1 has returned nullopt and nobody superseded it
  CHECK(notified.count() == 0);
  CHECK(!w.collect().has_value()); // nothing was parked for it
  w.submit(2);
  CHECK(notified.waitFor(1));
  auto r = w.collect();
  CHECK((r && *r == 2));
  w.shutdown();
  CHECK(notified.count() == 1);
}

// ------------------------------------------ results & notify: where and when
TEST_CASE("latest_worker: notifyThreadAndOutsideLock", "[latest_worker][threaded]")
{
  Latch inNotify;
  Gate release;
  std::atomic<bool> timedOut{false};
  std::thread::id notifyThread;
  const std::thread::id mainThread = std::this_thread::get_id();
  IntWorker w([](int &&j, WorkerCancel) -> std::optional<int>
              { return j; });
  w.setNotify([&]
              {
                notifyThread = std::this_thread::get_id();
                inNotify.signal();
                // Hold the worker inside notify until main has called collect().
                // If notify ran under the worker's lock, that collect() could not
                // get in and this wait would time out.
                if (!release.wait())
                  timedOut = true; });
  w.submit(5);
  CHECK(inNotify.waitFor(1));
  CHECK(notifyThread != mainThread);
  auto r = w.collect(); // needs the lock the worker must not be holding
  CHECK((r && *r == 5));
  release.open();
  w.shutdown();
  CHECK(!timedOut);
}

TEST_CASE("latest_worker: waitForLatest", "[latest_worker][threaded]")
{
  Latch started;
  Gate gate;
  IntWorker w([&](int &&j, WorkerCancel) -> std::optional<int>
              {
                started.signal();
                if (j == 1)
                  gate.wait();
                return j * 3; });
  w.submit(1);
  CHECK(started.waitFor(1));
  w.submit(2);
  gate.open();
  CHECK(w.outstanding());
  const auto r = w.waitForLatest(); // waits for job 2 (the latest at call time)
  CHECK(r.status == IntWorker::WaitStatus::completed);
  CHECK((r.value && *r.value == 6));
  CHECK(!w.outstanding());
  CHECK(!w.collect().has_value());
  w.shutdown();
}

TEST_CASE("latest_worker: collectBeforeCompletion", "[latest_worker][threaded]")
{
  Latch started, notified;
  Gate gate;
  IntWorker w([&](int &&j, WorkerCancel) -> std::optional<int>
              {
                started.signal();
                gate.wait();
                return j; });
  w.setNotify([&] { notified.signal(); });
  w.submit(9);
  CHECK(started.waitFor(1));
  CHECK(!w.collect().has_value()); // still running
  CHECK(w.outstanding());          // ...and still outstanding
  gate.open();
  CHECK(notified.waitFor(1));
  auto r = w.collect();
  CHECK((r && *r == 9));
  CHECK(!w.collect().has_value()); // once only
  w.shutdown();
}

// --------------------------------------------------------- shutdown, destruction
TEST_CASE("latest_worker: shutdown", "[latest_worker][threaded]")
{
  {
    Latch notified;
    IntWorker w([](int &&j, WorkerCancel) -> std::optional<int>
                { return j; });
    w.setNotify([&] { notified.signal(); });
    w.shutdown(); // before the first submit: nothing to join
    w.shutdown(); // twice
    CHECK(notified.count() == 0);
  }
  {
    Latch started, notified;
    IntWorker w([&](int &&, WorkerCancel c) -> std::optional<int>
                {
                  started.signal();
                  waitCancelled(c);
                  return 1; }); // returns a value even though it was cancelled
    w.setNotify([&] { notified.signal(); });
    w.submit(1);
    CHECK(started.waitFor(1));
    w.shutdown(); // cancels the running job and joins
    w.shutdown();
    CHECK(notified.count() == 0); // nothing is notified once shut down
    CHECK(!w.collect().has_value());
  }
}

TEST_CASE("latest_worker: destruction", "[latest_worker][threaded]")
{
  const auto t0 = std::chrono::steady_clock::now();
  {
    Latch started;
    IntWorker w([&](int &&, WorkerCancel c) -> std::optional<int>
                {
                  started.signal();
                  waitCancelled(c);
                  return std::nullopt; });
    w.submit(1);
    w.submit(2); // may be queued behind the running one
    CHECK(started.waitFor(1));
  } // ~LatestWorker cancels and joins
  {
    IntWorker idle([](int &&j, WorkerCancel) -> std::optional<int>
                   { return j; });
  } // never started a thread
  CHECK(std::chrono::steady_clock::now() - t0 < 3s);
}

// ------------------------------------------------------ move-only job and value
TEST_CASE("latest_worker: moveOnly", "[latest_worker][threaded]")
{
  using W = LatestWorker<std::unique_ptr<int>, std::unique_ptr<int>>;
  Latch notified;
  W w([](std::unique_ptr<int> &&j, WorkerCancel) -> std::optional<std::unique_ptr<int>>
      { return std::make_unique<int>(*j + 1); });
  w.setNotify([&] { notified.signal(); });
  w.submit(std::make_unique<int>(41));
  CHECK(notified.waitFor(1));
  auto r = w.collect();
  CHECK((r && *r && **r == 42));
  w.submit(std::make_unique<int>(1));
  auto v = w.waitForLatest();
  CHECK(v.status == W::WaitStatus::completed);
  CHECK((v.value && *v.value && **v.value == 2));
  w.shutdown();
}

// --------------------------------------------------------------------- stress
// 2000 submits mixed with random invalidates. Main is the only thread that
// submits/invalidates/collects, so at the moment collect() hands over a value
// the only legal one is that of the newest submit. (After an invalidate,
// collect() returns nothing at all: nothing is outstanding.)
TEST_CASE("latest_worker: stress", "[latest_worker][threaded]")
{
  std::atomic<int> jobsRun{0};
  std::atomic<int> notifies{0};
  IntWorker w([&](int &&id, WorkerCancel c) -> std::optional<int>
              {
                ++jobsRun;
                const int spins = (id * 7919) % 50;
                for (int i = 0; i < spins; ++i)
                {
                  if (c.cancelled())
                    return std::nullopt;
                  std::this_thread::yield();
                }
                return id; });
  w.setNotify([&] { ++notifies; });
  std::mt19937 rng(12345);
  int lastSubmit = 0;
  int delivered = 0, stale = 0;
  for (int id = 1; id <= 2000; ++id)
  {
    w.submit(id);
    lastSubmit = id;
    const unsigned dice = rng() % 8;
    if (dice == 0)
      w.invalidate();
    if (dice <= 3)
    {
      if (auto r = w.collect())
      {
        ++delivered;
        if (*r != lastSubmit)
          ++stale;
      }
    }
    if (dice == 4)
      std::this_thread::yield();
    if (dice == 5)
    { // let this one finish: nothing supersedes it while main waits
      std::optional<int> r;
      const auto end = std::chrono::steady_clock::now() + kWait;
      while (!(r = w.collect()) && std::chrono::steady_clock::now() < end)
        std::this_thread::yield();
      CHECK(r.has_value());
      if (r)
      {
        ++delivered;
        if (*r != lastSubmit)
          ++stale;
      }
    }
  }
  CHECK(stale == 0);
  w.submit(5000); // the last word
  const auto last = w.waitForLatest();
  CHECK(last.status == IntWorker::WaitStatus::completed);
  CHECK((last.value && *last.value == 5000));
  CHECK(!w.outstanding());
  w.shutdown();
  const int jobs = jobsRun, n = notifies;
  CHECK(jobs >= 1);
  CHECK(n >= delivered + 1); // every delivered value was notified first
  CHECK(n <= jobs);
  INFO("stress: " << jobs << " jobs ran, " << n << " notified, " << delivered << " collected");
}

TEST_CASE("latest_worker: waitForLatestCancelledBySubmit", "[latest_worker][threaded]")
{
  Latch started;
  Gate gate;
  IntWorker w([&](int &&j, WorkerCancel) -> std::optional<int>
              { started.signal(); if (j == 1) gate.wait(); return j; });
  w.submit(1);
  CHECK(started.waitFor(1));
  IntWorker::WaitResult r{IntWorker::WaitStatus::completed, std::nullopt, nullptr};
  std::thread t([&] { r = w.waitForLatest(); });
  grace(300ms); // the helper must be waiting on job 1 before job 2 is submitted
  w.submit(2);   // supersedes the request being waited on
  gate.open();
  t.join();
  CHECK(r.status == IntWorker::WaitStatus::cancelled); // does NOT switch to job 2
  CHECK(!r.value.has_value());
  w.shutdown();
}

TEST_CASE("latest_worker: waitForLatestFailedAndAbandoned", "[latest_worker][threaded]")
{
  IntWorker w([](int &&j, WorkerCancel) -> std::optional<int>
              { if (j == 1) throw std::runtime_error("boom");
                if (j == 2) return std::nullopt;
                return j; });
  w.submit(1);
  auto a = w.waitForLatest();
  CHECK(a.status == IntWorker::WaitStatus::failed);
  CHECK(a.error != nullptr);
  CHECK(!w.outstanding());
  w.submit(2);
  auto b = w.waitForLatest();
  CHECK(b.status == IntWorker::WaitStatus::cancelled);
  w.shutdown();
}
// ------------------------------------------- failures, shutdown, nothing outstanding
TEST_CASE("latest_worker: collectRethrows", "[latest_worker][threaded]")
{
  Latch notified;
  IntWorker w([](int &&j, WorkerCancel) -> std::optional<int>
              { if (j == 1) throw std::runtime_error("boom");
                return j; });
  w.setNotify([&] { notified.signal(); });
  w.submit(1);
  CHECK(notified.waitFor(1)); // a failure is announced like a result
  CHECK_THROWS_AS(w.collect(), std::runtime_error);
  CHECK(!w.outstanding());    // the request was consumed
  CHECK(!w.collect().has_value());
  w.submit(2);                // and the worker carries on
  CHECK(notified.waitFor(2));
  auto r = w.collect();
  CHECK((r && *r == 2));
  w.shutdown();
}

TEST_CASE("latest_worker: staleExceptionNotDelivered", "[latest_worker][threaded]")
{ // a job that throws after being superseded is dropped, like a stale value
  Latch started, notified;
  Gate gate;
  IntWorker w([&](int &&j, WorkerCancel) -> std::optional<int>
              {
                started.signal();
                if (j == 1)
                {
                  gate.wait();
                  throw std::runtime_error("stale");
                }
                return j; });
  w.setNotify([&] { notified.signal(); });
  w.submit(1);
  CHECK(started.waitFor(1));
  w.submit(2); // supersedes job 1 while it is held
  gate.open(); // job 1 now throws: nobody wants it
  CHECK(notified.waitFor(1));
  std::optional<int> r;
  CHECK_NOTHROW(r = w.collect());
  CHECK((r && *r == 2));
  w.shutdown();
  CHECK(notified.count() == 1);
}

TEST_CASE("latest_worker: shutdownReleasesWaiter", "[latest_worker][threaded]")
{ // shutdown() must wake a thread blocked in waitForLatest(), not just stop the worker
  Latch started;
  IntWorker w([&](int &&, WorkerCancel c) -> std::optional<int>
              {
                started.signal();
                waitCancelled(c);
                return std::nullopt; });
  w.submit(1);
  CHECK(started.waitFor(1));
  IntWorker::WaitResult r{IntWorker::WaitStatus::failed, std::nullopt, nullptr};
  std::thread t([&] { r = w.waitForLatest(); });
  grace();
  w.shutdown();
  t.join();
  CHECK(r.status == IntWorker::WaitStatus::cancelled);
}

TEST_CASE("latest_worker: waitWithNothingOutstanding", "[latest_worker][threaded]")
{ // without the guard in waitForLatest() this test hangs instead of failing
  IntWorker w([](int &&j, WorkerCancel) -> std::optional<int> { return j; });
  CHECK(w.waitForLatest().status == IntWorker::WaitStatus::cancelled); // never submitted
  w.submit(1);
  w.invalidate();
  CHECK(w.waitForLatest().status == IntWorker::WaitStatus::cancelled);
  w.submit(2);
  w.shutdown();
  CHECK(!w.outstanding()); // nothing can arrive any more
  CHECK(w.waitForLatest().status == IntWorker::WaitStatus::cancelled);
}

TEST_CASE("latest_worker: throwingNotifyAndRestart", "[latest_worker][threaded]")
{
  Latch notified;
  IntWorker w([](int &&j, WorkerCancel) -> std::optional<int> { return j; });
  w.setNotify([&]
              {
                notified.signal();
                throw std::runtime_error("wake-up failed"); });
  w.submit(1);
  CHECK(notified.waitFor(1));
  auto a = w.collect();
  CHECK((a && *a == 1)); // the throw cost neither the result nor the worker
  w.submit(2);
  CHECK(notified.waitFor(2));
  w.shutdown();
  CHECK(!w.outstanding());
  w.submit(3); // shutdown() joined the thread; submit() starts a fresh one
  CHECK(notified.waitFor(3));
  auto b = w.collect();
  CHECK((b && *b == 3));
  w.shutdown();
}

// ------------------------------------- waiter vs the UI thread's own calls
// waitForLatest() sleeps on a helper thread while the UI thread keeps polling
// collect() or calls invalidate(). Whatever the interleaving, the waiter must
// RETURN (never sleep on a request that can no longer finish), and a result
// must go to exactly one of the two callers. A stuck waiter is released with
// shutdown() so a regression fails the test instead of hanging it.
// (A logic hang, not a data race: this shows up on multi-core machines only
// some of the time, so the loop runs many rounds.)
TEST_CASE("latest_worker: waiterVsCollect", "[latest_worker][threaded]")
{
  int hung = 0, bothGot = 0, nobodyGot = 0;
  for (int round = 0; round < 300; ++round)
  {
    Gate gate;
    IntWorker w([&](int &&j, WorkerCancel) -> std::optional<int>
                { gate.wait(); return j; });
    w.submit(round);
    Latch returned;
    IntWorker::WaitResult r{IntWorker::WaitStatus::failed, std::nullopt, nullptr};
    std::thread t([&] { r = w.waitForLatest(); returned.signal(); });
    grace(2ms);   // the waiter is (almost certainly) asleep before the result lands
    gate.open();
    std::optional<int> got;
    const auto end = std::chrono::steady_clock::now() + 2s;
    while (returned.count() == 0 && std::chrono::steady_clock::now() < end)
    {
      if (!got)
        got = w.collect();
      std::this_thread::yield();
    }
    if (returned.count() == 0)
    {
      ++hung;
      w.shutdown(); // releases the stuck waiter
    }
    t.join();
    const bool waiterGot = r.status == IntWorker::WaitStatus::completed && r.value == round;
    if (got.has_value() && waiterGot)
      ++bothGot;
    if (!got.has_value() && !waiterGot)
      ++nobodyGot;
  }
  INFO("hung=" << hung << " bothGot=" << bothGot << " nobodyGot=" << nobodyGot);
  CHECK(hung == 0);       // the waiter always came back
  CHECK(bothGot == 0);    // the value was handed out once
  CHECK(nobodyGot == 0);  // ...and it was not lost
}

TEST_CASE("latest_worker: waiterVsInvalidate", "[latest_worker][threaded]")
{
  int hung = 0, notCancelled = 0;
  for (int round = 0; round < 300; ++round)
  {
    Latch started, returned;
    IntWorker w([&](int &&, WorkerCancel c) -> std::optional<int>
                {
                  started.signal();
                  waitCancelled(c);
                  return std::nullopt; });
    w.submit(round);
    started.waitFor(1);
    IntWorker::WaitResult r{IntWorker::WaitStatus::failed, std::nullopt, nullptr};
    std::thread t([&] { r = w.waitForLatest(); returned.signal(); });
    if (round % 2)
      grace(1ms); // half the rounds: invalidate while the waiter is asleep, half: around its entry
    w.invalidate();
    if (!returned.waitFor(1, 2000ms))
    {
      ++hung;
      w.shutdown();
    }
    t.join();
    if (r.status != IntWorker::WaitStatus::cancelled)
      ++notCancelled;
  }
  INFO("hung=" << hung << " notCancelled=" << notCancelled);
  CHECK(hung == 0);
  CHECK(notCancelled == 0);
}

// ----------------------------------------------- setNotify at any time
TEST_CASE("latest_worker: setNotifyWhileRunning", "[latest_worker][threaded]")
{ // the callback is replaced while jobs finish on the worker thread (run it under
  // ThreadSanitizer to check the copy-under-lock rule), and nothing is lost
  std::atomic<int> a{0}, b{0};
  IntWorker w([](int &&j, WorkerCancel) -> std::optional<int>
              { return j; });
  for (int i = 0; i < 2000; ++i)
  {
    if (i % 2)
      w.setNotify([&] { ++a; });
    else
      w.setNotify([&] { ++b; });
    w.submit(i);
    if (i % 7 == 0)
      (void)w.collect();
  }
  w.setNotify([&] { ++a; });
  w.submit(-1);
  const auto last = w.waitForLatest();
  CHECK(last.status == IntWorker::WaitStatus::completed);
  CHECK((last.value && *last.value == -1));
  w.shutdown(); // joins: every notify that will ever run has run
  CHECK(a + b >= 1);
}

TEST_CASE("latest_worker: replacedNotifyDestroyedOutsideLock", "[latest_worker][threaded]")
{ // setNotify() documents that the OLD callback is destroyed after the lock is
  // released. The old callback's destructor takes that lock (collect() with a
  // request outstanding); if setNotify destroyed it under the lock this would
  // deadlock, so it runs on a helper thread with a timeout.
  Gate gate;
  IntWorker w([&](int &&j, WorkerCancel) -> std::optional<int>
              { gate.wait(); return j; });
  w.submit(1); // outstanding, so collect() really takes the lock
  Latch destroyed, replaced;
  struct Sentinel
  {
    IntWorker *w;
    Latch *done;
    Sentinel(IntWorker *w_, Latch *d) : w(w_), done(d) {}
    ~Sentinel()
    {
      (void)w->collect();
      done->signal();
    }
  };
  w.setNotify([s = std::make_shared<Sentinel>(&w, &destroyed)] {});
  std::thread t([&]
                {
                  w.setNotify({}); // destroys the sentinel
                  replaced.signal(); });
  if (!replaced.waitFor(1, 3000ms))
  {
    std::fputs("latest_worker: setNotify deadlocked destroying the old callback\n", stderr);
    std::abort(); // the helper is stuck on the lock: nothing can be joined
  }
  t.join();
  gate.open();
  w.shutdown();
  CHECK(destroyed.count() == 1);
}

// ----------------------------------------------------- the Cancel contract
TEST_CASE("latest_worker: cancelGenerations", "[latest_worker][threaded]")
{ // routines such as renderWith take (c.latest, c.gen) and compare them directly
  Log gens;
  std::atomic<int> consistent{0};
  IntWorker w([&](int &&, WorkerCancel c) -> std::optional<int>
              {
                gens.add(static_cast<int>(c.gen));
                if (c.latest != nullptr && c.latest->load() == c.gen && !c.cancelled())
                  ++consistent;
                return 0; });
  w.submit(0);
  CHECK(w.waitForLatest().status == IntWorker::WaitStatus::completed);
  w.invalidate(); // takes a generation number of its own
  w.submit(0);
  CHECK(w.waitForLatest().status == IntWorker::WaitStatus::completed);
  w.submit(0);
  CHECK(w.waitForLatest().status == IntWorker::WaitStatus::completed);
  w.shutdown();
  const std::vector<int> g = gens.get();
  CHECK(g.size() == 3);
  if (g.size() == 3)
  {
    CHECK((g[0] < g[1] && g[1] < g[2]));
    CHECK(g[1] > g[0] + 1); // the invalidate() in between consumed a number
  }
  CHECK(consistent == 3);   // while it runs, an unsuperseded job sees latest == gen
}

// An abandoned (nullopt) job leaves the request outstanding until collect()
// consumes the abandonment: pinned here so a change to it is a decision.
TEST_CASE("latest_worker: outstandingAfterNullopt", "[latest_worker][threaded]")
{
  Latch finished;
  IntWorker w([&](int &&, WorkerCancel) -> std::optional<int>
              { finished.signal(); return std::nullopt; });
  w.submit(1);
  CHECK(finished.waitFor(1));
  grace();
  CHECK(w.outstanding());           // not superseded, not collected yet
  CHECK(!w.collect().has_value());  // consumes the abandonment
  CHECK(!w.outstanding());
  w.shutdown();
}

} // namespace latest_worker_cases
