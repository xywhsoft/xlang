"""Current-source native callback component and standalone SDK integration.

Caller-lifetime declarations here name immutable resident API tables only.
They do not enable managed view language types or XGE/XUI package migration.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from xlir_public_build_inputs import verify_public_build

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('root','public-directory','out'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root,public,out=[v.resolve() for v in (a.root,a.public_directory,a.out)]
    build,pinned,_=verify_public_build(root,public);driver=Path(build['Driver'])
    cc=shutil.which('gcc');assert cc
    case=root/'test/cases/foreign_native_tables.xl';provider=root/'test/cases/foreign_native_tables_native.c'
    pinned.update((Path(__file__).resolve(),root/'tools/xlir_public_build_inputs.py',case,provider,Path(cc)))
    pinned.update((root/'src/runtime').glob('*.h'));pinned.add(root/'src/api/xl_foreign_error.h')
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs={str(path):sha(path) for path in pinned};out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(root),TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    report=dict(complete=False,passed=False,scope=__doc__,networkTests=False,sourceBuild=str(public/'build.json'),inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,cwd=root):
        started=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=cwd,env=env,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,
            seconds=round(time.monotonic()-started,3),logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);return r.returncode==0
    sdk=out/'include';sdk.mkdir()
    for path in (root/'src/runtime').glob('*.h'):shutil.copy2(path,sdk/path.name)
    shutil.copy2(root/'src/api/xl_foreign_error.h',sdk/'xl_foreign_error.h')
    unit=out/'headers.c';unit.write_text('#include "xl_foreign_error.h"\n#include "xl6_native_callback.h"\n'
        '#include "xl6_foreign_view_capture.h"\n#include "xl6_foreign_view_value.h"\n#include "xl6_foreign_view_sequence.h"\n#include "xl6_native_graph_future_payload.h"\nint main(void){return 0;}\n')
    for language,standard in (('c','c11'),('c++','c++11')):
        run('headers-'+language.replace('+','p'),[cc,'-x',language,'-std='+standard,'-Wall','-Wextra','-Werror',
            '-fsyntax-only','-I'+str(sdk),unit],out)
    for mode in ('emit','run','module','exe'):
        run('source-'+mode,[driver,case,mode,out/('source-'+mode),provider])
    run('standalone',[out/('source-exe.exe' if os.name=='nt' else 'source-exe')])
    report['inputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
    verify_public_build(root,public)
    report['complete']=True;report['passed']=report['inputsUnchanged'] and len(report['rows'])==7 and all(row['exitCode']==0 for row in report['rows'])
    save();assert report['passed'],out/'results.json'

if __name__=='__main__':main()
