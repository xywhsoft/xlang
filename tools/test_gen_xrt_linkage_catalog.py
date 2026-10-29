"""Combined root discovery must carry real presence/removal witnesses."""
import unittest
from gen_xrt_linkage_catalog import minimize_roots


class CombinedCatalogTests(unittest.TestCase):
    def test_combined_root_witness(self):
        calls=[]
        def provides(roots):calls.append(set(roots));return {'one','two'}<=set(roots)
        self.assertEqual(minimize_roots(['unused','one','other','two'],provides),('one','two'))
        self.assertIn({'one'},calls);self.assertIn({'two'},calls)

    def test_unconditional_combination_is_empty(self):
        self.assertEqual(minimize_roots(['one','two'],lambda _:True),())

    def test_combination_must_actually_provide(self):
        with self.assertRaises(ValueError):minimize_roots(['one'],lambda _:False)

if __name__=='__main__':unittest.main()
