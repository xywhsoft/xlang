"""Exercise the verified public source driver and its compiler-capable VFS.

Generated files live in a fresh evidence directory. No release product is
rebuilt or replaced by this gate; compiler inputs/core hashes stay pinned.
"""
from pathlib import Path
import argparse,ctypes,hashlib,json,os,subprocess

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True)
    p.add_argument('--directory',type=Path,required=True)
    args=p.parse_args();receipt=args.build.resolve();build=json.loads(receipt.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged']
    driver=Path(build['Driver']);assert sha(driver).lower()==build['DriverSha256'].lower()
    for row in build['Inputs']+build['Objects']+build['Headers']:
        assert sha(row['Path']).lower()==row['Sha256'].lower(),row['Path']
    core=Path(build['TccCore']);assert sha(core).lower()==build['TccCoreSha256'].lower()
    inputs={str(receipt):sha(receipt),str(driver):sha(driver),str(core):sha(core),str(Path(__file__)):sha(__file__)}
    out=args.directory.resolve();out.mkdir(parents=True,exist_ok=False);rows=[]
    def run(name,command):
        result=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,timeout=240)
        data=result.stdout+result.stderr;(out/(name+'.log')).write_bytes(data)
        rows.append(dict(name=name,exit=result.returncode))
        assert result.returncode==0,(name,data[-4000:].decode(errors='replace'))
        print(name+': PASS',flush=True)
    extension='.exe' if os.name=='nt' else ''
    # Keep emitted C away from the source basename: an adjacent same-name C
    # file is deliberately discovered as a native sidecar by the compiler.
    source=out/'scalar_input.xl';source.write_text('export "source_value" func value() as int\n    return 42\nend\nassert value()==42\n')
    run('emit',[driver,source,'emit',out/'scalar'])
    assert (out/'scalar.c').is_file() and (out/'scalar.h').is_file()
    run('jit',[driver,source,'run',out/'scalar-jit'])
    target=out/('scalar'+extension)
    run('aot-build',[driver,source,'exe',out/'scalar']);run('aot',[target])
    if os.name=='nt':
        shared=out/'shared_input.xl'
        shared.write_text('export "source_value" func value() as int\n    return 42\nend\n')
        library=out/'scalar.dll';run('shared-build',[driver,shared,'dll',out/'scalar'])
        dll=ctypes.CDLL(str(library));dll.source_value.restype=ctypes.c_int64
        assert dll.source_value()==42;rows.append(dict(name='shared-call',exit=0))
    nested=out/'nested.xl';nested.write_text('import xlang\nassert xlang.run("assert 6*7==42\\n")\n')
    run('nested-jit',[driver,nested,'run',out/'nested-jit'])
    target=out/('nested'+extension)
    run('nested-aot-build',[driver,nested,'exe',out/'nested']);run('nested-aot',[target])
    assert all(sha(path)==digest for path,digest in inputs.items())
    (out/'result.json').write_text(json.dumps(dict(complete=True,checks=rows,inputs=inputs),indent=2))
if __name__=='__main__':main()
