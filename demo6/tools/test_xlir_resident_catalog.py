"""Unit tests for modular registry discovery, not shipping acceptance."""
from pathlib import Path
import tempfile
import unittest
from check_xlir_resident_catalog import symbol_tables, declarations


class SymbolTables(unittest.TestCase):
    def fixture(self, root, duplicate=False, cycle=False):
        compile_dir=root/'src/compile'
        runtime_dir=root/'src/runtime'
        compile_dir.mkdir(parents=True)
        runtime_dir.mkdir(parents=True)
        (compile_dir/'xl6_artifact_runtime_symbols.inc').write_text(
            '#include "src/runtime/xl6_text_symbols.inc"\n',encoding='utf-8')
        (runtime_dir/'xl6_text_symbols.inc').write_text(
            '#include "xl6_concurrent_symbols.inc"\n'*(2 if duplicate else 1),encoding='utf-8')
        (runtime_dir/'xl6_concurrent_symbols.inc').write_text(
            '#include "xl6_text_symbols.inc"\n' if cycle else
            'XL6_TEXT_EXPORT(xl6CancellationSourceCreateV1, CANCELLATION_SOURCE)\n',encoding='utf-8')

    def test_relative_and_root_includes(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);self.fixture(root)
            self.assertEqual([p.name for p in symbol_tables(root)],[
                'xl6_artifact_runtime_symbols.inc','xl6_text_symbols.inc','xl6_concurrent_symbols.inc'])

    def test_duplicate_include_is_not_hidden(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);self.fixture(root,duplicate=True)
            self.assertEqual([p.name for p in symbol_tables(root)].count('xl6_concurrent_symbols.inc'),2)

    def test_include_cycle_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);self.fixture(root,cycle=True)
            with self.assertRaisesRegex(AssertionError,'include cycle'):symbol_tables(root)

    def test_real_registry_includes_split_families(self):
        root=Path(__file__).resolve().parents[1]
        names=[p.name for p in symbol_tables(root)]
        for family in ('atomic','concurrent','asn1','crypto','compress','template'):
            self.assertEqual(names.count('xl6_'+family+'_symbols.inc'),1)

    def test_real_entries_ignore_macro_definitions(self):
        root=Path(__file__).resolve().parents[1]
        ids,entries=declarations(root)
        self.assertTrue(entries)
        self.assertEqual(len(entries),len({name for name,_ in entries}))
        self.assertTrue(all(owner in ids for _,owner in entries))
        for required in ('xl6AtomicBoolLoadV1','xl6CancellationSourceCreateV1',
                         'xlCancellationRetirementRetryCheckedV1'):
            self.assertIn(required,{name for name,_ in entries})


if __name__=='__main__':unittest.main()
