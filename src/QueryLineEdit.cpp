#include "SciQLopPlots/Products/QueryLineEdit.hpp"
#include "SciQLopPlots/Products/QueryHighlighter.hpp"
#include "SciQLopPlots/Products/SubsequenceMatcher.hpp"
#include <QAbstractItemView>
#include <QKeyEvent>
#include <QListView>
#include <QScrollBar>
#include <QStringListModel>
#include <QTimer>
#include <algorithm>

QueryLineEdit::QueryLineEdit(QWidget* parent) : QTextEdit(parent)
{
    setAcceptRichText(false);
    setLineWrapMode(QTextEdit::NoWrap);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTabChangesFocus(true);
    setPlaceholderText("Search products... (e.g. magnetic provider:amda type:vector)");

    QFontMetrics fm(font());
    int h = fm.height() + 2 * document()->documentMargin() + 2;
    setFixedHeight(h);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_highlighter = new QueryHighlighter(document());

    m_popup = new QListView(this);
    m_popup->setWindowFlags(Qt::ToolTip);
    m_popup->setFocusPolicy(Qt::NoFocus);
    m_popup->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_popup->setSelectionMode(QAbstractItemView::SingleSelection);
    m_popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_popup->hide();

    m_completion_model = new QStringListModel(this);
    m_popup->setModel(m_completion_model);

    m_debounce_timer = new QTimer(this);
    m_debounce_timer->setSingleShot(true);
    m_debounce_timer->setInterval(150);
    connect(m_debounce_timer, &QTimer::timeout, this, [this]() {
        auto query = QueryParser::parse(toPlainText());
        emit queryChanged(query);
    });

    connect(this, &QTextEdit::textChanged, this, &QueryLineEdit::on_text_changed);

    connect(m_popup, &QListView::clicked, this,
            [this](const QModelIndex&) { accept_completion(); });
}

void QueryLineEdit::set_completions(const QStringList& field_names,
                                     const QMap<QString, QStringList>& field_values,
                                     const QStringList& product_names)
{
    m_field_names = field_names;
    m_field_values = field_values;
    m_product_names = product_names;
}

void QueryLineEdit::set_known_fields(const QSet<QString>& fields)
{
    m_highlighter->set_known_fields(fields);
}

void QueryLineEdit::on_text_changed()
{
    m_debounce_timer->start();
    update_completions();
}

void QueryLineEdit::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if (m_popup->isVisible())
            accept_completion();
        return;
    }

    if (event->key() == Qt::Key_Space && event->modifiers() & Qt::ControlModifier)
    {
        show_all_keywords();
        return;
    }

    if (m_popup->isVisible())
    {
        if (event->key() == Qt::Key_Down)
        {
            int row = m_popup->currentIndex().row() + 1;
            if (row < m_completion_model->rowCount())
                m_popup->setCurrentIndex(m_completion_model->index(row));
            return;
        }
        if (event->key() == Qt::Key_Up)
        {
            int row = m_popup->currentIndex().row() - 1;
            if (row >= 0)
                m_popup->setCurrentIndex(m_completion_model->index(row));
            return;
        }
        if (event->key() == Qt::Key_Escape)
        {
            hide_popup();
            return;
        }
        if (event->key() == Qt::Key_Tab)
        {
            accept_completion();
            return;
        }
    }

    QTextEdit::keyPressEvent(event);
}

void QueryLineEdit::focusOutEvent(QFocusEvent* event)
{
    hide_popup();
    QTextEdit::focusOutEvent(event);
}

void QueryLineEdit::update_completions()
{
    QString word = current_word();
    if (word.isEmpty() || word.length() < 2)
    {
        hide_popup();
        return;
    }

    QStringList candidates;

    QString full_text = toPlainText();
    int cursor_pos = textCursor().position();
    int colon_pos = full_text.lastIndexOf(':', cursor_pos - 1);
    int space_pos = full_text.lastIndexOf(' ', cursor_pos - 1);

    if (colon_pos > space_pos && colon_pos >= 0)
    {
        // Completing a field value — show matching values for that field
        QString field_name = full_text.mid(space_pos + 1, colon_pos - space_pos - 1);
        QString partial_value = full_text.mid(colon_pos + 1, cursor_pos - colon_pos - 1);

        if (m_field_values.contains(field_name) && partial_value.length() >= 1)
        {
            for (const auto& val : m_field_values[field_name])
            {
                if (subsequence_score(partial_value, val) > 0)
                    candidates.append(val);
                if (candidates.size() >= 10)
                    break;
            }
        }
    }
    else
    {
        // Completing a field name — only show field keywords
        for (const auto& fname : m_field_names)
        {
            if (fname.startsWith(word, Qt::CaseInsensitive))
                candidates.append(fname + ":");
        }
    }

    if (candidates.isEmpty())
        hide_popup();
    else
        show_popup(candidates);
}

void QueryLineEdit::accept_completion()
{
    auto idx = m_popup->currentIndex();
    if (!idx.isValid())
        return;

    QString completion = m_completion_model->data(idx).toString();
    QString full_text = toPlainText();
    int cursor_pos = textCursor().position();

    int word_start = cursor_pos;
    while (word_start > 0 && !full_text[word_start - 1].isSpace())
        --word_start;

    int colon_pos = full_text.lastIndexOf(':', cursor_pos - 1);
    int replace_start
        = (colon_pos > word_start) ? colon_pos + 1 : word_start;

    auto cursor = textCursor();
    cursor.setPosition(replace_start);
    cursor.setPosition(cursor_pos, QTextCursor::KeepAnchor);

    cursor.insertText(completion);
    if (!completion.endsWith(':'))
        cursor.insertText(" ");
    setTextCursor(cursor);
    hide_popup();
}

void QueryLineEdit::show_popup(const QStringList& items)
{
    m_completion_model->setStringList(items);
    m_popup->setCurrentIndex(m_completion_model->index(0));

    QPoint pos = mapToGlobal(QPoint(0, height()));
    int popup_width = width();
    int popup_height
        = std::min(static_cast<int>(items.size()) * m_popup->sizeHintForRow(0) + 4, 200);

    m_popup->setGeometry(pos.x(), pos.y(), popup_width, popup_height);
    m_popup->show();
}

void QueryLineEdit::show_all_keywords()
{
    QStringList candidates;

    QString full_text = toPlainText();
    int cursor_pos = textCursor().position();
    int colon_pos = full_text.lastIndexOf(':', cursor_pos - 1);
    int space_pos = full_text.lastIndexOf(' ', cursor_pos - 1);

    if (colon_pos > space_pos && colon_pos >= 0)
    {
        QString field_name = full_text.mid(space_pos + 1, colon_pos - space_pos - 1);
        if (m_field_values.contains(field_name))
        {
            candidates = m_field_values[field_name];
            if (candidates.size() > 20)
                candidates = candidates.mid(0, 20);
        }
    }
    else
    {
        for (const auto& fname : m_field_names)
            candidates.append(fname + ":");
    }

    if (!candidates.isEmpty())
        show_popup(candidates);
}

void QueryLineEdit::hide_popup()
{
    m_popup->hide();
}

QString QueryLineEdit::current_word() const
{
    auto cursor = textCursor();
    QString text = toPlainText();
    int pos = cursor.position();

    int start = pos;
    while (start > 0 && !text[start - 1].isSpace())
        --start;

    return text.mid(start, pos - start);
}
