"""Current-source cryptoAdvanced facade/IR/JIT/module/AOT and pruning.
Fixed ordinary vectors only. No network/security campaigns or shipping claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from xlir_public_build_inputs import verify_public_build

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['root','build','out']:parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root=args.root.resolve();out=args.out.resolve()
    assert not out.exists();out.mkdir(parents=True)
    build,pinned,_=verify_public_build(root,args.build.resolve().parent)
    controls=Path(__file__).resolve().parents[1];cases=controls/'test/cases'
    positives=[cases/(name+'.xl') for name in ['stdlib_crypto_advanced_blocks','stdlib_crypto_aes_block_only','stdlib_crypto_poly1305_only']]
    negatives=[cases/(name+'.xl') for name in ['bad_stdlib_crypto_advanced_default','bad_stdlib_crypto_poly1305_reset']]
    observers=[cases/(name+'.c') for name in ['xlir_codec_template_contract','stdlib53_pruning_plan']]
    driver=Path(build['Driver']);gcc=Path(shutil.which('gcc')).resolve()
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update([Path(__file__).resolve(),gcc,runtime,*positives,*negatives,*observers])
    report=dict(Complete=False,Inputs={str(p):sha(p) for p in sorted(pinned)},Rows=[],Failures=[],
        NetworkTraffic=False,SecurityTests=False,ShippingAccepted=False,FullGoalComplete=False)
    env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,failure=False):
        command=list(map(str,command));executed=command
        if Path(command[0])==gcc:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n',encoding='utf8')
            executed=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as stream:code=subprocess.run(executed,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=900).returncode
        report['Rows'].append(dict(Name=name,Command=command,Exit=code,ExpectedFailure=failure,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start))
        save();print(name,code,flush=True);assert (code!=0 if failure else code==0),str(log)
        return log.read_text(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-Wno-missing-field-initializers',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ['xlir_public_source_driver.c','xlir_public_source_linux_driver.c']]
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
    binaries={}
    for source in observers:
        binary=out/(source.stem+('.exe' if os.name=='nt' else ''));binaries[source.stem]=binary
        run(source.stem+'.build',[gcc,*flags,'-O0',source,*objects,runtime,*libs,'-o',binary])
    strict=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-but-set-variable','-Wno-unused-const-variable']
    for case in positives:
        try:
            name=case.stem;prefix=out/name;generated=out/(name+'.c')
            run(name+'.emit',[driver,case,'cgen-default',generated]);text=generated.read_text(encoding='utf8')
            assert 'xl6Crypto' in text
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Crypto\w*\([^;]*\)\s*\{',text,re.M)
            assert not re.search(r'__xl_std_cryptoAdvanced_\w+\([^;]*\)\s*\{',text)
            for stage in ['typed','lowered']:run(name+'.'+stage,[driver,case,'ir-'+stage,out/(name+'.'+stage+'.json')])
            for opt in ['O0','O2']:run(name+'.strict.'+opt,[gcc,*flags,*strict,'-'+opt,'-c',generated,'-o',out/(name+'.'+opt+'.o')])
            for mode in ['run','module']:run(name+'.'+mode,[driver,case,mode,out/(name+'.'+mode)])
            binary=out/(name+('.exe' if os.name=='nt' else ''));target=binary.with_suffix('') if os.name=='nt' else binary
            run(name+'.aot.build',[driver,case,'exe',target]);run(name+'.aot.run',[binary])
            imports=run(name+'.aot.imports',['objdump','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
            assert not any(token in imports.lower() for token in ['xl.dll','xlc.dll','libxl.so','libxlc.so'])
            run(name+'.ir.contract',[binaries['xlir_codec_template_contract'],case])
            plan=json.loads(run(name+'.plan',[binaries['stdlib53_pruning_plan'],case]))
            sources={Path(p).name for p in plan['Sources']};modules=set(plan['NativeModules'])
            if name.endswith('_only'):
                expected={'xl6_crypto_aes_block.c','xl6_crypto_aes_block_domain.c'} if 'aes_block' in name else {'xl6_crypto_poly1305.c'}
                assert {p for p in sources if p.startswith('xl6_crypto_')}==expected
                assert not any(p.startswith(('xl6_tls_','xl6_x509_','xl6_der_','xl6_pem_')) for p in sources)
                assert 'XRT_MODULE_BUFFER' not in modules
            implementation=(Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text(encoding='utf8')
            assert '_MODULE_ALL' not in implementation
        except Exception as error:report['Failures'].append(dict(Name=case.stem,Error=repr(error)));save()
    for case in negatives:
        try:
            generated=out/(case.stem+'.c')
            log=run(case.stem,[driver,case,'cgen-default',generated],failure=True)
            prefix='public source compile failed:' if os.name=='nt' else 'public Linux source cgen-default failed:'
            diagnostic=('[stdlib] standard library namespace has no such symbol' if case.stem.endswith('_default')
                else "[sem] no matching callable class member 'Poly1305.reset'")
            assert prefix in log and diagnostic in log and 'unresolved reference' not in log
            assert not generated.exists()
        except Exception as error:report['Failures'].append(dict(Name=case.stem,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(p)==h for p,h in report['Inputs'].items())
    report['Complete']=report['InputsUnchanged'] and not report['Failures']
    report['Products']={str(p):sha(p) for p in out.rglob('*') if p.is_file() and p.name!='results.json'}
    save();assert report['Complete'],report['Failures']

if __name__=='__main__':main()
