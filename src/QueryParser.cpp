#include "SciQLopPlots/Products/QueryParser.hpp"
#include <QDate>
#include <QRegularExpression>
#include <QTimeZone>

namespace
{

// "2008" → 2008-01-01, "2008-03" → 2008-03-01, "2008-03-15" → 2008-03-15
QDateTime parse_partial_date(const QString& value, bool end_of_period)
{
    static const QRegularExpression year_only("^\\d{4}$");
    static const QRegularExpression year_month("^\\d{4}-\\d{1,2}$");

    if (year_only.match(value).hasMatch())
    {
        int y = value.toInt();
        if (end_of_period)
            return QDateTime(QDate(y, 12, 31), QTime(23, 59, 59), QTimeZone::UTC);
        return QDateTime(QDate(y, 1, 1), QTime(0, 0, 0), QTimeZone::UTC);
    }

    if (year_month.match(value).hasMatch())
    {
        auto parts = value.split('-');
        int y = parts[0].toInt();
        int m = parts[1].toInt();
        if (end_of_period)
        {
            QDate d(y, m, 1);
            return QDateTime(QDate(y, m, d.daysInMonth()), QTime(23, 59, 59), QTimeZone::UTC);
        }
        return QDateTime(QDate(y, m, 1), QTime(0, 0, 0), QTimeZone::UTC);
    }

    auto date = QDate::fromString(value, "yyyy-MM-dd");
    if (date.isValid())
    {
        if (end_of_period)
            return QDateTime(date, QTime(23, 59, 59), QTimeZone::UTC);
        return QDateTime(date, QTime(0, 0, 0), QTimeZone::UTC);
    }

    return QDateTime::fromString(value, Qt::ISODate);
}

QString extract_token(const QString& input, int& pos)
{
    if (pos < input.size() && input[pos] == '"')
    {
        ++pos;
        int start = pos;
        while (pos < input.size() && input[pos] != '"')
            ++pos;
        QString token = input.mid(start, pos - start);
        if (pos < input.size())
            ++pos;
        return token;
    }

    int start = pos;
    while (pos < input.size() && !input[pos].isSpace())
        ++pos;
    return input.mid(start, pos - start);
}

} // namespace

Query QueryParser::parse(const QString& input)
{
    Query result;
    int pos = 0;

    while (pos < input.size())
    {
        while (pos < input.size() && input[pos].isSpace())
            ++pos;
        if (pos >= input.size())
            break;

        int token_start = pos;

        bool is_filter = false;
        int colon_pos = -1;
        if (input[pos] != '"')
        {
            for (int i = pos; i < input.size() && !input[i].isSpace(); ++i)
            {
                if (input[i] == ':')
                {
                    colon_pos = i;
                    is_filter = true;
                    break;
                }
            }
        }

        if (is_filter)
        {
            QString field = input.mid(pos, colon_pos - pos);
            int field_token_length = colon_pos - pos + 1;

            pos = colon_pos + 1;
            int value_start = pos;
            QString value = extract_token(input, pos);

            QueryFilter filter;
            filter.field = field;
            filter.value = value;
            if (field.compare("after", Qt::CaseInsensitive) == 0
                || field.compare("before", Qt::CaseInsensitive) == 0)
                filter.parsed_date = parse_partial_date(value, false);
            result.filters.append(filter);

            QueryToken field_tok;
            field_tok.kind = QueryTokenKind::FIELD_NAME;
            field_tok.start = token_start;
            field_tok.length = field_token_length;
            result.tokens.append(field_tok);

            QueryToken value_tok;
            value_tok.kind = QueryTokenKind::FIELD_VALUE;
            value_tok.start = value_start;
            value_tok.length = pos - value_start;
            result.tokens.append(value_tok);
        }
        else
        {
            QString text = extract_token(input, pos);
            if (!text.isEmpty())
            {
                result.free_text_tokens.append(text);

                QueryToken tok;
                tok.kind = QueryTokenKind::FREE_TEXT;
                tok.start = token_start;
                tok.length = pos - token_start;
                result.tokens.append(tok);
            }
        }
    }

    return result;
}
