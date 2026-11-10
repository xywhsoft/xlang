"""Exact final-Core ABI family with independently pinned source revisions."""
from pathlib import Path
import hashlib
import json

RECORD = json.loads(Path(__file__).with_name('native_final_c_interop_pins.json').read_text())
COHORT = RECORD['cohort']
PINS = RECORD['pins']
COMPLETION_PINS = {**PINS, 'src/sem/xl6_sem.c': '4ac3e20d91dbf0a5fc994d24d6c1fdb1e2e7b777d01b5bfd793f6b24cacf175d'}
CURRENT_RECORD_PATH = Path(__file__).with_name('native_current_core_pins_20261108.json')
CURRENT_RECORD = json.loads(CURRENT_RECORD_PATH.read_text(encoding='utf-8'))
CURRENT_RECORD_SHA256 = hashlib.sha256(CURRENT_RECORD_PATH.read_bytes()).hexdigest()
CURRENT_PINS = CURRENT_RECORD['pins']
PRODUCT_RECORD_PATH = Path(__file__).with_name('native_current_core_product_062_pins.json')
PRODUCT_RECORD = json.loads(PRODUCT_RECORD_PATH.read_text(encoding='utf-8'))
PRODUCT_RECORD_SHA256 = hashlib.sha256(PRODUCT_RECORD_PATH.read_bytes()).hexdigest()
PRODUCT_PINS = PRODUCT_RECORD['pins']
PROJECTION_RECORD_PATH = Path(__file__).with_name('native_current_projection_pins_20261111.json')
PROJECTION_RECORD = json.loads(PROJECTION_RECORD_PATH.read_text(encoding='utf-8'))
PROJECTION_PINS = PROJECTION_RECORD['pins']
PROJECTION_RECORD_SHA256 = hashlib.sha256(PROJECTION_RECORD_PATH.read_bytes()).hexdigest()
_projection_parent_lf = PRODUCT_RECORD_PATH.read_bytes().replace(b'\r\n',b'\n')
if (PROJECTION_RECORD['parentRecordSha256'] not in {hashlib.sha256(data).hexdigest() for data in (_projection_parent_lf,_projection_parent_lf.replace(b'\n',b'\r\n'))} or
    set(PROJECTION_PINS) != set(PRODUCT_PINS) | {'src/sem/xl6_sem_foreign_projection_compat.c','src/ir/xl6_ir_foreign_projection_compat.c'}):
    raise ValueError('projected-record frontend changes its reviewed source family')
_parent_lf = CURRENT_RECORD_PATH.read_bytes().replace(b'\r\n',b'\n')
_parent_hashes = {hashlib.sha256(data).hexdigest() for data in
                  (_parent_lf,_parent_lf.replace(b'\n',b'\r\n'))}
if (PRODUCT_RECORD['parentRecordSha256'] not in _parent_hashes or
    PRODUCT_RECORD['sourceRevision'] != CURRENT_RECORD['sourceRevision'] or
    PRODUCT_RECORD['sourcePaths'] != CURRENT_RECORD['sourcePaths'] or
    set(PRODUCT_PINS) != set(CURRENT_PINS) or
    [name for name in CURRENT_PINS if PRODUCT_PINS[name] != CURRENT_PINS[name]] != ['release/include/xl_version.h']):
    raise ValueError('product-only frontend revision changes its source/ABI family')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _recover_current(data, name, pins=None):
    # Share the existing exact-digest EOL recovery. No new content repair rule.
    from native_frontend_inputs import recover
    return recover(data, name, CURRENT_PINS if pins is None else pins)


def _current_runtime(frontend):
    path=frontend/'lib/xrt/xrt.h'
    if not path.is_file():return False
    data=path.read_bytes();digest=hashlib.sha256(data).hexdigest()
    if digest==PINS['lib/xrt/xrt.h']:return False
    if digest==CURRENT_PINS['lib/xrt/xrt.h']:return True
    return _recover_current(data,'lib/xrt/xrt.h') is not None


