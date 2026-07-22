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
#include <limits>
#include <vector>

struct MatchResult
{
    int score = 0;
    QList<int> match_positions;
};

namespace SubsequenceMatcherPrivate
{
// Bonus for matching at candidate position i: start of string, right after a
// separator ('_', ' ', '/'), or a lower->Upper camelCase transition. Depends
// only on the position, not on which earlier position was matched before it.
inline int word_start_bonus(QStringView candidate, qsizetype i)
{
    if (i == 0)
        return 3;
    QChar prev = candidate[i - 1];
    if (prev == QLatin1Char('_') || prev == QLatin1Char(' ') || prev == QLatin1Char('/'))
        return 3;
    if (candidate[i].isUpper() && prev.isLower())
        return 2;
    return 0;
}

constexpr int kNegInf = std::numeric_limits<int>::min() / 2;

// Highest-scoring way to match `query` as a subsequence of `candidate`.
//
// A naive left-to-right greedy scan commits to the first occurrence of each
// query character wherever it happens to appear, which can "waste" an early
// throwaway match (e.g. a stray lowercase letter buried in an unrelated word)
// and then miss a clean, contiguous, word-boundary-aligned occurrence of the
// whole query later in the same text — systematically under-scoring exactly
// the matches that should rank highest. This DP instead considers every
// valid alignment and keeps the best-scoring one.
//
// O(candidate.size() * query.size()) time, O(candidate.size()) space — no
// backpointers, since only the score is needed here (see
// best_alignment_with_positions for the position-reporting variant).
//
// `candidate_lower`/`bonus` depend only on the candidate position `i`, not on
// which query row `j` is being scored, so they're precomputed once (O(n))
// instead of being recomputed on every one of the O(n*m) DP cells. Measured
// via `perf record` on the realistic multi-token benchmark
// (tests/perf/bench_search.cpp): ~68% of all cycles were spent in
// QChar::toLower()/QUnicodeTables::caseConversion() before this change, all
// of it redundant re-folding of the same candidate characters once per query
// character. The DP levels and the two precomputed arrays are thread_local
// (this function runs concurrently across cpp_utils::threading::pool()
// workers, one call per leaf) so steady-state calls do zero heap allocation
// instead of two QList allocations each.
inline int best_alignment_raw_score(QStringView query, QStringView candidate)
{
    const qsizetype n = candidate.size();
    const qsizetype m = query.size();
    if (m > n)
        return -1;

    thread_local std::vector<char16_t> candidate_lower;
    thread_local std::vector<int> bonus;
    thread_local std::vector<int> prev_level;
    thread_local std::vector<int> cur_level;

    candidate_lower.resize(n);
    bonus.resize(n);
    for (qsizetype i = 0; i < n; ++i)
    {
        candidate_lower[i] = candidate[i].toLower().unicode();
        bonus[i] = word_start_bonus(candidate, i);
    }

    prev_level.assign(n, kNegInf);
    cur_level.resize(n);

    const char16_t q0 = query[0].toLower().unicode();
    for (qsizetype i = 0; i < n; ++i)
        if (q0 == candidate_lower[i])
            prev_level[i] = 1 + bonus[i];

    for (qsizetype j = 1; j < m; ++j)
    {
        std::fill(cur_level.begin(), cur_level.end(), kNegInf);
        int prefix_max = kNegInf;
        const char16_t qj = query[j].toLower().unicode();

        for (qsizetype i = 0; i < n; ++i)
        {
            int adjacent
                = (i >= 1 && prev_level[i - 1] != kNegInf) ? prev_level[i - 1] + 4 : kNegInf;
            int best_prev = std::max(prefix_max, adjacent);

            if (best_prev != kNegInf && qj == candidate_lower[i])
                cur_level[i] = best_prev + 1 + bonus[i];

            if (i >= 1 && prev_level[i - 1] > prefix_max)
                prefix_max = prev_level[i - 1];
        }
        prev_level.swap(cur_level);
    }

    int best = kNegInf;
    for (qsizetype i = 0; i < n; ++i)
        best = std::max(best, prev_level[i]);
    return best == kNegInf ? -1 : best;
}

// Same recurrence as best_alignment_raw_score, but keeps a full backpointer
// table to reconstruct the winning positions. Only used for position
// reporting (not on the per-node/per-keystroke filtering hot path), so the
// extra O(n*m) memory is fine.
inline int best_alignment_with_positions(QStringView query, QStringView candidate,
                                          QList<int>& out_positions)
{
    const qsizetype n = candidate.size();
    const qsizetype m = query.size();
    if (m > n)
        return -1;

    QList<QList<int>> level_score(m, QList<int>(n, kNegInf));
    QList<QList<qsizetype>> level_back(m, QList<qsizetype>(n, -1));

    for (qsizetype i = 0; i < n; ++i)
        if (query[0].toLower() == candidate[i].toLower())
            level_score[0][i] = 1 + word_start_bonus(candidate, i);

    for (qsizetype j = 1; j < m; ++j)
    {
        const QList<int>& prev_score = level_score[j - 1];
        int prefix_max = kNegInf;
        qsizetype prefix_max_pos = -1;

        for (qsizetype i = 0; i < n; ++i)
        {
            int adjacent
                = (i >= 1 && prev_score[i - 1] != kNegInf) ? prev_score[i - 1] + 4 : kNegInf;
            int best_prev = prefix_max;
            qsizetype best_prev_pos = prefix_max_pos;
            if (adjacent >= best_prev)
            {
                best_prev = adjacent;
                best_prev_pos = i - 1;
            }

            if (best_prev != kNegInf && query[j].toLower() == candidate[i].toLower())
            {
                level_score[j][i] = best_prev + 1 + word_start_bonus(candidate, i);
                level_back[j][i] = best_prev_pos;
            }

            if (i >= 1 && prev_score[i - 1] > prefix_max)
            {
                prefix_max = prev_score[i - 1];
                prefix_max_pos = i - 1;
            }
        }
    }

    int best = kNegInf;
    qsizetype best_pos = -1;
    const QList<int>& last_level = level_score[m - 1];
    for (qsizetype i = 0; i < n; ++i)
    {
        if (last_level[i] > best)
        {
            best = last_level[i];
            best_pos = i;
        }
    }
    if (best_pos < 0)
        return -1;

    out_positions.resize(m);
    qsizetype pos = best_pos;
    for (qsizetype j = m - 1; j >= 0; --j)
    {
        out_positions[j] = int(pos);
        pos = level_back[j][pos];
    }
    return best;
}
}

inline int subsequence_score(QStringView query, QStringView candidate)
{
    if (query.isEmpty())
        return 1;
    if (candidate.isEmpty())
        return 0;

    int raw = SubsequenceMatcherPrivate::best_alignment_raw_score(query, candidate);
    if (raw < 0)
        return 0;

    int length_penalty = candidate.size() - query.size();
    return std::max(1, raw * 100 / (100 + length_penalty));
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

    QList<int> positions;
    int raw
        = SubsequenceMatcherPrivate::best_alignment_with_positions(query, candidate, positions);
    if (raw < 0)
        return result;

    int length_penalty = candidate.size() - query.size();
    result.score = std::max(1, raw * 100 / (100 + length_penalty));
    result.match_positions = positions;
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
