#!/bin/sh -xe

app_name=@APPLICATION_SHORTNAME@
version=@MIRALL_VERSION_FULL@
src_app_folder="/Users/chingencheng/CraftMaster/macos-64-clang/build/$app_name/$app_name-client/archive/Applications/KDE/"
identity="Developer ID Application: Metaways Infosystems GmbH (TC62G4GBN8)"
github_repo_dir="/Users/chingencheng/$app_name/"

cd "$src_app_folder"

echo "Signing contents"
find "$app_name.app" -name "*.dylib" -o -name "*.exe" -o -name "*.app"| while read fname; do sudo codesign \
        --sign "$identity" \
        --options=runtime \
        --timestamp \
        --force \
        --preserve-metadata=entitlements \
        --deep \
        --verbose=4 \
        "$fname";
done

echo "Signing application"
    sudo codesign \
        --sign "$identity" \
        --options=runtime \
        --timestamp \
        --force \
        --preserve-metadata=entitlements \
        --deep \
        --verbose=4 \
        "$app_name.app"

# Verify the signature
codesign --deep -vvv --verify "$app_name.app"
sudo /usr/bin/ditto -c -k --keepParent "$app_name.app" "$app_name.zip"

#when do notary the first time , we could enter the apple id and App-specific pwd
#to find app specific password , go to https://appleid.apple.com/ , 
#it is one time generated password , if forget of lost just create a new one
#Profile name:
#c.cheng@metaways.de
#App-specific password for c.cheng@metaways.de: 
#Validating your credentials...
#Success. Credentials validated.
#Credentials saved to Keychain.
#To use them, specify `--keychain-profile "c.cheng@metaways.de"`

sudo xcrun notarytool submit "$app_name.zip"  --keychain-profile "c.cheng@metaways.de" --wait --webhook "https://example.com/notarization"
sudo xcrun stapler staple "$app_name.app"

spctl -a -t open --context context:primary-signature -v "$app_name.app"
spctl -a -t exec -vv "$app_name.app"

echo "Do packaging and sign the package"
cd 
# create package again from signed archive app
sudo python3 CraftMaster/macos-64-clang/craft/bin/craft.py --no-cache --src-dir "$github_repo_dir" --package "$app_name"

find "$github_repo_dir/binaries" -name "$app_name-client*.pkg" | while read fname; do sudo productsign \
        --sign "Developer ID Installer: Metaways Infosystems GmbH (TC62G4GBN8)" \
        "$fname" \
        "$github_repo_dir/binaries/$app_name-$version.pkg"
done

#xcrun notarytool log cfe00c3b-9d79-439a-b085-5c772fb257e3 --keychain-profile "c.cheng@metaways.de" developer_log.json
