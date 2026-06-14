/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/

#pragma once
#include <QPainter>
#include <QRect>

class QWidget;

enum class SciQLopExportTarget
{
    Vector, // painting onto a vector device (QPdfWriter): keep plots vectorial
    Raster  // painting onto a raster device (QPixmap): rasterize each widget
};

// Capability mixin: a widget that can paint a higher-fidelity representation of
// itself than a generic QWidget::render() (true vector, GPU framebuffer, or a
// recursive sub-tree) implements this. Discovered by dynamic_cast cross-cast,
// so it is intentionally NOT a QObject.
class SciQLopExportable
{
public:
    virtual ~SciQLopExportable() = default;

    // Paint self into `target` (device/page coordinates) on `painter`.
    // Precondition: for SciQLopExportTarget::Vector the painter must be a
    // QCPPainter (the export entry points construct one) so plot implementations
    // can render true vector output via QCustomPlot::toPainter.
    virtual void export_paint(QPainter* painter, const QRect& target,
                              SciQLopExportTarget kind)
        = 0;
};

// Paint any widget into `target`. Uses SciQLopExportable when the widget
// implements it; otherwise scales QWidget::render() into `target` — which
// renders plain widgets (labels, matplotlib Qt canvases, ...) for free.
void export_widget(QWidget* w, QPainter* painter, const QRect& target,
                   SciQLopExportTarget kind);
