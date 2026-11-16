"""Derive non-node IR ownership from selected native headers.

Named C value records only. Explicit pointer policies fail closed;generated C
uses actual declarations/sizeof/alignment,never a parallel compiler layout.
"""
from pathlib import Path
import re
import hashlib
import json

STRINGS = {
 'XL6_IrAggregateField':'name',
 'XL6_IrCallArgument':'name',
 'XL6_IrCallTarget':'nativeHandlePolicyCName cName',
 'XL6_IrClassField':'nativeHandlePolicyCName name jsonPersistenceKeyLiteral xlonPersistenceKeyLiteral',
 'XL6_IrClassDispatchSlot':'name',
 'XL6_IrProtocolDispatchSlot':'name',
 'XL6_IrClassStaticField':'name',
 'XL6_IrCompiledPackage':'name version abiVersion',
 'XL6_IrDynamicCallArgument':'name',
 'XL6_IrDynamicCallSite':'memberName',
 'XL6_IrDynamicMethod':'name',
 'XL6_IrDynamicMethodParameter':'name defaultLiteral',
 'XL6_IrEnumPayload':'name',
 'XL6_IrEnumVariant':'name',
 'XL6_IrExport':'cName',
 'XL6_IrExternGlobal':'cName',
 'XL6_IrFunction':'debugName',
 'XL6_IrGlobalExport':'cName initializer',
 'XL6_IrInstruction':'immediateText',
 'XL6_IrPersistenceProjectField':'targetKey',
 'XL6_IrPersistenceRestoreClaim':'key',
 'XL6_IrPersistenceRestoreField':'sourceKey',
 'XL6_IrType':'name displayName foreignHeader foreignRecordSource foreignPolicy foreignScope foreignWatch foreignDomain',
}
VECTORS = {
 'XL6_IrCallTarget':('parameterCount','parameterTypes parameterAbis parameterLifetimes parameterPhysicalAbis parameterConstraintAbis parameterPhysicalFieldDeclarations asyncCaptureAbis asyncCaptureTraceAbis asyncCaptureDestroyAbis generatorCaptureAbis'),
 'XL6_IrClassDispatchSlot':('parameterCount','parameterTypes parameterAbis'),
 'XL6_IrProtocolDispatchSlot':('parameterCount','parameterTypes parameterAbis'),
 'XL6_IrClosureTarget':('callParameterCount','callParameterDefaultAbis callParameterDefaultFunctions'),
 'XL6_IrExport':('parameterCount','parameterTypes parameterAbis parameterValueAbis'),
 'XL6_IrFunction':('parameterCount','parameterCells'),
 'XL6_IrGenericSpecialization':('typeArgCount','typeArgs'),
 'XL6_IrType':('paramCount','params parameterModes parameterAbis'),
}
SPECIAL = {
 ('XL6_IrClosureTarget','callParameterNames'):('strings','callParameterCount',0),
 ('XL6_IrClosureTarget','callParameterDefaultLiterals'):('strings','callParameterCount',0),
 ('XL6_IrExport','parameterNames'):('strings','parameterCount',0),
 ('XL6_IrType','returns'):('vector','returnCount',0),
 ('XL6_IrCompiledPackage','interfaceData'):('vector','interfaceSize',0),
 ('XL6_IrCompiledPackage','entries'):('vector','entryCount',0),
 ('XL6_IrCompiledPackage','dependencies'):('vector','dependencyCount',0),
 ('XL6_IrInstruction','immediateData'):('vector','immediateSize',1),
}
SCALARS={'char','signed char','unsigned char','short','unsigned short','int','unsigned int',
 'unsigned','long','unsigned long','long long','unsigned long long','size_t','bool','_Bool',
 'float','double','uint8','uint16','uint32','uint64','int8','int16','int32','int64',
 'uint8_t','uint16_t','uint32_t','uint64_t','int8_t','int16_t','int32_t','int64_t'}
BORROWS={'XL6_IrPersistenceProjectField':'targetKey',
         'XL6_IrPersistenceRestoreField':'sourceKey',
         'XL6_IrPersistenceRestoreClaim':'key'}

def clean(text):return re.sub(r'/\*.*?\*/|//[^\n]*','',text,flags=re.S)

