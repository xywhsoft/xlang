"""Windows response helper protocol, not actual compiler/runtime acceptance.
Isolated paths with spaces; exact expanded response bytes; successful stderr,
nonzero native exit, missing executable and unresolved environment rejection.
No workspace release products or build directories are written.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert os.name=='nt' and not out.exists()
    cc,ps=[shutil.which(n) for n in ('gcc','powershell')];assert cc and ps
    source=root/'test/cases/stdlib_dll_response_compiler_probe.c'
    helper=root/'tools/build_release_dll.ps1'
    templates={name:root/'tools'/(name+'_dll.rsp.in') for name in ('runtime','compiler')}
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in (Path(__file__).resolve(),Path(cc),Path(ps),source,helper,*templates.values())]
    out.mkdir();project=out/'project space';tools=project/'tools';tools.mkdir(parents=True)
    build=out/'build space';build.mkdir();shutil.copy2(helper,tools/helper.name)
    probe=out/'compiler probe.exe';rows=[];proofs=[];failures=[]
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_')}
    env.update(XL6_BUILD_TMP=str(build),XL6_RELEASE_CC=str(probe),XL6_RELEASE_CFLAGS='-std=c11 -I"include space"',
        XL6_RELEASE_LDFLAGS='-shared -s',XL6_EMBED_TCC_SOURCE_ROOT=str(project/'tcc space'),
        XL6_TCC_LIB_FLAGS='-L"library space"',XL6_RELEASE_DIR=str(project/'release space'),
        XL6_DLL_LINKS='"'+str(build/'xrt object.o')+'" -lWs2_32',XL6_DLL_RESPONSE_PROBE_EXIT='0')
    def run(name,command,environment=env):
        r=subprocess.run(list(map(str,command)),cwd=out,env=environment,capture_output=True,timeout=60)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=r.returncode,Log=str(log),LogSha256=sha(log)))
        return r
    try:
        r=run('probe.build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2',source,'-o',probe]);assert r.returncode==0,r.stderr
        for target,path in templates.items():
            text=path.read_bytes().decode('utf-8-sig');shutil.copy2(path,tools/path.name)
            expected=re.sub(r'%([A-Za-z0-9_]+)%',lambda m:env[m[1]],text).replace('\\','/').encode('utf-8')
            command=[ps,'-NoProfile','-ExecutionPolicy','Bypass','-File',tools/helper.name,'-Target',target]
            for code in (0,29):
                r=run(target+'.exit.'+str(code),command,dict(env,XL6_DLL_RESPONSE_PROBE_EXIT=str(code)))
                assert r.returncode==code and b'response-probe stderr' in r.stderr,(target,code,r.returncode,r.stderr)
                assert re.findall(rb'response-probe bytes=(\d+)',r.stdout)==[str(len(expected)).encode()]
                assert (build/(target+'_dll.rsp')).read_bytes()==expected
                proofs.append(dict(Target=target,Case='native-exit-'+str(code),ExpectedExit=code,StderrPreserved=True,ExactBytes=True,SpacePaths=True))
            r=run(target+'.missing',command,dict(env,XL6_RELEASE_CC=str(out/'does not exist.exe')))
            assert r.returncode!=0 and r.stderr and b'response-probe bytes=' not in r.stdout,(target,'missing executable reported success')
            proofs.append(dict(Target=target,Case='missing-executable',Rejected=True))
            # Mechanical malformed-input fixture; the real template is intact.
            (tools/path.name).write_text(text+'\n%XL6_RESPONSE_UNRESOLVED_TEST%\n',encoding='utf-8')
            r=run(target+'.unresolved',command)
            assert r.returncode!=0 and b'Unresolved compiler environment variable' in r.stderr and b'response-probe bytes=' not in r.stdout
            proofs.append(dict(Target=target,Case='unresolved-environment',Rejected=True))
        assert len(proofs)==8
    except Exception as e:failures.append(repr(e))
    unchanged=all(sha(p['Path'])==p['Sha256'] for p in inputs)
    report=dict(Complete=unchanged and len(proofs)==8 and not failures,InputsUnchanged=unchanged,Inputs=inputs,Rows=rows,
        Proofs=proofs,Failures=failures,Scope=__doc__,ActualCompilerAcceptance=False,RuntimeAcceptance=False,Full53Acceptance=False)
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Proofs=len(proofs),Failures=failures)),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
