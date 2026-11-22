"""Own-process affinity and explicit background options must fail safely."""
import os
import subprocess
import sys
import unittest
from types import SimpleNamespace
from benchmark_lsp_analysis import Counters,validate_affinity_options


class AffinityOptionTests(unittest.TestCase):
    def test_default_is_unchanged(self):validate_affinity_options(None,None,False)
    def test_disjoint_masks(self):validate_affinity_options(2,1,True)
    def test_both_masks_are_required(self):
        for pair in((2,None),(None,1)):
            with self.assertRaisesRegex(ValueError,'both positive'):validate_affinity_options(*pair,True)
    def test_masks_must_be_positive(self):
        for pair in((0,1),(-1,2),(True,2)):
            with self.assertRaisesRegex(ValueError,'positive'):validate_affinity_options(*pair,True)
    def test_overlap_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'disjoint'):validate_affinity_options(3,1,True)
    def test_live_peer_mode_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'isolated'):validate_affinity_options(2,1,False)


@unittest.skipUnless(os.name=='nt','Windows process affinity backend')
class WindowsAffinityTests(unittest.TestCase):
    def setUp(self):self.counters=Counters();self.original=self.counters.affinity()
    def tearDown(self):
        self.counters.set_affinity(self.original['processMask'])
        self.assertEqual(self.counters.affinity(),self.original)
    def test_current_process_mask_is_verified_and_restored(self):
        bit=self.original['processMask']&-self.original['processMask']
        value=self.counters.set_affinity(bit);self.assertEqual(value['before'],self.original);self.assertEqual(value['after']['processMask'],bit)
    def test_invalid_masks_do_not_change_process(self):
        for value in(0,-1,True,1<<self.original['systemMask'].bit_length()):
            with self.assertRaisesRegex(ValueError,'positive subset'):self.counters.set_affinity(value)
            self.assertEqual(self.counters.affinity(),self.original)
    def test_owned_child_can_use_cores_outside_inherited_driver_mask(self):
        bits=[1<<i for i in range(self.original['processMask'].bit_length())if self.original['processMask']&(1<<i)]
        if len(bits)<2:self.skipTest('two allowed logical CPUs required')
        self.counters.set_affinity(bits[0]);child=None
        try:
            child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(30)'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            client=SimpleNamespace(process=child);value=self.counters.set_affinity(bits[-1],client);self.assertEqual(value['after']['processMask'],bits[-1])
        finally:
            if child is not None:child.kill();child.communicate(timeout=10)


if __name__=='__main__':unittest.main()
