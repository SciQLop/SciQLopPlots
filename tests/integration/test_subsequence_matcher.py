"""Tests for SubsequenceMatcher — subsequence scoring for fuzzy product search."""
import pytest
from SciQLopPlots import SubsequenceMatcher


class TestSubsequenceScore:
    def test_exact_match_scores_highest(self):
        assert SubsequenceMatcher.score("magnetic", "magnetic") > 0

    def test_no_match_returns_zero(self):
        assert SubsequenceMatcher.score("xyz", "magnetic") == 0

    def test_prefix_match_scores_higher_than_mid_match(self):
        prefix = SubsequenceMatcher.score("mag", "magnetic_field")
        mid = SubsequenceMatcher.score("net", "magnetic_field")
        assert prefix > mid

    def test_consecutive_match_scores_higher_than_scattered(self):
        consecutive = SubsequenceMatcher.score("mag", "magnetic")
        scattered = SubsequenceMatcher.score("mgc", "magnetic")
        assert consecutive > scattered

    def test_word_boundary_match_scores_higher(self):
        boundary = SubsequenceMatcher.score("mf", "magnetic_field")
        non_boundary = SubsequenceMatcher.score("mf", "somefield")
        assert boundary > non_boundary

    def test_shorter_candidate_scores_higher(self):
        short = SubsequenceMatcher.score("mag", "magnetic")
        long = SubsequenceMatcher.score("mag", "magnetic_field_strength_data")
        assert short > long

    def test_case_insensitive(self):
        assert SubsequenceMatcher.score("MAG", "magnetic") > 0
        assert SubsequenceMatcher.score("mag", "MAGNETIC") > 0

    def test_empty_query_matches_everything(self):
        assert SubsequenceMatcher.score("", "anything") > 0

    def test_empty_candidate_with_nonempty_query(self):
        assert SubsequenceMatcher.score("mag", "") == 0

    def test_both_empty(self):
        assert SubsequenceMatcher.score("", "") > 0

    def test_query_longer_than_candidate(self):
        assert SubsequenceMatcher.score("magnetic_field", "mag") == 0


class TestSubsequenceMatch:
    def test_returns_match_positions(self):
        result = SubsequenceMatcher.match("mag", "magnetic")
        assert result.score > 0
        assert list(result.match_positions) == [0, 1, 2]

    def test_no_match_empty_positions(self):
        result = SubsequenceMatcher.match("xyz", "magnetic")
        assert result.score == 0
        assert len(result.match_positions) == 0

    def test_scattered_match_positions(self):
        result = SubsequenceMatcher.match("mgc", "magnetic")
        assert result.score > 0
        assert len(result.match_positions) == 3
        positions = list(result.match_positions)
        assert positions[0] == 0  # m
        assert positions[1] > positions[0]  # g after m
        assert positions[2] > positions[1]  # c after g
