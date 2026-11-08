"""Reject real C omissions in native selection, context identity and missing-file proofs."""
from pathlib import Path
import argparse,json,subprocess,sys
from build_lsp_lifecycle_profile import verify_build,sha


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    build=a.build.resolve();server=a.server.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);r=json.loads((build/'build-result.json').read_text(encoding='utf-8'));verify_build(r,build)
    assert sha(server)==r['outputs']['xlang-ls.exe'];ide=Path(__file__).resolve().parents[1];tool=ide/'tools/check_lsp_native_selection.py'
    protected={**r['inputs'],str(tool):sha(tool),str(Path(__file__).resolve()):sha(__file__)};report=dict(status='running',scope=__doc__,cases=[],commands=[],protected=protected)
    def save():(out/'result.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
    def run(name,cmd,rejected=False,needle=None):
        value=subprocess.run(cmd,cwd=out,capture_output=True,timeout=180);log=out/(name+'.log');log.write_bytes(value.stdout+value.stderr);report['commands'].append(dict(name=name,command=cmd,exit=value.returncode,logSha256=sha(log)));save()
        assert value.returncode==(1 if rejected else 0),(name,value.returncode,value.stderr[-1600:])
        if needle:assert needle in value.stderr,(name,value.stderr[-1600:])
    host=next(c for c in r['commands'] if '-c' not in c and c[-1]==str(build/'xlang-ls.exe'))
    try:
        run('normal-protocol',[sys.executable,'-X','utf8','-B',str(tool),'--server',str(server),'--output',str(out/'normal-protocol')]);report['cases'].append('normal-protocol-control')
        # The actual single-allocation fixture includes the core implementation.
        # Select a private include copy without changing its original assertions.
        fixture=ide/'tests/analysis_context_test.c';cmd=next(c for c in r['commands'] if '-c' in c and str(fixture) in c);old=cmd[cmd.index('-o')+1];link=next(c for c in r['commands'] if '-c' not in c and old in c)
        folder=out/'omit-context-identity';private=folder/'include/analysis/context.c';private.parent.mkdir(parents=True);source=(ide/'src/analysis/context.c').read_text(encoding='utf-8');before='header[2]=(uint64_t)spec->nativeSelection;';assert source.count(before)==1;private.write_bytes(source.replace(before,'header[2]=0;',1).encode());protected[str(private)]=sha(private)
        obj=folder/'fixture.o';exe=folder/'fixture.exe';compile=cmd.copy();compile.insert(1,'-I'+str(folder/'include'));compile[compile.index('-o')+1]=str(obj);run('identity-compile',compile);cmd=[str(obj) if v==old else v for v in link];cmd[cmd.index('-o')+1]=str(exe);run('identity-link',cmd);run('identity-rejected',[str(exe)],True,b'analysis_context_test:');report['cases'].append('omitted-native-mode-from-canonical-identity-rejected')
        for name,unit,before,after in [('omit-import-selection','xl6_imports.c','    deps->nativeSelection = xlsFrontendContextNativeSelection();','    /* deliberate omission */'),('omit-missing-proof','xl6_package.c','        (report && !xl6SourceReportExists(report, path)) ||\n','')]:
            folder=out/name;folder.mkdir();row=next(x for x in r['sourceOverlays'] if Path(x['source']).name==unit);normal=Path(row['compiledSource']);copy=folder/normal.name;copy.write_bytes(normal.read_bytes());original=normal
            if name=='omit-missing-proof':
                original=normal.with_name('xl6_package_native_variants.inc');changed=folder/original.name
            else:changed=copy
            source=original.read_text(encoding='utf-8');assert source.count(before)==1,name;changed.write_bytes(source.replace(before,after,1).encode());protected[str(copy)]=sha(copy);protected[str(changed)]=sha(changed)
            cmd=next(c for c in r['commands'] if '-c' in c and str(normal) in c).copy();cmd[cmd.index('-c')+1]=str(copy);obj=folder/'producer.o';cmd[cmd.index('-o')+1]=str(obj);run(name+'-compile',cmd)
            exe=server.with_name('.lsp-selection-'+name+'.exe');assert not exe.exists()
            try:
                cmd=host.copy();cmd.insert(1,str(obj));cmd[cmd.index('-o')+1]=str(exe);run(name+'-link',cmd)
                run(name+'-rejected',[sys.executable,'-X','utf8','-B',str(tool),'--server',str(exe),'--output',str(folder/'protocol')],True,b'AssertionError');report['cases'].append(name+'-rejected')
            finally:exe.unlink(missing_ok=True)
        verify_build(r,build)
        for path,digest in protected.items():assert sha(path)==digest,path
        report['status']='passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=report['status'],cases=len(report['cases']),commands=len(report['commands']))))


if __name__=='__main__':main()
