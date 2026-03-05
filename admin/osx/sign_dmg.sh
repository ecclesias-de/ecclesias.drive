#!/bin/bash
set -xe

APP_NAME="ecclesiasdrive"
PLATFORM="macos-clang-arm64"
VERSION="6.0.3.1"
VERSIONSHORT="6.0.3"
ROOT_DIR="$HOME"
DOWNLOADS="$ROOT_DIR/Downloads"
SRC_DIR="$DOWNLOADS/$APP_NAME-$VERSION-$PLATFORM"
APP="$SRC_DIR/$APP_NAME.app"
ZIP_SRC="$SRC_DIR.zip"
DMG_NAME="$APP_NAME-client-$APP_NAME-$VERSIONSHORT-master-$PLATFORM.dmg"
DMG_OUT="$SRC_DIR/$APP_NAME-$VERSION.dmg"
CERT="Developer ID Application: Metaways Infosystems GmbH (TC62G4GBN8)"
NOTARY_PROFILE="c.cheng@metaways.de"

sign() {
  codesign --force --timestamp --options runtime --sign "$CERT" "$1"
}

# --- Extract zip if present (overwrites existing files) ----------------------
mkdir -p "$SRC_DIR"
if [ -f "$ZIP_SRC" ]; then
  ditto -x -k "$ZIP_SRC" "$SRC_DIR" || true
fi

# --- Locate DMG --------------------------------------------------------------
DMG_SRC="$(find "$SRC_DIR" -name "$DMG_NAME" -type f 2>/dev/null | head -n 1 || true)"
[ -n "$DMG_SRC" ] || DMG_SRC="$(find "$SRC_DIR" -name "*.dmg" -type f 2>/dev/null | head -n 1 || true)"
[ -n "$DMG_SRC" ] || { [ -f "$DOWNLOADS/$DMG_NAME" ] && DMG_SRC="$DOWNLOADS/$DMG_NAME" || true; }
[ -n "$DMG_SRC" ] || DMG_SRC="$(ls -t "$DOWNLOADS/$APP_NAME-client-"*"$PLATFORM"*.dmg 2>/dev/null | head -n 1 || true)"
[ -n "$DMG_SRC" ] || { echo "No DMG found in $SRC_DIR or $DOWNLOADS" >&2; exit 1; }

cd "$SRC_DIR"

# --- Extract app from DMG ----------------------------------------------------
MOUNT_POINT="$(mktemp -d)"
hdiutil attach "$DMG_SRC" -mountpoint "$MOUNT_POINT" -nobrowse -readonly
ditto "$MOUNT_POINT/$APP_NAME.app" "$APP"
hdiutil detach "$MOUNT_POINT" || hdiutil detach -force "$MOUNT_POINT" || true

# --- Sign inner to outer: binaries -> xpc -> frameworks -> app ---------------
find "$APP" -type f -print0 | while IFS= read -r -d '' f; do
  if file "$f" | grep -q "Mach-O"; then sign "$f"; fi
done
find "$APP" -name "*.xpc" -print0 | while IFS= read -r -d '' f; do sign "$f"; done
find "$APP/Contents/Frameworks" -name "*.framework" | sort -r | while IFS= read -r f; do sign "$f"; done
sign "$APP"

# --- Verify ------------------------------------------------------------------
codesign --verify --deep --strict --verbose=2 "$APP"

# --- Notarize app ------------------------------------------------------------
/usr/bin/ditto -c -k --keepParent "$APP" "$APP_NAME.zip"
xcrun notarytool submit "$APP_NAME.zip" --keychain-profile "$NOTARY_PROFILE" --wait
xcrun stapler staple "$APP"

# --- Create, notarize and staple DMG -----------------------------------------
create-dmg \
  --volname "${APP_NAME}Installer" \
  --window-pos 200 120 \
  --window-size 600 400 \
  --icon-size 100 \
  --icon "$APP_NAME.app" 150 100 \
  --app-drop-link 450 100 \
  --hide-extension "$APP_NAME.app" \
  "$DMG_OUT" "$APP"

xcrun notarytool submit "$DMG_OUT" --keychain-profile "$NOTARY_PROFILE" --wait
xcrun stapler staple "$DMG_OUT"

# --- Sparkle signature -------------------------------------------------------
"$ROOT_DIR/sparkle/bin/sign_update" "$DMG_OUT"