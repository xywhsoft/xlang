"""Normal-product fixed executor: public JIT/module/AOT/strict C and SDK-only ABI.

No implementation override, source runtime fallback, external network target or
security-style test. The ordinary build receipt must already be complete.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out','build'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root,out,build=args.root.resolve(),args.out.resolve(),args.build.resolve()
    receipt=json.loads(build.read_text(encoding='utf-8'))
    assert receipt['Complete'] and receipt['InputsUnchanged'] and not receipt.get('Error')
    assert not out.exists();out.mkdir(parents=True)
    task=Path(__file__).resolve().parents[1];cc=shutil.which('gcc');assert cc
    win=os.name=='nt';products=('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so')
    files={Path(__file__).resolve(),task/'tools/check_stdlib_five_groups.py',task/'test/cases/xlir_fixed_executor_product.c',
           Path(cc).resolve(),build,*[root/'release'/name for name in products]}
    def seal():return {str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(files)}
    before=seal();report={'Complete':False,'Scope':__doc__,'Rows':[],'Inputs':before,'NetworkTests':False,'NetworkSecurityTests':False}
    environment=dict(os.environ)
    if win:environment['PATH']=str(root/'release')+os.pathsep+environment.get('PATH','')
    def save():(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,cwd=root):
        started=time.time()
        with (out/(name+'.log')).open('wb') as log:
            result=subprocess.run(list(map(str,command)),cwd=cwd,env=environment,stdout=log,stderr=subprocess.STDOUT,timeout=1200)
        report['Rows'].append({'Name':name,'Exit':result.returncode,'Command':list(map(str,command)),'Seconds':time.time()-started});save()
        assert result.returncode==0,(name,(out/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-1800:])
    save()
    try:
        language=out/'language'
        run('language',[sys.executable,task/'tools/check_stdlib_five_groups.py','--stage','fixed-executor','--root',root,'--out',language])
        accepted=json.loads((language/'results.json').read_text(encoding='utf-8'))
        assert accepted['Complete'] and accepted['InputsUnchanged'] and accepted['LogsUnchanged'] and not accepted['Failures']
        for product in products:
            for level in ('0','2'):
                label=product+'.o'+level;binary=out/(label+('.exe' if win else ''))
                flags=['-std=c11','-Wall','-Wextra','-Werror','-O'+level,'-I'+str(language/'sdk')]
                links=[root/'release'/product] if win else ['-L'+str(root/'release'),'-Wl,-rpath,'+str(root/'release'),'-l:'+product,'-pthread','-ldl','-lm']
                run(label+'.build',[cc,*flags,task/'test/cases/xlir_fixed_executor_product.c',*links,'-o',binary])
                run(label+'.run',[binary])
        report['LanguagePhases']=len(accepted['Rows']);report['InputsUnchanged']=seal()==before
        assert report['InputsUnchanged'];report['Complete']=True
    except Exception as error:report['Error']=repr(error);raise
    finally:save()
    print('Normal fixed executor products accepted:',report['LanguagePhases'],'language phases + 8 ABI phases',flush=True)


if __name__=='__main__':main()
