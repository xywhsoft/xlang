"""Pure checked-C-integer arithmetic against an independent 128-bit oracle.

This validates the arithmetic foundation; compiler/IR wiring is a separate gate.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest();files=[Path(__file__).resolve(),root/'src/runtime/xl6_checked_c_integer.h',root/'test/cases/xlir_checked_c_integer.c'];inputs={str(v):sha(v)for v in files}
    source=out/'source'
    for old in files:
        new=source/old.relative_to(root);new.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(old,new);assert sha(new)==inputs[str(old)]
    report=dict(complete=False,passed=False,compilerIntegration=False,scope=__doc__,inputs=inputs,rows=[]);save=lambda:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,cmd):
        cmd=list(map(str,cmd));start=time.monotonic();p=subprocess.run(cmd,capture_output=True,timeout=180);log=out/(name+'.log');log.write_bytes(p.stdout+p.stderr)
        report['rows'].append(dict(name=name,command=cmd,exitCode=p.returncode,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,p.returncode,flush=True);assert not p.returncode,log;return p.stdout
    try:
        cc=shutil.which('gcc');cpp=shutil.which('g++');assert cc and cpp
        for opt in('O0','O2'):
            program=out/(opt+('.exe'if os.name=='nt'else''));run(opt+'-build',[cc,'-std=c11','-'+opt,'-Wall','-Wextra','-Werror','-I'+str(source),source/'test/cases/xlir_checked_c_integer.c','-o',program]);run(opt+'-execute',[program])
        for standard in('c++11','c++17'):
            run('header-'+standard,[cpp,'-std='+standard,'-Wall','-Wextra','-Werror','-x','c++','-fsyntax-only',source/'src/runtime/xl6_checked_c_integer.h'])
        if os.name!='nt':
            program=out/'ubsan';run('ubsan-build',[cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(source),source/'test/cases/xlir_checked_c_integer.c','-o',program]);run('ubsan-execute',[program])
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));assert report['inputsUnchanged']
    except Exception as e:report.update(complete=True,error=str(e));raise
    finally:save()
if __name__=='__main__':main()
