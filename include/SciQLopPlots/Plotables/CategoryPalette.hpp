/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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
#pragma once
#include <QColor>
#include <QObject>
#include <QStringList>
#include <QVector>

//! Process-wide: a category has one colour in every plot. Lives in its own header
//! (rather than inline in SciQLopTimeline.cpp) because it needs Q_OBJECT/moc and
//! this project's meson build has no precedent for moc-ing a .cpp file.
class CategoryPalette : public QObject
{
    Q_OBJECT

public:
    QStringList names;
    QVector<QColor> colors;

    static CategoryPalette& instance()
    {
        static CategoryPalette palette;
        return palette;
    }

    int index(const QString& name)
    {
        if (const int i = names.indexOf(name); i >= 0)
            return i;
        names.append(name);
        colors.append(default_color(colors.size()));
        return names.size() - 1;
    }

    void set_color(const QString& name, const QColor& color)
    {
        colors[index(name)] = color;
        Q_EMIT changed();
    }

Q_SIGNALS:
    void changed();

private:
    static QColor default_color(int i)
    {
        static const QColor table[] = { "#4e79a7", "#f28e2b", "#e15759", "#76b7b2", "#59a14f",
                                         "#edc948", "#b07aa1", "#ff9da7", "#9c755f", "#bab0ac" };
        if (i < 10)
            return table[i];
        return QColor::fromHsv((i * 137) % 360, 160, 220);
    }
};
