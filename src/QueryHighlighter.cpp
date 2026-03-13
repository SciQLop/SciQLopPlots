#include "SciQLopPlots/Products/QueryHighlighter.hpp"
#include <QApplication>
#include <QPalette>

QueryHighlighter::QueryHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent)
{
    auto palette = QApplication::palette();

    m_field_name_format.setForeground(palette.link());
    m_field_name_format.setFontWeight(QFont::Bold);

    m_invalid_field_name_format.setForeground(QColor(200, 80, 80));
    m_invalid_field_name_format.setFontItalic(true);

    m_field_value_format.setForeground(QColor(46, 160, 67));

    m_free_text_format.setForeground(palette.text().color());
}

void QueryHighlighter::set_known_fields(const QSet<QString>& fields)
{
    m_known_fields = fields;
    rehighlight();
}

void QueryHighlighter::highlightBlock(const QString& text)
{
    auto query = QueryParser::parse(text);

    int filter_idx = 0;
    for (const auto& token : query.tokens)
    {
        switch (token.kind)
        {
        case QueryTokenKind::FIELD_NAME:
        {
            bool valid = m_known_fields.isEmpty();
            if (!m_known_fields.isEmpty() && filter_idx < query.filters.size())
                valid = m_known_fields.contains(query.filters[filter_idx].field.toLower());
            setFormat(token.start, token.length,
                      valid ? m_field_name_format : m_invalid_field_name_format);
            break;
        }
        case QueryTokenKind::FIELD_VALUE:
            setFormat(token.start, token.length, m_field_value_format);
            ++filter_idx;
            break;
        case QueryTokenKind::FREE_TEXT:
            setFormat(token.start, token.length, m_free_text_format);
            break;
        }
    }
}