def fields(body):
 rows=[]
 for declaration in body.split(';'):
  if not declaration.strip():continue
  base=None
  for part in declaration.split(','):
   part=part.strip()
   extent=None
   if '['in part:
    match=re.fullmatch(r'(.+)\[([A-Za-z_0-9+*/()\s-]+)\]',part,re.S)
    if not match or not match[2].strip()or match[2].strip()=='0':raise ValueError('Unsupported native array: '+part)
    extent=match[2].strip();part=match[1].strip()
   tokens=re.findall(r'[A-Za-z_]\w*|\d+|\*|\[|\]',part)
   if re.sub(r'[A-Za-z_]\w*|\d+|\*|\[|\]|\s+','',part):raise ValueError('Unsupported native declaration: '+part)
   if '['in tokens or']'in tokens:raise ValueError('Unsupported nested native array: '+part)
   if not tokens or not re.fullmatch(r'[A-Za-z_]\w*',tokens[-1]):raise ValueError('Unnamed native field: '+part)
   name=tokens[-1];prefix=tokens[:-1];stars=prefix.count('*');words=[v for v in prefix if v!='*']
   if base is None:
    base=' '.join(words)
    if not base:raise ValueError('Missing native field type: '+part)
   elif words:raise ValueError('Ambiguous grouped native field: '+part)
   if any(v['name']==name for v in rows):raise ValueError('Duplicate native field: '+name)
   rows.append(dict(name=name,type=base,pointers=stars,extent=extent))
 return rows

