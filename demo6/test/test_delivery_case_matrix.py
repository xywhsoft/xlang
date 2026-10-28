from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from check_delivery_case_matrix import accepted


class MatrixEvidence(unittest.TestCase):
    def test_rejection_requires_exit_diagnostic_and_no_artifact(self):
        row={'ExpectedExit':1,'DiagnosticRegex':r'\[(sem|checked)\]'}
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory)/'module.c'
            self.assertTrue(accepted(row,1,'source.xl:1:2: [sem] invalid type',target))
            self.assertTrue(accepted(row,1,'source.xl:1:2: [checked] ABI refused',target))
            for code,text in ((0,'[sem] invalid'),(-1073741819,'[sem] invalid'),(1,''),(1,'[parse] invalid'),
                              (1,'[sem] error after [PARSE] failure'),(1,'[sem] out of memory'),
                              (1,'[checked] internal compiler error'),(1,'error: invalid')):
                self.assertFalse(accepted(row,code,text,target))
            target.write_text('partial output')
            self.assertFalse(accepted(row,1,'[sem] invalid type',target))

    def test_success_requires_nonempty_output(self):
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory)/'module.c';row={'ExpectedExit':0}
            self.assertFalse(accepted(row,0,'',target))
            target.write_text('')
            self.assertFalse(accepted(row,0,'',target))
            target.write_text('int main(void){return 0;}')
            self.assertTrue(accepted(row,0,'',target))
            self.assertFalse(accepted(row,1,'',target))


if __name__=='__main__':unittest.main()
