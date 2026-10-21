"""Measure actual mixed-program aggregate layout, then exercise the real IDE bridge."""
from pathlib import Path
import argparse
from datetime import datetime,timezone
import hashlib,json,os,subprocess,sys,uuid
parser=argparse.ArgumentParser(description=__doc__)
for name in ('compiler','server','fixture','legacy','output'):parser.add_argument('--'+name,required=True,type=Path)
options=parser.parse_args();compiler,server,fixture,legacy=(getattr(options,name).resolve() for name in ('compiler','server','fixture','legacy'))
root=options.output.absolute()/('c-native-aggregates-'+uuid.uuid4().hex);root.mkdir(parents=True,exist_ok=False)
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
protected={str(p):sha(p) for p in (compiler,compiler.with_name('xl.dll'),compiler.with_name('xlc.dll'),server,fixture,legacy)}
records=[];status='measuring';receipt=root/'evidence.json'
def publish(**kw):
 data={'status':status,'records':records,'protected':protected,'protectedUnchanged':all(sha(p)==d for p,d in protected.items()),'formalRelease':False,**kw}
 receipt.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def run(label,args):
 env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env.get('PATH','')
 result=subprocess.run(list(map(str,args)),cwd=root,env=env,capture_output=True,timeout=150)
 out=root/(label+'.stdout');err=root/(label+'.stderr');out.write_bytes(result.stdout);err.write_bytes(result.stderr)
 records.append({'label':label,'arguments':list(map(str,args)),'exitCode':result.returncode,'stdout':str(out),'stderr':str(err)})
 return result
definitions='''#include <stddef.h>
struct Plain { char lead; int integer; double value; void *pointer; char tail; };
struct LongDouble { char lead; long double value; char tail; };
union ScalarUnion { char character; long long integer; long double value; char bytes[13]; };
struct Nested { char lead; struct Plain inner; union ScalarUnion variant; char tail; };
struct Flexible { char lead; int count; char data[]; };
struct Array { char lead; short values[3]; char tail; };
struct Anon { char lead; union { int integer; double floating; }; struct { short a; char b; }; char tail; };
struct Aligned { char lead; long long value; char tail; } __attribute__((aligned(16)));
struct Packed { char lead; int integer; double value; char tail; } __attribute__((packed));
struct PackedAligned { char lead; int integer; double value; char tail; } __attribute__((packed,aligned(8)));
#pragma pack(push,1)
struct Pack1 { char lead; int integer; long double value; char tail; };
#pragma pack(push,2)
struct Pack2 { char lead; int integer; double value; char tail; };
#pragma pack(pop)
struct Pack1Restored { char lead; int integer; double value; char tail; };
#pragma pack(pop)
struct Restored { char lead; int integer; double value; char tail; };
struct BitsInt { unsigned int a:3; unsigned int b:7; unsigned int c:6; char tail; };
struct BitsMixed { unsigned char a:3; unsigned int b:10; unsigned short c:4; unsigned long d:6; char tail; };
struct BitsZero { unsigned int a:3; unsigned int :0; unsigned int b:3; char tail; };
struct BitsZeroFirst { unsigned int :0; char tail; };
struct BitsShortZero { char a; unsigned short :0; char tail; };
struct BitsWide { unsigned long long a:40; unsigned int b:6; char tail; };
struct BitsSigned { signed int a:3; signed char b:3; char tail; };
struct BitsPacked { unsigned char a:3; unsigned int b:10; unsigned short c:4; char tail; } __attribute__((packed));
#pragma pack(push,1)
struct BitsPack1 { unsigned char a:3; unsigned int b:10; unsigned short c:4; char tail; };
#pragma pack(pop)
'''
types=['Plain','LongDouble','ScalarUnion','Nested','Flexible','Array','Anon','Aligned','Packed','PackedAligned','Pack1','Pack2','Pack1Restored','Restored','BitsInt','BitsMixed','BitsZero','BitsZeroFirst','BitsShortZero','BitsWide','BitsSigned','BitsPacked','BitsPack1']
union={'ScalarUnion'};fields=[]
for name in types:
 tag=('union ' if name in union else 'struct ')+name
 fields.extend([(name+'_size','sizeof('+tag+')'),(name+'_align','_Alignof('+tag+')')])
 if name not in union and name!='Flexible':fields.append((name+'_tail','offsetof('+tag+', tail)'))
