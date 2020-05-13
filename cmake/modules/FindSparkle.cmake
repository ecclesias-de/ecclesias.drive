# Find Sparkle.framework
#
# Once done this will define
#  SPARKLE_FOUND - system has Sparkle
#  SPARKLE_LIBRARY - The framework needed to use Sparkle
# Copyright (c) 2009, Vittorio Giovara <vittorio.giovara@gmail.com>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.

include(FindPackageHandleStandardArgs)

file(GLOB USR_LOCAL_HINT "/usr/local/Sparkle-[1-9]*/")
file(GLOB HOMEBREW_HINT "/usr/local/Caskroom/sparkle/[1-9]*/")

find_path(SPARKLE_INCLUDE_DIR Sparkle.h
  HINTS ${USR_LOCAL_HINT} ${HOMEBREW_HINT}
)
find_library(SPARKLE_LIBRARY NAMES Sparkle
  HINTS ${USR_LOCAL_HINT} ${HOMEBREW_HINT}
)

find_package_handle_standard_args(Sparkle DEFAULT_MSG SPARKLE_INCLUDE_DIR SPARKLE_LIBRARY)

if(SPARKLE_FOUND)
  set(SPARKLE_LIBRARIES ${SPARKLE_LIBRARY} )
  set(SPARKLE_INCLUDE_DIRS ${SPARKLE_INCLUDE_DIR} )
else(SPARKLE_FOUND)
  set(SPARKLE_LIBRARIES )
  set(SPARKLE_INCLUDE_DIRS )
endif(SPARKLE_FOUND)

mark_as_advanced(SPARKLE_INCLUDE_DIR SPARKLE_LIBRARY)


