from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from wiring_python_literals import literals


class WiringPythonLiterals(unittest.TestCase):
    def test_finite_tables(self):
        values = literals("""
name = 'alpha'
files = ['stdlib_' + item + '_host' for item in ('codec', 'compress')]
for stem in files:
    fixture = stem + '.c'
for stem, opt in [('beta', 'O0'), ('gamma', 'O2')]:
    source = f'{stem}_contract.c'
importlib.import_module('local_runner')
""")
        for expected in ('stdlib_codec_host.c', 'stdlib_compress_host.c',
                         'beta_contract.c', 'gamma_contract.c', 'local_runner.py'):
            self.assertIn(expected, values)

    def test_unknowns_and_calls_are_not_evaluated(self):
        values = literals("""
filename = input('prompt')
source = filename + '_host.c'
danger = __import__('os').system('do not execute')
# from comment_only import runner
""")
        self.assertNotIn('comment_only.py', values)
        self.assertNotIn('filename_host.c', values)
        self.assertNotIn('None_host.c', values)

    def test_unknown_function_arguments_shadow_outer_names(self):
        values = literals("""
filename = 'outer'
def run(filename):
    source = filename + '_host.c'
def local():
    filename = input('path')
    source = filename + '_contract.c'
""")
        self.assertNotIn('outer_host.c', values)
        self.assertNotIn('outer_contract.c', values)


if __name__ == '__main__':
    unittest.main()
