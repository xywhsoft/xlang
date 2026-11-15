"""Stage preserved declarations and six explicitly reviewed plain value records.

Managed declarations must preserve class membership and all whole contracts.
This is a local candidate operation, never an installation into release/ or
the source tree. Existing SDK binaries/configuration are reused unchanged.
"""
from pathlib import Path
from collections import Counter
import argparse,hashlib,json,os,re,shutil
from xui_managed_contracts import managed_units


SELECTED=(
    'xge/src/constants.xl','xge/src/functions.xl','xge/src/records.xl',
    'xge/debug/src/functions.xl','xge/three_d/src/functions.xl',
    'xge/three_d/src/records.xl','xui/src/constants.xl','xui/src/records.xl',
    'xui/src/managed.xl',
)

# Plain value records exposed once symbolic native array bounds are resolved.
# No pointers, owners, callbacks or new language conversion contracts.
ADDITIONS={
    'xge/src/records.xl':(
        'export extern "xge_touch_event_t" struct TouchEvent @header("xge3_runtime.h")\nc_int iPhase\nc_int iCount\nTouchPoint arrPoints[8]\nend',),
    'xui/src/records.xl':(
        'export extern "xui_property_grid_category_t" struct PropertyGridCategory @header("xge3_runtime.h")\nc_char sId[64]\nc_char sName[96]\nc_int bExpanded\nc_int iPropertyCount\nend',
        'export extern "xui_code_search_result_t" struct CodeSearchResult @header("xge3_runtime.h")\nc_uint iSize\nc_int iStart\nc_int iEnd\nc_int iCaptureCount\nCodeRange arrCaptures[10]\nend',
        'export extern "xui_find_result_t" struct FindResult @header("xge3_runtime.h")\nc_uint iSize\nc_int iStart\nc_int iEnd\nc_int iCaptureCount\nCodeRange arrCaptures[10]\nend',
        'export extern "xui_code_document_batch_layout_t" struct CodeDocumentBatchLayout @header("xge3_runtime.h")\nc_uint iSize\nuint64 iCapture\nc_size iDocuments\nc_size iCaptureBytes\nend',
        'export extern "xui_code_document_batch_source_t" struct CodeDocumentBatchSource @header("xge3_runtime.h")\nc_uint iSize\nc_int iLength\nc_int iUndoCount\nend',),
}


def source_units(path):
    """Ignore declaration order/comments, preserve record field order and text."""
    if Path(path).name=='managed.xl':return managed_units(path)
    lines=Path(path).read_text(encoding='utf-8-sig').splitlines();units=[];record=None
    for line in lines:
        text=line.strip()
        if not text or text.startswith('//'):continue
        if record is not None:
            record.append(text)
            if text=='end':units.append('\n'.join(record));record=None
        elif re.match(r'export extern "[^"]+" struct \w+\b',text):record=[text]
        else:units.append(text)
    if record is not None:raise ValueError('unterminated record: '+str(path))
    return Counter(units)


def compare_selected(root,generated):
    result={}
    for relative in SELECTED:
        current=Path(root)/'lib/bindings'/relative;candidate=Path(generated)/'lib/bindings'/relative
        before,after=source_units(current),source_units(candidate)
        approved=Counter(ADDITIONS.get(relative,()))
        if before-after or (after-before)-approved or any(after[unit]!=1 for unit in approved):
            raise ValueError('regenerated declaration units differ: '+relative+' removed='+repr(list((before-after).elements())[:2])+' added='+repr(list((after-before).elements())[:2]))
        result[relative]=dict(preserved=sum(before.values()),approvedAdditions=sum((after-before).values()))
    return result

def installed_path(relative):
    if relative.startswith('xge/three_d/'):return 'xge3d/'+relative[len('xge/three_d/'):]
    if relative.startswith('xge/debug/'):return 'xgeDebug/'+relative[len('xge/debug/'):]
    return relative


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in('root','generated','dynamic-packages','static-packages','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,generated,dynamic,static,out=[v.resolve()for v in(a.root,a.generated,a.dynamic_packages,a.static_packages,a.out)]
    assert not out.is_relative_to(root)and not out.is_relative_to(generated)and not out.is_relative_to(dynamic)and not out.is_relative_to(static)
    units=compare_selected(root,generated);sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs={str(root/'lib/bindings'/r):sha(root/'lib/bindings'/r)for r in SELECTED}
    inputs[str(Path(__file__).resolve())]=sha(Path(__file__).resolve())
    inputs.update({str(generated/'lib/bindings'/r):sha(generated/'lib/bindings'/r)for r in SELECTED})
    for folder in(dynamic,static):inputs.update({str(f):sha(f)for f in folder.rglob('*')if f.is_file()})
    debug_source=root/'lib/bindings/xge/debug'
    inputs.update({str(f):sha(f)for f in debug_source.rglob('*')if f.is_file()})
    out.mkdir(parents=True,exist_ok=False)
    def copy(source,dest):
        # Only precompiled, immutable link/load inputs may share disk storage.
        # Source, headers, manifests and settings always get independent bytes.
        if Path(source).suffix in('.dll','.so','.a','.lib'):
            try:os.link(source,dest);return dest
            except OSError:pass
        return shutil.copy2(source,dest)
    for name,baseline in(('host',dynamic),('static',static)):
        target=out/name;shutil.copytree(baseline,target,copy_function=copy)
        if not(target/'xgeDebug/xlang.package.json').is_file():
            shutil.copytree(debug_source,target/'xgeDebug',copy_function=copy)
        for relative in SELECTED:
            src=generated/'lib/bindings'/relative;dest=target/installed_path(relative)
            assert dest.resolve().is_relative_to(target.resolve())
            shutil.copyfile(src,dest);assert sha(dest)==sha(src)
    assert all(sha(path)==digest for path,digest in inputs.items())
    report=dict(complete=True,passed=True,shippingDelivery=False,fullGeneratedPackagePromoted=False,nativeBinariesRebuilt=False,declarationUnits=units,selected=SELECTED,sourceInputs=inputs,outputs={name:{r:sha(out/name/installed_path(r))for r in SELECTED}for name in('host','static')})
    (out/'stage-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');print('Staged',len(SELECTED),'reviewed declaration files in host/static candidates')


if __name__=='__main__':main()
