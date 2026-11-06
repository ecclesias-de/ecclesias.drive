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

# Finder extension: must equal APPLICATION_REV_DOMAIN (SOCKETAPI_TEAM_IDENTIFIER_PREFIX is empty)
APP_GROUP="com.tine20.ecclesiasdesktopclient"
EXT="$APP/Contents/PlugIns/FinderSyncExt.appex"
EXT_ENT="$SRC_DIR/FinderSyncExt.entitlements"

sign() {
  codesign --force --timestamp --options runtime --sign "$CERT" "$1"
}

# --- Prepare working dir + extension entitlements ----------------------------
mkdir -p "$SRC_DIR"

cat > "$EXT_ENT" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>com.apple.security.app-sandbox</key><true/>
<key>com.apple.security.application-groups</key>
<array><string>$APP_GROUP</string></array>
</dict></plist>
EOF

# --- Extract zip if present (overwrites existing files) ----------------------
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

# --- Extract app from DMG (fresh copy, no stale signatures) -------------------
rm -rf "$APP"
MOUNT_POINT="$(mktemp -d)"
hdiutil attach "$DMG_SRC" -mountpoint "$MOUNT_POINT" -nobrowse -readonly
ditto "$MOUNT_POINT/$APP_NAME.app" "$APP"
hdiutil detach "$MOUNT_POINT" || hdiutil detach -force "$MOUNT_POINT" || true

# --- Sign inner to outer: binaries -> xpc -> frameworks -> appex -> app ------
# The Finder extension is excluded here and signed separately WITH entitlements.
find "$APP" -type f -not -path "*/FinderSyncExt.appex/*" -print0 | while IFS= read -r -d '' f; do
  if file "$f" | grep -q "Mach-O"; then sign "$f"; fi
done
find "$APP" -name "*.xpc" -print0 | while IFS= read -r -d '' f; do sign "$f"; done
find "$APP/Contents/Frameworks" -name "*.framework" | sort -r | while IFS= read -r f; do sign "$f"; done

codesign --force --timestamp --options runtime --entitlements "$EXT_ENT" --sign "$CERT" "$EXT"

# Seal the app last. Do NOT use --deep: it would re-sign the extension without entitlements.
sign "$APP"

# --- Verify ------------------------------------------------------------------
codesign --verify --deep --strict --verbose=2 "$APP"

EXT_ENT_OUT="$(codesign -d --entitlements :- "$EXT" 2>/dev/null)"
echo "$EXT_ENT_OUT" | grep -q "com.apple.security.app-sandbox" \
  || { echo "FinderSyncExt: sandbox entitlement missing" >&2; exit 1; }
echo "$EXT_ENT_OUT" | grep -q "<string>$APP_GROUP</string>" \
  || { echo "FinderSyncExt: app group missing or wrong" >&2; exit 1; }

# Informational: the extension's Info.plist should resolve to the same group as APP_GROUP
plutil -p "$EXT/Contents/Info.plist" | grep -i -E "OC_|BundleIdentifier" || true

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