def pins_for(frontend):
    header=source_path(frontend,'src/ir/xl6_ir.h')
    if header.is_file() and _recover_current(header.read_bytes(),'src/ir/xl6_ir.h',PROJECTION_PINS) is not None:
        return PROJECTION_PINS
    if _current_runtime(frontend):
        path=source_path(frontend,'release/include/xl_version.h')
        if path.is_file() and _recover_current(path.read_bytes(),'release/include/xl_version.h',PRODUCT_PINS) is not None:
            return PRODUCT_PINS
        return CURRENT_PINS
    path=frontend/'src/sem/xl6_sem.c'
    return COMPLETION_PINS if path.is_file() and sha(path)==COMPLETION_PINS['src/sem/xl6_sem.c'] else PINS


def completion_facts(frontend):
    return pins_for(frontend) is not PINS


def source_revision(frontend):
    pins=pins_for(frontend)
    if pins is PROJECTION_PINS:return PROJECTION_RECORD['sourceRevision']
    return CURRENT_RECORD['sourceRevision'] if pins is CURRENT_PINS or pins is PRODUCT_PINS else 'final-completion-facts-20261108' if pins is COMPLETION_PINS else 'final-core-r2-20261105'


def selected(frontend):
    if _current_runtime(frontend):
        path=frontend/'src/ast/xl6_ast.h'
        return path.is_file() and _recover_current(path.read_bytes(),'src/ast/xl6_ast.h') is not None
    return all((frontend/name).is_file() and sha(frontend/name)==PINS[name]
               for name in ('src/ast/xl6_ast.h','lib/xrt/xrt.h'))


def source_path(frontend, name):
    direct=frontend/name
    if direct.is_file():return direct
    return frontend/PROJECTION_RECORD['sourcePaths'].get(name,CURRENT_RECORD['sourcePaths'].get(name,name))


def profile(frontend):
    if not selected(frontend):
        raise ValueError('final frontend AST/XRT anchors differ')
    pins=pins_for(frontend);inputs={}
    for name,expected in pins.items():
        path=source_path(frontend,name)
        if not path.is_file():raise ValueError('final frontend contract dependency missing: '+name)
        data=path.read_bytes();actual=hashlib.sha256(data).hexdigest()
        if actual!=expected and (all(pins is not p for p in (CURRENT_PINS,PRODUCT_PINS,PROJECTION_PINS)) or _recover_current(data,name,pins) is None):
            raise ValueError('final frontend contract dependency changed: '+name)
        inputs[str(path)]=actual
    return inputs


def prepare(frontend,destination):
    original=profile(frontend);pins=pins_for(frontend);revision=source_revision(frontend)
    if pins is CURRENT_PINS:original[str(CURRENT_RECORD_PATH.resolve())]=CURRENT_RECORD_SHA256
    if pins is PRODUCT_PINS:
        original[str(CURRENT_RECORD_PATH.resolve())]=CURRENT_RECORD_SHA256
        original[str(PRODUCT_RECORD_PATH.resolve())]=PRODUCT_RECORD_SHA256
    if pins is PROJECTION_PINS:
        original[str(PROJECTION_RECORD_PATH.resolve())]=PROJECTION_RECORD_SHA256
        original[str(PRODUCT_RECORD_PATH.resolve())]=PRODUCT_RECORD_SHA256
    destination=destination.resolve();destination.mkdir(parents=True,exist_ok=False)
    projected=[]
    for name,expected in pins.items():
        source=source_path(frontend,name);target=destination/name;data=source.read_bytes()
        operation='SDK-layout-projection'
        if any(pins is p for p in (CURRENT_PINS,PRODUCT_PINS,PROJECTION_PINS)):
            recovered=_recover_current(data,name,pins)
            if recovered is None:raise ValueError('final frontend changed during source projection: '+name)
            data,operation=recovered
        target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
        if sha(target)!=expected:
            raise ValueError('final frontend changed during source projection: '+name)
        if name!=source.relative_to(frontend).as_posix() or operation not in ('exact','SDK-layout-projection'):
            projected.append(dict(path=name,source=str(source),sha256=expected,operation=operation))
    if any(sha(path)!=expected for path,expected in original.items()):
        raise ValueError('final frontend changed during source projection')
    return destination,dict(status='final-cohort-layout-projection',cohort=COHORT,
        productVersion=PROJECTION_RECORD['productVersion'] if pins is PROJECTION_PINS else PRODUCT_RECORD['productVersion'] if pins is PRODUCT_PINS else None,
        sourceRevision=revision,sourceRoot=str(frontend),frontendRoot=str(destination),
        originalInputs=original,transformations=projected)