fields += [('Flexible_data','offsetof(struct Flexible, data)'),('Plain_integer','offsetof(struct Plain, integer)'),('Plain_value','offsetof(struct Plain, value)'),('Nested_inner','offsetof(struct Nested, inner)'),('Nested_variant','offsetof(struct Nested, variant)')]
fields += [('size_11','sizeof(long double)'),('__TINYC__','__TINYC__')]
xl=root/'main.xl';native=root/'native.c';project=root/'app.xlp';exe=root/'probe.exe'
xl.write_text('extern "aggregate_probe" func aggregateProbe() as c_int\nfunc main(...) as int\n return aggregateProbe()\nend\n',encoding='utf-8')
fmt='{'+','.join('\\"'+key+'\\":%lld' for key,_ in fields)+'}\\n'
native.write_text(definitions+'\nint printf(const char *, ...);\nint aggregate_probe(void) { printf("'+fmt+'",\n'+',\n'.join('(long long)('+expression+')' for _,expression in fields)+'); return 0; }\n',encoding='utf-8')
project.write_text(json.dumps({'schema':1,'name':'Native aggregate comparison','kind':'exe','entry':'main.xl','files':['main.xl','native.c'],'cSources':['native.c']}),encoding='utf-8')
for p in (xl,native,project):protected[str(p)]=sha(p)
measured={};diagnostics=[]
try:
 built=run('mixed-build',[compiler,'build',project,'-o',exe]);assert built.returncode==0,built.stderr[-1500:]
 executed=run('mixed-execute',[exe]);assert executed.returncode==0,executed.stderr[-1500:]
 measured=json.loads(executed.stdout);(root/'native-layout.json').write_text(json.dumps(measured,indent=2)+'\n',encoding='utf-8')
 assertions=definitions+'\n'+''.join('_Static_assert(('+expression+') == '+str(measured[key])+', "native_'+key+'");\n' for key,expression in fields)
 assertions+='\n_Static_assert(__builtin_types_compatible_p(__typeof__(xl_add(1,2)), double), "draft ABI");\ndouble answer(void) { return xl_add(1,2); }\n'
 check=root/'measured-aggregate-check.c';check.write_text(assertions,encoding='utf-8');protected[str(check)]=sha(check)
 query=run('compiler-environment',[compiler,'c-bridge',project,'--json','--c-environment']);assert query.returncode==0
 environment=json.loads(query.stdout)['cBridge']['nativeEnvironment'];profile=environment.get('recordLayout');assert profile=={'schema':1,'scope':'tcc-win64-bitfield-default','msBitfields':False},profile
 assert environment['cPreprocessorGraphComplete'] is False and environment['abiEquivalenceGuaranteed'] is False
 result=run('real-ide-bridge',[fixture,root/'native',server,compiler,legacy,check])
 for p in (root/'native').rglob('*diagnostics*.json'):
  obj=json.loads(p.read_text(encoding='utf-8'));rows=obj.get('params',{}).get('diagnostics',[])
  diagnostics.append({'path':str(p),'diagnostics':rows})
 assert result.returncode==0,(result.stderr[-1500:])
 prepared=list((root/'native').glob('bridge-native-*/prepared-commands.json'));assert len(prepared)==1
 arguments=json.loads(prepared[0].read_text(encoding='utf-8'))[0]['arguments'];assert '-mno-ms-bitfields' in arguments
 status='native-aggregate-and-ide-bridge-passed'
finally:
 publish(measured=measured,aggregateTypes=len(types),measurements=len(fields),diagnostics=diagnostics,
  recordLayout=locals().get('profile'),scope='Actual mixed-project sizes, alignments and member offsets for ordinary, nested, union, flexible, anonymous, aligned, packed and bitfield aggregates; not universal ABI equivalence')
assert all(sha(p)==d for p,d in protected.items())
print(json.dumps({'status':status,'measurements':len(fields),'aggregateTypes':len(types),'evidence':str(receipt)}))
