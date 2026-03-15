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
#include <QMap>
#include <QSet>
#include <QTextEdit>

class QueryHighlighter;
class QListView;
class QStringListModel;
class QTimer;

class QueryLineEdit : public QTextEdit
{
    Q_OBJECT
    QueryHighlighter* m_highlighter;
    QListView* m_popup;
    QStringListModel* m_completion_model;
    QTimer* m_debounce_timer;

    QStringList m_field_names;
    QMap<QString, QStringList> m_field_values;
    QStringList m_product_names;

public:
    QueryLineEdit(QWidget* parent = nullptr);

    void set_completions(const QStringList& field_names,
                         const QMap<QString, QStringList>& field_values,
                         const QStringList& product_names);

    void set_known_fields(const QSet<QString>& fields);

    Q_SIGNAL void queryChanged(const Query& query);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void on_text_changed();
    void update_completions();
    void accept_completion();
    void show_all_keywords();
    void show_popup(const QStringList& items);
    void hide_popup();
    QString current_word() const;
};
