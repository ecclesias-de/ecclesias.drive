set( APPLICATION_NAME       "Ecclesias Drive" )
set( APPLICATION_SHORTNAME  "ecclesiasdrive" )
set( APPLICATION_EXECUTABLE "ecclesiasdrive" )
set( APPLICATION_DOMAIN     "ecclesias.de" )
set( APPLICATION_BASE       "owncloud" )
set( APPLICATION_VENDOR     "Metaways Infosystems GmbH" )
set( APPLICATION_UPDATE_URL "https://api.tine20.net/driveClientUpdateCheck/" CACHE STRING "URL for updater" )
set( APPLICATION_ICON_NAME  "ecclesiasdrive" )
set( APPLICATION_VIRTUALFILE_SUFFIX "ecclesiasdrive" CACHE STRING "Virtual file suffix (not including the .)")

set( LINUX_PACKAGE_SHORTNAME "ecclesiasdrive" )

set( THEME_CLASS            "ecclesiasdriveTheme" )
set( APPLICATION_REV_DOMAIN "com.tine20.ecclesiasdesktopclient" )
set( WIN_SETUP_BITMAP_PATH  "${CMAKE_SOURCE_DIR}/admin/win/nsi" )

set( MAC_INSTALLER_BACKGROUND_FILE "${CMAKE_SOURCE_DIR}/admin/osx/installer-background.png" CACHE STRING "The MacOSX installer background image")

# set( THEME_INCLUDE          "${OEM_THEME_DIR}/mytheme.h" )
# set( APPLICATION_LICENSE    "${OEM_THEME_DIR}/license.txt )

option( WITH_CRASHREPORTER "Build crashreporter" OFF )
set( CRASHREPORTER_SUBMIT_URL "https://api.tine20.net/submit/" CACHE STRING "URL for crash reporter" )

