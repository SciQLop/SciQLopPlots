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
#include <QList>
#include <QString>
#include <QStringView>
#include <algorithm>

struct MatchResult
{
    int score = 0;
    QList<int> match_positions;
};

inline int subsequence_score(QStringView query, QStringView candidate)
{
    if (query.isEmpty())
        return 1;
    if (candidate.isEmpty())
        return 0;

    int score = 0;
    int query_idx = 0;
    int prev_match = -2;

    for (int i = 0; i < candidate.size() && query_idx < query.size(); ++i)
    {
        if (query[query_idx].toLower() == candidate[i].toLower())
        {
            int char_score = 1;

            if (i == prev_match + 1)
                char_score += 4;

            if (i == 0)
                char_score += 3;
            else if (candidate[i - 1] == '_' || candidate[i - 1] == ' '
                     || candidate[i - 1] == '/')
                char_score += 3;
            else if (candidate[i].isUpper() && candidate[i - 1].isLower())
                char_score += 2;

            score += char_score;
            prev_match = i;
            ++query_idx;
        }
    }

    if (query_idx < query.size())
        return 0;

    int length_penalty = candidate.size() - query.size();
    score = std::max(1, score * 100 / (100 + length_penalty));

    return score;
}

inline MatchResult subsequence_match(QStringView query, QStringView candidate)
{
    MatchResult result;
    if (query.isEmpty())
    {
        result.score = 1;
        return result;
    }
    if (candidate.isEmpty())
        return result;

    int query_idx = 0;
    int prev_match = -2;

    for (int i = 0; i < candidate.size() && query_idx < query.size(); ++i)
    {
        if (query[query_idx].toLower() == candidate[i].toLower())
        {
            int char_score = 1;

            if (i == prev_match + 1)
                char_score += 4;

            if (i == 0)
                char_score += 3;
            else if (candidate[i - 1] == '_' || candidate[i - 1] == ' '
                     || candidate[i - 1] == '/')
                char_score += 3;
            else if (candidate[i].isUpper() && candidate[i - 1].isLower())
                char_score += 2;

            result.score += char_score;
            result.match_positions.append(i);
            prev_match = i;
            ++query_idx;
        }
    }

    if (query_idx < query.size())
    {
        result.score = 0;
        result.match_positions.clear();
        return result;
    }

    int length_penalty = candidate.size() - query.size();
    result.score = std::max(1, result.score * 100 / (100 + length_penalty));

    return result;
}

// Python-bindable wrapper (const QString& -> QStringView delegation)
class SubsequenceMatcher
{
public:
    static int score(const QString& query, const QString& candidate)
    {
        return subsequence_score(query, candidate);
    }

    static MatchResult match(const QString& query, const QString& candidate)
    {
        return subsequence_match(query, candidate);
    }
};
