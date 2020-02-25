#!/bin/bash

# Script for MAC build of gitlab ci 

whoami
cd admin/win/docker
docker build . -t owncloud-client-win32:2.5.4
cd ../../..
docker run -v "$CI_PROJECT_DIR:/home/user/client" owncloud-client-win32:2.5.4  /home/user/client/admin/win/docker/build.sh client/  $(id -u)
ls -al $CI_PROJECT_DIR