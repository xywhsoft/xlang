"""Offline owned Future/native-record Channel slots and allocation prefixes.
Source objects are explicitly NOT normal-product acceptance. Without
--public-directory, link ordinary compiler/runtime products independently.
No private provider override, network or security testing is performed.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--public-directory',type=Path)
    args=parser.parse_args();root=args.root.resolve();out=args.out.resolve()
    assert not out.exists();out.mkdir(parents=True);cc=shutil.which('gcc');assert cc
    source=args.public_directory is not None;win=os.name=='nt';objects=[]
    inputs={Path(__file__).resolve(),Path(cc),root/'test/cases/stdlib_channel_owned_profiles_native.c'}
    if source:
        public=args.public_directory.resolve();receipt=public/'build.json'
        build=json.loads(receipt.read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly'];inputs.add(receipt)
        for row in build['Inputs']+build['Headers']+build['Objects']:
            assert sha(row['Path'])==row['Sha256'].lower(),row['Path'];inputs.add(Path(row['Path']))
        objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
            ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        products=[root/'release/xl.dll'] if win else [Path(build['NativeRoot'])/'release/libxl.so']
    else:products=[root/'release'/name for name in (('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so'))]
    inputs.update(products)
    for folder in ('src','lib/xrt','lib/xruntime','release/include'):
        inputs.update(p for p in (root/folder).rglob('*') if p.is_file())
    before={str(p):sha(p) for p in sorted(inputs)}
    artifacts=[]
    report=dict(Complete=False,Inputs=before,Rows=[],Products=artifacts,Scope=__doc__,NormalProductAcceptance=not source,
        NetworkTests=False,NetworkSecurityTests=False)
    env=dict(os.environ)
    if win:
        for product in products:shutil.copy2(product,out/product.name)
    def save():(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def record_product(path):artifacts.append(dict(Path=str(path),Sha256=sha(path)));save()
    if win:
        for library in products:record_product(out/library.name)
    def run(name,command):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['Rows'].append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,Sha256=sha(log)));save()
        assert result.returncode==0,(name,log.read_bytes()[-5000:])
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-Wno-unused-parameter',
        *['-I'+str(root/folder) for folder in ('.','release','release/include')]]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if win else ['-pthread','-ldl','-lm']
    try:
        for product in products:
            for opt in ('O0','O2'):
                name=product.stem+'.'+opt;binary=out/(name+('.exe' if win else ''));response=out/(name+'.rsp')
                links=[str(product),*systems]+([] if win else ['-Wl,-rpath,'+str(product.parent)])
                command=[*flags,'-'+opt,root/'test/cases/stdlib_channel_owned_profiles_native.c',*objects,*links,'-o',binary]
                response.write_text(''.join('"'+str(v).replace('\\','/').replace('"','\\"')+'"\n' for v in command),encoding='utf8')
                record_product(response);run(name+'.build',[cc,'@'+str(response)]);record_product(binary);run(name+'.run',[binary])
        assert products and len(report['Rows'])==len(products)*4,'Every selected library must execute both real optimization lanes'
        report['InputsUnchanged']=all(sha(p)==h for p,h in before.items())
        report['LogsUnchanged']=all(sha(out/(row['Name']+'.log'))==row['Sha256'] for row in report['Rows'])
        assert report['InputsUnchanged'] and report['LogsUnchanged'];report['Complete']=True
    except Exception as error:report['Error']=repr(error);raise
    finally:save()
    print('Owned Channel profile native gate passed:',len(report['Rows']),'phases',flush=True)
if __name__=='__main__':main()
