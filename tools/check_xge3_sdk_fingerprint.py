"""Check complete native SDK ABI inputs without building or loading an engine."""
from pathlib import Path
import argparse,ast,hashlib,json,shutil,subprocess,sys

def load(path):
    source=path.read_text(encoding='utf-8');tree=ast.parse(source)
    function=next(node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name=='public_abi_inputs')
    scope={};exec(compile(ast.Module(body=[function],type_ignores=[]),str(path),'exec'),scope)
    return scope['public_abi_inputs'],ast.dump(function,include_attributes=False)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','native','out'):parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args();a.root=a.root.resolve();a.native=a.native.resolve();out=a.out.resolve();out.mkdir(exist_ok=False)
    native=a.native/'tools/build/build.py';stage=a.root/'tools/stage_xge3_packages.py'
    builder,ba=load(native);stager,sa=load(stage);assert ba==sa
    sha=lambda data:hashlib.sha256(data).hexdigest()
    inputs={str(p):sha(p.read_bytes())for p in (native,stage,Path(__file__).resolve())}
    tree=out/'headers';(tree/'include').mkdir(parents=True);(tree/'extlibs/xrt').mkdir(parents=True)
    headers=sorted((a.native/'include').glob('*.h'))
    assert {'xge.h','xui.h','xnative_transfer.h','xui_document.h','xui_document_ui.h'}<={p.name for p in headers}
    for p in [*headers,a.native/'extlibs/xrt/xrt.h']:
        inputs[str(p)]=sha(p.read_bytes());dest=tree/p.relative_to(a.native);shutil.copy2(p,dest)
    rows=[]
    def digest():
        b=builder(tree);s=stager(tree);assert b==s;return sha(b.encode())
    original=digest();rows.append(dict(name='builder-and-stager-identical',passed=True))
    for relative in [p.relative_to(a.native)for p in headers]+[Path('extlibs/xrt/xrt.h')]:
        p=tree/relative;raw=p.read_bytes();p.write_bytes(raw+b'\n/* isolated ABI mutation */\n');assert digest()!=original;p.write_bytes(raw);assert digest()==original
        rows.append(dict(name='mutation:'+relative.as_posix(),passed=True))
    added=tree/'include/z_added_api.h';added.write_bytes(b'');assert digest()!=original;added.unlink();assert digest()==original
    rows.append(dict(name='new-empty-header-included',passed=True))
    p=tree/'include/xnative_transfer.h';raw=p.read_bytes();p.write_bytes(raw.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'));assert digest()==original;p.write_bytes(raw)
    rows.append(dict(name='line-endings-normalized',passed=True))
    sdk=out/'old-sdk';sdk.mkdir();(sdk/'build-report.json').write_text(json.dumps(dict(xrt_mode='host',linked=True)))
    command=[sys.executable,'-B',str(stage),'--root',str(a.root),'--engine-root',str(a.native),'--engine-build',str(sdk),'--out',str(out/'rejected-packages'),'--report',str(out/'rejected.json')]
    result=subprocess.run(command,capture_output=True);assert result.returncode and b'schema 2' in result.stderr+result.stdout
    assert not(out/'rejected-packages').exists() and not(out/'rejected.json').exists();rows.append(dict(name='old-sdk-rejected-before-output',passed=True))
    assert all(sha(Path(p).read_bytes())==expected for p,expected in inputs.items())
    report=dict(complete=True,passed=True,shippingDelivery=False,fullEngineRebuild=False,scope=__doc__,inputs=inputs,inputsUnchanged=True,headerCount=len(headers),rows=rows)
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(passed=True,rows=len(rows),headerCount=len(headers))))
if __name__=='__main__':main()