class Schema:
 def __init__(self,headers):
  self.headers=headers;self.records={};self.bodies={};self.aliases={};self.enums=set();self.leaves=set()
  for path,raw in headers.items():
   text=clean(raw)
   for m in re.finditer(r'typedef\s+struct\s+(XL6_\w+)\s*\{([^{}]*)\}\s*\1\s*;',text,re.S):
    if m[1]in self.bodies and self.bodies[m[1]]!=m[2]:raise ValueError('Conflicting record: '+m[1])
    self.bodies[m[1]]=m[2]
   for m in re.finditer(r'typedef\s+([A-Za-z_]\w*(?:\s+[A-Za-z_]\w*)*)\s+(XL6_\w+)\s*;',text):
    self.aliases[m[2]]=m[1]
   self.enums.update(re.findall(r'typedef\s+enum(?:\s+\w+)?\s*\{[^{}]*\}\s*(\w+)\s*;',text,re.S))
  self.visited={}
 def record(self,name):
  if name not in self.records:self.records[name]=fields(self.bodies[name])
  return self.records[name]
 def resolve(self,name):
  seen=set()
  while name not in self.bodies and name in self.aliases:
   if name in seen:raise ValueError('Native alias cycle')
   seen.add(name);name=self.aliases[name]
  return name
 def value(self,name,active=()):
  original=name;name=self.resolve(name)
  if name in SCALARS or name in self.enums:self.leaves.add(original);return
  if name not in self.bodies:raise ValueError('Unaudited native value type: '+name)
  if name in active:raise ValueError('Recursive native value record: '+name)
  for row in self.record(name):
   if row['pointers']:raise ValueError('Hidden native value pointer: '+name+'.'+row['name'])
   self.value(row['type'],(*active,name))
 def pointer(self,name,row):
  field=row['name'];key=(name,field)
  if field in STRINGS.get(name,'').split():policy=('text',None,0)
  elif name in VECTORS and field in VECTORS[name][1].split():policy=('vector',VECTORS[name][0],0)
  elif key in SPECIAL:policy=SPECIAL[key]
  else:raise ValueError('Unowned native pointer: '+name+'.'+field)
  kind,count,extra=policy
  if row['extent']is not None:raise ValueError('Pointer field array needs audit: '+name+'.'+field)
  if kind=='text':
   if row['pointers']!=1 or row['type']not in ('char','const char'):raise ValueError('Native text ownership changed: '+name+'.'+field)
  elif kind=='strings':
   if row['pointers']!=2 or row['type']!='char':raise ValueError('Native string vector changed: '+name+'.'+field)
  else:
   if row['pointers']!=1:raise ValueError('Native vector indirection changed: '+name+'.'+field)
   self.value(row['type'])
  if count:
   fields_={r['name']:r for r in self.record(self.resolve(name))}
   if count not in fields_ or fields_[count]['type']!='size_t'or fields_[count]['pointers']or fields_[count]['extent']:raise ValueError('Native vector count changed: '+name+'.'+count)
   self.value(fields_[count]['type'])
  return dict(field=field,kind=kind,count=count,extra=extra,type=row['type'])
 def row(self,name):
  actual=self.resolve(name)
  if actual not in self.bodies:self.value(name);return []
  result=[]
  for row in self.record(actual):
   if row['pointers']:result.append(self.pointer(actual,row))
   else:self.value(row['type'])
  self.visited[name]=dict(actual=actual,fields=self.record(actual),pointers=result)
  return result
 def build(self):
  if 'XL6_IrModule'not in self.bodies:raise ValueError('Missing selected native module')
  root=self.record('XL6_IrModule');names={v['name']:v for v in root};tables=[]
  for field,ty in (('nodes','XL6_IrNode'),('children','XL6_IrNodeId')):
   if field not in names or names[field]['type']!=ty or names[field]['pointers']!=1 or names[field]['extent']:raise ValueError('Native node arena changed: '+field)
  for row in root:
   name=row['name']
   if name in ('nodes','children'):continue
   if name=='types':
    if row['type']!='XL6_IrTypePool'or row['pointers']or row['extent']:raise ValueError('Native type pool changed')
    pool=self.record('XL6_IrTypePool')
    expected=[('items','XL6_IrType',1),('count','size_t',0),('capacity','size_t',0),('slots','size_t',1),('slotCapacity','size_t',0)]
    if [(v['name'],v['type'],v['pointers'])for v in pool]!=expected or any(v['extent']for v in pool):raise ValueError('Native type-pool fields changed')
    tables.extend([dict(field='types.items',type='XL6_IrType',count='types.count',capacity='types.capacity',scalar=False),
                   dict(field='types.slots',type='size_t',count='types.slotCapacity',capacity=None,scalar=True)])
   elif row['pointers']:
    if row['pointers']!=1 or row['extent']is not None:raise ValueError('Native root pointer requires audit: '+name)
    candidates=[name+'Count',name[:-1]+'Count'if name.endswith('s')else'',name[:-3]+'yCount'if name.endswith('ies')else'']
    if name=='symbolNodes':count='symbolNodeCapacity';capacity=None
    else:
     counts=[v for v in candidates if v in names]
     if len(set(counts))!=1:raise ValueError('Native table count ambiguous: '+name)
     count=counts[0];capacity=count[:-5]+'Capacity'
     if capacity not in names:capacity=None
    self.value(names[count]['type'])
    if names[count]['type']!='size_t'or names[count]['pointers']or names[count]['extent']:raise ValueError('Native table count representation changed: '+name)
    if capacity:self.value(names[capacity]['type'])
    if capacity and(names[capacity]['type']!='size_t'or names[capacity]['pointers']or names[capacity]['extent']):raise ValueError('Native table capacity representation changed: '+name)
    tables.append(dict(field=name,type=row['type'],count=count,capacity=capacity,scalar=self.resolve(row['type'])not in self.bodies))
   else:self.value(row['type'])
  for row in tables:self.row(row['type'])
  self.tables=tables
  return dict(tables=tables,records=self.visited,valueLeaves=sorted(self.leaves),pointerFields=sum(len(v['pointers'])for v in self.visited.values()),
   normalizedHeaders={str(p):hashlib.sha256(t.encode()).hexdigest()for p,t in self.headers.items()})

