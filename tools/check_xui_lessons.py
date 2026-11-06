"""Check tutorial organization and generate interaction probes from lesson code.

These checks do not replace compiling/running the lessons. Probes reuse the
actual initializer bodies, then add assertions outside the learning material.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
from update_xui_gallery import generate, lesson_body


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = root / 'lib/bindings/xui/examples'
    catalog = json.loads((source / 'gallery.json').read_text(encoding='utf-8'))
    expected = {row['name'] for row in catalog['examples']}
    actual = {path.stem for path in source.glob('*.xl') if not path.name.startswith('_')}
    assert expected == actual, (sorted(expected - actual), sorted(actual - expected))
    assert (source / 'all_controls.xl').read_text(encoding='utf-8') == generate(source)
    for row in catalog['examples']:
        text = (source / (row['name'] + '.xl')).read_text(encoding='utf-8')
        assert not re.search(r'^\s*(?:export )?class\s', text, re.M), row['name']
        assert row['name'] == 'all_controls' or not re.search(r'\bassert\b|XUI_EXAMPLE_', text), row['name']
        assert row['name'] == 'all_controls' or text.startswith('// '), row['name']
        if row['name'] != 'all_controls' and row.get('kind') != 'cpu':
            lesson_body(text)
    if args.out:
        args.out.mkdir(parents=True, exist_ok=False)
        injections = {
            'slider': '''
 ui.layout()
 auto bounds=slider.getWorldRect()
 auto x=c_int(int(bounds.fX)+int(bounds.fW)*3/4)
 auto y=c_int(int(bounds.fY)+int(bounds.fH)/2)
 ui.inputPointerDown(x,y,c_int(xui.XUI_POINTER_BUTTON_LEFT),c_uint(xui.XUI_POINTER_BUTTON_LEFT))
 ui.inputPointerUp(x,y,c_int(xui.XUI_POINTER_BUTTON_LEFT),c_uint(0))
 ui.dispatchPendingEvents()
 assert float(slider.getValue())>60
 assert (value.getText() as string)!="当前值：35"
 println("slider pointer and float callback delivered")
''',
            'virtualjoystick': '''
 assert joystick.setValue(c_float(0.5),c_float(-0.25),c_int(1))==0
 assert (position.getText() as string)=="X: 0.5, Y: -0.25"
 println("joystick float state callback delivered")
''',
            'button': '''
 ui.layout()
 auto bounds = button.getWorldRect()
 auto x = c_int(int(bounds.fX)+10)
 auto y = c_int(int(bounds.fY)+10)
 ui.inputPointerDown(x,y,c_int(xui.XUI_POINTER_BUTTON_LEFT),c_uint(xui.XUI_POINTER_BUTTON_LEFT))
 ui.inputPointerUp(x,y,c_int(xui.XUI_POINTER_BUTTON_LEFT),c_uint(0))
 ui.dispatchPendingEvents()
 assert (button.getText() as string)=="你点击了按钮"
 println("button click delivered")
''',
            'radiogroup': '''
 group.setSelectedIndex(1)
 assert light.getChecked()==0 and dark.getChecked()!=0
 group.setSelectedIndex(0)
 assert light.getChecked()!=0 and dark.getChecked()==0
 println("radio exclusivity verified")
''',
            'cascader': '''
 assert categories.getItemCount()==5
 bool rejected=false
 try
  categories.setItems(names,values,[c_int(-1)],flags)
 catch error
  rejected=true
 end
 assert rejected and categories.getItemCount()==5
 println("cascader mismatch rejected without mutation")
''',
            'msgbox': '''
 box.setOpen(1)
 ui.layout()
 assert box.isOpen()!=0 and box.buttonCount()==2
 box.setOpen(0)
 println("message box opened and closed")
''',
            'filedialog': '''
 dialog.setOpen(1)
 ui.layout()
 assert dialog.isOpen()!=0
 dialog.cancel()
 assert dialog.isOpen()==0
 assert (selection.getText() as string)=="已取消选择"
 println("file dialog cancelled without file operations")
''',
            'msgtip': '''
 tip.show(0,"probe",2)
 assert tip.isOpen()!=0
 tip.dismiss()
 assert tip.isOpen()==0
 println("tip shown and dismissed")
''',
            'toast': '''
 c_int id=notifications.show(0,"probe","local notification",3)
 assert notifications.activeCount()+notifications.pendingCount()>0
 notifications.dismiss(id)
 notifications.clear()
 println("toast shown and dismissed")
''',
            'webview': '''
 println("WebView initialize result: "+string(int(result)))
 assert result==0 or result==xui.XUI_ERROR_UNSUPPORTED
''',
        }
        manifest = {}
        for name, injection in injections.items():
            text = (source / (name + '.xl')).read_text(encoding='utf-8')
            body = lesson_body(text)
            probe = ('// Generated acceptance probe; not a public tutorial.\nimport xui\n'
                     'from "' + (source / '_window.xl').as_posix() + '" import Window\n'
                     'Window.show("acceptance",func(xui.Context ui,xui.Widget root)\n'
                     + '\n'.join(' ' + line for line in body.splitlines()) + injection + 'end)\n')
            target = args.out / (name + '.xl')
            target.write_text(probe, encoding='utf-8')
            manifest[name] = dict(lessonSha256=hashlib.sha256(text.encode()).hexdigest(),
                                  probeSha256=hashlib.sha256(target.read_bytes()).hexdigest())
        (args.out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Tutorial organization verified:', len(actual), 'programs')


if __name__ == '__main__':
    main()
