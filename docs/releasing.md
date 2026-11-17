# Building, CI and releasing

## CI

`.github/workflows/build.yml` runs on every push to `main` and on pull requests. Each job configures with a preset, builds, and runs `ctest`:

- Windows (MSVC): Debug and Release
- Linux (GCC 14): Debug, Release, and Debug + AddressSanitizer

The golden render test is skipped (not failed) when there is no golden file for that compiler and configuration. See [testing.md](testing.md).

## Cutting a release

1. Make sure `main` is green.
2. Tag and push:
   ```
   git tag v0.2.0
   git push origin v0.2.0
   ```
3. `.github/workflows/release.yml` builds and tests both platforms (Release), then publishes a GitHub release with:
   - `literoom-windows-x64.zip`: `literoom.exe`
   - `literoom-linux-x64.tar.gz`: the `literoom` binary

If either platform fails, no release is created.

The tag only names the release. The version inside the build comes from `project(literoom VERSION ...)` in the root `CMakeLists.txt`, so bump it by hand before tagging.

To try the pipeline without a real release, push a throwaway tag such as `v0.0.1-test`, then delete the release and the tag:
```
git push --delete origin v0.0.1-test
git tag -d v0.0.1-test
```

## What to edit for what

| Change | File |
|--------|------|
| Version for local builds | `project(literoom VERSION ...)` in root `CMakeLists.txt` |
| New Linux system library the app needs | `third_party/liteui/CMakeLists.txt` (`pkg_check_modules` and link), the apt list in **both** workflows, and the requirements in `README.md` |
| Compiler version on Linux CI | `g++-14` lines in **both** workflows |
| Update liteui | replace `third_party/liteui/liteui.hpp`; if its Wayland protocols changed, add or replace the files in `third_party/liteui/wayland-protocols/` (every `*-protocol.c` there is compiled automatically) |
| Add a build or test configuration | `CMakePresets.json` (configure, build and test presets), then the matrix in `build.yml` |
| Change what a release contains | the "Package" steps in `release.yml` |

## Things that must not change

- **The vendored `wayland-protocols/` files must stay committed.** CI has no `wayland-scanner` step.
- **Golden files in `tests/golden/`.** They are the reference for `literoom_golden_render`, one per compiler and configuration. Re-record one only on purpose (`literoom_golden_record`), never to make a failing test pass.

## Notes

- **No ONNX Runtime in releases.** CI and release builds use `LITEROOM_ONNX=OFF`, so the published binaries have the basic color-grow object selection only. To ship MobileSAM, the workflows would have to download ONNX Runtime, configure with `-DLITEROOM_ONNX=ON -DONNXRUNTIME_DIR=...`, and package `models/` and the runtime library with the binary.
- MSVC builds link the C++ runtime statically (`CMAKE_MSVC_RUNTIME_LIBRARY`), so the Windows zip needs no Visual C++ Redistributable.
- The Windows exe is unsigned, so SmartScreen shows an "unknown publisher" warning. Code signing is a separate step if you want it.
- There is no installer or `.deb`. The release is a plain zip and tarball. Adding an installer means adding packaging files, a CPack or Inno Setup step in CMake and `release.yml`, and a row for them in the table above.
