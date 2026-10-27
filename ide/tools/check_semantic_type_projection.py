"""Verify the real static semantic type-projection certificate and refusal seams.

Includes the exact generated semantic translation unit from a successful native
producer. Only XRT array resizing on the fixture's explicitly named output is
wrapped; production objects and compiler rules are unchanged. Use a fresh output.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys

FIXTURE=r'''
#include <xrt.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static _Thread_local xarray* refusalTarget;
static _Thread_local unsigned refusals;
static bool certificateResize(xarray* target,size_t count)
{if(target==refusalTarget){++refusals;return false;}return xrtArrayResize(target,count);}
#define xrtArrayResize certificateResize
#include XLS_TYPE_PROJECTION_FIXTURE
#undef xrtArrayResize
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"type_projection:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static uint64 live(void){xmemstats stats;xrtMemStatsGet(&stats);return stats.BlockAllocCalls-stats.BlockFreeCalls;}
static xls_sem_position position(size_t n)
{xls_sem_position p={0};p.range=(XL6_AstRange){1,n,n+1,2,3,2,4};p.token=p.range;p.line=2;p.column=3;return p;}
static int initialize(xarray* old,xarray* output,xls_sem_apply* apply,xls_sem_type_copy* copy,int changed)
{
 if(!xrtArrayInit(old,sizeof(xls_sem_position)) || !xrtArrayResize(old,6) || !xrtArrayInit(output,sizeof(xls_sem_position)) || !xrtArrayResize(output,2))return 0;
 for(size_t i=0;i<6;++i)*(xls_sem_position*)xrtArrayGet(old,i)=position(10+i%2);
 for(size_t i=0;i<2;++i)*(xls_sem_position*)xrtArrayGet(output,i)=position((changed ? 100 : 10)+i);
 *apply=(xls_sem_apply){.previous=old,.positions=output,.visited=2,.different=changed};
 *copy=(xls_sem_type_copy){.options={.transform=xlsSemApplyPosition,.user=apply}};return 1;
}
static int same(xls_sem_position a,xls_sem_position b)
{return xlsSemSameRange(a.range,b.range) && xlsSemSameRange(a.token,b.token) && a.line==b.line && a.column==b.column;}
static int run(void)
{
 CHECK(!xrtPoolInitLayout(NULL,0,0,0));uint64 baseline=live();
 for(unsigned previous=0;previous<2;++previous)for(unsigned changed=0;changed<2;++changed){
  xarray old={0},output={0};xls_sem_apply apply;xls_sem_type_copy copy;CHECK(initialize(&old,&output,&apply,&copy,(int)changed));if(!previous)old.Count=0;
  CHECK(xlsSemTypeProjectionMark(&copy)==2 && xlsSemTypeProjectionReplay(&copy,0,2));CHECK(output.Count==4 && apply.visited==4 && apply.different==(int)changed);
  CHECK(same(*(xls_sem_position*)xrtArrayGet(&output,0),*(xls_sem_position*)xrtArrayGet(&output,2)) && same(*(xls_sem_position*)xrtArrayGet(&output,1),*(xls_sem_position*)xrtArrayGet(&output,3)));
  CHECK(xlsSemTypeProjectionReplay(&copy,0,2) && apply.visited==6 && output.Count==6);
  CHECK(xlsSemTypeProjectionReplay(&copy,6,0) && apply.visited==6 && output.Count==6);
  if(previous)CHECK(!xlsSemTypeProjectionReplay(&copy,0,1));
  xrtArrayUnit(&old);xrtArrayUnit(&output);CHECK(live()==baseline);
 }
#define FIELD(member) offsetof(xls_sem_position,member)
 const size_t offsets[]={FIELD(range.known),FIELD(range.startOffset),FIELD(range.endOffset),FIELD(range.startLine),FIELD(range.startColumn),FIELD(range.endLine),FIELD(range.endColumn),FIELD(token.known),FIELD(token.startOffset),FIELD(token.endOffset),FIELD(token.startLine),FIELD(token.startColumn),FIELD(token.endLine),FIELD(token.endColumn),FIELD(line),FIELD(column)};
#undef FIELD
 for(unsigned unknown=0;unknown<2;++unknown)for(size_t i=0;i<sizeof(offsets)/sizeof(*offsets);++i){
  xarray old={0},output={0};xls_sem_apply apply;xls_sem_type_copy copy;CHECK(initialize(&old,&output,&apply,&copy,1));
  if(unknown)for(size_t j=0;j<old.Count;++j){xls_sem_position* p=xrtArrayGet(&old,j);p->range.known=p->token.known=0;}
  ((unsigned char*)xrtArrayGet(&old,2))[offsets[i]]^=1;size_t capacity=output.Capacity;xls_sem_position snapshot[2];memcpy(snapshot,output.Data,sizeof(snapshot));
  CHECK(!xlsSemTypeProjectionReplay(&copy,0,2) && apply.visited==2 && output.Count==2 && output.Capacity==capacity && apply.different==1 && !memcmp(snapshot,output.Data,sizeof(snapshot)));
  xrtArrayUnit(&old);xrtArrayUnit(&output);CHECK(live()==baseline);
 }
 for(unsigned mode=0;mode<9;++mode){
  xarray old={0},output={0};xls_sem_apply apply;xls_sem_type_copy copy;CHECK(initialize(&old,&output,&apply,&copy,0));size_t begin=0,count=2;
  if(mode==0)begin=3;else if(mode==1)count=3;else if(mode==2)apply.visited=3;else if(mode==3)old.Count=3;
  else if(mode==4){apply.visited=XLS_SEM_MEMO_BYTES/sizeof(xls_sem_position);output.Count=apply.visited;count=1;}
  else if(mode==5){apply.visited=XLS_SEM_MEMO_BYTES/sizeof(xls_sem_position)+1;output.Count=apply.visited;count=0;}
  else if(mode==6)begin=SIZE_MAX;else if(mode==7)count=SIZE_MAX;else copy.options.transform=NULL;
  size_t before=apply.visited,beforeCount=output.Count;CHECK(!xlsSemTypeProjectionReplay(&copy,begin,count) && apply.visited==before && output.Count==beforeCount && !apply.different);
  if(mode==2 || mode==8)CHECK(xlsSemTypeProjectionMark(&copy)==SIZE_MAX);
  output.Count=2;old.Count=6;xrtArrayUnit(&old);xrtArrayUnit(&output);CHECK(live()==baseline);
 }
 /* Refusal at the exact real resize API boundary changes neither the cursor
  * nor existing rows. This is API refusal,not a claim of arbitrary OOM proof. */
 xarray old={0},output={0};xls_sem_apply apply;xls_sem_type_copy copy;CHECK(initialize(&old,&output,&apply,&copy,1));refusalTarget=&output;xls_sem_position snapshot[2];memcpy(snapshot,output.Data,sizeof(snapshot));size_t capacity=output.Capacity;
 CHECK(!xlsSemTypeProjectionReplay(&copy,0,2) && refusals==1 && apply.visited==2 && output.Count==2 && output.Capacity==capacity && !memcmp(snapshot,output.Data,sizeof(snapshot)));
 refusalTarget=NULL;CHECK(xlsSemTypeProjectionReplay(&copy,0,2) && apply.visited==4 && output.Count==4);xrtArrayUnit(&old);xrtArrayUnit(&output);CHECK(live()==baseline);
 printf("semantic_type_projection passed: %u checks;actual compiler-private certificate,equal/mismatched16fields including unknown ranges,complete old sequence,bounds,zero span,atomic resize refusal/retry and balanced live blocks\n",checks);return 1;
}
int main(void){xrtMemStatsEnable(true);return run()?0:1;}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native',type=Path,required=True);parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();native=args.native.resolve(strict=True);source=args.source.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    read=lambda p:json.loads(Path(p).read_text('utf8'));sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
    raw=read(native/'build-result.json');assert raw['status']=='passed';compiled=native/'frontend/xl6_sem.c';semantic=source/'src/analysis/frontend_sem.inc';assert compiled.is_file() and semantic.is_file()
    recorded=[h for p,h in raw['inputs'].items() if p.replace('\\','/').endswith('/src/analysis/frontend_sem.inc')];assert recorded and all(h==sha(semantic) for h in recorded)
    fixture=out/'certificate.c';fixture.write_text(FIXTURE,'utf8');snapshot=out/'runner.py';snapshot.write_bytes(Path(__file__).read_bytes())
    template=next(c for c in raw['commands'] if '-c' in c and Path(c[c.index('-o')+1]).stem=='xl6_sem')
    prefix=[template[0],'-I'+str(source/'src'),*template[1:template.index('-c')]];obj=out/'certificate.o';exe=out/'certificate.exe';dep=out/'certificate.d'
    compile=[*prefix,'-DXLS_TYPE_PROJECTION_FIXTURE="'+compiled.as_posix()+'"','-MMD','-MT','certificateObject','-MF',str(dep),'-c',str(fixture),'-o',str(obj)]
    link=next(c.copy() for c in raw['commands'] if '-c' not in c and '-o' in c and Path(c[c.index('-o')+1]).stem=='analysis_semantic_memo_test');link[1]=str(obj);link=[str(native/'xlang-analysis-core.a') if x.endswith('xlang-analysis-core.a') else x for x in link];link[link.index('-o')+1]=str(exe)
    commands=[];report=dict(status='failed',scope=__doc__,commands=commands,tool=dict(original=str(Path(__file__).resolve()),snapshot=str(snapshot)),inputs={str(p):sha(p) for p in [snapshot,fixture,semantic,compiled,native/'build-result.json',native/'xlang-analysis-core.a']})
    try:
        for name,command in [('compile',compile),('link',link),('run',[str(exe)])]:
            cp=subprocess.run(command,capture_output=True,cwd=out,timeout=900);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr);commands.append(dict(name=name,command=command,exit=cp.returncode,log=str(log),logSha256=sha(log)));print(name,cp.returncode,(cp.stdout+cp.stderr).decode('utf8','replace')[-1800:],flush=True);assert not cp.returncode,name
        sys.path.insert(0,str(source/'tools'));from build_lsp_core import dependencies
        report['inputs'].update({str(p):sha(p) for p in dependencies(dep.read_text())});assert all(sha(p)==h for p,h in report['inputs'].items());report['outputs']={str(p):sha(p) for p in [obj,exe,dep]};report['status']='passed'
    finally:(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf8')
if __name__=='__main__':main()
