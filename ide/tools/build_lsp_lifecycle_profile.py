"""Build a private lifecycle-instrumented LS from an intact normal build.

The original core/runtime/host remain the oracle. This never installs an image
or changes compiler/IDE source. Inclusive timings locate work; they do not
qualify performance. The normal build must pass build_lsp_core's own checks.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess
from check_native_bridge_catalog import function


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
    args=parser.parse_args();B=args.build.resolve(strict=True);O=args.output.resolve()
    b=read(B/'build-result.json');verify_build(b,B)
    O.mkdir(parents=True,exist_ok=False);temporary=O/'compiler-temp';temporary.mkdir()
    rec=dict(status='running',accepted=False,scope='Diagnostic inclusive per-thread worker/session/adapter/projection/retirement timings;not formal performance or a per-query retirement binding.',commands=[],inputs={str(Path(__file__).resolve()):sha(__file__),str(Path(__file__).with_name('check_native_bridge_catalog.py')):sha(Path(__file__).with_name('check_native_bridge_catalog.py')),str(B/'build-result.json'):sha(B/'build-result.json')},programs={},normalBuild=str(B),normalBuildInputs=len(b['inputs']),normalBuildOutputs=len(b['outputs']))
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
     for name in ['check_session','frontend_entry_cache','syntax','check','language_snapshot','projection','worker','semantic_definition','language_check']:
      c=next(c for c in b['commands'] if '-c' in c and Path(c[c.index('-c')+1]).stem==name);p=Path(c[c.index('-c')+1]);copies[name]=p.read_text('utf8');assert b['inputs'][str(p)]==sha(p),p;rec['inputs'][str(p)]=sha(p)
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
     def adapter(body):
      body=calls(body,{'nativeAnalysis','loadGraph','buildDiagnostics','buildNativeDiagnostics','xideSemanticDefinitionsAcquireTyped','validateGraph','xrtJsonRead','xrtJsonParse','xlsSemanticCheckValidate'},'adapter.')
      return body
     copies['language_snapshot']=edit(copies['language_snapshot'],'xideLanguageSnapshotRun',adapter)
     copies['language_snapshot']=surround(copies['language_snapshot'],'xideLanguageSnapshotRun','q,output,e','snapshot','adapter.total')
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
      point='if(d) {';assert body.count(point)==1;body=body.replace(point,point+'double lifecycleTotal=xrtTimer();',1)
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
void xlsLifecycleBegin(void);void xlsLifecycleAdd(size_t,double);void xlsLifecycleStep(size_t,double*);void xlsLifecycleEmit(const char*);
    #define XLS_LIFECYCLE(id,expression) ({double lifecycleStart=xrtTimer();__auto_type lifecycleResult=(expression);xlsLifecycleAdd(id,lifecycleStart);lifecycleResult;})
    #define XLS_LIFECYCLE_VOID(id,expression) do{double lifecycleStart=xrtTimer();expression;xlsLifecycleAdd(id,lifecycleStart);}while(0)
    #endif
    ''',encoding='utf8',newline='\n')
     assert len(labels)<128 and all(re.fullmatch(r'[A-Za-z0-9_.-]{1,80}',x) for x in labels)
     diag=O/'diagnostic.c';diag.write_text('''#include "profile.h"
    #include <stdio.h>
    #include <string.h>
    typedef struct{double ms;unsigned long long calls;} lifecycle_cell;
    static _Thread_local lifecycle_cell frames[8][128];
    static _Thread_local size_t depth;
    static const char* const labels[]={'''+','.join(json.dumps(x) for x in labels)+'''};
    void xlsLifecycleBegin(void){if(depth<8)memset(frames[depth],0,sizeof(frames[depth]));++depth;}
    void xlsLifecycleAdd(size_t at,double begin){if(depth && depth<=8 && at<128){lifecycle_cell* cell=&frames[depth-1][at];cell->ms+=(xrtTimer()-begin)*1000;++cell->calls;}}
    void xlsLifecycleStep(size_t at,double* begin){xlsLifecycleAdd(at,*begin);*begin=xrtTimer();}
    void xlsLifecycleEmit(const char* group){
     char record[32768];size_t at=0;static _Thread_local unsigned long long sequence;
     if(!depth || depth>8){if(depth)--depth;fprintf(stderr,"XLS_LIFECYCLE_ERROR depth\\n");return;}
     const lifecycle_cell* cells=frames[--depth];
     int written=snprintf(record,sizeof(record),"XLS_LIFECYCLE {\\"group\\":\\"%s\\",\\"thread\\":%llu,\\"sequence\\":%llu,\\"cells\\":{",group,(unsigned long long)xrtThreadCurrentId(),++sequence);
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
      src=O/(name+'.c');src.write_text(text,encoding='utf8',newline='\n');c=next(c for c in b['commands'] if '-c' in c and Path(c[c.index('-c')+1]).stem==name);obj=O/(name+'.o');flags=[c[0],'-I'+str(O),*c[1:c.index('-c')],'-include',str(header)];run(flags+['-c',str(src),'-o',str(obj)],'compile-'+name)
      if name in ['language_snapshot','semantic_definition','language_check']:host[str(B/(name+'.o'))]=str(obj)
      else:run([ar,'rcs',str(core),str(obj)],'archive-'+name)
      print(name,'profile compiled',flush=True)
     c=next(c for c in b['commands'] if '-c' in c and Path(c[c.index('-c')+1]).stem=='check');obj=O/'diagnostic.o';run(c[:c.index('-c')]+['-I'+str(O),'-c',str(diag),'-o',str(obj)],'compile-diagnostic')
     exe=O/'profile-ls.exe';link=next(c for c in b['commands'] if '-c' not in c and str(B/'xlang-ls.exe') in c);assert all(p in link for p in host)
     link=[str(exe) if x==str(B/'xlang-ls.exe') else str(core) if x==str(B/'xlang-analysis-core.a') else host.get(x,x) for x in link];link.insert(link.index(str(core)),str(obj));run(link,'link')
     rec['programs']=dict(diagnostic=dict(path=str(exe),sha256=sha(exe)),normal=dict(path=str(B/'xlang-ls.exe'),sha256=sha(B/'xlang-ls.exe')));rec['labels']=labels
     for p,h in rec['inputs'].items():assert sha(p)==h,p
     verify_build(b,B)
     rec['outputs']={str(p):sha(p) for p in O.iterdir() if p.is_file() and p.suffix in ('.exe','.a','.o','.h','.c')}
     rec['status']='built'
    except BaseException as e:rec.update(status='failed',error=repr(e));raise
    finally:save()


if __name__=='__main__':
    main()
