set( APPLICATION_NAME       "Tine Drive" )
set( APPLICATION_SHORTNAME  "tineDrive" )
set( APPLICATION_EXECUTABLE "tineDrive" )
set( APPLICATION_DOMAIN     "tine20.com" )
set( APPLICATION_VENDOR     "Metaways Infosystems GmbH" )
set( APPLICATION_UPDATE_URL "https://api.tine20.net/driveClientUpdateCheck/" CACHE STRING "URL for updater" )
set( APPLICATION_ICON_NAME  "tineDrive" )
set( APPLICATION_VIRTUALFILE_SUFFIX "tineDrive" CACHE STRING "Virtual file suffix (not including the .)")

set( LINUX_PACKAGE_SHORTNAME "tineDrive" )

set( THEME_CLASS            "ownCloudTheme" )
set( APPLICATION_REV_DOMAIN "com.tine20.desktopclient" )
set( WIN_SETUP_BITMAP_PATH  "${CMAKE_SOURCE_DIR}/admin/win/nsi" )

set( MAC_INSTALLER_BACKGROUND_FILE "${CMAKE_SOURCE_DIR}/admin/osx/tineDrive-installer-background.png" CACHE STRING "The MacOSX installer background image")

# set( THEME_INCLUDE          "${OEM_THEME_DIR}/mytheme.h" )
# set( APPLICATION_LICENSE    "${OEM_THEME_DIR}/license.txt )

option( WITH_CRASHREPORTER "Build crashreporter" OFF )
set( CRASHREPORTER_SUBMIT_URL "https://api.tine20.net/submit/" CACHE STRING "URL for crash reporter" )

