# Releasing, Signing, and Notarization

This document explains the release behavior for `main` and `dev`, what end users may see on macOS, and how signing/notarization is expected to work.

## Current State

Right now the repo has:
- a local packaging script: [`scripts/package-app.sh`](../scripts/package-app.sh)
- a GitHub Actions release workflow: [`.github/workflows/release.yml`](../.github/workflows/release.yml)

Current branch intent:
- `dev` is for source/integration work
- `main` is for stable tagged releases

Release workflow intent:
- `dev` should not be used for public downloadable app releases
- tagged commits on `main` should produce signed/notarized macOS releases
- if signing secrets are missing, tagged `main` releases should fail instead of publishing unsigned artifacts

## What End Users May See

If you open an older or manually packaged unsigned build, macOS Gatekeeper may block it.

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
- sign it
- notarize it
- staple it
- publish the final zip to GitHub Releases

## Proper macOS Distribution Flow

For a normal user-facing macOS release, the flow is:

1. build the app
2. sign the app with a `Developer ID Application` certificate
3. zip the signed app
4. submit the zip to Apple notarization
5. staple the notarization ticket to the app
6. create the final distributable zip from the stapled app

That gives users a much smoother experience and avoids the current Gatekeeper warnings.

## Required Secrets For CI Releases

The GitHub Actions release workflow expects:
- `APPLE_SIGNING_IDENTITY`
- `APPLE_DEVELOPER_ID_P12_BASE64`
- `APPLE_DEVELOPER_ID_P12_PASSWORD`
- `APPLE_API_KEY_ID`
- `APPLE_API_ISSUER_ID`
- `APPLE_API_PRIVATE_KEY`
- optional: `APPLE_KEYCHAIN_PASSWORD`

Without those secrets, tagged `main` releases should fail.

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
- `main` for stable signed/notarized tagged releases

For older unsigned builds or local manual packaging:
- keep the quarantine workaround documented
- do not present those builds as polished public releases
