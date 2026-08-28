/*
 * WistOpenboard fork.
 *
 * The minimalist skin's palette, in ONE place. Every custom-painted widget
 * (zen buttons, dock chevrons, palette dots, pen popup, tab strip, clock)
 * reads these instead of carrying its own hex literals -- one drifted value
 * and the skin stops looking deliberate.
 *
 * Also the seam for dark mode: App/DarkTheme=true flips every colour here,
 * and UBApplication loads etc/OpenBoard-dark.css instead of the light file.
 * The theme is decided once at startup; switching requires a restart.
 */

#ifndef UBTHEME_H_
#define UBTHEME_H_

#include <QColor>
#include <QString>

namespace UBTheme
{
    // Called by UBSettings once its members exist. isDark() lazily reading
    // settings from inside the UBSettings CONSTRUCTOR deadlocked startup: the
    // singleton pointer is only assigned after construction, so the lazy read
    // re-entered UBSettings::settings(), constructed a second instance, and
    // re-entered isDark() while its static guard was mid-initialisation.
    void initialize(bool dark);

    bool isDark();

    QColor ink();              // primary text and glyphs
    QColor inkMuted();         // secondary text (clock, page labels)
    QColor hairline();         // subtle separators
    QColor ring();             // visible borders on controls
    QColor surface();          // cards, popups, palettes
    QColor surfaceMuted();     // quiet fills, hover
    QColor surfacePressed();   // pressed fills
    QColor window();           // main window background

    QColor paletteBackground();    // floating palettes (surface + alpha)

    // Icon accents: one coloured element per glyph keeps the set lively
    // without returning to the glossy look. Same hues in dark mode.
    QColor accentRed();
    QColor accentBlue();
    QColor accentGreen();
    QColor accentAmber();

    QString hex(const QColor& color);   // "#RRGGBB" for stylesheet strings
}

#endif /* UBTHEME_H_ */
