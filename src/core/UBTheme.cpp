/*
 * WistOpenboard fork. See UBTheme.h.
 */

#include "UBTheme.h"

#include "core/UBSettings.h"

namespace UBTheme
{

namespace
{
    bool sDark = false;
    bool sInitialized = false;
}

void initialize(bool dark)
{
    sDark = dark;
    sInitialized = true;
}

bool isDark()
{
    // Normally initialised by UBSettings before anyone asks. The fallback is
    // for a call that somehow arrives first -- NEVER from inside the UBSettings
    // constructor (see header).
    if (!sInitialized)
        initialize(UBSettings::settings()->appDarkTheme->get().toBool());

    return sDark;
}

QColor ink()             { return isDark() ? QColor(0xE8, 0xE8, 0xE6) : QColor(0x3A, 0x3A, 0x38); }
QColor inkMuted()        { return isDark() ? QColor(0x9A, 0x9A, 0x96) : QColor(0x8A, 0x8A, 0x86); }
QColor hairline()        { return isDark() ? QColor(0x3E, 0x3E, 0x3C) : QColor(0xE5, 0xE5, 0xE3); }
QColor ring()            { return isDark() ? QColor(0x4A, 0x4A, 0x48) : QColor(0xD0, 0xD0, 0xCC); }
QColor surface()         { return isDark() ? QColor(0x26, 0x26, 0x24) : QColor(0xFF, 0xFF, 0xFF); }
QColor surfaceMuted()    { return isDark() ? QColor(0x30, 0x30, 0x2E) : QColor(0xF2, 0xF2, 0xF0); }
QColor surfacePressed()  { return isDark() ? QColor(0x3A, 0x3A, 0x38) : QColor(0xE5, 0xE3, 0xDC); }
QColor window()          { return isDark() ? QColor(0x1E, 0x1E, 0x1C) : QColor(0xF7, 0xF7, 0xF5); }

QColor accentRed()   { return QColor(0xD8, 0x5A, 0x30); }
QColor accentBlue()  { return QColor(0x37, 0x8A, 0xDD); }
QColor accentGreen() { return QColor(0x63, 0x99, 0x22); }
QColor accentAmber() { return QColor(0xEF, 0x9F, 0x27); }

QColor paletteBackground()
{
    QColor background = surface();
    background.setAlpha(246);

    return background;
}

QString hex(const QColor& color)
{
    return color.name(QColor::HexRgb);
}

}
