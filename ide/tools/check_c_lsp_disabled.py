"""Verify the default root release keeps C editing/build/run without a C LSP."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

IDE=Path(__file__).resolve().parents[1]
def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,default=IDE/'build/Release/gui-build-inputs.json')
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();m=json.loads(a.build.read_text('utf-8-sig'))
    assert m['cLspEnabled'] is False
    runtime=Path(m['release']).resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    record={'status':'running','build':str(a.build.resolve()),'runtime':str(runtime),'tests':[]}
    binaries=['xlang_ide.exe','xlang_ide_console.exe','xlang-ls.exe','xge.dll','xl.exe','xl.dll','xlc.dll']
    protected={str(runtime/name):sha(runtime/name) for name in binaries}
    def save():
        (out/'result.json').write_text(json.dumps(record,indent=2)+'\n','utf8')
    save();fixture=runtime/'xide_c_lsp_disabled_test.exe';assert not fixture.exists()
    try:
        for path,h in m['inputs'].items():assert sha(path)==h,path
        forbidden=('xideCCompletionView','xideCLanguageView','xideCSession','xideCCompletionJob','xideCInputTrace','xideCBridgePrepare','xideCCommandsCreate')
        symbols=subprocess.check_output(['nm','-g','--defined-only',str(runtime/'xlang_ide.exe')],text=True,encoding='utf8')
        assert not any(name in symbols for name in forbidden)
        help_text=subprocess.check_output([str(runtime/'xlang_ide_console.exe'),'--help'],text=True,encoding='utf8')
        assert '--c-server' not in help_text and 'clangd' not in help_text
        assert not (runtime/'toolchains/clangd').exists()
        command=[m['compiler'],*m['flags'],str(IDE/'tests/c_lsp_disabled_test.c'),*m['objects'],*m['libraries'],'-o',str(fixture)]
        r=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (out/'compile.log').write_bytes(r.stdout);assert r.returncode==0,r.stdout.decode('utf8',errors='replace')
        with (out/'fixture.log').open('wb') as log:
            r=subprocess.run([str(fixture),str(out/'fixture'),str(runtime/'xl.exe')],cwd=runtime,stdout=log,stderr=subprocess.STDOUT,timeout=70)
        record['tests'].append({'name':'real-GUI-C-editing-and-build-without-LSP','exit':r.returncode})
        assert r.returncode==0,(out/'fixture.log').read_text('utf8',errors='replace')
        for path,h in protected.items():assert sha(path)==h,path
        record.update(status='passed',symbolsAbsent=list(forbidden),helpHasNoCServer=True,clangdAbsent=True,runtimeHashes=protected)
    except BaseException as error:
        record.update(status='failed',error=repr(error));raise
    finally:
        if fixture.exists():
            assert fixture.parent==runtime and not (out/'fixture.exe').exists()
            fixture.replace(out/'fixture.exe')
        save()
    print(json.dumps({'status':record['status'],'runtime':str(runtime)}))

if __name__=='__main__':
    main()
