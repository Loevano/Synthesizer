#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_PATH="${1:-build-release/Synthesizer.app}"
VERSION_LABEL="${2:-}"
DIST_DIR="${DIST_DIR:-dist}"

APPLE_SIGNING_IDENTITY="${APPLE_SIGNING_IDENTITY:-}"
APPLE_DEVELOPER_ID_P12_BASE64="${APPLE_DEVELOPER_ID_P12_BASE64:-}"
APPLE_DEVELOPER_ID_P12_PASSWORD="${APPLE_DEVELOPER_ID_P12_PASSWORD:-}"
APPLE_API_KEY_ID="${APPLE_API_KEY_ID:-}"
APPLE_API_ISSUER_ID="${APPLE_API_ISSUER_ID:-}"
APPLE_API_PRIVATE_KEY="${APPLE_API_PRIVATE_KEY:-}"
APPLE_KEYCHAIN_PASSWORD="${APPLE_KEYCHAIN_PASSWORD:-temporary-release-keychain}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "Signing and notarization are only supported on macOS." >&2
  exit 1
fi

cd "$ROOT_DIR"

if [[ ! -d "$APP_PATH" ]]; then
  echo "Expected app bundle at $APP_PATH" >&2
  exit 1
fi

required=(
  APPLE_SIGNING_IDENTITY
  APPLE_DEVELOPER_ID_P12_BASE64
  APPLE_DEVELOPER_ID_P12_PASSWORD
  APPLE_API_KEY_ID
  APPLE_API_ISSUER_ID
  APPLE_API_PRIVATE_KEY
)

missing=()
for key in "${required[@]}"; do
  if [[ -z "${!key:-}" ]]; then
    missing+=("$key")
  fi
done

if (( ${#missing[@]} > 0 )); then
  printf 'Missing required signing env vars: %s\n' "${missing[*]}" >&2
  exit 1
fi

if [[ -z "$VERSION_LABEL" ]]; then
  VERSION_LABEL="$(git describe --tags --always --dirty 2>/dev/null || echo dev)"
fi

mkdir -p "$DIST_DIR"

ARCH_LABEL="$(uname -m)"
FINAL_ZIP="$DIST_DIR/Synthesizer-${VERSION_LABEL}-macOS-${ARCH_LABEL}.zip"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

KEYCHAIN_PATH="$TMP_DIR/release.keychain-db"
CERT_PATH="$TMP_DIR/developer-id.p12"
API_KEY_PATH="$TMP_DIR/AuthKey_${APPLE_API_KEY_ID}.p8"
SUBMISSION_ZIP="$TMP_DIR/Synthesizer-${VERSION_LABEL}-submission.zip"

printf '%s' "$APPLE_DEVELOPER_ID_P12_BASE64" | base64 --decode > "$CERT_PATH"
printf '%s' "$APPLE_API_PRIVATE_KEY" > "$API_KEY_PATH"
chmod 600 "$CERT_PATH" "$API_KEY_PATH"

security create-keychain -p "$APPLE_KEYCHAIN_PASSWORD" "$KEYCHAIN_PATH"
security set-keychain-settings -lut 21600 "$KEYCHAIN_PATH"
security unlock-keychain -p "$APPLE_KEYCHAIN_PASSWORD" "$KEYCHAIN_PATH"
security list-keychains -d user -s "$KEYCHAIN_PATH"
security default-keychain -s "$KEYCHAIN_PATH"
security import "$CERT_PATH" -k "$KEYCHAIN_PATH" -P "$APPLE_DEVELOPER_ID_P12_PASSWORD" -T /usr/bin/codesign -T /usr/bin/security
security set-key-partition-list -S apple-tool:,apple: -s -k "$APPLE_KEYCHAIN_PASSWORD" "$KEYCHAIN_PATH"

codesign --force --deep --options runtime --timestamp --sign "$APPLE_SIGNING_IDENTITY" "$APP_PATH"
codesign --verify --deep --strict --verbose=2 "$APP_PATH"

ditto -c -k --sequesterRsrc --keepParent "$APP_PATH" "$SUBMISSION_ZIP"

xcrun notarytool submit "$SUBMISSION_ZIP" \
  --key "$API_KEY_PATH" \
  --key-id "$APPLE_API_KEY_ID" \
  --issuer "$APPLE_API_ISSUER_ID" \
  --wait

xcrun stapler staple "$APP_PATH"
spctl --assess --type execute --verbose=4 "$APP_PATH"

rm -f "$FINAL_ZIP"
ditto -c -k --sequesterRsrc --keepParent "$APP_PATH" "$FINAL_ZIP"

echo "Created signed and notarized release artifact: $FINAL_ZIP"
