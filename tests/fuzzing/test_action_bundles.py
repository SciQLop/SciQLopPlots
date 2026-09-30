"""The fuzzer's state machine only holds the bundles some action targets.

An action drawing from a bundle nobody fills breaks the whole fuzzing module at
collection time (a KeyError in build_state_machine), not just that action.
"""
from tests.fuzzing.fuzzer_actions import FUZZER_ACTIONS


def test_every_consumed_bundle_is_produced():
    produced = {a._ui_meta.target for a in FUZZER_ACTIONS} - {None}
    orphans = sorted(
        (a.__name__, bundle)
        for a in FUZZER_ACTIONS
        for bundle in (a._ui_meta.bundles or {}).values()
        if bundle not in produced
    )
    assert orphans == [], f"actions drawing from bundles no action produces: {orphans}"
