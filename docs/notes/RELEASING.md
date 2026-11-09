# Releasing

[`.github/workflows/release.yml`](../../.github/workflows/release.yml) builds
every backend in [`platforms.json`](../../scripts/build/platforms.json): Linux,
Haiku and Windows in their Docker SDKs, and macOS on a GitHub-hosted Mac
(`macos-15`, Xcode's Apple clang). It publishes one zip per backend, for example
`isaac-1.2.3-haiku.zip` or `isaac-1.2.3-linux-gemix-gemd.zip`, plus
`SHA256SUMS`.

## Publish a version

```sh
git tag v1.2.3
git push origin v1.2.3
```

Pushing a tag of the form `vX.Y.Z` starts the workflow. If every backend builds,
it creates the GitHub release `v1.2.3` with the zips attached. If any backend
fails, no release is created; fix the problem, then move the tag:

```sh
git tag -f v1.2.3 && git push -f origin v1.2.3
```

## Test without making a release

- **Locally, one backend:** `python3 scripts/build/package.py haiku --version 9.9.9`
  writes `dist/isaac-9.9.9-haiku.zip`. This uses the same script as CI. It
  needs `docker`, `patchelf` and `zip`.
- **On GitHub, no tag:** go to Actions > Release > Run workflow. Choose the
  branch, and either `all` or a list such as `haiku windows`. The zips appear as
  artifacts on the run page, versioned `0.0.0-dev.<run>`. No tag or release is
  created.
- **The tag path itself:** push a throwaway tag to a fork. To push one here,
  delete it afterwards with
  `gh release delete v0.0.1 --cleanup-tag --yes`.

## What goes in each zip

`scripts/build/package.py` decides what each zip contains:

- The executable, `README.md` written for that backend from the `package`
  entry in `platforms.json`, `LICENSE` and `THIRD-PARTY-NOTICES.md`.
- **Linux:** every library in the executable's closure that no Ubuntu package
  owns, copied to `lib/` (the executable's RUNPATH is `$ORIGIN/lib`). Libraries
  that a package owns are not copied; the README lists their packages on an
  `apt install` line.
- **Windows:** the MinGW runtime DLLs, the Windows App Runtime bootstrap DLL
  and the Windows App SDK license texts. Users install Windows App Runtime 1.8.
- **Haiku:** the executable alone; it links only system libraries.
- **macOS:** a universal (arm64 + x86_64) `Isaac.app`, macOS 13 or newer,
  signed ad hoc. Packaging fails if the app links any library outside
  `/System` or `/usr/lib`. It is not Developer ID signed or notarized, so users
  must approve the first launch; the README explains how. `package.py macos`
  runs only on a Mac; `platforms.py all` skips it elsewhere.

The GEMix zip is a `gemd` client and needs a GEMix server already running.
