/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef TINE20DRIVE_MIRALL_THEME_H
#define TINE20DRIVE_MIRALL_THEME_H

#include "theme.h"

namespace OCC {

/**
 * @brief The tine20driveTheme class
 * @ingroup libsync
 */
class tine20driveTheme : public Theme
{
    Q_OBJECT
public:
    tine20driveTheme();
#ifndef TOKEN_AUTH_ONLY
    QVariant customMedia(CustomMediaType type) Q_DECL_OVERRIDE;

    QColor wizardHeaderBackgroundColor() const Q_DECL_OVERRIDE;
    QColor wizardHeaderTitleColor() const Q_DECL_OVERRIDE;
    QPixmap wizardHeaderLogo() const Q_DECL_OVERRIDE;
#endif

    // For tine20driveTheme-brandings *do* show the virtual files option.
    bool showVirtualFilesOption() const override { return true; }

private:
};
}
#endif // TINE20DRIVE_MIRALL_THEME_H
