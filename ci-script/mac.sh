#!/bin/bash

# Script for MAC build of gitlab ci 

whoami
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH=/usr/local/opt/qt5 -DCMAKE_INSTALL_PREFIX=$CI_PROJECT_DIR/install/ -DNO_SHIBBOLETH=1 ..
make install
sudo make macdeployqt
sudo $CI_PROJECT_DIR/admin/osx/sign_app.sh $CI_PROJECT_DIR/install/tine20drive.app "$DEVELOPER_ID_APPLICATION" "$TEAM_IDENTITY"
sudo admin/osx/create_mac.sh $CI_PROJECT_DIR/install/ $PWD "$DEVELOPER_ID_INSTALLER" "$APPLE_ID_MAIL" "$APPLE_APP_PASSWORD" "$ASC_PROVIDER"
