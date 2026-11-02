from pathlib import Path
import json,hashlib,subprocess,shutil,sys,threading,io
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;P=O/'receipt-probe-v2'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build
from benchmark_lsp_analysis import make_body
from check_lsp_module_semantics import initialize,checked
from check_lsp_automatic_diagnostics import AutomaticClient
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
seed=json.loads((O/'source-base.json').read_text());B=Path(seed['baseline']);b=json.loads((B/'build-result.json').read_text());verify_build(b,B)
P.mkdir();shadow=P/'shadow/analysis';shadow.mkdir(parents=True)
record=dict(status='running',accepted=False,scope='Actual ordinary semantic receipt rejection reasons;private diagnostics only.',commands=[],runs=[],protected={str(Path(__file__)):sha(__file__),**seed['protected']})
def save():(P/'result.json').write_text(json.dumps(record,indent=2)+'\n')
def run(name,c):
 v=subprocess.run(c,cwd=R,capture_output=True,timeout=240);log=P/(name+'.log');log.write_bytes(v.stdout+v.stderr);record['commands'].append(dict(name=name,command=c,exit=v.returncode,log=str(log),sha256=sha(log)));save();assert not v.returncode,(name,v.stderr[-1500:])
try:
 text=(R/'ide/src/analysis/frontend_sem_ir.inc').read_text();seam='int xlsFrontendSemMemoIndexEquivalent(';assert text.count(seam)==1
 text=text.replace(seam,'static int xlsProbeIndexEquivalent(',1)
 text+='''
int xlsFrontendSemMemoIndexEquivalent(const xls_sem_memo* memo,const XL6_AstNode* root,
 const xls_sem_memo* previous,const XL6_AstNode* previousRoot,size_t slot,size_t* count)
{
 static _Thread_local const xls_sem_memo* seen;
 if(previous && memo && memo!=seen && root && previousRoot && root->childCount==memo->entries.Count && memo->entries.Count==previous->entries.Count){
  seen=memo;size_t masks[512]={0},accepted=0;
  for(size_t i=0;i<memo->entries.Count;++i){
   if(!xlsSemFunction(root->children[i]))continue;
   const xls_sem_entry* a=xrtArrayConstGet(&memo->entries,i);const xls_sem_entry* z=xrtArrayConstGet(&previous->entries,i);
   unsigned m=(!a->unit || a->unit!=z->unit) | ((!a->finalInput || a->finalInput!=z->finalInput)<<1) |
    ((!a->finalCandidate)<<2) | ((!a->finalReplayed)<<3) | ((a->start!=z->start || a->end!=z->end)<<4) |
    ((a->typeStart!=z->typeStart || a->typeEnd!=z->typeEnd)<<5) | ((memo->finalContext!=previous->finalContext)<<6);
   ++masks[m];size_t ignored=0;accepted+=xlsProbeIndexEquivalent(memo,root,previous,previousRoot,i,&ignored)!=0;
  }
  fprintf(stderr,"RECEIPT functions=%zu admitted=%zu memoBytes=%zu",root->childCount,accepted,memo->bytes);
  for(unsigned i=0;i<512;++i)if(masks[i])fprintf(stderr," mask%u=%zu",i,masks[i]);fputc('\\n',stderr);
 }
 return xlsProbeIndexEquivalent(memo,root,previous,previousRoot,slot,count);
}
'''
 path=shadow/'frontend_sem_ir.inc';path.write_text(text,newline='\n');record['protected'][str(path)]=sha(path)
 final=(R/'ide/src/analysis/frontend_sem_final.inc').read_text()
 seam='    if(mask&~XLS_FINAL_PATCH_MASK)return 0;';assert final.count(seam)==1
 final=final.replace(seam,'    if(mask&~XLS_FINAL_PATCH_MASK){static unsigned seen;if(seen++<4)fprintf(stderr,"MASKFAIL mask=%u forbidden=%u nodes=%zu types=%zu\\n",mask,mask&~XLS_FINAL_PATCH_MASK,reader->nodes,reader->types);return 0;}',1)
 seam='if(value>=reader->end || value>UINT32_MAX)return 0;';assert final.count(seam)==1
 final=final.replace(seam,'if(value>=reader->end || value>UINT32_MAX){fputs("REGIONOVERFLOW\\n",stderr);return 0;}',1)
 seam='}else if(value!=1)return 0;';assert final.count(seam)==1
 final=final.replace(seam,'}else if(value!=1){static unsigned seen;if(seen++<4)fprintf(stderr,"REGIONFAIL field=%u value=%llu old=%u..%u current=%u..%u\\n",field,(unsigned long long)value,reader->oldStart,reader->oldEnd,reader->start,reader->end);return 0;}',1)
 p=shadow/'frontend_sem_final.inc';p.write_text(final,newline='\n');record['protected'][str(p)]=sha(p)
 c=list(next(c for c in b['commands'] if '-c' in c and Path(c[c.index('-c')+1]).name=='xl6_sem.c'));c.insert(1,'-I'+str(shadow.parent));c[-1]=str(P/'xl6_sem.o');run('sem-compile',c)
 shutil.copyfile(B/'xlang-analysis-core.a',P/'core.a');run('archive',[str(Path(b['compiler']).with_name('ar.exe')),'rcs',str(P/'core.a'),str(P/'xl6_sem.o')])
 c=list(next(c for c in b['commands'] if c[-1]==str(B/'xlang-ls.exe')));c=[str(P/'core.a') if x==str(B/'xlang-analysis-core.a') else x for x in c];c[-1]=str(P/'xlang-ls.exe');run('server-link',c)
 answers={}
 for functions in [32,512]:
  folder=P/('f'+str(functions));folder.mkdir();entry=folder/'main.xl';entry.write_text('// immutable disk\n')
  for variant,program in [('normal',B/'xlang-ls.exe'),('diagnostic',P/'xlang-ls.exe')]:
   client=AutomaticClient(program,P/'absent-xl.exe',folder);log=P/(str(functions)+'-'+variant+'.stderr.log');stream=client.process.stderr
   def drain():
    with log.open('wb') as out:shutil.copyfileobj(stream,out)
   reader=threading.Thread(target=drain);reader.start();client.process.stderr=io.BytesIO();rows=[]
   try:
    initialize(client,entry,[entry],folder)
    for generation in range(4):
     text=make_body(functions,generation,'single','declaration')
     if generation:client.change(entry,text,generation+1)
     else:client.open(entry,text,1)
     value=checked(client,entry,generation+1);assert value['ok'] and not value['count']
     facts={k:v for k,v in value.items() if k not in ['analysis','durationMs','checkId']};key=(functions,generation)
     if key in answers:assert answers[key]==facts
     answers[key]=facts;rows.append(value)
   finally:
    client.stop();reader.join(5);record['runs'].append(dict(functions=functions,variant=variant,exit=client.process.returncode,readerAlive=reader.is_alive(),results=rows,stderr=str(log),stderrSha256=sha(log)));save()
   assert not client.process.returncode and not reader.is_alive()
   if variant=='normal':assert not log.read_bytes()
   else:print(functions,log.read_text(),flush=True)
 verify_build(b,B)
 for p,d in record['protected'].items():assert sha(p)==d,p
 record['status']='passed'
except BaseException as e:record.update(status='failed',error=repr(e));raise
finally:save()
