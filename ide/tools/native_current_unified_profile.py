"""Reviewed unified XRT and C-owned callable/match fixes,2026-11-01.

These are exact committed assets,not permission to mix arbitrary new checker
or runtime versions with old memo assumptions. Callers validate every returned
pin; the older parser/AST/IR/include sets stay independently guarded.
"""
from pathlib import Path
import hashlib

COHORT = 'current-unified-20261101'
PINS = {
    'lib/xrt/xrt.h': '20b781e898c38a79a602024530313acfff2e558826e0039a89bbf974896eee06',
    'src/sem/xl6_sem.c': '0513f1fd31c4b23359e029ebf54ee3408ef104bafc1be0db1d1382f2f522c3d2',
    'src/sem/xl6_sem_c_owned.c': 'a79c57aeb3b4e2a5c467291acf45b6d19d7b4b302c36deac1b6df21b1cab963a',
}
CORPUS = (
    ('extern_owned_match_values', 1), ('extern_owned_bound_match', 1),
    ('extern_owned_bound_compound', 1), ('extern_owned_bound_callback_defaults', 1),
    ('extern_owned_bound_callback_arguments', 1), ('xlir_c_owned_nullable_callables', 1),
    ('bad_extern_owned_match_private', 0), ('bad_extern_owned_match_constraint', 0),
    ('bad_extern_owned_bound_compound_private', 0), ('bad_extern_owned_bound_compound_constraint', 0),
)


def selected(frontend: Path):
    path = frontend/'lib/xrt/xrt.h'
    return path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == PINS['lib/xrt/xrt.h']


def updated_pins(frontend: Path, previous):
    from native_c_interop_profile import selected as c_interop, role_pins
    if c_interop(frontend):return role_pins(frontend,previous)
    return {**previous, **PINS} if selected(frontend) else previous
