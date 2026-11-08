"""Independent, exact final-Core cohort; historical pins are never rewritten."""
from pathlib import Path
import hashlib
import json

RECORD = json.loads(Path(__file__).with_name('native_final_c_interop_pins.json').read_text())
COHORT = RECORD['cohort']
PINS = RECORD['pins']
COMPLETION_PINS = {**PINS, 'src/sem/xl6_sem.c': '4ac3e20d91dbf0a5fc994d24d6c1fdb1e2e7b777d01b5bfd793f6b24cacf175d'}

def pins_for(frontend):
    path=frontend/'src/sem/xl6_sem.c'
    return COMPLETION_PINS if path.is_file() and sha(path)==COMPLETION_PINS['src/sem/xl6_sem.c'] else PINS

def completion_facts(frontend):
    return pins_for(frontend) is COMPLETION_PINS


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def selected(frontend):
    return all((frontend/name).is_file() and sha(frontend/name)==PINS[name]
               for name in ('src/ast/xl6_ast.h','lib/xrt/xrt.h'))


def source_path(frontend, name):
    direct=frontend/name
    return direct if direct.is_file() else frontend/RECORD['sourcePaths'][name]


def profile(frontend):
    if not selected(frontend):
        raise ValueError('final frontend AST/XRT anchors differ')
    inputs={}
    for name,expected in pins_for(frontend).items():
        path=source_path(frontend,name)
        if not path.is_file() or sha(path)!=expected:
            raise ValueError('final frontend contract dependency changed: '+name)
        inputs[str(path)]=expected
    return inputs


def prepare(frontend,destination):
    original=profile(frontend)
    destination=destination.resolve();destination.mkdir(parents=True,exist_ok=False)
    projected=[]
    for name,expected in pins_for(frontend).items():
        source=source_path(frontend,name);target=destination/name
        target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(source.read_bytes())
        if sha(target)!=expected:
            raise ValueError('final frontend changed during source projection: '+name)
        if name!=source.relative_to(frontend).as_posix():
            projected.append(dict(path=name,source=str(source),sha256=expected,operation='SDK-layout-projection'))
    if any(sha(path)!=expected for path,expected in original.items()):
        raise ValueError('final frontend changed during source projection')
    return destination,dict(status='final-cohort-layout-projection',cohort=COHORT,
        sourceRevision='final-completion-facts-20261108' if completion_facts(frontend) else 'final-core-r2-20261105',sourceRoot=str(frontend),frontendRoot=str(destination),
        originalInputs=original,transformations=projected)
