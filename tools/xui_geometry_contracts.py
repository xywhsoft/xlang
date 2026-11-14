"""Reviewed zero-status XUI geometry outputs, shared by generation and patching.

Only native XUI_OK/status producers belong here. Chart's view/brush/animation
and optional series queries return 0/1 state as well as outputs; they must not
be adapted with this rule. Ordinary methods call their native function once.
"""
from pathlib import Path
import argparse
import re

# owner, native suffix, method, physical first output, logical inputs, outputs
QUERIES = [
    ('Button','GetIconLayout','getIconLayout',1,'',('c_int','c_float','c_float')),
    ('CheckCard','GetMetrics','getMetrics',1,'',('c_float',)*4),
    ('ScrollBar','GetMetrics','getMetrics',1,'',('c_float',)*3),
    ('Slider','GetMetrics','getMetrics',1,'',('c_float',)*3),
    ('RangeSlider','GetMetrics','getMetrics',1,'',('c_float',)*2),
    ('Carousel','GetMetrics','getMetrics',1,'',('c_float',)*4),
    ('VirtualJoystick','GetMetrics','getMetrics',1,'',('c_float',)*3),
    ('SplitLayout','GetDividerMetrics','getDividerMetrics',1,'',('c_float',)*3),
    ('Window','GetChrome','getChrome',1,'',('c_float',)*4),
    ('ScrollFrame','GetContentSize','getContentSize',1,'',('c_float',)*2),
    ('ScrollFrame','GetOffset','getOffset',1,'',('c_float',)*2),
    ('ScrollFrame','GetMetrics','getMetrics',1,'',('c_float',)*3),
    ('ScrollView','GetContentSize','getContentSize',1,'',('c_float',)*2),
    ('ScrollView','GetViewportHint','getViewportHint',1,'',('c_float',)*2),
    ('ScrollView','GetOffset','getOffset',1,'',('c_float',)*2),
    ('ScrollView','GetScrollbarPolicy','getScrollbarPolicy',1,'',('c_int',)*2),
    ('ScrollView','GetMetrics','getMetrics',1,'',('c_float',)*3),
    ('ScrollView','GetColors','getColors',1,'',('c_uint',)*6),
    ('ScrollView','GetButtonColors','getButtonColors',1,'',('c_uint',)*2),
    ('ScrollView','GetCornerColors','getCornerColors',1,'',('c_uint',)*2),
    ('Canvas','GetViewportHint','getViewportHint',1,'',('c_float',)*2),
    ('Canvas','GetOffset','getOffset',1,'',('c_float',)*2),
    ('Popup','GetScroll','getScroll',1,'',('c_float',)*2),
    ('Popup','GetScrollbarStyle','getScrollbarStyle',1,'',('c_int','c_float')),
    ('Popup','GetMetrics','getMetrics',1,'',('c_float',)*3),
    ('Chart','DataToPixel','dataToPixel',3,'c_double x,c_double y',('Vec2',)),
    ('Chart','PixelToData','pixelToData',3,'c_float x,c_float y',('c_double',)*2),
]


def declarations():
    for owner,suffix,method,first,params,outputs in QUERIES:
        native='xui'+owner+suffix
        slots=','.join(str(first+i)for i in range(len(outputs)))
        projection=('@c_output('+slots+',0)'if len(outputs)==1 else'@c_outputs('+slots+') @c_status(0)')
        result=outputs[0]if len(outputs)==1 else'('+','.join(outputs)+')'
        yield owner,native,method,' extern "'+native+'" @no_raise '+projection+' func '+method+'('+params+') as '+result


def extend(context,classes,mapped):
    for owner,native,method,row in declarations():
        target=classes[owner]
        target[:]=[v for v in target if 'extern "'+native+'"'not in v]
        target.append(row);mapped[native]='xui.'+owner+'.'+method


def patch(text):
    grouped={}
    for owner,native,method,row in declarations():grouped.setdefault(owner,[]).append((native,row))
    for owner,rows in grouped.items():
        pattern=r'^export extern "[^"]+" class '+re.escape(owner)+r'\b[^\n]*\n'
        match=re.search(pattern,text,re.M);assert match,owner
        start=match.end();end=text.index('\nend',start);body=text[start:end]
        for native,row in rows:
            body=re.sub(r'^\s*extern "'+re.escape(native)+r'"[^\n]*\n?', '',body,flags=re.M)
        body=body.rstrip()+'\n\n'+'\n\n'.join(row for _,row in rows)+'\n'
        text=text[:start]+body+text[end:]
    return text


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--apply',action='store_true',help='Apply only these reviewed methods; preserve all other declarations')
    a=p.parse_args();target=a.root/'lib/bindings/xui/src/managed.xl';text=target.read_text(encoding='utf-8');expected=patch(text)
    if a.apply:target.write_text(expected,encoding='utf-8',newline='\n')
    else:assert text==expected,'Run xui_geometry_contracts.py --apply to synchronize reviewed declarations'
    assert patch(expected)==expected,'projection patch must be idempotent'
    for _,native,_,row in declarations():assert expected.count('extern "'+native+'"')==1 and row in expected,native
    print(str(len(QUERIES))+' reviewed geometry queries synchronized')


if __name__=='__main__':main()
