"""Compile the real private policy in static mode; reject old compiler schema."""
from pathlib import Path
import argparse,hashlib,json,re,shutil,subprocess

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','packages','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,packages,out=[v.resolve()for v in(a.root,a.packages,a.out)]
    assert not out.exists();out.mkdir(parents=True);private=packages/'_xge_native';manifest=json.loads((private/'xlang.package.json').read_text())
    assert manifest['loadlibs']==[]and manifest['native']['linkage']=='static'
    assert manifest['artifact']['nativeLink']==dict(gcSections=True,functionSections=True,dataSections=True)
    assert 'xl6HostXrtResolveV1'not in manifest['artifact']['runtimeSymbols']
    flags=['-O2','-ffunction-sections','-fdata-sections','-I'+str(private/'include'),'-I'+str(root/('release/win_x64/include'if manifest['native']['platform']=='win_x64'else'release/linux_x64/include')),'-I'+str(root/'src/runtime')]
    probe=out/'header.c';probe.write_text('#include <xge3_runtime.h>\n#include <xge_static_xrt_contract.h>\nint main(void){return 0;}\n')
    rows=[]
    def run(name,command,reject=False):
        result=subprocess.run([str(v)for v in command],capture_output=True);log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        rows.append(dict(name=name,command=[str(v)for v in command],exitCode=result.returncode,passed=(result.returncode!=0 if reject else result.returncode==0),logSha256=sha(log)))
        if not rows[-1]['passed']:raise RuntimeError(str(log))
        return result.stdout.decode(errors='replace')
    run('old-schema-rejected',['gcc','-std=c11',*flags,'-fsyntax-only',probe],True)
    for standard in ('c11','c++11','c++17'):
        run('header-'+standard,['g++'if'++'in standard else'gcc','-x','c++'if'++'in standard else'c','-std='+standard,'-Wall','-Wextra','-Werror','-DXL6_NATIVE_LINK_SCHEMA=1',*flags,'-fsyntax-only',probe])
    obj=out/'runtime.o';run('real-policy',['gcc','-std=c11','-DXL6_NATIVE_LINK_SCHEMA=1',*flags,'-c',private/'src/runtime.c','-o',obj])
    undefined=run('policy-undefined',['nm','-u',obj]);assert not re.search(r'(?:xl6HostXrtResolve|xgeHostXrtBind|XgeHostXrtSlot_)',undefined)
    names=set(re.findall(r'\b(xl6\w+)\s*$',undefined,re.M));assert names==set(manifest['artifact']['runtimeSymbols'])
    (out/'results.json').write_text(json.dumps(dict(passed=True,scope=__doc__,results=rows,inputs={str(v):sha(v)for v in [Path(__file__),private/'xlang.package.json',private/'src/runtime.c',private/'include/xge3_runtime.h']},residentSymbols=sorted(names),notes=['No linked XGE DLL or generated host-XRT slot is used by this static policy object.','This compile/header audit is separate from actual compiler/TCC link qualification.']),indent=2)+'\n');print(out/'results.json')
if __name__=='__main__':main()
