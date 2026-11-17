"""Stage preserved declarations and explicitly reviewed plain value records.

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

# Reviewed pointer-free host configuration. Backend callbacks and owners remain
# separate resources; this record contains no pointer or ownership projection.
ADDITIONS['xge/src/records.xl'] += (
    'export extern "xge_host_session_desc_t" struct HostSessionDesc @header("xge3_runtime.h")\nc_uint size\nc_int width\nc_int height\nc_float dpiScale\nc_uint framebuffer\nend',
)


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


def verify_replay_package(packages, receipt):
    """Verify a replay's selected units and every unchanged package byte."""
    packages,receipt=Path(packages).resolve(),Path(receipt).resolve()
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    data=json.loads(receipt.read_text(encoding='utf8'))
    if receipt.parent!=packages.parent or packages.name not in ('host','static'):
        raise ValueError('Replay receipt does not belong to this package role')
    if not(data.get('complete') and data.get('passed')) or data.get('shippingDelivery') is not False:
        raise ValueError('Replay receipt is not a completed local candidate')
    if set(data.get('selected',()))!=set(SELECTED) or set(data.get('declarationUnits',{}))!=set(SELECTED):
        raise ValueError('Replay receipt omits declaration preservation')
    pins=data.get('sourceInputs',{})
    if not isinstance(pins,dict) or not pins or any(not Path(p).is_file() or sha(p)!=h for p,h in pins.items()):
        raise ValueError('Replay source input changed or is absent')
    outputs=data.get('outputs',{}).get(packages.name,{})
    if set(outputs)!=set(SELECTED):raise ValueError('Replay receipt omits selected outputs')
    selected={installed_path(r):h for r,h in outputs.items()}
    for r,h in selected.items():
        if not(packages/r).is_file() or sha(packages/r)!=h:raise ValueError('Replay output changed: '+r)
    for r in SELECTED:
        row=data['declarationUnits'][r]
        if not isinstance(row,dict) or set(row)!=set(('preserved','approvedAdditions')) or any(type(v) is not int or v<0 for v in row.values()):
            raise ValueError('Invalid declaration preservation counts')
        if sum(row.values())!=sum(source_units(packages/installed_path(r)).values()):
            raise ValueError('Replay declaration preservation count changed: '+r)
    manifest='_xge_native/xlang.package.json';identity=sha(packages/manifest)
    origins={Path(p).parent.parent for p,h in pins.items()
        if Path(p).as_posix().endswith('/'+manifest) and h==identity}
    if len(origins)!=1:raise ValueError('Replay has no unique native package origin')
    origin=next(iter(origins))
    expected_names={Path(p).relative_to(origin).as_posix() for p in pins if Path(p).is_relative_to(origin)}
    marker='/lib/bindings/xge/debug/'
    expected_names.update('xgeDebug/'+Path(p).as_posix().split(marker,1)[1] for p in pins if marker in Path(p).as_posix())
    expected_names.update(selected)
    present={path.relative_to(packages).as_posix():path for path in packages.rglob('*') if path.is_file()}
    if set(present)!=expected_names:raise ValueError('Replay package file population changed')
    for relative,path in present.items():
        if relative in selected:continue
        expected=pins.get(str(origin/relative))
        if expected is None and relative.startswith('xgeDebug/'):
            suffix='/lib/bindings/xge/debug/'+relative[len('xgeDebug/'):]
            choices={h for p,h in pins.items() if Path(p).as_posix().endswith(suffix)}
            if len(choices)==1:expected=next(iter(choices))
        if expected is None or sha(path)!=expected:raise ValueError('Unreviewed replay package file: '+relative)
    return [receipt,Path(__file__).resolve(),Path(__file__).with_name('xui_managed_contracts.py').resolve(),*[Path(p) for p in pins]]


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
