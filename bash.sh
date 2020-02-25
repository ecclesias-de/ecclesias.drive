#!/bin/bash

if [ $# -lt 1 ]; then
  echo "Usage: $(basename $0) directory_relative_to_home [uid]"
  exit
fi

useradd user -u ${2:-1000}
su - user << EOF
  apt-get update && apt-get install -y gnupg2 gnupg gnupg1
  apt-key adv –keyserver keyserver.ubuntu.com –recv-keys 4ABE1AC7557BEFF9
  echo 'deb-src http://download.opensuse.org/repositories/isv:/ownCloud:/desktop/Ubuntu_18.04/ /' >> /etc/apt/sources.list.d/owncloud-client.list
  apt-get build-dep owncloud-client 
  apt-get install -y git
  cd /home/user/$1
  rm -rf build-linux
  mkdir build-linux
  cd build-linux
  cmake ..
  make -j4
  make package
EOF
