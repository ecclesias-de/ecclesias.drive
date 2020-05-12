# tin20drive Desktop Client

[![Build Status](https://drone.owncloud.com/api/badges/owncloud/client/status.svg)](https://drone.owncloud.com/owncloud/client) [![Build status](https://ci.appveyor.com/api/projects/status/a1x3dslys7de6e21/branch/master?svg=true)](https://ci.appveyor.com/project/ownclouders/client/branch/master)


## Introduction

The tin20drive Desktop Client is a tool to synchronize files from ownCloud Server
with your computer.

## Download

### Binary packages

* Refer to the download page https://owncloud.org/download/#owncloud-desktop-client

### Source code

The ownCloud Desktop Client is developed in Git. Since Git makes it easy to
fork and improve the source code and to adapt it to your need, many copies
can be found on the Internet, in particular on GitHub. However, the
authoritative repository maintained by the developers is located at
https://github.com/owncloud/client.

## Building the source code



## Reporting issues and contributing

If you find any bugs or have any suggestion for improvement, please
file an issue at https://github.com/owncloud/client/issues. Do not
contact the authors directly by mail, as this increases the chance
of your report being lost.

If you created a patch, please submit a [Pull
Request](https://github.com/owncloud/client/pulls). For non-trivial
patches, we need you to sign the [Contributor
Agreement](https://owncloud.org/contribute/agreement) before
we can accept your patch.


## Maintainers and Contributors



## Building the Documentation

The documentation has been migrated from Sphinx-Doc to [Antora](https://docs.antora.org/), which is based on [the AsciiDoc format](https://github.com/owncloud/docs/blob/master/docs/getting-started.md).
For the moment, it is not possible to generate the client documentation directly, as was previously the case, via a CMake command.
Instead, the client documentation is built when the entire ownCloud documentation is built; (this is a combination of the _administration_, _developer_, and _user_, _iOS_, and _Android_ manuals).

**Please note:** investigations are underway to see if it can be built both standalone and as as part of the larger documentation bundle.
This README will be updated as more information is available.

## License

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
    for more details.


