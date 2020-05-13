#!/bin/bash

# Uncomment to enable automatic updates using Sparkle
SPARKLE_VERSION=1.22.0
no_build=false

 if [ -n "$installed_sparkle_version" ]; then
        echo "Uninstalling Sparkle:"
        sudo rm -rf "/usr/local/Sparkle-$installed_sparkle_version"
        if [ "$#" -eq 1 ] && [ "$1" = "-r" ] ; then
            rm -f "Sparkle-$installed_sparkle_version.tar.bz2"
        fi

        installed_sparkle_version=""
fi

if [ "$SPARKLE_VERSION" ] && [ ! -f sparkle-$SPARKLE_VERSION-done ] ; then
    echo "Downloading and installing Sparkle:"
    #
    # Download the tarball and unpack it in /usr/local/Sparkle-x.y.z
    #
    [ -f Sparkle-$SPARKLE_VERSION.tar.bz2 ] || curl -L -o Sparkle-$SPARKLE_VERSION.tar.bz2 https://github.com/sparkle-project/Sparkle/releases/download/$SPARKLE_VERSION/Sparkle-$SPARKLE_VERSION.tar.bz2 || exit 1
    $no_build && echo "Skipping installation" && return
    test -d "/usr/local/Sparkle-$SPARKLE_VERSION" || sudo mkdir "/usr/local/Sparkle-$SPARKLE_VERSION"
    sudo tar -C "/usr/local/Sparkle-$SPARKLE_VERSION" -xpof Sparkle-$SPARKLE_VERSION.tar.bz2
    touch sparkle-$SPARKLE_VERSION-done
    sudo cp -R /usr/local/Sparkle-$SPARKLE_VERSION/Sparkle.framework /usr/local/opt/qt5/lib/
fi
