from pathlib import Path
import os
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from check_delivery_native_fixtures import report_inputs


class NativeInputReports(unittest.TestCase):
    def test_path_normalization(self):
        inputs = {'source/../source/item.c': 'abc', 'source/item.c': 'abc'}
        self.assertEqual(report_inputs(inputs), {os.path.normcase('source/item.c'): 'abc'})
        with self.assertRaises(ValueError):
            report_inputs({'source/../source/item.c': 'abc', 'source/item.c': 'def'})

    @unittest.skipUnless(os.name == 'nt', 'Windows paths are case-insensitive')
    def test_ps5_case_aliases(self):
        self.assertEqual(len(report_inputs({'C:/tools/gcc.exe': 'abc', 'c:\\tools\\GCC.EXE': 'abc'})), 1)
        with self.assertRaises(ValueError):
            report_inputs({'C:/tools/gcc.exe': 'abc', 'c:\\tools\\GCC.EXE': 'def'})


if __name__ == '__main__':
    unittest.main()
