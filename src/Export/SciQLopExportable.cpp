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

#include "SciQLopPlots/Export/SciQLopExportable.hpp"

#include <QFileInfo>
#include <QLayout>
#include <QPdfWriter>
#include <QPixmap>
#include <QRegion>
#include <QWidget>
#include <qcustomplot.h>

namespace
{

// Paints w even when hidden: a save asks for that widget, and a top-level widget never shown
// counts as hidden.
void paint_widget(QWidget* w, QPainter* painter, const QRect& target, SciQLopExportTarget kind)
{
    if (!w || target.isEmpty())
        return;

    if (auto* exportable = dynamic_cast<SciQLopExportable*>(w))
    {
        exportable->export_paint(painter, target, kind);
        return;
    }

    // Generic fallback: scale the widget's software render into `target`.
    const int ww = w->width();
    const int wh = w->height();
    if (ww <= 0 || wh <= 0)
        return;

    painter->save();
    painter->translate(target.topLeft());
    painter->scale(static_cast<double>(target.width()) / ww,
                   static_cast<double>(target.height()) / wh);
    w->render(painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter->restore();
}

} // namespace

void export_widget(QWidget* w, QPainter* painter, const QRect& target,
                   SciQLopExportTarget kind)
{
    // isHidden(), not !isVisible(): skip only explicitly-hidden widgets, so
    // export still works for a panel rendered before it is shown (offscreen).
    if (w && !w->isHidden())
        paint_widget(w, painter, target, kind);
}

void export_children(const QWidget* parent, const QList<QWidget*>& children, QPainter* painter,
                     const QRect& target, SciQLopExportTarget kind)
{
    const int pw = parent->width();
    const int ph = parent->height();
    if (pw <= 0 || ph <= 0 || target.isEmpty())
        return;
    // A widget never shown has never laid out its children: they would all sit at Qt's default
    // geometry.
    if (auto* layout = parent->layout())
        layout->activate();

    const double sx = static_cast<double>(target.width()) / pw;
    const double sy = static_cast<double>(target.height()) / ph;

    for (auto* child : children)
    {
        if (!child || child->isHidden()) // skip only explicitly-hidden children
            continue;
        const QRect g = child->geometry();
        const QRect child_target(target.x() + qRound(g.x() * sx), target.y() + qRound(g.y() * sy),
                                 qRound(g.width() * sx), qRound(g.height() * sy));
        export_widget(child, painter, child_target, kind);
    }
}

bool save_widget_pdf(QWidget* w, const QString& filename, int width, int height)
{
    const int totalW = (width > 0) ? width : w->width();
    const int totalH = (height > 0) ? height : w->height();
    if (totalW <= 0 || totalH <= 0)
        return false;

    QPdfWriter writer(filename);
    writer.setPageSize(QPageSize(QSizeF(totalW, totalH), QPageSize::Point));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setResolution(72);

    QCPPainter painter(&writer);
    if (!painter.isActive())
        return false;
    painter.setMode(QCPPainter::pmVectorized);
    painter.setMode(QCPPainter::pmNoCaching);

    paint_widget(w, &painter, QRect(0, 0, totalW, totalH), SciQLopExportTarget::Vector);
    painter.end();
    return true;
}

bool save_widget_raster(QWidget* w, const QString& filename, const char* format, int width,
                        int height, double scale, int quality)
{
    if (!w)
        return false;

    const auto fullSize = w->sizeHint().expandedTo(w->size());
    const int targetW = (width > 0) ? width : static_cast<int>(fullSize.width() * scale);
    const int targetH = (height > 0) ? height : static_cast<int>(fullSize.height() * scale);
    if (targetW <= 0 || targetH <= 0)
        return false;

    QPixmap pixmap(targetW, targetH);
    // The widget background, so gaps and margins between plots match the theme instead of
    // staying transparent.
    pixmap.fill(w->palette().color(w->backgroundRole()));

    QCPPainter painter(&pixmap);
    if (!painter.isActive())
        return false;
    paint_widget(w, &painter, QRect(0, 0, targetW, targetH), SciQLopExportTarget::Raster);
    painter.end();

    if (pixmap.isNull())
        return false;
    return pixmap.save(filename, format, quality);
}

bool save_widget(QWidget* w, const QString& filename, int width, int height, double scale,
                 int quality)
{
    const auto ext = QFileInfo(filename).suffix().toLower();
    if (ext == "pdf")
        return save_widget_pdf(w, filename, width, height);
    if (ext == "png")
        return save_widget_raster(w, filename, "PNG", width, height, scale, quality);
    if (ext == "jpg" || ext == "jpeg")
        return save_widget_raster(w, filename, "JPG", width, height, scale, quality);
    if (ext == "bmp")
        return save_widget_raster(w, filename, "BMP", width, height, scale, -1);
    return false;
}
