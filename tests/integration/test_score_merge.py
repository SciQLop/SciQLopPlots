"""Tests for merge_scores() / ScoreMergeStrategy — combining the native
fuzzy score with externally-supplied signals (e.g. BM25, semantic search)."""
import pytest
from SciQLopPlots import ScoreMerger, ScoreMergeStrategy


class TestMaxStrategy:
    def test_picks_higher_normalized_signal(self):
        # fuzzy raw=10 (its own max=10 -> normalized 100), bm25 raw=5 (its
        # own max=20 -> normalized 25): Max picks fuzzy's 100.
        result = ScoreMerger.merge(
            {"fuzzy": 10.0, "bm25": 5.0}, ScoreMergeStrategy.Max,
            {}, {"fuzzy": 10.0, "bm25": 20.0}, "")
        assert result == pytest.approx(100.0)

    def test_absent_signal_excluded_not_zero(self):
        # bm25 is absent for this leaf entirely -- must not count as a
        # literal 0 and drag Max down; only "fuzzy" is considered.
        result = ScoreMerger.merge(
            {"fuzzy": 5.0}, ScoreMergeStrategy.Max,
            {}, {"fuzzy": 10.0, "bm25": 20.0}, "")
        assert result == pytest.approx(50.0)


class TestSumStrategy:
    def test_sums_all_normalized_signals(self):
        result = ScoreMerger.merge(
            {"fuzzy": 10.0, "bm25": 10.0}, ScoreMergeStrategy.Sum,
            {}, {"fuzzy": 10.0, "bm25": 20.0}, "")
        # fuzzy normalized 100 + bm25 normalized 50 = 150
        assert result == pytest.approx(150.0)


class TestWeightedSumStrategy:
    def test_applies_per_signal_weight(self):
        result = ScoreMerger.merge(
            {"fuzzy": 10.0, "bm25": 20.0}, ScoreMergeStrategy.WeightedSum,
            {"fuzzy": 1.0, "bm25": 0.5}, {"fuzzy": 10.0, "bm25": 20.0}, "")
        # fuzzy: 100*1.0 + bm25: 100*0.5 = 150
        assert result == pytest.approx(150.0)

    def test_unweighted_signal_defaults_to_one(self):
        result = ScoreMerger.merge(
            {"fuzzy": 10.0}, ScoreMergeStrategy.WeightedSum,
            {}, {"fuzzy": 10.0}, "")
        assert result == pytest.approx(100.0)

    def test_zero_weight_excludes_signal(self):
        result = ScoreMerger.merge(
            {"fuzzy": 10.0, "bm25": 20.0}, ScoreMergeStrategy.WeightedSum,
            {"fuzzy": 0.0, "bm25": 1.0}, {"fuzzy": 10.0, "bm25": 20.0}, "")
        assert result == pytest.approx(100.0)


class TestOverrideStrategy:
    def test_uses_only_the_override_signal(self):
        result = ScoreMerger.merge(
            {"fuzzy": 1000.0, "bm25": 5.0}, ScoreMergeStrategy.Override,
            {}, {"fuzzy": 1000.0, "bm25": 20.0}, "bm25")
        assert result == pytest.approx(25.0)

    def test_missing_override_signal_is_no_match(self):
        result = ScoreMerger.merge(
            {"fuzzy": 1000.0}, ScoreMergeStrategy.Override,
            {}, {"fuzzy": 1000.0}, "bm25")
        assert result is None

    def test_unset_override_signal_is_no_match(self):
        result = ScoreMerger.merge(
            {"fuzzy": 1000.0}, ScoreMergeStrategy.Override,
            {}, {"fuzzy": 1000.0}, "")
        assert result is None


def test_empty_raw_signals_is_no_match():
    assert ScoreMerger.merge({}, ScoreMergeStrategy.Max, {}, {}, "") is None
