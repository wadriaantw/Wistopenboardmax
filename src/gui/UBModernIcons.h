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
class QIcon;

namespace UBModernIcons
{
    void apply(UBMainWindow* mainWindow);

    // For the hand-built toolbar buttons that are not QActions.
    QIcon zoomInIcon();
    QIcon zoomOutIcon();
    QIcon toolsIcon();
    QIcon boardIcon();
    QIcon mathToolsIcon();
    QIcon captureAreaIcon();
    QIcon captureScreenIcon();
    QIcon showHideIcon();       // eye open (On) / closed (Off)
    QIcon chevronLeftIcon();
    QIcon chevronRightIcon();
    QIcon clearIcon();          // trash can (Clear Ink on the web toolbar)
    QIcon geometryIcon();       // drawing compass (Geometry submenu)
    QIcon scienceIcon();        // flask (Science submenu)
    QIcon mediaIcon();          // play card (Media submenu)
    QIcon bookmarkIcon();       // amber ribbon (Shortcuts dropdown)
}

#endif /* UBMODERNICONS_H_ */
