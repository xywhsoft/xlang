from pathlib import Path
import hashlib,json,subprocess,sys
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;B=O/'baseline-v2';P=O/'native-diagnostic-v1';P.mkdir()
def verify_build(b,root):
 assert b['status']=='passed'
 for path,digest in b['inputs'].items():assert sha(path)==digest,path
 for name,digest in b['outputs'].items():
  p=Path(name) if Path(name).is_absolute() else root/name
  assert sha(p)==digest,str(p)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
b=json.loads((B/'build-result.json').read_text());verify_build(b,B)
record=dict(status='running',scope='Private native retention counters and inclusive phase diagnostics;not performance admission. Production rules and quotas unchanged.',normalBuild=str(B),inputs={str(B/'build-result.json'):sha(B/'build-result.json'),str(Path(__file__)):sha(__file__)},commands=[],outputs={})
def save():(P/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
def once(s,a,z):assert s.count(a)==1,a[:80];return s.replace(a,z,1)
def run(name,args):
 log=P/(name+'.log');row=dict(name=name,command=list(map(str,args)),log=str(log));record['commands'].append(row);save()
 with log.open('wb') as stream:p=subprocess.run(list(map(str,args)),cwd=R,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
 row.update(exit=p.returncode,sha256=sha(log));save()
 if p.returncode:raise ValueError(name+': '+log.read_text('utf8',errors='replace')[-1600:])
try:
 shadow=P/'analysis';shadow.mkdir()
 native=R/'ide/src/analysis/frontend_sem_native.inc';text=native.read_text();record['inputs'][str(native)]=sha(native)
 text=once(text,'    xrtMapUnit(&memo->nativeOwners);memo->nativeOwnersReady=0;',r'''    fprintf(stderr,"XLS_NATIVE_BIND entries=%zu retained=%llu owners=%llu reused=%llu bytes=%zu declined=%llu\n",memo->entries.Count,(unsigned long long)memo->nativeWork.retained,(unsigned long long)memo->nativeWork.owners,(unsigned long long)memo->nativeWork.reused,memo->nativeWork.bytes,(unsigned long long)memo->nativeWork.readonlyDeclined);
    xrtMapUnit(&memo->nativeOwners);memo->nativeOwnersReady=0;''')
 (shadow/'frontend_sem_native.inc').write_text(text,encoding='utf8')
 sources={}
 path=R/'ide/src/analysis/typed_module.c';text=path.read_text();record['inputs'][str(path)]=sha(path)
 text=once(text,'    if(okay)forest->knownBytes=forest->bytes+size.bytes;',r'''    fprintf(stderr,"XLS_NATIVE_SIZE okay=%d metadata=%zu measured=%zu nodes=%llu limit=%zu\n",okay,forest->bytes,forest->bytes+size.bytes,(unsigned long long)size.nodes,limit);
    if(okay)forest->knownBytes=forest->bytes+size.bytes;''')
 sources['typed_module']='#include <stdio.h>\n'+text
 path=R/'ide/src/analysis/check.c';text=path.read_text();record['inputs'][str(path)]=sha(path)
 text=once(text,'    r->phase="parse"; int okay;','    double diagnosticStart=xrtTimer(),diagnosticSemantic=0,diagnosticIndex=0,diagnosticIr=0;\n    r->phase="parse"; int okay;')
 text=once(text,'        r->phase="sem";','        diagnosticSemantic=xrtTimer();r->phase="sem";')
 text=once(text,'    char indexError[256]={0};','    diagnosticIndex=xrtTimer();char indexError[256]={0};')
 text=once(text,'        r->phase="ir";','        diagnosticIr=xrtTimer();r->phase="ir";')
 text=once(text,'    if(r->accepted)xlsFrontendSemMemoBindFunctions(r->memo,r->typedModule);',r'''    if(r->accepted)xlsFrontendSemMemoBindFunctions(r->memo,r->typedModule);
    fprintf(stderr,"XLS_NATIVE_PHASE accepted=%d parse=%.6f sem=%.6f index=%.6f ir=%.6f\n",r->accepted,(diagnosticSemantic-diagnosticStart)*1000,(diagnosticIndex-diagnosticSemantic)*1000,(diagnosticIr-diagnosticIndex)*1000,(xrtTimer()-diagnosticIr)*1000);''')
 sources['check']=text
 # Native work is copied to the returned check after the private attempts retire.
 needle='    if(result)result->nativeWork.retries=retried;'
 sources['check']=once(sources['check'],needle,needle+r'''
    if(result)fprintf(stderr,"XLS_NATIVE_RESULT retained=%llu reused=%llu retries=%llu nodes=%llu\n",(unsigned long long)result->nativeWork.retained,(unsigned long long)result->nativeWork.reused,(unsigned long long)result->nativeWork.retries,(unsigned long long)result->materializationWork.nodes);''')
 objects={}
 for name in ['semantic','typed_module','check']:
  c=next(c for c in b['commands'] if '-c' in c and c[-1]==str(B/(name+'.o')));args=list(c);args.insert(1,'-I'+str(P))
  if name in sources:
   p=P/(name+'.c');p.write_text(sources[name],encoding='utf8');args[args.index('-c')+1]=str(p)
  args[args.index('-o')+1]=str(P/(name+'.o'));run('compile-'+name,args);objects[str(B/(name+'.o'))]=str(P/(name+'.o'))
 archive=next(c for c in b['commands'] if Path(c[0]).name.lower()=='ar.exe');args=[objects.get(v,v) for v in archive];args[2]=str(P/'xlang-analysis-core.a');run('archive',args)
 link=next(c for c in b['commands'] if '-c' not in c and str(B/'xlang-ls.exe') in c);link=[str(P/'xlang-analysis-core.a') if v==str(B/'xlang-analysis-core.a') else v for v in link];link[link.index('-o')+1]=str(P/'xlang-ls.exe');run('link',link)
 verify_build(b,B)
 for p,d in record['inputs'].items():assert sha(p)==d,p
 record.update(status='built',outputs={str(p):sha(p) for p in P.rglob('*') if p.is_file() and p.name!='result.json'});save();print('Private native diagnostic built')
except BaseException as error:record.update(status='failed',error=repr(error));save();raise
