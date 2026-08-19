/*
 * Copyright (C) 2015-2022 Département de l'Instruction Publique (DIP-SEM)
 *
 * Copyright (C) 2013 Open Education Foundation
 *
 * Copyright (C) 2010-2013 Groupement d'Intérêt Public pour
 * l'Education Numérique en Afrique (GIP ENA)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License,
 * with a specific linking exception for the OpenSSL project's
 * "OpenSSL" library (or with modified versions of it that use the
 * same license as the "OpenSSL" library).
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */




#ifndef UBSTYLUSPALLETTE_H_
#define UBSTYLUSPALLETTE_H_

#include <QButtonGroup>
#include <QGridLayout>
#include <QWidget>
#include <QList>
#include <QPointer>

class QToolBar;
class QSlider;
class QLabel;

#include "UBActionPalette.h"



// WistOpenboard fork: the pen's colour and width, reachable by holding the pen
// button on the floating palette. Matters most in full screen, where the top
// toolbar that normally carries these is hidden.
class UBPenPropertiesPopup : public QWidget
{
    Q_OBJECT

    public:
        explicit UBPenPropertiesPopup(QWidget* parent = nullptr);

        // Re-read colour and width from settings before showing.
        void refresh();

    private slots:
        void pickCustomColour();
        void widthChanged(int sliderValue);

    private:
        void buildColourGrid();

        QGridLayout* mColourGrid = nullptr;
        QSlider* mWidthSlider = nullptr;
        QLabel* mWidthPreview = nullptr;
};

class UBStylusPalette : public UBActionPalette
{
    Q_OBJECT

    public:
        UBStylusPalette(QWidget *parent = 0, Qt::Orientation orient = Qt::Vertical);
        virtual ~UBStylusPalette();

        void initPosition();

    private slots:

        void stylusToolDoubleClicked();

        // WistOpenboard fork: hide the window title bar and the top toolbars so the
        // whole screen is board. The floating palette stays, which is how the
        // teacher gets back out again.
        void toggleFullScreen(bool on);

        // WistOpenboard fork: press and hold the pen to pick colour and width
        // without going up to the top toolbar -- which in full screen is hidden.
        void showPenProperties();

    private:
        int mLastSelectedId;

        QAction* mFullScreenAction = nullptr;
        UBPenPropertiesPopup* mPenPropertiesPopup = nullptr;
        QList<QPointer<QToolBar> > mHiddenToolBars;   // restored on leaving full screen
        bool mWasMaximized = false;

    signals:
        void stylusToolDoubleClicked(int tool);
};

#endif /* UBSTYLUSPALLETTE_H_ */
