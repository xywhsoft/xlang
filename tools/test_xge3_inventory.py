"""C callback parameter names must not be invented as API typedefs."""
from pathlib import Path
import tempfile,unittest
from check_xge3_bindings import header_inventory


class InventoryTests(unittest.TestCase):
    def parse(self,text):
        with tempfile.TemporaryDirectory(prefix='xge-inventory-')as temporary:
            header=Path(temporary)/'xui.h';header.write_text(text)
            return header_inventory(header)

    def test_nested_named_callbacks(self):
        result=self.parse('''typedef struct Payload Payload;
typedef unsigned long size_t;
XUI_API int stage(const Payload* payload, size_t budget,
 int (*cancelled)(void*), void (*notify)(Payload*, int), void* user);
''')
        self.assertEqual(result['parseErrors'],[])
        parameters=result['functions'][0]['type']['parameters']
        self.assertEqual([v['name']for v in parameters],['payload','budget','cancelled','notify','user'])
        self.assertEqual(parameters[2]['type']['to']['kind'],'function')
        self.assertEqual(parameters[2]['type']['to']['returns']['names'],['int'])
        self.assertEqual(parameters[3]['type']['to']['parameters'][0]['type']['to']['names'],['Payload'])

    def test_undeclared_type_is_rejected(self):
        result=self.parse('XUI_API int invalid(Misspelled payload);')
        self.assertEqual(result['functions'],[])
        self.assertEqual(len(result['parseErrors']),1)

    def test_declared_callback_alias(self):
        result=self.parse('typedef int (*Callback)(void*); XUI_API int valid(Callback callback);')
        self.assertEqual(result['parseErrors'],[])
        self.assertEqual(result['functions'][0]['type']['parameters'][0]['type']['names'],['Callback'])


if __name__=='__main__':unittest.main()
