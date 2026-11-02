"""Reject actual C ownership/clone omissions against a matching native build.

Fault objects keep the complete native/public data ABI and all other objects.
The protected-page control independently proves that cloning reads logical
source values without touching the sibling-mutated native ownership counter.
This is correctness evidence,not GUI or performance admission.
"""
from pathlib import Path
import argparse,ctypes,json,os,subprocess
from build_lsp_lifecycle_profile import verify_build,sha

GUARD=r'''
#define _DEFAULT_SOURCE 1
#include "analysis/ast_clone.h"
#include <stddef.h>
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
int main(void)
{
    _Static_assert(offsetof(XL6_AstSource,path)==sizeof(size_t),"source ownership word boundary changed");
    size_t page;unsigned char* pages;
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    SYSTEM_INFO info;GetSystemInfo(&info);page=info.dwPageSize;
    pages=VirtualAlloc(NULL,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!pages)return 10;
#else
    page=(size_t)sysconf(_SC_PAGESIZE);pages=mmap(NULL,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(pages==MAP_FAILED)return 10;
#endif
    XL6_AstSource* source=(XL6_AstSource*)(pages+page-sizeof(size_t));
    source->references=1;source->path="logical-source-ownership.xl";
    source->completionQuery=1;source->completionStart=7;source->completionEnd=19;
#if defined(_WIN32)
    DWORD old;if(!VirtualProtect(pages,page,PAGE_NOACCESS,&old))return 11;
#else
    if(mprotect(pages,page,PROT_NONE))return 11;
#endif
    XL6_AstNode input={0};input.kind=XL6_AST_TYPE;input.text="int";input.source=source;
    XL6_AstNode* copied=xlsAstClone(&input,NULL,NULL);
    int okay=copied && copied->source && copied->source!=source && copied->source->references==1 &&
        copied->source->completionQuery==1 && copied->source->completionStart==7 && copied->source->completionEnd==19 &&
        copied->source->path && !strcmp(copied->source->path,"logical-source-ownership.xl");
    xl6AstFree(copied);
#if defined(_WIN32)
    VirtualFree(pages,0,MEM_RELEASE);
#else
    munmap(pages,page*2);
#endif
    if(!okay)return 12;
    puts("logical source values cloned with original ownership word unreadable");return 0;
}
'''

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();build=a.build.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    normal=json.loads((build/'build-result.json').read_text());verify_build(normal,build)
    assert normal['status']=='passed' and normal['configuration']=='Release' and normal['testsBuilt']
    root=Path(__file__).resolve().parents[2];test=root/'ide/tests/analysis_typed_function_test.c'
    protected={**normal['inputs'],str(build/'build-result.json'):sha(build/'build-result.json'),str(Path(__file__).resolve()):sha(__file__)}
    for name,digest in normal['outputs'].items():protected[str(Path(name) if Path(name).is_absolute() else build/name)]=digest
    report=dict(status='running',accepted=False,scope=__doc__,protected=protected,commands=[],cases=[])
    def save():(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8',newline='\n')
    def run(name,command,rejected=False):
        result=subprocess.run(command,cwd=out,capture_output=True,timeout=30)
        stdout=out/(name+'.stdout.log');stderr=out/(name+'.stderr.log');stdout.write_bytes(result.stdout);stderr.write_bytes(result.stderr)
        row=dict(name=name,command=command,exit=result.returncode,stdout=str(stdout),stdoutSha256=sha(stdout),stderr=str(stderr),stderrSha256=sha(stderr),expected='nonzero' if rejected else 'zero')
        report['commands'].append(row);save()
        assert (result.returncode!=0)==rejected,(name,result.returncode,result.stderr[-2000:])
        return row
    def compile(source,reference,name):
        command=list(next(c for c in normal['commands'] if '-c' in c and str(reference) in c));command[command.index(str(reference))]=str(source)
        for flag in ['-MF','-MT','-o']:
            if flag in command:command[command.index(flag)+1]=str(out/(name+('.d' if flag=='-MF' else '.o')))
        run(name+'-compile',command);return out/(name+'.o')
    def link(name,objects,wrapped=True):
        original=next(c for c in normal['commands'] if '-o' in c and Path(c[c.index('-o')+1]).name=='analysis_typed_function_test.exe')
        command=[v for v in original if wrapped or not v.startswith('-Wl,--wrap=')];command[1:2]=[str(v) for v in objects];target=out/(name+'.exe');command[command.index('-o')+1]=str(target)
        run(name+'-link',command);return target
    previousMode=ctypes.windll.kernel32.SetErrorMode(0x8003) if os.name=='nt' else None
    try:
        run('normal-owner-control',[str(build/'analysis_typed_function_test.exe')]);report['cases'].append(dict(name='normal-owner-control',status='passed'))
        source=root/'ide/src/analysis/typed_module.c';body=source.read_text(encoding='utf8')
        seam='    xrtMutexLock(&forest->lock);xl6AstFree(f->root);f->root=NULL;xrtMutexUnlock(&forest->lock);'
        assert body.count(seam)==1
        changed=out/'typed_module.no_function_lock.c';changed.write_text(body.replace(seam,'    xl6AstFree(f->root);f->root=NULL;'),encoding='utf8',newline='\n')
        obj=compile(changed,source,'no-function-lock');exe=link('no-function-lock',[build/'analysis_typed_function_test.o',obj]);run('no-function-lock-run',[str(exe)],True)
        report['cases'].append(dict(name='sibling-teardown-lock-omitted',status='rejected',source=str(changed),sha256=sha(changed)))
        source=root/'ide/src/analysis/function_unit.c';body=source.read_text(encoding='utf8');seam='    if(owned){';assert body.count(seam)==1
        changed=out/'function_unit.force_clone.c';changed.write_text(body.replace(seam,'    if(owned && 0){'),encoding='utf8',newline='\n')
        obj=compile(changed,source,'force-function-clone');exe=link('force-function-clone',[build/'analysis_typed_function_test.o',obj]);run('force-function-clone-run',[str(exe)],True)
        report['cases'].append(dict(name='native-function-owner-bypassed',status='rejected',source=str(changed),sha256=sha(changed)))
        guard=out/'source_logical_values.c';guard.write_text(GUARD,encoding='utf8',newline='\n');obj=compile(guard,test,'source-logical-values')
        exe=link('source-logical-control',[obj],False);run('source-logical-control-run',[str(exe)]);report['cases'].append(dict(name='unreadable-source-counter-control',status='passed',source=str(guard),sha256=sha(guard)))
        source=root/'ide/src/analysis/ast_clone.c';body=source.read_text(encoding='utf8')
        seam='        value->completionQuery=in->completionQuery;\n        value->completionStart=in->completionStart;value->completionEnd=in->completionEnd;\n        value->path=duplicate(c,in->path);'
        assert body.count(seam)==1
        changed=out/'ast_clone.read_source_counter.c';changed.write_text(body.replace(seam,'        *value=*in;value->references=0;value->path=duplicate(c,in->path);'),encoding='utf8',newline='\n')
        mutant=compile(changed,source,'read-source-counter');exe=link('read-source-counter',[obj,mutant],False);run('read-source-counter-run',[str(exe)],True)
        report['cases'].append(dict(name='source-owner-counter-read-restored',status='rejected',source=str(changed),sha256=sha(changed)))
        verify_build(normal,build)
        for path,digest in protected.items():assert sha(path)==digest,path
        report['status']='passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:
        if previousMode is not None:ctypes.windll.kernel32.SetErrorMode(previousMode)
        save()
    print(json.dumps(dict(status=report['status'],cases=len(report['cases']),commands=len(report['commands']),report=str(out/'result.json'))))

if __name__=='__main__':main()
