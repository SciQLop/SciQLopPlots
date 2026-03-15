"""Tests for QueryParser — parses query strings into structured queries."""
import pytest
from SciQLopPlots import QueryParser, Query, QueryToken, QueryTokenKind, QueryFilter


class TestPlainText:
    def test_single_word(self):
        q = QueryParser.parse("magnetic")
        assert q.free_text_tokens == ["magnetic"]
        assert len(q.filters) == 0

    def test_multiple_words(self):
        q = QueryParser.parse("magnetic field")
        assert q.free_text_tokens == ["magnetic", "field"]

    def test_empty_string(self):
        q = QueryParser.parse("")
        assert q.free_text_tokens == []
        assert len(q.filters) == 0


class TestFieldFilters:
    def test_single_filter(self):
        q = QueryParser.parse("provider:amda")
        assert len(q.free_text_tokens) == 0
        assert len(q.filters) == 1
        assert q.filters[0].field == "provider"
        assert q.filters[0].value == "amda"

    def test_multiple_filters(self):
        q = QueryParser.parse("provider:amda type:vector")
        assert len(q.filters) == 2
        assert q.filters[0].field == "provider"
        assert q.filters[1].field == "type"
        assert q.filters[1].value == "vector"

    def test_colon_in_value(self):
        q = QueryParser.parse("uid:some:complex:id")
        assert len(q.filters) == 1
        assert q.filters[0].field == "uid"
        assert q.filters[0].value == "some:complex:id"

    def test_empty_value(self):
        q = QueryParser.parse("provider:")
        assert len(q.filters) == 1
        assert q.filters[0].field == "provider"
        assert q.filters[0].value == ""


class TestMixed:
    def test_free_text_and_filter(self):
        q = QueryParser.parse("magnetic field provider:amda")
        assert q.free_text_tokens == ["magnetic", "field"]
        assert len(q.filters) == 1
        assert q.filters[0].field == "provider"

    def test_filter_between_free_text(self):
        q = QueryParser.parse("magnetic provider:amda field")
        assert q.free_text_tokens == ["magnetic", "field"]
        assert len(q.filters) == 1

    def test_full_query(self):
        q = QueryParser.parse("magnetic field provider:amda type:vector")
        assert q.free_text_tokens == ["magnetic", "field"]
        assert len(q.filters) == 2


class TestQuotedStrings:
    def test_quoted_free_text(self):
        q = QueryParser.parse('"magnetic field"')
        assert q.free_text_tokens == ["magnetic field"]

    def test_quoted_filter_value(self):
        q = QueryParser.parse('provider:"amda test"')
        assert len(q.filters) == 1
        assert q.filters[0].value == "amda test"

    def test_mixed_quoted_and_unquoted(self):
        q = QueryParser.parse('"magnetic field" provider:amda')
        assert q.free_text_tokens == ["magnetic field"]
        assert len(q.filters) == 1


class TestTokenPositions:
    def test_free_text_positions(self):
        q = QueryParser.parse("magnetic")
        assert len(q.tokens) == 1
        assert q.tokens[0].kind == QueryTokenKind.FREE_TEXT
        assert q.tokens[0].start == 0
        assert q.tokens[0].length == 8

    def test_filter_positions(self):
        q = QueryParser.parse("provider:amda")
        assert len(q.tokens) == 2
        assert q.tokens[0].kind == QueryTokenKind.FIELD_NAME
        assert q.tokens[0].start == 0
        assert q.tokens[0].length == 9  # "provider:" including colon
        assert q.tokens[1].kind == QueryTokenKind.FIELD_VALUE
        assert q.tokens[1].start == 9
        assert q.tokens[1].length == 4

    def test_multiple_token_positions(self):
        q = QueryParser.parse("mag provider:amda")
        assert len(q.tokens) == 3  # FREE_TEXT, FIELD_NAME, FIELD_VALUE
        assert q.tokens[0].kind == QueryTokenKind.FREE_TEXT
        assert q.tokens[1].kind == QueryTokenKind.FIELD_NAME
        assert q.tokens[2].kind == QueryTokenKind.FIELD_VALUE


class TestWhitespace:
    def test_leading_trailing_whitespace(self):
        q = QueryParser.parse("  magnetic  ")
        assert q.free_text_tokens == ["magnetic"]

    def test_multiple_spaces_between_tokens(self):
        q = QueryParser.parse("magnetic   field")
        assert q.free_text_tokens == ["magnetic", "field"]


class TestDateFilters:
    def test_after_filter(self):
        q = QueryParser.parse("after:2024-01-01")
        assert len(q.filters) == 1
        assert q.filters[0].field == "after"
        assert q.filters[0].value == "2024-01-01"

    def test_before_filter(self):
        q = QueryParser.parse("before:2024-12-31")
        assert len(q.filters) == 1
        assert q.filters[0].field == "before"
        assert q.filters[0].value == "2024-12-31"

    def test_date_range_with_text(self):
        q = QueryParser.parse("magnetic after:2020-01-01 before:2024-12-31")
        assert q.free_text_tokens == ["magnetic"]
        assert len(q.filters) == 2
        assert q.filters[0].field == "after"
        assert q.filters[1].field == "before"
