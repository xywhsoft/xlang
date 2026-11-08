"""Check current derived cache safety with real private C producer omissions."""
from pathlib import Path
import argparse,json,subprocess
from build_lsp_lifecycle_profile import verify_build,sha
from check_native_bridge_catalog import function


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    receipt=json.loads((build/'build-result.json').read_text(encoding='utf-8'));verify_build(receipt,build)
    ide=Path(__file__).resolve().parents[1];fixture=ide/'tests/analysis_ir_derived_test.c';compile=next(c for c in receipt['commands'] if '-c' in c and str(fixture) in c);obj=compile[compile.index('-o')+1];link=next(c for c in receipt['commands'] if '-c' not in c and obj in c)
    protected={**receipt['inputs'],str(build/'build-result.json'):sha(build/'build-result.json'),str(Path(__file__).resolve()):sha(__file__)}
    report=dict(status='running',scope=__doc__,cases=[],commands=[],protected=protected)
    def save():(out/'result.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
    def run(name,cmd,reject=False):
        p=subprocess.run(cmd,cwd=out,capture_output=True,timeout=180);log=out/(name+'.log');log.write_bytes(p.stdout+p.stderr);report['commands'].append(dict(name=name,command=cmd,exit=p.returncode,logSha256=sha(log)));save()
        assert p.returncode==(1 if reject else 0),(name,p.returncode,p.stderr[-1800:])
        if reject:assert b'analysis_ir_derived_test:' in p.stderr,(name,p.stderr[-1000:])
    try:
        run('control',[str(build/'analysis_ir_derived_test.exe')]);report['cases'].append('normal-complete-native-control');save()
        inc=ide/'src/analysis/frontend_ir_derived.inc';text=inc.read_text(encoding='utf-8');ir=(build/'frontend/xl6_ir.c').read_text(encoding='utf-8')
        raw=function(text,'xlsIrDerivedRawClean')
        variants=[('omit-sparse-table-gate','analysis/frontend_ir_derived.inc',text,'if(!xlsFinalIrTablesEmpty(module))return NULL;','/* deliberate omission */'),('omit-complete-type-context','analysis/frontend_ir_derived.inc',text,'if(!xlsIrMemoTypes(&module->types,&context->key,bytes) ||','if(0 ||'),('ignore-raw-cleanliness','analysis/frontend_ir_derived.inc',text,raw,raw[:raw.index('{')]+'{return 1;}'),('omit-named-type-input','xl6_ir.c',ir,'if(xlsDerivedInputChanged)xlsIrDerivedInputPatch(module,i);','(void)xlsDerivedInputChanged;')]
        for name,rel,source,before,after in variants:
            assert source.count(before)==1,(name,'producer seam changed');folder=out/name;include=folder/'include';path=include/rel;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(source.replace(before,after,1).encode());protected[str(path)]=sha(path)
            target=folder/'fixture.o';program=folder/'fixture.exe';cmd=compile.copy();cmd.insert(1,'-I'+str(include));cmd[cmd.index('-o')+1]=str(target);run(name+'-compile',cmd);cmd=[str(target) if v==obj else v for v in link];cmd[cmd.index('-o')+1]=str(program);run(name+'-link',cmd);run(name+'-run',[str(program)],reject=True);report['cases'].append(name+'-rejected');save()
        verify_build(receipt,build)
        for path,digest in protected.items():assert sha(path)==digest,path
        report['status']='passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=report['status'],cases=len(report['cases']),commands=len(report['commands']))))


if __name__=='__main__':main()