def generate(headers,path):
 schema=Schema(headers);metadata=schema.build();lines=['/* Generated from selected native headers;do not edit. */']
 for leaf in sorted(schema.leaves):lines.append('XLS_TABLE_VALUE('+leaf+');')
 for name,record in schema.visited.items():
  lines+=['static int pack_'+name+'(table_writer* writer,const void* value){',
          'const '+name+'* src=value;'+name+'* dst=writer->base ? (void*)(writer->base+writer->valueOffset) : NULL;',
          'if(dst)memcpy(dst,src,sizeof(*dst));']
  for p in record['pointers']:
   f=p['field'];count=p['count']
   if p['kind']=='text':lines.append('TABLE_TEXT('+f+');')
   elif p['kind']=='strings':lines.append('TABLE_STRINGS('+f+','+count+');')
   else:lines.append('TABLE_VECTOR('+f+','+count+','+p['type']+','+str(p['extra'])+');')
  lines+=['return !writer->failed;','}']
 for i,t in enumerate(schema.tables):
  f=t['field'];cnt=t['count'];cap=t['capacity']or cnt
  lines+=['static const void* readTable'+str(i)+'(const XL6_IrModule* m,size_t* n,size_t* c){*n=m->'+cnt+';*c=m->'+cap+';return m->'+f+';}',
          'static void setTable'+str(i)+'(XL6_IrModule* m,void* p,size_t n){m->'+f+'=p;m->'+cnt+'=n;'+('m->'+cap+'=n;'if cap!=cnt else'')+'}']
 lines.append('enum {TABLE_CLASS_FIELDS='+str(next(i for i,t in enumerate(schema.tables)if t['field']=='classFields'))+'};')
 lines.append('static const table_descriptor tableDescriptors[]={')
 for i,t in enumerate(schema.tables):
  f=t['field'];ty=t['type']
  pack='NULL'if t['scalar']else'pack_'+ty
  alias={'XL6_IrPersistenceProjectField':1,'XL6_IrPersistenceRestoreField':2,'XL6_IrPersistenceRestoreClaim':3}.get(ty,0)
  lines.append('{"'+f+'",sizeof('+ty+'),_Alignof('+ty+'),'+('1'if t['scalar']else'0')+','+str(alias)+','+pack+',readTable'+str(i)+',setTable'+str(i)+'},')
 lines.append('};')
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True);path.write_text('\n'.join(lines)+'\n',encoding='utf8')
 return metadata

