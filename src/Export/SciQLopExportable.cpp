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
#include <stdexcept>

#include <QFile>
#include <QFileInfo>
#include <QSvgGenerator>
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

// The widget background, so gaps and margins between plots match the theme instead of
// staying transparent (white paper in a PDF viewer).
void fill_background(QWidget* w, QPainter* painter, const QRect& target)
{
    painter->fillRect(target, w->palette().color(w->backgroundRole()));
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

    const QRect target(0, 0, totalW, totalH);
    fill_background(w, &painter, target);
    paint_widget(w, &painter, target, SciQLopExportTarget::Vector);
    painter.end();
    return true;
}

bool save_widget_svg(QWidget* w, const QString& filename, int width, int height)
{
    const int totalW = (width > 0) ? width : w->width();
    const int totalH = (height > 0) ? height : w->height();
    if (totalW <= 0 || totalH <= 0)
        return false;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    // SVG 1.1, not the default SVG Tiny 1.2: only 1.1 writes clip paths, so graphs stay inside
    // their plot area.
    QSvgGenerator generator(QSvgGenerator::SvgVersion::Svg11);
    generator.setOutputDevice(&file);
    generator.setSize(QSize(totalW, totalH));
    generator.setViewBox(QRect(0, 0, totalW, totalH));

    QCPPainter painter(&generator);
    if (!painter.isActive())
        return false;
    painter.setMode(QCPPainter::pmVectorized);
    painter.setMode(QCPPainter::pmNoCaching);

    const QRect target(0, 0, totalW, totalH);
    fill_background(w, &painter, target);
    paint_widget(w, &painter, target, SciQLopExportTarget::Vector);
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
    QCPPainter painter(&pixmap);
    if (!painter.isActive())
        return false;
    const QRect target(0, 0, targetW, targetH);
    fill_background(w, &painter, target);
    paint_widget(w, &painter, target, SciQLopExportTarget::Raster);
    painter.end();

    if (pixmap.isNull())
        return false;
    return pixmap.save(filename, format, quality);
}

namespace
{
const QStringList save_formats = {"pdf", "svg", "png", "jpg", "bmp"};

QString normalized_format(const QString& format)
{
    auto name = format.trimmed().toLower();
    if (name.startsWith('.'))
        name.remove(0, 1);
    return name == "jpeg" ? QString("jpg") : name;
}
} // namespace

QString resolve_save_format(const QString& filename, const QString& format)
{
    if (format.isEmpty())
    {
        const auto ext = normalized_format(QFileInfo(filename).suffix());
        if (!save_formats.contains(ext))
            throw_unknown_save_format(filename);
        return ext;
    }
    const auto name = normalized_format(format);
    if (!save_formats.contains(name))
        throw std::invalid_argument("can't save " + filename.toStdString() + ": unknown format '"
                                    + format.toStdString() + "' (pdf, svg, png, jpg or bmp)");
    return name;
}

bool save_widget(QWidget* w, const QString& filename, int width, int height, double scale,
                 int quality, const QString& format)
{
    const auto name = resolve_save_format(filename, format);
    if (name == "pdf")
        return save_widget_pdf(w, filename, width, height);
    if (name == "svg")
        return save_widget_svg(w, filename, width, height);
    if (name == "png")
        return save_widget_raster(w, filename, "PNG", width, height, scale, quality);
    if (name == "jpg")
        return save_widget_raster(w, filename, "JPG", width, height, scale, quality);
    return save_widget_raster(w, filename, "BMP", width, height, scale, -1);
}

bool saved_or_throw(bool saved, const QString& filename)
{
    if (!saved)
        throw std::runtime_error("failed to save " + filename.toStdString()
                                 + ": check that its folder exists and is writable");
    return true;
}

void throw_unknown_save_format(const QString& filename)
{
    throw std::invalid_argument("can't save " + filename.toStdString() + ": unknown format '"
                             + QFileInfo(filename).suffix().toStdString()
                             + "' (pdf, svg, png, jpg or bmp)");
}
