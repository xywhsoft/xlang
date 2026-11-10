"""Build a private lifecycle-instrumented LS from an intact normal build.

The original core/runtime/host remain the oracle. This never installs an image
or changes compiler/IDE source. Inclusive timings locate work; they do not
qualify performance. The normal build must pass build_lsp_core's own checks.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess
from check_native_bridge_catalog import function
from build_lsp_core import dependencies
from lsp_profile_inputs import profile_inputs


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def read(path):
    return json.loads(Path(path).read_text('utf8'))


def verify_build(build,root):
    if build['status']!='passed' or not build['runs']:
        raise ValueError('a complete passed normal build with native runs is required')
    for path,digest in build['inputs'].items():
        if sha(path)!=digest:raise ValueError('normal build input changed: '+path)
    for name,digest in build['outputs'].items():
        path=Path(name) if Path(name).is_absolute() else root/name
        if sha(path)!=digest:raise ValueError('normal build output changed: '+str(path))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path,required=True,help='intact passed normal build directory')
    parser.add_argument('--output',type=Path,required=True,help='fresh private output directory')
    parser.add_argument('--ir-verifier-detail',action='store_true',help='diagnostic-only inclusive timings for direct native verifier helpers; does not qualify performance')
    parser.add_argument('--preflight-only',action='store_true',help='verify archive/linked object recipes and source identities without compiling or running a probe')
    args=parser.parse_args();B=args.build.resolve(strict=True);O=args.output.resolve()
    b=read(B/'build-result.json');verify_build(b,B)
    originalBindings={v['original']:v['sha256'] for v in b.get('sourceRebinding',{}).get('bindings',[])}
    for k,v in b.items():
     if k.endswith('Extension') and isinstance(v,dict):
      originalBindings.update({p:d['sha256'] for p,d in v.get('actualInputBindings',{}).items()})
    O.mkdir(parents=True,exist_ok=False);temporary=O/'compiler-temp';temporary.mkdir()
    rec=dict(status='running',accepted=False,scope='Diagnostic inclusive per-thread worker/session/adapter/projection/retirement timings;not formal performance or a per-query retirement binding.',commands=[],inputs={str(Path(__file__).resolve()):sha(__file__),str(Path(__file__).with_name('check_native_bridge_catalog.py')):sha(Path(__file__).with_name('check_native_bridge_catalog.py')),str(B/'build-result.json'):sha(B/'build-result.json'),str(Path(__file__).with_name('lsp_profile_inputs.py')):sha(Path(__file__).with_name('lsp_profile_inputs.py'))},programs={},normalBuild=str(B),normalBuildInputs=len(b['inputs']),normalBuildOutputs=len(b['outputs']))
    labels=[];copies={};host={};env=dict(os.environ,TMP=str(temporary),TEMP=str(temporary))
    env['PATH']=str(Path(b['compiler']).parent)+os.pathsep+env.get('PATH','')
    def slot(label):
     if label not in labels:labels.append(label)
     return labels.index(label)
    def save():(O/'result.json').write_text(json.dumps(rec,indent=2)+'\n',encoding='utf8',newline='\n')
    def run(c,n):
     rec['commands'].append(c);save();p=subprocess.run(c,cwd=O,env=env,capture_output=True,timeout=180);(O/(n+'.log')).write_bytes(p.stdout+p.stderr);save();assert not p.returncode,(n,(p.stdout+p.stderr).decode('utf8')[-2400:]);return p
    def calls(body,names,group,void=()):
     hits=[]
     for m in re.finditer(r'\b([A-Za-z_]\w*)\s*\(',body):
      if m[1] not in names:continue
      at=m.end();depth=1
      while depth:
       if body[at]=='(':depth+=1
       elif body[at]==')':depth-=1
       at+=1
      hits.append((m.start(),at,m[1]))
     for start,end,name in reversed(hits):body=body[:start]+('XLS_LIFECYCLE_VOID' if name in void else 'XLS_LIFECYCLE')+'('+str(slot(group+name))+','+body[start:end]+')'+body[end:]
     return body
    def edit(source,name,transform):
     body=function(source,name);return source.replace(body,transform(body),1)
    def surround(source,name,arguments,group,label,*,result='int',nullable=False):
     body=function(source,name);brace=body.index('{');signature=body[:brace].strip()
     original='xlsLifecycleOriginal_'+name
     renamed=signature.replace(name,original,1)
     if not renamed.startswith('static '):renamed='static '+renamed
     invoke=original+'('+arguments+');'
     if result!='void':invoke=result+' profileResult='+invoke
     wrapper=signature+'\n{\n'+(' if(!q)return;\n' if nullable else '')+' xlsLifecycleBegin();double start=xrtTimer();\n '+invoke+'\n xlsLifecycleAdd('+str(slot(label))+',start);xlsLifecycleEmit("'+group+'");\n'+(' return profileResult;\n' if result!='void' else '')+'}\n'
     return source.replace(body,renamed+'\n'+body[brace:]+'\n'+wrapper,1)
    try:
     names=('check_session','frontend_entry_cache','syntax','check','language_snapshot','projection','worker','semantic_definition','language_check','xl6_ir')
     normalLink,originalCore,units,memberHashes=profile_inputs(b,B,O,names,run)
     rec['profileSelection']={name:dict(command=command,object=str(Path(command[command.index('-o')+1]).resolve()),sha256=sha(command[command.index('-o')+1])) for name,command in units.items()}
     rec['originalCoreMemberHashes']=memberHashes
     rec['linkedInputs']={str(Path(part).resolve()):sha(part) for part in normalLink if part.endswith(('.o','.a'))}
     for name in names:
      c=units[name];p=Path(c[c.index('-c')+1]);copies[name]=p.read_text('utf8');assert ({str(Path(q).resolve()):h for q,h in originalBindings.items()} | {str(Path(q).resolve()):h for q,h in b['inputs'].items()}).get(str(p.resolve()))==sha(p),p;rec['inputs'][str(p)]=sha(p)
     if args.preflight_only:
      verify_build(b,B);rec['status']='preflight-passed';rec['programs']=dict(normal=dict(path=str(B/'xlang-ls.exe'),sha256=sha(B/'xlang-ls.exe')));return
     def session(body):
      body=body.replace('{','{\n    double lifecycleTotal=xrtTimer();',1)
      body=calls(body,{'xlsInputBatchCreateInContext','xlsSyntaxBuild','xlsSemanticCheckCreateBatch','xlsSemanticCheckValidate','xlsSemanticCheckInputUsage'},'session.')
      point='return result;';assert body.count(point)==1
      return body.replace(point,'xlsLifecycleAdd('+str(slot('session.total'))+',lifecycleTotal); '+point,1)
     copies['check_session']=edit(copies['check_session'],'xlsCheckSessionCheckWithHistory',session)
     for name in ['sharedParse','moduleParse']:
      copies['frontend_entry_cache']=edit(copies['frontend_entry_cache'],name,lambda body:calls(body,{'xl6ParseSourceWithCompletion','xlsRawBodyCopyParse','capture','xlsTextCreate','xlsSyntaxAdoptOrdinary','captureSyntax','xlsRawBodyRegisterSyntax'},'import.',{'xlsRawBodyRegisterSyntax'}))
     copies['frontend_entry_cache']=edit(copies['frontend_entry_cache'],'capture',lambda body:calls(body,{'xlsAstClone'},'capture.'))
     copies['syntax']=edit(copies['syntax'],'build',lambda body:calls(body,{'xl6ParseSource','xl6ParseSourceWithCompletion','decompose'},'syntax.'))
     copies['syntax']=edit(copies['syntax'],'decompose',lambda body:calls(body,{'xlsAstSetSourceTreeSized'},'syntax.'))
     copies['syntax']=edit(copies['syntax'],'xlsSyntaxAdoptOrdinary',lambda body:calls(body,{'decompose','cacheTreeBytes'},'adopt.'))
     def check(body):
      body=body.replace('{','{\n    double lifecycleStage=xrtTimer();',1)
      for point,label in [('r->phase="parse";','setup'),('r->phase="import";','parse'),('r->phase="stdlib";','import'),('r->phase="sem";','stdlib'),('    if(okay && captureIndex)r->typedIndex=xlsFrontendIndexCreate','semantic'),('        r->phase="ir";','index')]:
       assert body.count(point)==1;body=body.replace(point,'xlsLifecycleStep('+str(slot('body.'+label))+',&lifecycleStage); '+point,1)
      point='    if(r->accepted) r->phase="checked";';assert body.count(point)==1
      return body.replace(point,'    xlsLifecycleStep('+str(slot('body.ir-and-finish'))+',&lifecycleStage);\n'+point,1)
     copies['check']=edit(copies['check'],'createBody',check)
     # Measure the actual raw builder and complete module passes separately.
     # Only outer calls in the audited module-build function are wrapped;
     # recursive builders and the original complete verifier keep their rules.
     ir_build=function(copies['xl6_ir'],'xlsIrBuildModuleOriginal')
     ir_names=set(re.findall(r'\b(xl6Ir(?:Build|Refresh|Intern|Normalize|TypePoolFinalize|VerifyModule)\w*)\s*\(',ir_build))
     copies['xl6_ir']=edit(copies['xl6_ir'],'xlsIrBuildModuleOriginal',lambda body:calls(body,ir_names,'ir.',{'xl6IrBuildRuntimeTypeMap'}))
     if args.ir_verifier_detail:
      verifier=function(copies['xl6_ir'],'xlsIrVerifyModuleOriginal')
      verifier_names=set(re.findall(r'\b(xl6IrVerify\w*)\s*\(',verifier))
      copies['xl6_ir']=edit(copies['xl6_ir'],'xlsIrVerifyModuleOriginal',lambda body:calls(body,verifier_names,'verify.'))
      rec['verifierDetail']=dict(scope='Direct helper calls in the complete native verifier; inclusive diagnostic timers alter cost and are not performance qualification',functions=sorted(verifier_names))
     def adapter(body):
      body=calls(body,{'nativeAnalysis','loadGraph','buildDiagnostics','buildNativeDiagnostics','xideSemanticDefinitionsAcquireTyped','validateGraph','xrtJsonRead','xrtJsonParse','xlsSemanticCheckValidate','xideSemanticNavigationCapture','xideSemanticNavigationBind','xideSemanticNavigationBindAndPreparePoints','xideSemanticNavigationPreparePoints','xlsSemanticCheckDestroy','clearGraph'},'adapter.',{'xideSemanticNavigationBind','xlsSemanticCheckDestroy','clearGraph'})
      return body
     copies['language_snapshot']=edit(copies['language_snapshot'],'xideLanguageSnapshotRun',adapter)
     copies['language_snapshot']=surround(copies['language_snapshot'],'xideLanguageSnapshotRun','q,output,e','snapshot','adapter.total')
     copies['semantic_definition']=edit(copies['semantic_definition'],'preparePoints',lambda body:calls(body,{'navigationOwnedKey','xlsProjectionAcquire','nativePointsCreate','xlsProjectionCreate','xlsProjectionPublishAt'},'navigation.points.'))
     inc=next(Path(p) for p in b['inputs'] if p.endswith('semantic_native_points.inc'))
     assert sha(inc)==b['inputs'][str(inc)];rec['inputs'][str(inc)]=sha(inc)
     include='#include "services/semantic_native_points.inc"'
     assert copies['semantic_definition'].count(include)==1
     copies['semantic_definition']=copies['semantic_definition'].replace(include,inc.read_text(encoding='utf8'),1)
     def point_detail(body):
      body=body.replace('{','{\n double pointDetail=xrtTimer();',1)
      segmented='nativeFunctionsBegin' in body
      setup=' size_t functionSlot=0;' if segmented else ' for(size_t i=0;i<nodes;++i){\n  node n={0};xls_index_fact f;'
      cursors=' /* Scratch is no longer read.' if segmented else ' qsort(d->declarations'
      finish=' xrtArrayUnit(&reuse.keys);free(spans);free(ends);free(selections);return d;' if segmented else ' free(spans);free(ends);free(selections);return d;'
      for marker,label in [(setup,'setup'),(' int fast=budget ? convertDense','facts-and-ranges'),(' if(!xrtArrayInit(&d->cursors','coordinates'),(cursors,'cursors'),(finish,'sort-and-finish')]:
       assert body.count(marker)==1,(marker,body.count(marker))
       body=body.replace(marker,' xlsLifecycleStep('+str(slot('point.body.'+label))+',&pointDetail);\n'+marker,1)
      return body
     copies['semantic_definition']=edit(copies['semantic_definition'],'nativePointsCreate',point_detail)

     copies['worker']=surround(copies['worker'],'execute','cancel,value,output','check','worker.total',result='xtaskoutcome')
     def destroy(body):
      point='xrtAtomic32FetchSub(&r->references,1,XMEMORY_ACQ_REL)!=1) return;';assert body.count(point)==1
      body=body.replace(point,point+'\n double lifecycleTotal=xrtTimer();',1)
      body=calls(body,{'xlsFrontendIndexDestroy','xlsFrontendSemMemoDestroy','xlsInputTopologyDestroy','xlsModuleGraphRelease','xlsFrontendIrResultDestroy','xl6IrModuleFree','xlsTypedModuleRelease','xl6ParseResultFree','xlsInputBatchDestroy','xl6SourceReportFree','xlsFrontendOverlayClear'},'destroy.',{'xlsFrontendIndexDestroy','xlsFrontendSemMemoDestroy','xlsInputTopologyDestroy','xlsModuleGraphRelease','xlsFrontendIrResultDestroy','xl6IrModuleFree','xlsTypedModuleRelease','xl6ParseResultFree','xlsInputBatchDestroy','xl6SourceReportFree','xlsFrontendOverlayClear'})
      point=' xrtFree(r);';assert body.count(point)==1
      return body.replace(point,' xlsLifecycleAdd('+str(slot('destroy.total'))+',lifecycleTotal);'+point,1)
     copies['check']=edit(copies['check'],'xlsSemanticCheckDestroy',destroy)
     copies['projection']=edit(copies['projection'],'xlsProjectionPublish',lambda body:calls(body,{'xlsProjectionRelease'},'projection.publish.',{'xlsProjectionRelease'}))
     def release(body):
      point='p->destroy(p->value);';assert body.count(point)==1
      return body.replace(point,'XLS_LIFECYCLE_VOID('+str(slot('projection.valueDestroy'))+',p->destroy(p->value));',1)
     copies['projection']=edit(copies['projection'],'xlsProjectionRelease',release)
     def definitions(body):
      # Both audited destructor forms have one non-null path. Keep the timer
      # after its guard so a NULL no-op never contributes a retirement sample.
      points=[point for point in ('if(d) {','if(!d)return;') if body.count(point)==1]
      assert len(points)==1
      point=points[0];body=body.replace(point,point+'\n double lifecycleTotal=xrtTimer();',1)
      point='free(d);';assert body.count(point)==1
      return body.replace(point,'xlsLifecycleAdd('+str(slot('navigation.destroy'))+',lifecycleTotal);'+point,1)
     copies['semantic_definition']=edit(copies['semantic_definition'],'xideSemanticDefinitionsDestroy',definitions)
     copies['semantic_definition']=edit(copies['semantic_definition'],'xideSemanticDefinitionsAcquireTyped',lambda body:calls(body,{'navigationKey','xideSemanticDefinitionsCreateTyped','xlsProjectionPublish','xlsProjectionAcquire'},'navigation.'))
     def retire(body):
      body=calls(body,{'xideSemanticDefinitionsReleaseTyped','xlsTextRelease','xlsContextRelease','xrtValueRelease','WaitForSingleObject'},'retire.',{'xideSemanticDefinitionsReleaseTyped','xlsTextRelease','xlsContextRelease','xrtValueRelease'})
      return body
     copies['language_check']=edit(copies['language_check'],'xideLanguageCheckDestroy',retire)
     copies['language_check']=surround(copies['language_check'],'xideLanguageCheckDestroy','q','retire','retire.total',result='void',nullable=True)
     copies['language_snapshot']=edit(copies['language_snapshot'],'retainedCheck',lambda body:calls(body,{'xlsQueryDestroy'},'adapter.',{'xlsQueryDestroy'}))
     header=O/'profile.h';header.write_text('''#ifndef XLS_LIFECYCLE_PROFILE_H
    #define XLS_LIFECYCLE_PROFILE_H
    #include <xrt.h>
    #define XLS_LIFECYCLE_CELLS 256
void xlsLifecycleBegin(void);void xlsLifecycleAdd(size_t,double);void xlsLifecycleStep(size_t,double*);void xlsLifecycleEmit(const char*);
    #define XLS_LIFECYCLE(id,expression) ({double lifecycleStart=xrtTimer();__auto_type lifecycleResult=(expression);xlsLifecycleAdd(id,lifecycleStart);lifecycleResult;})
    #define XLS_LIFECYCLE_VOID(id,expression) do{double lifecycleStart=xrtTimer();expression;xlsLifecycleAdd(id,lifecycleStart);}while(0)
    #endif
    ''',encoding='utf8',newline='\n')
     assert len(labels)<256 and all(re.fullmatch(r'[A-Za-z0-9_.-]{1,80}',x) for x in labels)
     diag=O/'diagnostic.c';diag.write_text('''#include "profile.h"
    #include <stdio.h>
    #include <string.h>
    typedef struct{double ms;unsigned long long calls;} lifecycle_cell;
    static _Thread_local lifecycle_cell frames[8][XLS_LIFECYCLE_CELLS];
    static _Thread_local size_t depth;
    /* OS thread IDs can be reused after a worker exits. A process-wide
     * sequence distinguishes later lifetimes without allocating TLS records. */
    static xatomic64 sequence;
    static const char* const labels[]={'''+','.join(json.dumps(x) for x in labels)+'''};
    void xlsLifecycleBegin(void){if(depth<8)memset(frames[depth],0,sizeof(frames[depth]));++depth;}
    void xlsLifecycleAdd(size_t at,double begin){if(depth && depth<=8 && at<XLS_LIFECYCLE_CELLS){lifecycle_cell* cell=&frames[depth-1][at];cell->ms+=(xrtTimer()-begin)*1000;++cell->calls;}}
    void xlsLifecycleStep(size_t at,double* begin){xlsLifecycleAdd(at,*begin);*begin=xrtTimer();}
    void xlsLifecycleEmit(const char* group){
     char record[32768];size_t at=0;
     if(!depth || depth>8){if(depth)--depth;fprintf(stderr,"XLS_LIFECYCLE_ERROR depth\\n");return;}
     const lifecycle_cell* cells=frames[--depth];
     int written=snprintf(record,sizeof(record),"XLS_LIFECYCLE {\\"group\\":\\"%s\\",\\"thread\\":%llu,\\"sequence\\":%llu,\\"cells\\":{",group,(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtAtomic64FetchAdd(&sequence,1,XMEMORY_RELAXED)+1);
     if(written<0 || (size_t)written>=sizeof(record)){return;}
     at=(size_t)written;int comma=0;
     for(size_t i=0;i<sizeof(labels)/sizeof(*labels);++i)if(cells[i].calls){
      written=snprintf(record+at,sizeof(record)-at,"%s\\"%s\\":{\\"ms\\":%.9f,\\"calls\\":%llu}",comma ? "," : "",labels[i],cells[i].ms,cells[i].calls);
      if(written<0 || (size_t)written>=sizeof(record)-at){return;}
      at+=(size_t)written;comma=1;
     }
     if(sizeof(record)-at<4){return;}
     memcpy(record+at,"}}\\n",3);at+=3;fwrite(record,1,at,stderr);
    }
    ''',encoding='utf8',newline='\n')
     core=O/'core.a';core.write_bytes((B/'xlang-analysis-core.a').read_bytes());ar=str(Path(b['compiler']).with_name('ar.exe' if os.name=='nt' else 'ar'))
     for name,text in copies.items():
      src=O/(name+'.c');src.write_text(text,encoding='utf8',newline='\n');c=units[name];obj=O/(name+'.o');flags=[c[0],'-I'+str(O),'-I'+str(Path(c[c.index('-c')+1]).parent),*c[1:c.index('-c')],'-include',str(header)];dep=run(flags+['-MM','-MT','profileObject',str(src)],'deps-'+name)
      expected={str(Path(p).resolve()):h for p,h in originalBindings.items()};expected.update({str(Path(p).resolve()):h for p,h in b['inputs'].items()})
      closure={str(p.resolve()):sha(p) for p in dependencies(dep.stdout.decode())}
      for p,h in closure.items():assert Path(p).is_relative_to(O) or expected.get(p)==h,(name,p,'unmatched production dependency')
      rec.setdefault('actualDependencies',{}).update(closure)
      run(flags+['-c',str(src),'-o',str(obj)],'compile-'+name)
      if name in ['language_snapshot','semantic_definition','language_check']:host[str(Path(c[c.index('-o')+1]).resolve())]=str(obj)
      else:run([ar,'rcs',str(core),str(obj)],'archive-'+name)
      print(name,'profile compiled',flush=True)
     c=units['check'];obj=O/'diagnostic.o';run(c[:c.index('-c')]+['-I'+str(O),'-c',str(diag),'-o',str(obj)],'compile-diagnostic')
     exe=O/'profile-ls.exe';link=normalLink.copy();assert all(p in {str(Path(x).resolve()) for x in link if x.endswith('.o')} for p in host)
     link=[str(core) if x==originalCore else host.get(str(Path(x).resolve()),x) if x.endswith('.o') else x for x in link];link[link.index('-o')+1]=str(exe);link.insert(link.index(str(core)),str(obj));run(link,'link')
     rec['programs']=dict(diagnostic=dict(path=str(exe),sha256=sha(exe)),normal=dict(path=str(B/'xlang-ls.exe'),sha256=sha(B/'xlang-ls.exe')));rec['labels']=labels
     for p,h in rec['inputs'].items():assert sha(p)==h,p
     for path,h in rec['linkedInputs'].items():assert sha(path)==h,path
     verify_build(b,B)
     rec['outputs']={str(p):sha(p) for p in O.iterdir() if p.is_file() and p.suffix in ('.exe','.a','.o','.h','.c')}
     rec['status']='built'
    except BaseException as e:rec.update(status='failed',error=repr(e));raise
    finally:save()


if __name__=='__main__':
    main()
