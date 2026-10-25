"""Mutation operators: one table of (name, languages, pattern, replacement).

A mutant replaces one match of one operator on one line. Binary operators are
matched with their surrounding spaces: the code is clang-formatted (C++) or
PEP 8 (Python), and the spaces keep templates (`QVector<double>`), `->` and
`<<` out of the table.
"""
import re
from dataclasses import dataclass

CPP, PY = "cpp", "py"


@dataclass(frozen=True)
class Operator:
    name: str
    langs: frozenset
    pattern: re.Pattern
    replacement: object  # a re template string, or a callable(match) -> str


@dataclass(frozen=True)
class Mutant:
    path: str
    line: int
    col: int
    op: str
    before: str
    after: str

    @property
    def id(self):
        return f"{self.path}:{self.line}:{self.col}:{self.op}"


def _op(name, langs, pattern, replacement):
    return Operator(name, frozenset(langs), re.compile(pattern), replacement)


def _plus_one(match):
    return str(int(match[0]) + 1)


OPERATORS = (
    _op("lt_to_le", {CPP, PY}, r" < ", " <= "),
    _op("le_to_lt", {CPP, PY}, r" <= ", " < "),
    _op("gt_to_ge", {CPP, PY}, r" > ", " >= "),
    _op("ge_to_gt", {CPP, PY}, r" >= ", " > "),
    _op("eq_to_ne", {CPP, PY}, r" == ", " != "),
    _op("ne_to_eq", {CPP, PY}, r" != ", " == "),
    _op("and_to_or", {CPP}, r" && ", " || "),
    _op("or_to_and", {CPP}, r" \|\| ", " && "),
    _op("and_to_or", {PY}, r" and ", " or "),
    _op("or_to_and", {PY}, r" or ", " and "),
    _op("plus_to_minus", {CPP, PY}, r" \+ ", " - "),
    _op("minus_to_plus", {CPP, PY}, r" - ", " + "),
    _op("negate_if", {CPP}, r"\bif \((.*)\)(\s*\{?)\s*$", r"if (!(\1))\2"),
    _op("negate_if", {PY}, r"^(\s*)(el)?if (.*):\s*$", r"\1\2if not (\3):"),
    _op("int_plus_one", {CPP, PY}, r"(?<![\w.])\d+(?![\w.])", _plus_one),
    _op("true_to_false", {CPP}, r"\btrue\b", "false"),
    _op("false_to_true", {CPP}, r"\bfalse\b", "true"),
    _op("true_to_false", {PY}, r"\bTrue\b", "False"),
    _op("false_to_true", {PY}, r"\bFalse\b", "True"),
    _op("delete_statement", {CPP}, r"^(\s*)(?:\w+(?:->|\.|::))*\w+\s*(?:\(.*\)|[-+*/|&]?= .*);\s*$", r"\1;"),
    _op("delete_statement", {PY}, r"^(\s*)(?:\w+\.)*\w+\(.*\)\s*$", r"\1pass"),
)


def language(path):
    return PY if path.endswith(".py") else CPP


def code_part(line, lang):
    """The part of a line operators may touch: no comment, no preprocessor line."""
    stripped = line.lstrip()
    if stripped.startswith(("//", "/*", "*", "#") if lang == CPP else "#"):
        return ""
    return line.split("//" if lang == CPP else "#", 1)[0]


QUOTED = re.compile(r"\"(?:[^\"\\]|\\.)*\"|'(?:[^'\\]|\\.)*'")


def first_code_match(pattern, code):
    quoted = [m.span() for m in QUOTED.finditer(code)]
    return next((m for m in pattern.finditer(code) if not any(a <= m.start() < b for a, b in quoted)), None)


def mutants_for_line(path, number, line):
    # simplify: first match only, one mutant per operator per line; the ceiling is a missed
    # second `+` on the same line. Upgrade: finditer, when phase 1 shows those lines survive.
    lang = language(path)
    code = code_part(line, lang)
    found = []
    for op in OPERATORS:
        match = first_code_match(op.pattern, code) if lang in op.langs else None
        if match is None:
            continue
        replacement = op.replacement(match) if callable(op.replacement) else match.expand(op.replacement)
        found.append(Mutant(path, number, match.start(), op.name, line, code[: match.start()] + replacement + line[match.end():]))
    return found


def mutants_for_file(path, text, lines):
    source = text.splitlines()
    return [m for n in sorted(lines) if 1 <= n <= len(source) for m in mutants_for_line(path, n, source[n - 1])]


def apply(text, mutant):
    lines = text.splitlines(keepends=True)
    current = lines[mutant.line - 1]
    body = current.rstrip("\r\n")
    if body != mutant.before:
        raise ValueError(f"{mutant.id}: line changed since the plan")
    lines[mutant.line - 1] = mutant.after + current[len(body):]
    return "".join(lines)
