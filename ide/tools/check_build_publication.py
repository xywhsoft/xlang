"""Exercise build publication gates without running or replacing user programs.

Uses the real PowerShell build flow and matched SDK verifier in private fault
runtimes. A bounded synthetic compiler/producer makes non-executable sentinel
artifacts: this verifies ordering and rejection, not C/GUI/LSP correctness.
The previous committed build script must fail the same preservation contract.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

IDE=Path(__file__).resolve().parents[1]


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


FAKE_COMPILER=r'''
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
static int owned(const char* path){
 char full[4096],root[4096];const char* sandbox=getenv("XIDE_TEST_SANDBOX");
 if(!path || !sandbox)return 0;
 DWORD a=GetFullPathNameA(path,sizeof(full),full,NULL),b=GetFullPathNameA(sandbox,sizeof(root),root,NULL);
 if(!a || a>=sizeof(full) || !b || b>=sizeof(root))return 0;
 size_t n=strlen(root);return !_strnicmp(full,root,n) && (full[n]=='\\' || full[n]=='/');
}
static int write_file(const char* path,const char* data){
 if(!owned(path))return 0;
 FILE* f=fopen(path,"wb");if(!f)return 0;
 int okay=fputs(data,f)>=0;return fclose(f)==0 && okay;
}
int main(int argc,char** argv){
 int macros=0,preprocess=0;
 for(int i=1;i<argc;++i){if(!strcmp(argv[i],"-dM"))macros=1;if(!strcmp(argv[i],"-E"))preprocess=1;}
 if(macros && preprocess){
  const char* real=getenv("XIDE_TEST_REAL_COMPILER");if(!real)return 95;
  const char** forwarded=calloc((size_t)argc+1,sizeof(*forwarded));if(!forwarded)return 96;
  forwarded[0]=real;for(int i=1;i<argc;++i)forwarded[i]=argv[i];
  int code=(int)_spawnv(_P_WAIT,real,forwarded);free(forwarded);return code;
 }
 const char* output=NULL;const char* log=getenv("XIDE_TEST_COMPILER_LOG");
 if(!owned(log))return 90;
 FILE* f=fopen(log,"ab");if(!f)return 91;
 for(int i=0;i<argc;++i)fprintf(f,"%s%s",i?" ":"",argv[i]);
 fputc('\n',f);if(fclose(f))return 92;
 for(int i=1;i+1<argc;++i)if(!strcmp(argv[i],"-o"))output=argv[i+1];
 if(!output || !write_file(output,"SYNTHETIC-NONEXECUTABLE-BUILD-ARTIFACT\n"))return 93;
 if(strstr(output,"xlang_ide.link.exe") && !write_file(getenv("XIDE_TEST_LINK_MARKER"),"linked\n"))return 94;
 return 0;
}
'''


WRAPPER=r'''
$ErrorActionPreference='Stop'
$global:XideTestChecks=0
$global:XideTestNative=''
function global:python {
    $xideTestArguments=@($args | ForEach-Object {[string]$_})
    $xideTestTool=$xideTestArguments | Where-Object {$_.EndsWith('.py')} | Select-Object -First 1
    [pscustomobject]@{tool=$xideTestTool;arguments=$xideTestArguments} | ConvertTo-Json -Compress | Add-Content -LiteralPath $env:XIDE_TEST_PYTHON_LOG -Encoding utf8
    $global:LASTEXITCODE=0
    if($xideTestTool.EndsWith('check_catalog_coverage.py')) {
        $global:XideTestChecks++
        if($env:XIDE_TEST_MODE -eq 'early' -or
           ($env:XIDE_TEST_MODE -eq 'late' -and (Test-Path -LiteralPath $env:XIDE_TEST_LINK_MARKER)) -or
           ($env:XIDE_TEST_MODE -eq 'final' -and $global:XideTestChecks -ge 3)) {
            Write-Output 'SYNTHETIC catalog rejection'
            $global:LASTEXITCODE=73
        }
    } elseif($xideTestTool.EndsWith('build_lsp_core.py')) {
        $xideTestAt=[Array]::IndexOf($xideTestArguments,'--output')
        if($xideTestAt -lt 0){throw 'Missing synthetic native output'}
        $xideTestDirectory=$xideTestArguments[$xideTestAt+1]
        if(-not [IO.Path]::GetFullPath($xideTestDirectory).StartsWith([IO.Path]::GetFullPath($env:XIDE_TEST_SANDBOX)+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Synthetic native path escapes sandbox'}
        [void][IO.Directory]::CreateDirectory($xideTestDirectory)
        $global:XideTestNative=Join-Path $xideTestDirectory 'xlang-ls.exe'
        [IO.File]::WriteAllText($global:XideTestNative,"SYNTHETIC-NATIVE-SERVER`n")
        [IO.File]::WriteAllText((Join-Path $xideTestDirectory 'build-result.json'),'{"status":"synthetic-passed","notAnActualNativeBuild":true}')
    } elseif($xideTestTool.EndsWith('font_resources.py')) {
        if($env:XIDE_TEST_MODE -eq 'font'){$global:LASTEXITCODE=74}
        if($env:XIDE_TEST_MODE -eq 'server-change') {
            $xideTestTarget=if($env:XIDE_TEST_GUI_ONLY -eq '1'){$env:XIDE_TEST_QUALIFIED_SERVER}else{$global:XideTestNative}
            [IO.File]::AppendAllText($xideTestTarget,'SYNTHETIC-INPUT-DRIFT')
        }
    } elseif(-not $xideTestTool.EndsWith('catalog_publish.py')) {
        throw ('Unexpected synthetic tool: '+$xideTestTool)
    }
}
try {
    $xideTestBuildArguments=@{Configuration='Release';ReleaseDirectory=$env:XIDE_TEST_RUNTIME;FrontendRoot=$env:XIDE_TEST_REPOSITORY}
    if($env:XIDE_TEST_GUI_ONLY -eq '1'){$xideTestBuildArguments.GuiOnly=$true;$xideTestBuildArguments.LanguageServerPath=$env:XIDE_TEST_QUALIFIED_SERVER}
    & $env:XIDE_TEST_SCRIPT @xideTestBuildArguments
    exit 0
} catch {
    Write-Output $_.Exception.Message
    exit 1
}
'''


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--cc',default=shutil.which('gcc'))
    p.add_argument('--previous',default='5b56ea1d6')
    a=p.parse_args();root=IDE.parent;out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    compiler=Path(a.cc).resolve(strict=True);bin_dir=out/'bin';bin_dir.mkdir();temporary=out/'compiler-temp';temporary.mkdir()
    source=bin_dir/'synthetic_compiler.c';source.write_bytes(FAKE_COMPILER.encode('utf8'))
    env=dict(os.environ,TMP=str(temporary),TEMP=str(temporary))
    result=subprocess.run([str(compiler),'-std=c11','-O2','-Wall','-Wextra','-Werror',str(source),'-o',str(bin_dir/'gcc.exe')],capture_output=True,env=env)
    (out/'bootstrap.log').write_bytes(result.stdout+result.stderr);assert not result.returncode,result.stderr
    shutil.copyfile(bin_dir/'gcc.exe',bin_dir/'windres.exe')
    before_script=subprocess.check_output(['git','show',a.previous+':ide/tools/build.ps1'],cwd=root)
    current_script=(IDE/'tools/build.ps1').read_bytes()
    real_guard=[root/'release'/name for name in ['xlang_ide.exe','xlang-ls.exe','xge.dll','option/settings.json','data/commands.json','data/catalog-sources/index.json']]
    protected={str(x):sha(x) for x in real_guard}
    protected[str(IDE/'tools/build.ps1')]=sha(IDE/'tools/build.ps1')
    protected[str(Path(__file__).resolve())]=sha(Path(__file__))
    record=dict(status='running',scope=__doc__,previousRevision=a.previous,buildScriptSha256=sha(IDE/'tools/build.ps1'),protected=protected,cases=[])
    wrapper=out/'wrapper.ps1';wrapper.write_bytes(WRAPPER.encode('utf8'))
    def save():(out/'result.json').write_bytes((json.dumps(record,ensure_ascii=False,indent=2)+'\n').encode('utf8'))
    def case(label,raw,mode,gui_only,previous=False):
        folder=out/label;folder.mkdir();runtime=folder/'runtime';runtime.mkdir();objects=folder/'objects'
        for name,data in [('xlang_ide.exe',b'PREVIOUS-GUI'),('xlang-ls.exe',b'PREVIOUS-SERVER'),('xge.dll',b'PREVIOUS-SDK')]:
            (runtime/name).write_bytes(data)
        (runtime/'option').mkdir();(runtime/'option/settings.json').write_bytes(b'{"preserve":true}\n')
        (runtime/'keep.txt').write_bytes(b'USER-DATA\n')
        objects.mkdir();(objects/'gui-build-inputs.json').write_bytes(b'PREVIOUS-GUI-RECEIPT')
        (objects/'language-server-build.json').write_bytes(b'PREVIOUS-NATIVE-RECEIPT')
        qualified=folder/'qualified-ls.exe';qualified.write_bytes(b'QUALIFIED-EXTERNAL-SERVER')
        before={str(x):sha(x) for x in [*runtime.rglob('*'),*objects.iterdir()] if x.is_file()}
        text=raw.decode('utf-8-sig')
        object_assignment="$objectRoot = Join-Path $ideRoot ('build\\' + $Configuration)"
        assert text.count(object_assignment)==1
        # Rewire only tool/object locations in the diagnostic copy. The actual
        # build flow, SDK verification, gates and publication body are retained.
        text=text.replace(object_assignment,"$objectRoot = '"+str(objects).replace("'","''")+"'")
        text=text.replace('$PSScriptRoot','$xideTestTools')
        text="$xideTestTools = '"+str(IDE/'tools').replace("'","''")+"'\n"+text
        # param/attributes must remain first: inject the tool-root assignment
        # immediately after the existing ErrorActionPreference assignment.
        prefix=text[:text.index('\n')+1];text=text[len(prefix):]
        key="$ErrorActionPreference='Stop'";assert text.count(key)==1;text=text.replace(key,key+'\n'+prefix.strip(),1)
        script=folder/'build.ps1';script.write_bytes(text.encode('utf8'))
        case_env=dict(env,PATH=str(bin_dir)+os.pathsep+env.get('PATH',''),XIDE_TEST_REAL_COMPILER=str(compiler),XIDE_TEST_SANDBOX=str(folder),XIDE_TEST_MODE=mode,XIDE_TEST_GUI_ONLY='1' if gui_only else '0',XIDE_TEST_RUNTIME=str(runtime),XIDE_TEST_REPOSITORY=str(root),XIDE_TEST_SCRIPT=str(script),XIDE_TEST_QUALIFIED_SERVER=str(qualified),XIDE_TEST_COMPILER_LOG=str(folder/'compiler.log'),XIDE_TEST_LINK_MARKER=str(folder/'linked'),XIDE_TEST_PYTHON_LOG=str(folder/'python.jsonl'))
        proc=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(wrapper)],cwd=folder,env=case_env,capture_output=True,timeout=180)
        (folder/'run.log').write_bytes(proc.stdout+proc.stderr)
        output_text=(proc.stdout+proc.stderr).decode('utf8',errors='replace')
        changed=[x for x,h in before.items() if sha(x)!=h]
        row=dict(label=label,mode=mode,guiOnly=gui_only,previous=previous,exit=proc.returncode,changed=changed,log=str(folder/'run.log'),logSha256=sha(folder/'run.log'))
        record['cases'].append(row);save();print(label,proc.returncode,flush=True)
        programs=[str(runtime/name) for name in ['xlang_ide.exe','xlang-ls.exe','xge.dll']]
        if previous:
            assert proc.returncode and any(x in changed for x in programs) and 'SYNTHETIC catalog rejection' in output_text,row
        elif mode=='success':
            assert not proc.returncode and all(x in changed for x in programs),row
            metadata=json.loads((objects/'gui-build-inputs.json').read_text('utf-8-sig'))
            assert metadata['cLspEnabled'] is False
            assert metadata['outputs']['xlang_ide']==sha(runtime/'xlang_ide.exe')
            assert metadata['outputs']['xlang_ls']==sha(runtime/'xlang-ls.exe')
            assert not (runtime/'xlang_ide_console.exe').exists()
        else:
            assert proc.returncode and not changed,row
            expected='Pinned offline font resource verification failed' if mode=='font' else 'Qualified language server changed before publication' if mode=='server-change' else 'SYNTHETIC catalog rejection'
            assert expected in output_text,(row,output_text[-2000:])
            if mode=='early':assert not (folder/'compiler.log').exists() and not list(objects.glob('lsp-*'))
        assert sha(runtime/'keep.txt')==before[str(runtime/'keep.txt')]
        assert sha(runtime/'option/settings.json')==before[str(runtime/'option/settings.json')]
        for name,h in protected.items():assert sha(name)==h,name
    try:
        for mode in ['early','late','font','final','server-change','success']:
            for gui in [True,False]:case('current-'+mode+('-gui' if gui else '-native'),current_script,mode,gui)
        for gui in [True,False]:case('previous-late'+('-gui' if gui else '-native'),before_script,'late',gui,True)
        record['status']='passed'
    except BaseException as exc:record.update(status='failed',error=repr(exc));raise
    finally:save()
    print(json.dumps(dict(status='passed',cases=len(record['cases']),evidence=str(out/'result.json'))))


if __name__=='__main__':main()
