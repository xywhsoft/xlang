import unittest
from gen_xrt_xlang_matrix import _native_address_references


class NativeAddressReferences(unittest.TestCase):
    def test_positional_and_designated_tables(self):
        code='''static const Ops a={xrtDrive,xrtUpdate,NULL};
        static const Ops b={.run=&xrtDrive, .update=xrtUpdate};'''
        self.assertEqual(_native_address_references(code,{'xrtDrive','xrtUpdate'}),{'xrtDrive','xrtUpdate'})

    def test_comments_strings_and_types_do_not_claim_integration(self):
        code='''/* {xrtDrive,xrtUpdate} */
        const char* text="{xrtDrive,xrtUpdate}";
        char delimiter=',';
        // .run=xrtDrive,
        struct xrtDrive (*fn)(int);
        static const Data data={xrtData,unknownSymbol};'''
        self.assertEqual(_native_address_references(code,{'xrtDrive','xrtUpdate'}),set())

    def test_no_substrings_or_function_calls(self):
        code='''static const Ops a={xrtDriveMore, xrtDrive()};'''
        self.assertEqual(_native_address_references(code,{'xrtDrive'}),set())


if __name__=='__main__':
    unittest.main()