def generate_test(headers,path):
 """Generate broad ownership fixtures/books separately from the pack macros.

 These controlled records exercise memory ownership,not language validity.
 Real native IR verification and JSON remain independent semantic oracles.
 """
 schema=Schema(headers);metadata=schema.build();lines=['/* Selected native declarations,ownership fixtures only. */']
 for name,record in schema.visited.items():
  lines+=['static int book_'+name+'(xstrbuf* out,const void* value){',
          'const '+name+'* src=value;'+name+' normalized;memcpy(&normalized,src,sizeof(normalized));']
  for p in record['pointers']:lines.append('normalized.'+p['field']+'=NULL;')
  lines.append('if(!tableAppend(out,&normalized,sizeof(normalized)))return 0;')
  for p in record['pointers']:
   f=p['field'];count=p['count']
   if p['kind']=='text':lines.append('if(!tableAppendText(out,src->'+f+'))return 0;')
   elif p['kind']=='strings':
    lines+=['{unsigned char present=src->'+f+'!=NULL;if(!tableAppend(out,&present,1))return 0;',
            'if(present)for(size_t i=0;i<src->'+count+';++i)if(!tableAppendText(out,src->'+f+'[i]))return 0;}']
   else:
    lines+=['{unsigned char present=src->'+f+'!=NULL;size_t n=src->'+count+';',
            ('if(n==SIZE_MAX){return 0;}++n;'if p['extra']else''),
            'if(!tableAppend(out,&present,1))return 0;',
            'if(present && (n>SIZE_MAX/sizeof(*src->'+f+') || !tableAppend(out,src->'+f+',n*sizeof(*src->'+f+'))))return 0;}']
  lines+=['return 1;','}']
 lines+=['static int tableModuleBook(xstrbuf* out,const XL6_IrModule* m){',
         'XL6_IrModule normalized;memcpy(&normalized,m,sizeof(normalized));',
         'normalized.nodes=NULL;normalized.children=NULL;normalized.nodeCapacity=m->nodeCount;normalized.childCapacity=m->childCount;']
 for t in schema.tables:
  lines.append('normalized.'+t['field']+'=NULL;')
  if t['capacity']:lines.append('normalized.'+t['capacity']+'=m->'+t['count']+';')
 lines.append('if(!tableAppend(out,&normalized,sizeof(normalized)))return 0;')
 for t in schema.tables:
  f=t['field'];cnt=t['count'];ty=t['type']
  lines+=['{size_t n=m->'+cnt+';if(!tableAppend(out,&n,sizeof(n)) || (n && !m->'+f+'))return 0;']
  if t['scalar']:lines.append('if(n && !tableAppend(out,m->'+f+',n*sizeof('+ty+')))return 0;')
  else:lines.append('for(size_t j=0;j<n;++j)if(!book_'+ty+'(out,m->'+f+'+j))return 0;')
  lines.append('}')
 lines+=['return 1;}','static int tableUnitBook(xstrbuf* out,size_t table,const xls_ir_table_unit* unit){',
         'for(size_t row=0;row<xlsIrTableUnitRows(unit);++row){const void* value=xlsIrTableUnitRow(unit,row);if(!value)return 0;switch(table){']
 for i,t in enumerate(schema.tables):
  lines.append('case '+str(i)+':if(!'+('tableAppend(out,value,sizeof('+t['type']+'))'if t['scalar']else'book_'+t['type']+'(out,value)')+')return 0;break;')
 lines+=['default:return 0;}}return 1;}',
         'static int tableFixtureCreate(table_fixture* fixture){XL6_IrModule* m=&fixture->module;',
         '*m=(XL6_IrModule){.ready=1,.stage=XL6_IR_STAGE_TYPED,.nodeCount=1,.nodeCapacity=1,.root=1};',
         'm->nodes=tableFixtureAlloc(fixture,1,sizeof(*m->nodes));if(!m->nodes)return 0;m->nodes[0].kind=XL6_IR_NODE_MODULE;']
 for i,t in enumerate(schema.tables):
  f=t['field'];cnt=t['count'];ty=t['type'];n=3 if t['scalar']else 1
  lines+=['m->'+cnt+'='+str(n)+';', 'm->'+f+'=tableFixtureAlloc(fixture,'+str(n+3 if t['capacity']else n)+',sizeof('+ty+'));if(!m->'+f+')return 0;']
  if t['capacity']:lines.append('m->'+t['capacity']+'='+str(n+3)+';')
  if t['scalar']:lines.append('memset(m->'+f+',0x6b,'+str(n)+'*sizeof('+ty+'));');continue
  record=schema.visited[ty]
  lines+=['{'+ty+'* row=m->'+f+';memset(row,0x35,sizeof(*row));']
  for p in record['pointers']:lines.append('row->'+p['field']+'=NULL;')
  for p in record['pointers']:
   if p['count']:lines.append('row->'+p['count']+'=2;')
  for p in record['pointers']:
   field=p['field'];count=p['count'];label=t['field']+'.'+field
   if p['kind']=='text':lines.append('row->'+field+'=tableFixtureText(fixture,"'+label+' 中文😀");if(!row->'+field+')return 0;')
   elif p['kind']=='strings':
    lines+=['row->'+field+'=tableFixtureAlloc(fixture,row->'+count+',sizeof(*row->'+field+'));if(!row->'+field+')return 0;',
            'row->'+field+'[0]=tableFixtureText(fixture,"'+label+'[0] 中文😀");if(!row->'+field+'[0])return 0;']
   else:
    n_expr='row->'+count+(' + 1'if p['extra']else'')
    lines+=['row->'+field+'=tableFixtureAlloc(fixture,'+n_expr+',sizeof(*row->'+field+'));if(!row->'+field+')return 0;',
            'memset(row->'+field+',0x6b,('+n_expr+')*sizeof(*row->'+field+'));']
    if p['type']=='unsigned char':
     lines+=['row->'+field+'[0]=0xff;if(('+n_expr+')>1)row->'+field+'[1]=0;']
     if p['extra']:lines.append('row->'+field+'[('+n_expr+')-1]=0;')
  lines.append('}')
 lines+=['m->persistenceProjectFields[0].classField=0;m->persistenceProjectFields[0].targetKey=m->classFields[0].name;',
         'm->persistenceRestoreFields[0].classField=0;m->persistenceRestoreFields[0].sourceKey=m->classFields[0].jsonPersistenceKeyLiteral;',
         'm->persistenceRestoreClaims[0].classField=0;m->persistenceRestoreClaims[0].key=m->classFields[0].xlonPersistenceKeyLiteral;',
         'return 1;}']
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True);path.write_text('\n'.join(lines)+'\n',encoding='utf8')
 return metadata
