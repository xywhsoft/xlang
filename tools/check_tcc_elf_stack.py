"""Actual freshly rebuilt TCC ELF stack contracts, including foreign note merge.
Default executable and shared output must have RW GNU_STACK and really execute.
An explicit executable-stack request must survive a prior non-executable note;
that foreign control is inspected, not loaded or used to relax runtime security.
"""
from pathlib import Path
import argparse, ctypes, hashlib, json, os, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,out=args.root.resolve(),args.out.resolve()
    assert os.name!='nt' and not out.exists()
    project=Path(__file__).resolve().parents[1]
    cc,readelf=[shutil.which(name) for name in ('gcc','readelf')]; assert cc and readelf
    tcc=root/'build/linux/tcc'; dist=root/'build/linux/tcc-dist'
    source=project/'test/cases/tcc_elf_stack.c'; request=project/'test/cases/tcc_elf_stack_request.s'
    files={Path(__file__).resolve(),Path(cc).resolve(),Path(readelf).resolve(),tcc,source,request}
    files.update(path for path in dist.rglob('*') if path.is_file())
    files.update(path for path in (root/'lib/tcc').rglob('*') if path.is_file())
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); rows=[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Inputs':inputs,'Rows':rows,
        'DefaultNonExecutableStackAcceptance':False,'ExplicitRequestMergeAcceptance':False,
        'ShippingLibraryAcceptance':False,'Full53Acceptance':False}
    def save(): (out/'tcc.elf.stack.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); result=subprocess.run(command,cwd=out,capture_output=True,timeout=120)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert result.returncode==0,(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def stack(name,path,expected):
        text=run(name,[readelf,'-lW',path])
        lines=[line for line in text.splitlines() if 'GNU_STACK' in line]
        assert len(lines)==1 and lines[0].split()[-2]==expected,lines
    try:
        binary=out/'default.exe'; library=out/'default.so'
        base=[tcc,'-B'+str(dist)]
        run('default.exe.build',[*base,source,'-o',binary]); stack('default.exe.stack',binary,'RW')
        run('default.exe.execute',[binary])
        run('default.so.build',[*base,'-shared',source,'-o',library]); stack('default.so.stack',library,'RW')
        image=ctypes.CDLL(str(library)); probe=image.elfStackProbe; probe.argtypes=[]; probe.restype=ctypes.c_int
        assert probe()==42
        rows.append({'Name':'default.so.execute','Command':['ctypes.CDLL',str(library),'elfStackProbe'],
            'Exit':0}); save()
        plain=out/'plain.o'; marked=out/'request.o'; foreign=out/'foreign.so'
        run('plain.object',[cc,'-std=c11','-Wall','-Wextra','-Werror','-fPIC','-c',source,'-o',plain])
        run('request.object',[cc,'-c',request,'-o',marked])
        run('foreign.merge',[*base,'-shared',plain,marked,'-o',foreign])
        stack('foreign.merge.stack',foreign,'RWE')
        # Reverse input order verifies that the merged note never clears X.
        reverse=out/'reverse.so'
        run('foreign.reverse',[*base,'-shared',marked,plain,'-o',reverse])
        stack('foreign.reverse.stack',reverse,'RWE')
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged'] and len(rows)==12
        report['Complete']=True; report['DefaultNonExecutableStackAcceptance']=True
        report['ExplicitRequestMergeAcceptance']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('TCC ELF stack: 12 phases; RW defaults execute, explicit RWE merge preserved',flush=True)
if __name__=='__main__': main()
