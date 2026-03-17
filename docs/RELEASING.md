# Releasing, Signing, and Notarization

This document explains the release behavior for `main` and `dev`, what end users may see on macOS, and what the future signing/notarization upgrade would look like.

## Current State

Right now the repo has:
- a local packaging script: [`scripts/package-app.sh`](../scripts/package-app.sh)
- a GitHub Actions release workflow: [`.github/workflows/release.yml`](../.github/workflows/release.yml)

Current branch intent:
- `dev` is for source/integration work
- `main` is for stable tagged releases

Release workflow intent:
- `dev` should not be used for public downloadable app releases
- tagged commits on `main` should produce packaged macOS releases
- those releases are currently unsigned

## What End Users May See

If you open a downloaded or manually packaged unsigned build, macOS Gatekeeper may block it.

Common symptoms:
- `"Synthesizer" is damaged and can't be opened`
- `"Synthesizer" cannot be opened because the developer cannot be verified`

In that state, "damaged" usually means "quarantined and untrusted", not "the app bundle is corrupted".

## Workaround For Older Unsigned Tester Builds

### Option 1: Right-click Open

1. Unzip the release.
2. Right-click `Synthesizer.app`.
3. Choose `Open`.
4. Confirm `Open`.

### Option 2: Remove quarantine manually

```bash
xattr -dr com.apple.quarantine /path/to/Synthesizer.app
open /path/to/Synthesizer.app
```

Example:

```bash
xattr -dr com.apple.quarantine ~/Downloads/Synthesizer.app
open ~/Downloads/Synthesizer.app
```

## Local Packaging

Create a packaged app zip locally with:

```bash
./scripts/package-app.sh
```

That produces a zip under `dist/`, for example:

```bash
dist/Synthesizer-v1.0.0-macOS-arm64.zip
```

## Tagged Main Release Flow

To publish a real release:

```bash
git checkout main
git pull --ff-only
git tag v1.0.0
git push origin v1.0.0
```

That triggers the release workflow. The intended flow is:
- verify the tagged commit is on `main`
- build the app
- package it into a `.zip`
- publish the final zip to GitHub Releases

## Future Signed macOS Distribution Flow

If you later decide to pay for Apple Developer membership and sign releases properly, the flow would be:

1. build the app
2. sign the app with a `Developer ID Application` certificate
3. zip the signed app
4. submit the zip to Apple notarization
5. staple the notarization ticket to the app
6. create the final distributable zip from the stapled app

That would give users a smoother experience and avoid the current Gatekeeper warnings.

## Local Signing Example

Prerequisites:
- Apple Developer account
- `Developer ID Application` certificate installed in Keychain
- Xcode command line tools

Example signing command:

```bash
codesign --force --deep --options runtime --timestamp \
  --sign "Developer ID Application: YOUR NAME (TEAMID)" \
  build-release/Synthesizer.app
```

Verify the signature:

```bash
codesign --verify --deep --strict --verbose=2 build-release/Synthesizer.app
spctl --assess --type execute --verbose=4 build-release/Synthesizer.app
```

## Local Notarization Example

After signing, package the app:

```bash
ditto -c -k --sequesterRsrc --keepParent \
  build-release/Synthesizer.app \
  dist/Synthesizer-signed.zip
```

Submit it to Apple:

```bash
xcrun notarytool submit dist/Synthesizer-signed.zip \
  --keychain-profile "AC_NOTARY_PROFILE" \
  --wait
```

Then staple the ticket to the `.app`:

```bash
xcrun stapler staple build-release/Synthesizer.app
```

After stapling, create the final zip you actually distribute:

```bash
ditto -c -k --sequesterRsrc --keepParent \
  build-release/Synthesizer.app \
  dist/Synthesizer-notarized.zip
```

## Recommended Near-Term Release Policy

Use this split:
- `dev` for source, CI, and integration work
- `main` for stable packaged tagged releases

For current releases and local manual packaging:
- keep the quarantine workaround documented
- treat these as usable packaged builds, but not fully trusted macOS distributions
