"""Compile metadata omissions against a matching native build and current fixture."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from build_lsp_core import dependencies
from ir_record_schema import generate

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--family',choices=('calls','control','enums'),default='calls');args=p.parse_args();fixture='analysis_ir_'+{'calls':'call','control':'control','enums':'enum'}[args.family]+'_link_test'
 r=Path(__file__).resolve().parents[2];out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);b=json.loads(args.build.read_text());assert b['status']=='passed'
 for path,h in b['inputs'].items():assert sha(path)==h,path
 cc=next(c for c in b['commands']if '-c'in c and Path(c[c.index('-c')+1]).stem=='ir_record_link')
 tc=next(c for c in b['commands']if '-c'in c and Path(c[c.index('-c')+1]).stem==fixture)
 lc=next(c for c in b['commands']if '-c'not in c and '-o'in c and Path(c[c.index('-o')+1]).name==fixture+'.exe')
 core=Path(next(v for v in lc if v.endswith('xlang-analysis-core.a')));assert sha(core)==b['outputs'][core.name]
 inc=Path(next(v[2:]for v in cc if v.startswith('-I')and Path(v[2:]).name=='ir_record_link'))/'ir-record-records.inc';schema=json.loads(inc.with_name('schema.json').read_text());native=Path(cc[cc.index('-c')+1]).read_text();generated=inc.read_text()
 runtime=args.runtime.resolve();assert runtime==(r/'release/win_x64').resolve();env=dict(os.environ,TMP=str(out),TEMP=str(out));record=dict(status='failed',buildSha256=sha(args.build),toolSha256=sha(__file__),commands=[],inputs={},runs=[],schemaCases=[])
 def run(label,c):
  record['commands'].append(c);v=subprocess.run(c,env=env,cwd=out,capture_output=True,timeout=300);(out/(label+'.log')).write_bytes(v.stdout+v.stderr);return v
 def compile(label,flags,source):
  dep=run(label+'-deps',[*flags,'-MM','-MT','object',str(source)]);assert not dep.returncode
  for path in dependencies(dep.stdout.decode()):
   record['inputs'][str(path)]=sha(path)
   if path.resolve().is_relative_to(r/'ide/src'):
    frozen=Path(tc[tc.index('-c')+1]).parents[1]/'src'/path.relative_to(r/'ide/src');assert sha(frozen)==sha(path),path
  target=out/(label+'.o');v=run(label+'-compile',[*flags,'-c',str(source),'-o',str(target)]);assert not v.returncode,(label,v.stderr.decode(errors='replace')[-2000:]);return target
 test=r/'ide/tests'/(fixture+'.c');testObject=compile('current-fixture',tc[:tc.index('-c')],test)
 entry={'calls':'callNativeTests','control':'controlTests','enums':'enumTests'}[args.family];mixed={'calls':'mixedNativeCalls','control':'mixedNativeControl','enums':'mixedNativeEnums'}[args.family];private=out/'native-only.c';text=test.read_text(encoding='utf8').replace('static int '+entry+'(void)','int '+entry+'(void)').replace('int main(void){return '+entry+'()?0:1;}','int main(void){xrtAtomic32Init(&checks,0);return '+mixed+'()?0:1;}');private.write_text(text,encoding='utf8')
 nativeTest=compile('native-only',[*tc[:tc.index('-c')],'-I'+str(test.parent)],private)
 cases=[('positive',native,generated,testObject)]
 family_tags={'calls':{'SITE','ARGUMENT','DYNAMIC_SITE','DYNAMIC_ARGUMENT','METHOD','PARAMETER','INTRINSIC','OPERAND'},'control':{'CLEANUP','EXIT_STEP','EXIT_PLAN','INITIALIZER'},'enums':{'ENUM_LAYOUT','ENUM_VARIANT','ENUM_PAYLOAD'}}
 roles=[role for role in schema['roles']if role[0]in family_tags[args.family]]
 for i,(tag,field,domain)in enumerate(roles):
  start=generated.index('static int relocate_'+tag+'(');end=generated.index('static const void* read_'+tag,start);part=generated[start:end]
  expression=re.compile(r'dst->'+re.escape(field)+r'=(?:\(size_t\)|\(uint32\))?mapped;');assert len(expression.findall(part))==1,(tag,field)
  changed=generated[:start]+expression.sub('(void)mapped;',part,1)+generated[end:];cases.append(('omit-'+str(i),native,changed,testObject))
 def change(label,old,new):
  assert native.count(old)==1,label;cases.append((label,native.replace(old,new,1),generated,testObject))
 change('noncontiguous','else if(value!=begin+i)','else if(value!=begin+i && 0)')
 change('empty-offset','*out=empty;','*out=0;(void)empty;')
 if args.family=='calls':change('wrong-dispatch-domain','XLS_RECORD_CLASS_SLOT,"dispatchSlot"','XLS_RECORD_PROTOCOL_SLOT,"dispatchSlot"')
 change('missing-namespace','b->producer!=xlsIrTableUnitProducer(b->unit)','(b->producer!=xlsIrTableUnitProducer(b->unit) && 0)')
 change('leaked-retain','xlsIrTableUnitRetain(image->units[i]);++image->retained;','xlsIrTableUnitRetain(image->units[i]);xlsIrTableUnitRetain(image->units[i]);++image->retained;')
 nativeTag={'calls':'SITE','control':'INITIALIZER','enums':'ENUM_VARIANT'}[args.family];nativeField='origin'if args.family=='calls'else'declaration'
 start=generated.index('static int relocate_'+nativeTag+'(');end=generated.index('static const void* read_'+nativeTag,start);part=generated[start:end];assert part.count('dst->'+nativeField+'=mapped;')==1
 broken=part.replace('dst->'+nativeField+'=mapped;','(void)mapped;').replace('return equal_'+nativeTag+'(dst,current) || fail(s,XLS_RECORD_DIFFERENT,"record",XLS_RECORD_'+nativeTag+',0);','(void)current;return ((void)equal_'+nativeTag+',1);')
 cases.extend([('native-positive',native,generated,nativeTest),('native-stale-origin',native,generated[:start]+broken+generated[end:],nativeTest)])
 alias=runtime/'.ir-call-link-qa.exe';owned=None
 try:
  for label,body,roles,obj in cases:
   directory=out/label;directory.mkdir();source=directory/'ir_record_link.c';source.write_text(body);(directory/inc.name).write_text(roles)
   compiled=compile(label,[cc[0],'-I'+str(directory),*cc[1:cc.index('-c')]],source)
   link=[lc[0],str(compiled),*lc[1:]];link[next(i for i,v in enumerate(link)if Path(v).name==fixture+'.o')]=str(obj);program=directory/'test.exe';link[link.index('-o')+1]=str(program);v=run(label+'-link',link);assert not v.returncode
   assert not alias.exists();shutil.copy2(program,alias);owned=sha(alias);v=run(label+'-run',[str(alias)]);record['runs'].append(dict(label=label,exit=v.returncode,stdout=v.stdout.decode(errors='replace'),stderr=v.stderr.decode(errors='replace'),programSha256=owned));assert sha(alias)==owned;alias.unlink();owned=None
   if label in('positive','native-positive'):assert not v.returncode,(label,v.stderr)
   else:assert v.returncode and b'analysis_ir_memo_test:'in v.stderr,(label,v.returncode,v.stderr)
   if label=='native-stale-origin':assert b'xl6IrVerifyModule(module,&diags)'in v.stderr,v.stderr
  headers={str(path):path.read_text()for path in dependencies((args.build.parent/'ir-table-schema-deps.log').read_text())if path.suffix=='.h'and path.name.startswith('xl6_')};header=next(k for k in headers if Path(k).name=='xl6_ir.h')
  for label,ty,old,new in [('added','XL6_IrCallSite','XL6_IrNodeId origin;','XL6_IrNodeId origin; XL6_IrTypeId unaudited;'),('alias','XL6_IrCallSite','XL6_IrNodeId origin;','uint32 origin;'),('pointer','XL6_IrCallArgument','char* name;','char** name;'),('extent','XL6_IrIntrinsicOperand','XL6_IrNodeId expression;','XL6_IrNodeId expression[2];'),('abi','XL6_IrDynamicMethod','uint64 variadicElementIdentity;','XL6_IrTypeId variadicElementIdentity;'),('control-depth','XL6_IrExitPlan','uint32 targetScopeDepth;','XL6_IrNodeId targetScopeDepth;'),('control-region','XL6_IrExitStep','uint32 failureRegion;','size_t failureRegion;'),('control-new','XL6_IrCleanupAction','XL6_IrTypeId type;','XL6_IrTypeId type; uint32 unaudited;'),('enum-type','XL6_IrEnumLayout','XL6_IrTypeId type;','uint32 type;'),('enum-slice','XL6_IrEnumLayout','size_t firstVariant;','uint32 firstVariant;'),('enum-node','XL6_IrEnumVariant','XL6_IrNodeId valueExpression;','uint32 valueExpression;'),('enum-tag','XL6_IrEnumVariant','uint32 tag;','XL6_SymbolId tag;'),('enum-name','XL6_IrEnumPayload','char* name;','char** name;'),('enum-new','XL6_IrEnumPayload','XL6_IrEnumPayloadAbi abi;','XL6_IrEnumPayloadAbi abi; uint32 unaudited;')]:
   changed=headers.copy();pattern=re.compile(r'(typedef struct '+ty+r'\s*\{)(.*?)(\}\s*'+ty+r'\s*;)',re.S);match=pattern.search(changed[header]);assert match and match[2].count(old)==1,label
   changed[header]=changed[header][:match.start(2)]+match[2].replace(old,new,1)+changed[header][match.end(2):]
   try:generate(changed,out/('schema-'+label))
   except ValueError as e:record['schemaCases'].append(dict(label=label,rejected=True,reason=str(e)))
   else:raise AssertionError('Unaudited schema accepted: '+label)
  for path,h in record['inputs'].items():assert sha(path)==h,path
  record['status']='passed';print('PASS',args.family,'metadata:',len(cases)-2,'compiled defects,2controls,',len(record['schemaCases']),'schema mutations',flush=True)
 finally:
  if owned:assert sha(alias)==owned;alias.unlink()
  (out/'result.json').write_text(json.dumps(record,indent=2)+'\n')

if __name__=='__main__':main()
