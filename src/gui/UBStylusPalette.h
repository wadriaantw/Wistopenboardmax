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
#include <QElapsedTimer>
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

        // WistOpenboard fork: one popup serves pen, marker and eraser -- same
        // layout, different backing settings; the eraser hides the colour grid.
        enum class PopupTool { Pen, Marker, Eraser };

        // Re-read colour and width from settings before showing.
        void refresh(PopupTool tool);

    private slots:
        void pickCustomColour();
        void widthChanged(int sliderValue);

    protected:
        virtual void showEvent(QShowEvent* event) override;
        virtual void hideEvent(QHideEvent* event) override;
        virtual bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        void buildColourGrid();

        QGridLayout* mColourGrid = nullptr;
        QSlider* mWidthSlider = nullptr;
        QLabel* mWidthPreview = nullptr;
        QElapsedTimer mShownAt;
        PopupTool mTool = PopupTool::Pen;
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


        // WistOpenboard fork: press and hold the pen to pick colour and width
        // without going up to the top toolbar -- which in full screen is hidden.
        void showToolProperties(UBPenPropertiesPopup::PopupTool tool, QAction* anchorAction);

        // WistOpenboard fork: shrink the palette to pen + eraser so it takes up
        // almost no board, with an arrow to bring the rest back.
        void setCollapsed(bool collapsed);

    protected:
        // WistOpenboard fork: slimmer margins than the base palette, plus a wider
        // strip at the top to drag with a finger. Overridden because the base
        // class re-applies its own margins whenever it relayouts.
        virtual void updateLayout() override;
        virtual void paintEvent(QPaintEvent* event) override;

    private:
        int mLastSelectedId;

        QAction* mCollapseAction = nullptr;
        UBPenPropertiesPopup* mPenPropertiesPopup = nullptr;

    signals:
        void stylusToolDoubleClicked(int tool);
};

#endif /* UBSTYLUSPALLETTE_H_ */
