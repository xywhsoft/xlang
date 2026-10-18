"""Offline native Channel Future ABI against ordinary runtime/compiler products.
No implementation object, symbol override or language Future claim is injected.
Every provider, private header and ownership-catalog consumer is same-source.
The existing future-ready stage separately checks ordinary language regression.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--direct',action='store_true',help='Exercise the direct typed send/receive provider')
    parser.add_argument('--deadline',action='store_true',help='Finite provider with shared source; no source-only fault injection')
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists()
    win=os.name=='nt';names=('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so')
    products=[root/'release'/name for name in names];assert all(path.is_file() for path in products)
    cc=shutil.which('gcc');assert cc;out.mkdir(parents=True);release=out/'release';release.mkdir()
    for path in products:shutil.copy2(path,release/path.name)
    if win:
        for path in products:shutil.copy2(path,out/path.name)
    fixture='stdlib_channel_deadline_native.c' if args.deadline else 'stdlib_channel_direct_future_native.c' if args.direct else 'stdlib_channel_future_native.c'
    inputs={Path(__file__).resolve(),Path(cc).resolve(),*products,root/'test/cases'/fixture}
    for folder in ('src','lib/xrt','lib/xruntime','release/include'):
        inputs.update(path for path in (root/folder).rglob('*') if path.is_file())
    def seal():return {str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(inputs)}
    before=seal();report={'Complete':False,'Scope':__doc__,'Inputs':before,'Rows':[],
        'NetworkTests':False,'NetworkSecurityTests':False,'LanguageChannelFutureAcceptance':False,
        'DirectProvider':args.direct,'DeadlineProvider':args.deadline,'PublicFiniteMethodsAccepted':False}
    def save():(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    env={key:value for key,value in os.environ.items() if not key.upper().startswith('XL6_') and
        key.upper() not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    def run(name,command):
        result=subprocess.run(list(map(str,command)),cwd=out,env=env,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['Rows'].append({'Name':name,'Command':list(map(str,command)),'Exit':result.returncode,'Sha256':hashlib.sha256(log.read_bytes()).hexdigest()});save()
        assert result.returncode==0,(name,log.read_bytes()[-2200:])
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-Wno-unused-function',
        '-Wno-unused-parameter','-Wno-missing-field-initializers','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    if args.deadline:flags+=['-DTEST_SELECT='+str(int(not args.direct))]
    try:
        for product in products:
            links=[release/product.name] if win else ['-L'+str(release),'-l:'+product.name,'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
            for level in ('O0','O2'):
                lane=product.stem+'.'+level;binary=out/(lane+('.exe' if win else ''))
                run(lane+'.build',[cc,*flags,'-'+level,root/'test/cases'/fixture,*links,'-o',binary])
                run(lane+'.run',[binary])
        report['InputsUnchanged']=seal()==before;assert report['InputsUnchanged']
        report['LogsUnchanged']=all(hashlib.sha256((out/(row['Name']+'.log')).read_bytes()).hexdigest()==row['Sha256'] for row in report['Rows'])
        assert report['LogsUnchanged'];report['Complete']=True
    except Exception as error:report['Error']=repr(error);raise
    finally:save()
    print('Native Channel Future normal-product ABI passed:',len(report['Rows']),'phases',flush=True)

if __name__=='__main__':main()
