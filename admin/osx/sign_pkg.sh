#!/bin/sh -xe

APP_NAME="ecclesiasdrive"
PLATFORM="macos-clang-arm64"
VERSION="6.0.3.1"
SRC_DIR="/Users/ccheng/packaging-ecclesiasdrive/$PLATFORM"
APP="$SRC_DIR/$APP_NAME.app"
DMG_SRC="$APP_NAME-client-$APP_NAME-$VERSION-master-$PLATFORM.dmg"
PKG_UNSIGNED="$SRC_DIR/$APP_NAME-$VERSION-unsigned.pkg"
PKG_OUT="$SRC_DIR/$APP_NAME-$VERSION.pkg"
CERT_APP="Developer ID Application: Metaways Infosystems GmbH (TC62G4GBN8)"
CERT_PKG="Developer ID Installer: Metaways Infosystems GmbH (TC62G4GBN8)"
NOTARY_PROFILE="c.cheng@metaways.de"
BUNDLE_ID="com.tine20.desktopclient"

sign() {
  codesign --force --timestamp --options runtime --sign "$CERT_APP" "$1"
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

# Verify app signature
codesign --verify --deep --strict --verbose=2 "$APP"

# Build PKG
pkgbuild \
  --component "$APP" \
  --install-location "/Applications" \
  --identifier "$BUNDLE_ID" \
  --version "$VERSION" \
  "$PKG_UNSIGNED"

# Sign PKG with Developer ID Installer cert
productsign \
  --sign "$CERT_PKG" \
  "$PKG_UNSIGNED" \
  "$PKG_OUT"

rm -f "$PKG_UNSIGNED"

# Verify pkg signature
pkgutil --check-signature "$PKG_OUT"

# Notarize PKG
xcrun notarytool submit "$PKG_OUT" --keychain-profile "$NOTARY_PROFILE" --wait

# Staple PKG
xcrun stapler staple "$PKG_OUT"

echo "Done: $PKG_OUT"