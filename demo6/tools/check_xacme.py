"""Build the real optional resident closure and exercise XACME offline.

--xrt points to the upstream checkout supplying its isolated mock CA/DNS
server. --driver adds language JIT/module/AOT acceptance using a separately
verified public source driver. No public CA or cloud credentials are used.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,sys

ROOT=Path(__file__).resolve().parents[1]

def native_sources():
    def expand(text):
        return re.sub(r'#include "([^"\n]+\.inc)"',
            lambda m:(ROOT/'src/compile'/m[1]).read_text(encoding='utf-8'),text)
    unit=(ROOT/'src/compile/xl6_compile_unit.c').read_text(encoding='utf-8')
    body=expand(unit.split('xl6ArtifactComponents[] = {',1)[1].split('\n};',1)[0])
    enum=(ROOT/'src/compile/xl6_artifact_runtime.h').read_text(encoding='utf-8')
    ids=re.findall(r'XL6_ARTIFACT_(\w+)',expand(enum.split('typedef enum XL6_ArtifactRuntimeId {')[1].split('} XL6_ArtifactRuntimeId;')[0]))
    ids=ids[:ids.index('RUNTIME_COUNT')]
    rows=re.findall(r'\{"([^"\n]+\.c)",(.*?)(?=\n    \{|\Z)',body,re.S)
    assert len(ids)==len(rows),'runtime registry differs from enum'
    mapping=dict(zip(ids,rows));selected=set()
    def require(name):
        if name in selected:return
        selected.add(name)
        for dep in re.findall(r'R\((\w+)\)',mapping[name][1]):require(dep)
    require('XACME')
    # The implementation TU includes the canonical foreign error scope once.
    return [mapping[n][0] for n in ids if n in selected and n not in ('XACME','FOREIGN_ERROR')]+['tools/xrt_single_header_impl.c']

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory',type=Path,required=True)
    parser.add_argument('--xrt',type=Path,required=True)
    parser.add_argument('--driver',type=Path)
    parser.add_argument('--cli',type=Path,help='Use a normal xl CLI for language acceptance instead of --driver')
    parser.add_argument('--reuse-native',type=Path,help='An already built native executable; receipt records reuse explicitly')
    parser.add_argument('--cc',default='gcc')
    parser.add_argument('--source-head',help='Explicit revision for WSL reading a Windows-managed Git worktree')
    parser.add_argument('--cases',nargs='+',choices=['basic','owned','retirement','flow'],default=['basic','owned','retirement','flow'])
    args=parser.parse_args();work=args.directory.resolve()
    work.mkdir(parents=True,exist_ok=False);records=[];inputs={}
    env=os.environ.copy();env['XLANG_PACKAGE_PATH']=str(ROOT/'release/extlibs')
    env['PATH']=str(ROOT/'release')+os.pathsep+env.get('PATH','')
    def run(name,command,timeout=120):
        result=subprocess.run([str(v) for v in command],cwd=ROOT,env=env,capture_output=True,
            text=True,encoding='utf-8',errors='replace',timeout=timeout)
        (work/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
        records.append({'name':name,'exitCode':result.returncode})
        if result.returncode:raise RuntimeError(f'{name}: exit {result.returncode}\n'+(result.stdout+result.stderr)[-5000:])
        print(name+': PASS',flush=True)
        return result.stdout
    def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
    run('inventory',[sys.executable,ROOT/'tools/check_xacme_api.py'])
    native=args.reuse_native.resolve() if args.reuse_native else work/('native.exe' if os.name=='nt' else 'native')
    if not args.reuse_native:
        sources=native_sources();objects=[]
        for index,source in enumerate(sources+['test/cases/xlir_xacme_native.c']):
            obj=work/(Path(source).stem+'.o');dep=obj.with_suffix('.d')
            run('compile-'+Path(source).stem,[args.cc,'-std=c11','-O0','-Wall','-Wextra','-Werror',
                '-Wno-unused-function','-Wno-unused-variable','-Wno-missing-field-initializers',
                '-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include','-MMD','-MF',dep,'-c',source,'-o',obj],180)
            dependencies=dep.read_text(encoding='utf-8').split(': ',1)[1].replace('\\\n',' ').split()
            for item in dependencies:
                path=(ROOT/item).resolve()
                if path.is_file() and path.is_relative_to(ROOT):inputs[str(path.relative_to(ROOT))]=digest(path)
            objects.append(obj)
        libraries=['-lWs2_32','-lAdvapi32','-lIphlpapi','-lBcrypt','-lCrypt32','-lUser32','-lShell32','-lOle32','-lOleAut32','-lm'] if os.name=='nt' else ['-lm','-pthread','-ldl']
        run('native-link',[args.cc,*objects,*libraries,'-o',native])
    run('native-contract', [native])
    upstream=args.xrt.resolve();serverlog=(work/'server.log').open('wb')
    server=subprocess.Popen([sys.executable,str(upstream/'tools/acme_mock_server.py'),'--workdir',str(work/'mock')],
        cwd=upstream,stdout=subprocess.PIPE,stderr=serverlog,text=True,encoding='utf-8')
    try:
        line=server.stdout.readline();info=json.loads(line)
        env.update(XACME_MOCK_DIRECTORY=info['directory'],XACME_MOCK_CA=info['ca_pem'],
            XACME_MOCK_CHALL=f"http://127.0.0.1:{info['chall']}",XACME_MOCK_RESOLVER=f"127.0.0.1:{info['dns']}",
            XACME_MOCK_ROOT=str(work/'mock/store'))
        run('native-mock',[native])
        # Independent consumer of the committed Store result, not the adapter.
        from cryptography import x509
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric import ec
        store=work/'mock/store/certs/example.test'
        version=(store/'current').read_text().strip();grant=store/version
        chain=(grant/'fullchain.pem').read_bytes();key=(grant/'key.pem').read_bytes()
        cert=x509.load_pem_x509_certificate(chain);private=serialization.load_pem_private_key(key,None)
        form=(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo)
        assert cert.public_key().public_bytes(*form)==private.public_key().public_bytes(*form)
        records.append({'name':'independent-grant-pair','exitCode':0})
        if args.driver or args.cli:
            driver=(args.driver or args.cli).resolve()
            for case in args.cases:
                source=ROOT/f'release/extlibs/xacme/test/{case}.xl'
                sidecars=[ROOT/'test/cases/xlir_xacme_owned_probe.c'] if case=='owned' else [ROOT/'test/cases/xlir_xacme_mock_dns.c'] if case=='flow' else []
                mode='module' if case=='retirement' else 'run'
                env['XACME_LANGUAGE_ROOT']=str(work/'mock/language-jit')
                key_path=work/'mock/rollover-jit.pem'
                key_path.write_bytes(ec.generate_private_key(ec.SECP256R1()).private_bytes(
                    serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
                env['XACME_ROLLOVER_KEY']=str(key_path)
                run(case+'-jit',[driver,'run',source,*sidecars] if args.cli else [driver,source,mode,work/(case+'-jit'),*sidecars])
                prefix=work/(case+'-aot')
                run(case+'-aot-build',[driver,'build',source,*sidecars,'-o',prefix] if args.cli else [driver,source,'exe',prefix,*sidecars],240)
                env['XACME_LANGUAGE_ROOT']=str(work/'mock/language-aot')
                key_path=work/'mock/rollover-aot.pem'
                key_path.write_bytes(ec.generate_private_key(ec.SECP256R1()).private_bytes(
                    serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
                env['XACME_ROLLOVER_KEY']=str(key_path)
                run(case+'-aot',[prefix.with_suffix('.exe' if os.name=='nt' else '')])
    finally:
        server.terminate();server.wait(timeout=15);serverlog.close()
        if server.stdout:server.stdout.close()
    assert all(digest(ROOT/path)==value for path,value in inputs.items()),'Native build inputs changed'
    source_head=args.source_head or subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    receipt={'complete':True,'platform':sys.platform,'sourceHead':source_head,
        'nativeExecutable':str(native),'nativeSha256':digest(native),'nativeReused':bool(args.reuse_native),
        'driver':str(args.driver or args.cli) if args.driver or args.cli else None,'inputs':inputs,'checks':records}
    (work/'result.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
    print('XACME checks complete',flush=True)

if __name__=='__main__':main()
