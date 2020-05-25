#!/bin/sh -xe

[ "$#" -lt 2 ] && echo "Usage: sign_app.sh <app> <identity> <team_identifier>" && exit

src_app="$1"
identity="$2"
team_identifier="$3"

# sign Sparkle first
echo "Signing Sparkle's AutoUpdate.app"
	codesign \
		--sign "$2" \
		--force \
		--deep \
		--timestamp \
		--options runtime \
		--verbose \
		"$1/Contents/Frameworks/Sparkle.framework/Versions/A/Resources/AutoUpdate.app"
        
echo "Signing ecclesiasdrive.app"
    codesign \
        --sign "$2" \
        --options=runtime \
        --timestamp \
        --force \
        --preserve-metadata=entitlements \
        --verbose=4 \
        --deep \
        "$1"


# Verify the signature
codesign -dv $src_app
codesign --verify -v $src_app
# spctl -a -t exec -vv $src_app

# Validate that the key used for signing the binary matches the expected TeamIdentifier
# needed to pass the SocketApi through the sandbox
# codesign -dv $src_app 2>&1 | grep "TeamIdentifier=$team_identifier"
exit $?
