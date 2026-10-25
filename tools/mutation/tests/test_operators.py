import pytest

from operators import apply, mutants_for_file, mutants_for_line

CPP_SRC = """\
#include <vector>
// a < b in a comment
int clamp(int v, std::vector<double> &out)
{
    if (v < 0 && out.empty())
        return 0;
    out.push_back(v + 1);
    return v;
}
"""


def ops(path, line):
    return {m.op: m.after for m in mutants_for_line(path, 1, line)}


def test_comparison_logic_negation_and_constant():
    found = ops("a.cpp", "    if (v < 0 && out.empty())")
    assert found["lt_to_le"] == "    if (v <= 0 && out.empty())"
    assert found["and_to_or"] == "    if (v < 0 || out.empty())"
    assert found["negate_if"] == "    if (!(v < 0 && out.empty()))"
    assert found["int_plus_one"] == "    if (v < 1 && out.empty())"


def test_templates_comments_and_preprocessor_are_not_mutated():
    assert ops("a.cpp", "int clamp(int v, std::vector<double> &out)") == {}
    assert ops("a.cpp", "// a < b in a comment") == {}
    assert ops("a.cpp", "#include <vector>") == {}
    assert ops("a.cpp", "    double x = 1.0;") == {}


def test_trailing_comment_is_kept_and_not_mutated():
    found = ops("a.cpp", "    x = a + b; // a - b")
    assert found["plus_to_minus"] == "    x = a - b; // a - b"
    assert "minus_to_plus" not in found


def test_delete_statement_only_on_calls_and_assignments():
    assert ops("a.cpp", "    out.push_back(v + 1);")["delete_statement"] == "    ;"
    assert ops("a.cpp", "    mRange = range;")["delete_statement"] == "    ;"
    assert "delete_statement" not in ops("a.cpp", "    return v;")
    assert "delete_statement" not in ops("a.cpp", "    auto x = f();")


def test_return_constant_flip():
    assert ops("a.cpp", "        return true;")["true_to_false"] == "        return false;"
    assert ops("m.py", "        return False")["false_to_true"] == "        return True"


def test_python_operators():
    found = ops("m.py", "    if a is None and b > 2:")
    assert found["and_to_or"] == "    if a is None or b > 2:"
    assert found["gt_to_ge"] == "    if a is None and b >= 2:"
    assert found["negate_if"] == "    if not (a is None and b > 2):"
    assert found["int_plus_one"] == "    if a is None and b > 3:"
    assert ops("m.py", "    elif x:")["negate_if"] == "    elif not (x):"
    assert ops("m.py", "    plot.replot()")["delete_statement"] == "    pass"
    assert ops("m.py", "    # a < b") == {}


def test_one_mutant_per_operator_per_line():
    found = mutants_for_line("a.cpp", 1, "    x = a + b + c;")
    assert [m.op for m in found].count("plus_to_minus") == 1


def test_mutants_for_file_only_on_listed_lines():
    found = mutants_for_file("a.cpp", CPP_SRC, {5})
    assert found and {m.line for m in found} == {5}
    assert found[0].id == f"a.cpp:5:{found[0].col}:{found[0].op}"


def test_apply_replaces_one_line_and_keeps_newlines():
    mutant = next(m for m in mutants_for_file("a.cpp", CPP_SRC, {7}) if m.op == "plus_to_minus")
    out = apply(CPP_SRC, mutant)
    assert out.splitlines()[6] == "    out.push_back(v - 1);"
    assert out.count("\n") == CPP_SRC.count("\n")


def test_apply_refuses_a_changed_line():
    mutant = mutants_for_file("a.cpp", CPP_SRC, {7})[0]
    with pytest.raises(ValueError, match="line changed"):
        apply(CPP_SRC.replace("v + 1", "v + 2"), mutant)
