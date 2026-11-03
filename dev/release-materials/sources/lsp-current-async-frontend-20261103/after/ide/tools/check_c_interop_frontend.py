"""Verify C-interop admission and actual native cache-schema omissions.

Deliberate corruption uses private include/frontend copies. This does not
install a runtime or establish performance,GUI or full LSP acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,sys
from native_c_interop_profile import COHORT,pins_for,profile
from native_current_semantic_profile import profile as semantic_profile
from native_current_ir_overlay import derive

PARSER_FIXTURE=r'''
#include "analysis/frontend_internal.h"
#include <stdio.h>
#include "xl6_parser.c"
static void* live[8192];static size_t count,allocations;static int active,failed;
void* __real_xrtCalloc(size_t,size_t);void __real_xrtFree(void*);
void* __wrap_xrtCalloc(size_t n,size_t size)
{
 void* p=__real_xrtCalloc(n,size);
 if(active && p && n==1 && size==sizeof(XL6_AstNode)){
  size_t i=0;for(;i<count && live[i];++i){}
  if(i==8192)failed=1;else{live[i]=p;if(i==count)++count;++allocations;}
 }
 return p;
}
void __wrap_xrtFree(void* p)
{if(p)for(size_t i=0;i<count;++i)if(live[i]==p){live[i]=NULL;break;}__real_xrtFree(p);}
int main(void)
{
 const char* cases[]={
  "extern \"record\" struct Record @header(\"record.h\")\n c_int values[2]\nend\n",
  "extern \"handle*\" class Handle @header(\"handle.h\") @lifetime(\"caller\")\n native point handle\nend\n",
  "extern \"create\" @no_raise @factory(1, 0, dispose) @c_ref(1) func create(c_int mode, Record value) as Handle\n",
  "extern \"call\" @no_raise @c_static_callback(0) func call(c_callback func(c_int) as c_int callback) as c_int\n",
  "import dep as alias\nfrom dep import compute as run\nfunc main(...) as int\n return run(7)\nend\n"
 };
 size_t prefixes=0;active=1;
 for(size_t c=0;c<sizeof(cases)/sizeof(*cases);++c)for(size_t end=0;end<=strlen(cases[c]);++end){
  char source[512];memcpy(source,cases[c],end);source[end]=0;
  XL6_ParseResult parsed;xl6ParseResultInit(&parsed);xl6ParseSource(source,"foreign-stack.xl",&parsed);xl6ParseResultFree(&parsed);
  if(failed)return 1;
  for(size_t i=0;i<count;++i)if(live[i]){XL6_AstNode* n=live[i];fprintf(stderr,"foreign parser ownership prefix %zu/%zu leaks AST %s %s\n",c,end,xl6AstKindName(n->kind),n->text ? n->text : "");return 1;}
  ++prefixes;
 }
 active=0;printf("foreign parser ownership passed: %zu prefixes,%zu actual native AST allocations\n",prefixes,allocations);return 0;
}
'''

AGGREGATE_FIXTURE=r'''
#include "analysis/frontend_internal.h"
#include <stdio.h>
#include "xl6_ir.c"
static int different(const xls_ir_derived_context* base,const XL6_IrModule* module)
{
 xls_ir_derived_context* other=xlsIrDerivedContextBuild(module,1024u*1024u);
 int result=other && !xlsIrMemoEqual(&base->key,&other->key);xlsIrDerivedContextRelease(other);return result;
}
int main(void)
{
 XL6_IrType type={.kind=XL6_IR_TYPE_C_RECORD,.name="Record",.foreignHeader="record.h"};
 XL6_IrAggregateLayout layout={.type=1,.declaration=1,.firstField=0,.fieldCount=1};
 XL6_IrAggregateField field={.ownerType=1,.type=1,.declaration=2,.name="value",.cName="value"};
 XL6_IrModule module={.types={.items=&type,.count=1,.capacity=1},
  .aggregateLayouts=&layout,.aggregateLayoutCount=1,.aggregateLayoutCapacity=1,
  .aggregateFields=&field,.aggregateFieldCount=1,.aggregateFieldCapacity=1};
 xls_ir_derived_context* base=xlsIrDerivedContextBuild(&module,1024u*1024u);if(!base)return 1;
 for(unsigned change=0;change<5;++change){XL6_IrAggregateField saved=field;
  switch(change){case 0:++field.ownerType;break;case 1:++field.type;break;case 2:++field.declaration;break;
   case 3:field.name="changed";break;default:field.cName[0]='V';break;}
  if(!different(base,&module)){fprintf(stderr,"foreign aggregate key missing field %u\n",change);return 1;}field=saved;
 }
 xlsIrDerivedContextRelease(base);puts("foreign aggregate key passed: five exact native field facts");return 0;
}
'''

def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def replace(source,old,new):
    if source.count(old)!=1:raise ValueError('C omission seam changed: '+old)
    return source.replace(old,new,1)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();build=a.build.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    manifest=build/'build-result.json';receipt=json.loads(manifest.read_text('utf-8-sig'))
    assert receipt['status']=='passed' and receipt['testsBuilt'] and receipt['configuration']=='Release'
    frontend=Path(receipt['frontendRoot']);ide=Path(__file__).resolve().parents[1];PINS=pins_for(frontend)
    protected={**receipt['inputs'],str(manifest):sha(manifest),str(Path(__file__).resolve()):sha(__file__)}
    for name,digest in receipt['outputs'].items():protected[str(build/name)]=digest
    report=dict(status='running',accepted=False,scope=__doc__.strip(),protected=protected,guards=[],commands=[],omissions=[])
    def save():(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
    def stable():
        for path,digest in protected.items():assert sha(path)==digest,path
    def run(command,name,cwd=out):
        result=subprocess.run(command,cwd=cwd,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['commands'].append(dict(name=name,command=command,cwd=str(cwd),exit=result.returncode,log=str(log),logSha256=sha(log)));save()
        return result
    try:
        stable();profile(frontend);semantic_profile(frontend)
        target=out/'control';target.mkdir();proof=derive(frontend,target,phases=True)[2]
        assert proof['irDependencyCohort']==COHORT and proof['derivedPhases']['enabled']
        report['guards'].append(dict(name='complete-cohort',passed=True,inputs=len(PINS),cohort=COHORT));save()
        private=out/'guard-inputs';shutil.copytree(frontend,private)
        for rel in ('src/ast/xl6_ast.h','src/ir/xl6_ir.h','src/sem/xl6_sem.c',
                    'src/ir/xl6_ir_callable_result.c','src/ir/xl6_ir_foreign_call.c',
                    'src/type/xl6_foreign_decl.h','src/type/xl6_foreign_call_contract.h',
                    'src/ir/xl6_ir_lower.c','src/sem/xl6_sem_index.c','lib/xrt/xrt.h'):
            path=private/rel;original=path.read_bytes();path.write_bytes(original+b'\n')
            refusals={}
            try:
                for name,check in (('schema',profile),('semantic',semantic_profile),('ir',lambda root:derive(root,out/'refused',phases=True))):
                    try:check(private)
                    except ValueError as error:refusals[name]=str(error)
                    else:raise AssertionError('unreviewed '+name+' input admitted: '+rel)
                report['guards'].append(dict(name=rel,passed=True,sha256=sha(path),refusals=refusals));save()
            finally:path.write_bytes(original)
        fixture=ide/'tests/analysis_ir_sparse_decode_test.c'
        compile_command=next(c for c in receipt['commands'] if '-c' in c and str(fixture) in c)
        original_object=compile_command[compile_command.index('-o')+1]
        link_command=next(c for c in receipt['commands'] if '-c' not in c and original_object in c and '-o' in c)
        memo=ide/'src/analysis/frontend_ir_memo.inc';source=memo.read_text('utf8')
        variants=[('control',source)]
        variants += [
            ('missing-header',replace(source,'if(!xlsIrMemoString(key,t->foreignHeader,limit))return 0;','if(0)return 0;')),
            ('missing-base',replace(source,',t->foreignBase,t->nativeArrayExtent',',0,t->nativeArrayExtent')),
            ('missing-array-extent',replace(source,',t->foreignBase,t->nativeArrayExtent',',t->foreignBase,0')),
            ('uninitialized-padding',replace(source,'    memset(&value.foreignCall,0,sizeof(value.foreignCall));','    /* injected: retain source padding */')),
            ('missing-rollback-encode',replace(source,'if(!xlsIrMemoSymbol(capture,*fields[i],fields[i],&kind))return 0;',
                'if(i==4)continue;if(!xlsIrMemoSymbol(capture,*fields[i],fields[i],&kind))return 0;')),
            ('missing-rollback-decode',replace(source,'for(unsigned i=0;i<XLS_IR_SYMBOL_FIELDS;++i)if(!xlsIrMemoResolve(',
                'for(unsigned i=0;i<XLS_IR_SYMBOL_FIELDS;++i)if(i!=4)if(!xlsIrMemoResolve(')),
        ]
        for name,body in variants:
            folder=out/('native-'+name);include=folder/'include/analysis';include.mkdir(parents=True)
            alternate=include/'frontend_ir_memo.inc';alternate.write_text(body,encoding='utf8',newline='\n')
            obj=folder/'fixture.o';exe=folder/'fixture.exe'
            command=[*compile_command];command[command.index('-o')+1]=str(obj)
            command.insert(1,'-I'+str(folder/'include'))
            result=run(command,name+'-compile');assert result.returncode==0,name
            command=[str(obj) if v==original_object else v for v in link_command]
            command[command.index('-o')+1]=str(exe)
            result=run(command,name+'-link');assert result.returncode==0,name
            result=run([str(exe)],name+'-run')
            if name=='control':assert result.returncode==0 and b'passed:' in result.stdout,result.stdout+result.stderr
            else:assert result.returncode==1 and b'analysis_ir_sparse_decode_test:' in result.stderr,(name,result.returncode,result.stdout,result.stderr)
            report['omissions'].append(dict(name=name,passed=True,source=str(alternate),sourceSha256=sha(alternate),exit=result.returncode));save()
        # New Lemon composite ownership must retain its original destructor.
        # Count real AST allocation addresses,not unavailable Release heap stats.
        parser=(build/'frontend/xl6_parser.c').read_text('utf8')
        derived=(ide/'src/analysis/frontend_ir_derived.inc').read_text('utf8')
        extra=[
            ('parser-control',PARSER_FIXTURE,'xl6_parser.c',parser,None),
            ('parser-missing-contract-free',PARSER_FIXTURE,'xl6_parser.c',replace(parser,
                'xl6AstFree((yypminor->yy166).attributes);','(void)yypminor;'),b'foreign parser ownership prefix'),
            ('parser-missing-stack-free',PARSER_FIXTURE,'xl6_parser.c',replace(parser,
                'xl6AstFree(yypminor->yy230);','(void)yypminor;'),b'foreign parser ownership prefix'),
            ('parser-stale-destructor-bound',PARSER_FIXTURE,'xl6_parser.c',replace(parser,
                '#define YY_MIN_DSTRCTR       132','#define YY_MIN_DSTRCTR       138'),b'foreign parser ownership prefix'),
            ('aggregate-control',AGGREGATE_FIXTURE,'analysis/frontend_ir_derived.inc',derived,None),
            ('aggregate-missing-name',AGGREGATE_FIXTURE,'analysis/frontend_ir_derived.inc',replace(derived,
                '!xlsIrMemoString(&context->key,field->name,bytes) ||',''),b'foreign aggregate key missing field'),
            ('aggregate-missing-c-name',AGGREGATE_FIXTURE,'analysis/frontend_ir_derived.inc',replace(derived,
                '!xlsIrMemoData(&context->key,field->cName,sizeof(field->cName),bytes)','0'),b'foreign aggregate key missing field'),
        ]
        for name,body,rel,native,error_marker in extra:
            folder=out/('native-'+name);include=folder/'include';include.mkdir(parents=True)
            alternate=include/rel;alternate.parent.mkdir(parents=True,exist_ok=True);alternate.write_text(native,encoding='utf8',newline='\n')
            fixture=folder/'fixture.c';fixture.write_text(body,encoding='utf8',newline='\n')
            obj=folder/'fixture.o';exe=folder/'fixture.exe';command=[*compile_command]
            command[command.index('-c')+1]=str(fixture);command[command.index('-o')+1]=str(obj);command.insert(1,'-I'+str(include))
            result=run(command,name+'-compile');assert result.returncode==0,name
            command=[str(obj) if v==original_object else v for v in link_command];command[command.index('-o')+1]=str(exe)
            if name.startswith('parser-'):command+=['-Wl,--wrap=xrtCalloc','-Wl,--wrap=xrtFree']
            result=run(command,name+'-link');assert result.returncode==0,name
            result=run([str(exe)],name+'-run')
            if error_marker:assert result.returncode==1 and error_marker in result.stderr,(name,result.returncode,result.stderr)
            else:assert result.returncode==0 and b'passed:' in result.stdout,(name,result.returncode,result.stdout,result.stderr)
            report['omissions'].append(dict(name=name,passed=True,source=str(alternate),sourceSha256=sha(alternate),fixture=str(fixture),fixtureSha256=sha(fixture),exit=result.returncode));save()
        stable();report['status']='passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=report['status'],guards=len(report['guards']),nativeCases=len(report['omissions']),report=str(out/'result.json'))))


if __name__=='__main__':main()
