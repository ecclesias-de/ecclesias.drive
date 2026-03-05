#!/bin/sh -xe

APP_NAME="ecclesiasdrive"
PLATFORM="macos-clang-arm64"
VERSION="6.0.3.1"
VERSIONSHORT="6.0.3"
ROOT_DIR="/Users/ccheng"
SRC_DIR="/Users/ccheng/packaging/$APP_NAME-$VERSION-$PLATFORM"
APP="$SRC_DIR/$APP_NAME.app"
DMG_SRC="$APP_NAME-client-$APP_NAME-$VERSIONSHORT-master-$PLATFORM.dmg"
DMG_OUT="$SRC_DIR/$APP_NAME-$VERSION.dmg"
CERT="Developer ID Application: Metaways Infosystems GmbH (TC62G4GBN8)"
NOTARY_PROFILE="c.cheng@metaways.de"

sign() {
  codesign --force --timestamp --options runtime --sign "$CERT" "$1"
}

cd "$SRC_DIR"

# Extract DMG
hdiutil attach "$DMG_SRC"
ditto "/Volumes/$DMG_SRC/$APP_NAME.app" "$APP"

# Sign in order: binaries → xpc → frameworks → app (inner to outer)
find "$APP" -type f | while read f; do file "$f" | grep -q "Mach-O" && sign "$f"; done
find "$APP" -name "*.xpc" | while read f; do sign "$f"; done
find "$APP/Contents/Frameworks" -name "*.framework" | sort -r | while read f; do sign "$f"; done
sign "$APP"

# Verify
codesign --verify --deep --strict --verbose=2 "$APP"

# Notarize app
rm -f "$APP_NAME.zip"
/usr/bin/ditto -c -k --keepParent "$APP_NAME.app" "$APP_NAME.zip"
xcrun notarytool submit "$APP_NAME.zip" --keychain-profile "$NOTARY_PROFILE" --wait
xcrun stapler staple "$APP_NAME.app"

# Create, notarize and staple DMG
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
$ROOT_DIR/sparkle/bin/sign_update "$APP_NAME-$VERSION.dmg"
