"""Offline exact nullable-union member flow and ordinary representation regression.
Uses a frozen complete-source compiler; not fresh shipping or whole P1 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[1];manifest=a.source_build.resolve();out=a.out.resolve()
    assert not out.exists();build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    for item in build['Inputs']+build['Headers']+build['Objects']:assert sha(item['Path'])==item['Sha256'].lower(),item['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    native=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    source=root/'test/cases/xlir_nullable_union_member.xl';contract=root/'test/cases/xlir_nullable_union_member_contract.c'
    regressions=('cgen_union_value_semantics','xlir_union_container_domain','xlir_union_dynamic_box_abi',
        'stdlib_channel_union','stdlib_channel_codec_matrix','cgen_union','xlir_union_class_field_write_abi')
    host=root/'test/support/time_expect.c'
    files={Path(__file__).resolve(),manifest,driver,cc,native,source,contract,host,
        *[root/('test/cases/'+name+'.xl') for name in regressions]}
    files.update(Path(item['Path']) for item in build['Inputs'])
    inputs={str(f):sha(f) for f in sorted(files)};out.mkdir();rows=[];products=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,
        Products=products,Failures=failures,Scope=__doc__,SourceBuild=str(manifest),
        ShippingAcceptance=False,FullP1Acceptance=False,NetworkTests=False,NetworkSecurityTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    env=dict(os.environ)
    if os.name!='nt':env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def run(name,command):
        command=list(map(str,command));actual=command;log=out/(name+'.log');start=time.monotonic()
        if command[0]==str(cc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+v.replace('\\','/').replace('"','\\"')+'"' for v in command[1:])+'\n',encoding='utf8')
            product(rsp);actual=[str(cc),'@'+str(rsp)]
        with log.open('wb') as stream:result=subprocess.run(actual,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name+': '+str(result.returncode),flush=True);assert result.returncode==0,(name,str(log))
        return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable',
        '-Wno-unused-label','-Wno-missing-field-initializers']
    ext='.exe' if os.name=='nt' else '.out';save()
    try:
        generated=out/'flow.c';lowered=out/'flow.json'
        run('flow.emit',[driver,source,'cgen-default',generated]);product(generated)
        run('flow.ir',[driver,source,'ir-lowered',lowered]);product(lowered)
        for opt in ('O0','O2'):
            obj=out/('flow.'+opt+'.o');run('flow.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
        for mode in ('run','module'):run('flow.'+mode,[driver,source,mode,out/('flow.'+mode)])
        target=out/'flow.aot';binary=Path(str(target)+ext);argument=target if os.name=='nt' else binary
        run('flow.aot.build',[driver,source,'exe',argument]);product(binary);run('flow.aot.run',[binary])
        for name in regressions:
            fixture=root/('test/cases/'+name+'.xl')
            sidecars=[host] if name in ('cgen_union','xlir_union_dynamic_box_abi','xlir_union_class_field_write_abi') else []
            for mode in ('run','module'):run(name+'.'+mode,[driver,fixture,mode,out/(name+'.'+mode),*sidecars])
        objects=[item['Path'] for item in build['Objects'] if Path(item['Source']).name not in
            ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        assert len(objects)==len(build['Objects'])-1
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else \
            ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(native.parent)]
        if os.name=='nt':shutil.copy2(native,out/native.name);product(out/native.name)
        for opt in ('O0','O2'):
            binary=out/('contract-'+opt+ext)
            run('contract.'+opt+'.build',[cc,*flags,*compat,'-'+opt,contract,*objects,native,*systems,'-o',binary]);product(binary)
            text=run('contract.'+opt+'.run',[binary,source])
            match=re.search(r'narrowed=(\d+) reads=(\d+) writes=(\d+) wraps=(\d+) mutations=(\d+) zero-output=(\d+)',text)
            assert match;proof=list(map(int,match.groups()));assert proof[0]>=8 and proof[4]>0 and proof[5]==proof[4]*2
            if 'IRProof' in report:assert report['IRProof']==proof
            report['IRProof']=proof
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        assert len(rows)==26 and report['InputsUnchanged'] and report['LogsUnchanged']
        report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
