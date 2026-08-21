/*
 * WistOpenboard fork.
 *
 * The modern icon set: every everyday action's glyph drawn in code as a line
 * icon in the theme's ink colour, replacing the 2008-era skeuomorphic pixmaps
 * (glossy red pen, pink eraser, photoreal monitor) that clashed with the
 * minimalist chrome. Painted at 2x and assigned as QIcons, so they stay crisp
 * on high-DPI boards and follow the light/dark theme automatically.
 *
 * Applied once at startup over the actions the .ui files created -- the
 * resource PNGs stay untouched, so reverting is deleting one call.
 */

#ifndef UBMODERNICONS_H_
#define UBMODERNICONS_H_

class UBMainWindow;

namespace UBModernIcons
{
    void apply(UBMainWindow* mainWindow);
}

#endif /* UBMODERNICONS_H_ */
