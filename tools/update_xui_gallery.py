"""Generate the advanced gallery from the independently runnable lessons."""
from pathlib import Path
import argparse
import json
import re


def lesson_body(text):
    match = re.search(r'Window\.show(?:WithProxy)?\([^\n]+\)\n', text)
    if not match or not text.rstrip().endswith('end)'):
        raise ValueError('Expected one top-level Window.show initializer')
    return '\n'.join(line[1:] for line in text[match.end():].rstrip()[:-4].rstrip().splitlines())


def generate(source):
    catalog = json.loads((source / 'gallery.json').read_text(encoding='utf-8'))
    items = [row for row in catalog['examples'] if row['name'] != 'all_controls' and row.get('gallery', True)]
    chunks = ['// Generated gallery. Start learning with button.xl.\n'
              'import xui\nfrom xui import Widget\nfrom "./_window.xl" import Window\n']
    imported = {'from xui import Widget'}
    for row in items:
        text = (source / (row['name'] + '.xl')).read_text(encoding='utf-8')
        for line in text.splitlines():
            if line.startswith('from xui import ') and line not in imported:
                chunks.append(line)
                imported.add(line)
        body = lesson_body(text)
        returned = {'menu': 'menu', 'window': 'window'}.get(row['name'])
        chunks.append('func scene_' + row['name'] + '(xui.Context ui,xui.Widget root,xui.Proxy proxy) as xui.Widget?\n'
                      + '\n'.join(' ' + line if line else '' for line in body.splitlines())
                      + '\n return ' + (returned or 'null') + '\nend\n')
    chunks.append('func populate(c_int index,xui.Context ui,xui.Widget root,xui.Proxy proxy) as xui.Widget?\n'
                  + ''.join(' if index==' + str(i) + ' then return scene_' + row['name'] + '(ui,root,proxy) end\n'
                            for i, row in enumerate(items)) + ' return null\nend\n')
    chunks.append('''Window.showWithProxy("Control gallery",func(xui.Context ui,xui.Widget root,xui.Proxy proxy)
 auto selector=ui.comboBoxNative()
 Window.place(root,selector,24,12,640,40)
 selector.setItems([''' + ','.join(json.dumps(row['name']) for row in items) + '''])
 selector.setSelected(0)
 xui.Widget? current=null
 xui.Widget? floating=null
 auto showScene=func(c_int index)
  if floating!=null then
   auto overlay=floating as Widget
   overlay.close()
   floating=null
  end
  if current!=null then
   auto previous=current as Widget
   previous.close()
  end
  auto page=ui.widget()
  current=page
  Window.place(root,page,0,64,760,480)
  floating=populate(index,ui,page,proxy)
  ui.invalidateAll()
 end
 selector.setSelect(func(xui.Widget sender,c_int index,c_int value)
  if index>=0 and int(index)<''' + str(len(items)) + ''' then showScene(index) end
 end)
 showScene(c_int(0))
 // Acceptance slideshow; individual lessons contain no synthetic events.
 if env.get("XUI_EXAMPLE_CYCLE")!=null then
  selector.setSelect(null)
  println("SCENE 0")
  int tick=0
  int scene=0
  root.setUpdateCallback(func(xui.Widget sender,c_float delta) as c_int
   tick+=1
   if tick%3==0 and scene+1<''' + str(len(items)) + ''' then
    scene+=1
    showScene(c_int(scene))
    selector.setSelected(c_int(scene))
    println("SCENE "+string(scene))
   end
   return c_int(0)
  end)
 end
end)
''')
    return '\n'.join(chunks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1] / 'lib/bindings/xui/examples'
    expected = generate(source)
    target = source / 'all_controls.xl'
    if args.check:
        if target.read_text(encoding='utf-8') != expected:
            raise SystemExit('Regenerate all_controls.xl with tools/update_xui_gallery.py')
    else:
        target.write_text(expected, encoding='utf-8')
    print('Gallery matches its standalone lessons')


if __name__ == '__main__':
    main()
