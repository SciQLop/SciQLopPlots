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
#include <QHash>
#include <QString>
#include <QVariant>
#include <algorithm>
#include <optional>

enum class ScoreMergeStrategy
{
    Max,
    Sum,
    WeightedSum,
    Override
};

// Combines whichever named raw signals are present for one leaf (the
// native fuzzy scorer is just another entry, under the reserved name
// "fuzzy") into a single score, for one of four strategies:
//
//  - Max/Sum: every present signal, normalized to 0-100 relative to that
//    signal's own max across the current result set (raw scales differ
//    per signal -- subsequence bonus points vs. BM25 vs. cosine similarity
//    -- so combining raw values directly would let whichever signal
//    happens to produce bigger numbers dominate).
//  - WeightedSum: same normalization, each signal scaled by
//    weights.value(name, 1.0) before summing; a weight of 0 excludes that
//    signal entirely (this is how "fuzzy" itself can be excluded from
//    blending).
//  - Override: returns override_signal's normalized value if present,
//    ignoring every other signal and all weights; returns nullopt
//    (no match) if override_signal is absent, unset, or has no value for
//    this leaf -- there is no per-leaf fallback to another signal.
//
// Returns std::nullopt when there is no match at all: raw_signals is
// empty, or (Override only) the designated signal has no value here.
inline std::optional<double> merge_scores(const QHash<QString, double>& raw_signals,
                                           ScoreMergeStrategy strategy,
                                           const QHash<QString, double>& weights,
                                           const QHash<QString, double>& signal_maxes,
                                           const QString& override_signal)
{
    auto normalized = [&](const QString& name, double raw) -> double
    {
        double max_value = signal_maxes.value(name, 0.0);
        return max_value > 0.0 ? (raw * 100.0 / max_value) : 0.0;
    };

    if (strategy == ScoreMergeStrategy::Override)
    {
        auto it = raw_signals.constFind(override_signal);
        if (it == raw_signals.constEnd())
            return std::nullopt;
        return normalized(override_signal, it.value());
    }

    if (raw_signals.isEmpty())
        return std::nullopt;

    double result = 0.0;
    bool any = false;
    for (auto it = raw_signals.constBegin(); it != raw_signals.constEnd(); ++it)
    {
        double weight
            = strategy == ScoreMergeStrategy::WeightedSum ? weights.value(it.key(), 1.0) : 1.0;
        if (weight == 0.0)
            continue;
        double n = normalized(it.key(), it.value()) * weight;
        if (strategy == ScoreMergeStrategy::Max)
            result = any ? std::max(result, n) : n;
        else
            result += n;
        any = true;
    }
    return any ? std::optional<double>(result) : std::nullopt;
}

// Python-bindable wrapper for direct unit testing of merge_scores() --
// nothing in SciQLopPlots or SciQLop calls this from production code, it
// exists purely so the strategy math has an isolated test surface, the same
// way SubsequenceMatcher wraps subsequence_score/subsequence_match.
class ScoreMerger
{
public:
    static QVariant merge(const QHash<QString, QVariant>& raw_signals, ScoreMergeStrategy strategy,
                           const QHash<QString, QVariant>& weights,
                           const QHash<QString, QVariant>& signal_maxes,
                           const QString& override_signal)
    {
        auto to_double_hash = [](const QHash<QString, QVariant>& in)
        {
            QHash<QString, double> out;
            out.reserve(in.size());
            for (auto it = in.constBegin(); it != in.constEnd(); ++it)
                out.insert(it.key(), it.value().toDouble());
            return out;
        };

        auto result = merge_scores(to_double_hash(raw_signals), strategy,
                                    to_double_hash(weights), to_double_hash(signal_maxes),
                                    override_signal);
        return result ? QVariant(*result) : QVariant();
    }
};
