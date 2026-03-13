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
#include "SciQLopPlots/Products/QueryParser.hpp"
#include <QSet>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class QueryHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
    QTextCharFormat m_field_name_format;
    QTextCharFormat m_invalid_field_name_format;
    QTextCharFormat m_field_value_format;
    QTextCharFormat m_free_text_format;
    QSet<QString> m_known_fields;

public:
    QueryHighlighter(QTextDocument* parent = nullptr);
    void set_known_fields(const QSet<QString>& fields);

protected:
    void highlightBlock(const QString& text) override;
};
