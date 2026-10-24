from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--build','--source-build',dest='build',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--cases-root',type=Path)
a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();build=json.loads(a.build.read_text(encoding='utf-8-sig'))
assert build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly']
for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();r=dict(Complete=False,Rows=[],Failures=[],Plans=[],Inputs={str(Path(__file__)):sha(__file__)},SourceBuild=str(a.build),
    NetworkTraffic=False,SecurityTests=False,ShippingAccepted=False,FullTlsAccepted=False)
env=dict(os.environ);runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
def run(name,args,reject=False):
    args=list(map(str,args));command=args
    if args[0]==str(gcc):
        rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
    log=out/(name+'.log');start=time.monotonic()
    with log.open('wb') as f:code=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
    r['Rows'].append(dict(Name=name,Command=args,Exit=code,ExpectedReject=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
    assert (code!=0 if reject else code==0),str(log);return log.read_text(errors='replace')
flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
if os.name!='nt':flags+=['-D_GNU_SOURCE']
strict=['-Wall','-Wextra','-Werror','-fmax-errors=4'];compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
cases=[(a.cases_root or root)/'test/cases'/n for n in ['stdlib_tls_parameters.xl','stdlib_tls_parameters_owned.xl','stdlib_tls_policy_enum_only.xl','stdlib_tls_parameters_array_regression.xl']]
for source in cases:r['Inputs'][str(source)]=sha(source)
for source in cases:
    name=source.stem
    try:
        generated=out/(name+'.c');run(name+'.emit',[driver,source,'cgen-default',generated]);text=generated.read_text()
        assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls\w+\([^;]*\)\s*\{',text,re.M)
        run(name+'.jit',[driver,source,'run',out/(name+'.jit')]);run(name+'.module',[driver,source,'module',out/(name+'.module')])
        for stage in ['typed','lowered']:run(name+'.'+stage,[driver,source,'ir-'+stage,out/(name+'.'+stage+'.json')])
        for opt in ['O0','O2']:run(name+'.'+opt,[gcc,*flags,*strict,*compat,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
        binary=out/(name+'.aot'+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
        run(name+'.aot.build',[driver,source,'exe',target]);run(name+'.aot.run',[binary])
        imports=run(name+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
        assert not any(v in imports.lower() for v in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
        assert '_MODULE_ALL' not in (Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text()
    except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']]
assert len(objects)==len(build['Objects'])-1
libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
for name,source in [('contracts','test/cases/xlir_tls_parameters_contract.c'),('owned','test/support/tls_parameters_owned.c')]:
    for opt in ['O0','O2']:
        try:
            obj=out/(name+'.'+opt+'.o');binary=out/(name+'.'+opt+('.exe' if os.name=='nt' else ''))
            run(name+'.'+opt+'.compile',[gcc,*flags,*strict,*compat,'-'+opt,'-c',root/source,'-o',obj]);run(name+'.'+opt+'.link',[gcc,*objects,obj,*libs,'-o',binary])
            text=run(name+'.'+opt+'.run',[binary,*([cases[0]] if name=='contracts' else [])]);assert ('TLS parameters XLIR:' if name=='contracts' else 'TLS parameters owned:') in text
        except Exception as e:r['Failures'].append(dict(Name=name+'.'+opt,Error=repr(e)));save()
try:
    obj=out/'plan.o';binary=out/('plan'+('.exe' if os.name=='nt' else ''));run('plan.compile',[gcc,*flags,*strict,*compat,'-O0','-c',root/'test/support/container_projection_artifact.c','-o',obj]);run('plan.link',[gcc,*objects,obj,*libs,'-o',binary])
    for source in cases:
        prefix=out/(source.stem+'.selected');run(source.stem+'.plan',[binary,source,prefix]);plan=json.loads(Path(str(prefix)+'.plan.json').read_text())
        assert not any('XRT_MODULE_CRYPTO_' in m or 'TLS_CLIENT' in m or 'TLS_SERVER' in m or 'TLS_VERIFY' in m for m in plan['NativeModules'])
        if 'enum_only' in source.stem:assert not any('xl6_tls_' in s for s in plan['Sources'])
        r['Plans'].append(dict(Source=str(source),Plan=str(prefix)+'.plan.json',NativeModules=plan['NativeModules'],Sources=plan['Sources']))
except Exception as e:r['Failures'].append(dict(Name='plans',Error=repr(e)));save()
base='''array<TlsVersion> versions=[TlsVersion.v13]
array<TlsCipherSuite> ciphers=[TlsCipherSuite.aes128GcmSha256]
array<TlsGroup> groups=[TlsGroup.x25519]
array<TlsSignature> signatures=[TlsSignature.ed25519]
'''
for i in range(4):
    args=['versions','ciphers','groups','signatures'];args[i]=args[(i+1)%4]
    source=out/('wrong-enum-'+str(i)+'.xl');source.write_text(base+'auto bad=TlsPolicy.fromPreferences('+','.join(args)+')\n')
    try:
        target=out/('wrong-enum-'+str(i)+'.c');message=run('wrong-enum-'+str(i),[driver,source,'cgen-default',target],True)
        assert not target.exists() and any(v in message for v in ['[sem]','[ir]','[checked]','[stdlib]']) and '[parse]' not in message
    except Exception as e:r['Failures'].append(dict(Name='wrong-enum-'+str(i),Error=repr(e)));save()
r['Stages']=len(r['Rows']);r['InputsUnchanged']=all(sha(p)==d for p,d in r['Inputs'].items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers']) and sha(driver)==build['DriverSha256'].lower()
r['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in out.rglob('*') if p.is_file() and p.name!='results.json']
r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']
