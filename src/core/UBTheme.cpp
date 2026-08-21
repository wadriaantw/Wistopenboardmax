/*
 * WistOpenboard fork. See UBTheme.h.
 */

#include "UBTheme.h"

#include "core/UBSettings.h"

namespace UBTheme
{

bool isDark()
{
    // Decided once: widgets built at startup never re-read it, so flipping the
    // setting mid-session would leave the UI half-and-half.
    static const bool dark = UBSettings::settings()->appDarkTheme->get().toBool();

    return dark;
}

QColor ink()             { return isDark() ? QColor(0xE8, 0xE8, 0xE6) : QColor(0x3A, 0x3A, 0x38); }
QColor inkMuted()        { return isDark() ? QColor(0x9A, 0x9A, 0x96) : QColor(0x8A, 0x8A, 0x86); }
QColor hairline()        { return isDark() ? QColor(0x3E, 0x3E, 0x3C) : QColor(0xE5, 0xE5, 0xE3); }
QColor ring()            { return isDark() ? QColor(0x4A, 0x4A, 0x48) : QColor(0xD0, 0xD0, 0xCC); }
QColor surface()         { return isDark() ? QColor(0x26, 0x26, 0x24) : QColor(0xFF, 0xFF, 0xFF); }
QColor surfaceMuted()    { return isDark() ? QColor(0x30, 0x30, 0x2E) : QColor(0xF2, 0xF2, 0xF0); }
QColor surfacePressed()  { return isDark() ? QColor(0x3A, 0x3A, 0x38) : QColor(0xE5, 0xE3, 0xDC); }
QColor window()          { return isDark() ? QColor(0x1E, 0x1E, 0x1C) : QColor(0xF7, 0xF7, 0xF5); }

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
