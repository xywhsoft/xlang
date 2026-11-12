"""Focused real TinyCC literal GC/relocation and compact final output tests.

The GCC-built fixture includes the frozen production provider. CPU JIT runs;
emitted PE images are inspected, never executed. No window or network work.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,struct,subprocess

parser=argparse.ArgumentParser(description=__doc__)
for name in ('root','out'):parser.add_argument('--'+name,type=Path,required=True)
parser.add_argument('--expect-failure',action='store_true')
a=parser.parse_args();root,out=a.root.resolve(),a.out.resolve()
out.mkdir(exist_ok=False,parents=True);source=out/'source'
shutil.copytree(root/'lib/tcc',source/'lib/tcc',ignore=shutil.ignore_patterns('__pycache__'))
for name in ('test/cases/tcc_literal_gc.c','tools/check_tcc_literal_gc.py'):
 path=source/name;path.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/name,path)
sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
inputs={p.relative_to(source).as_posix():sha(p)for p in source.rglob('*')if p.is_file()}
assert all(sha(root/name)==h for name,h in inputs.items())
cc=Path(shutil.which('gcc')).resolve();steps=[]
report=dict(passed=False,scope=__doc__,sourceInputs=inputs,compiler=str(cc),compilerSha256=sha(cc),
    baselineControl=a.expect_failure,productionQualified=False,shippingDelivery=False,steps=steps)
def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def run(name,command,expected=0):
 command=list(map(str,command));z=subprocess.run(command,cwd=out,capture_output=True,timeout=240)
 log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
 steps.append(dict(name=name,command=command,exitCode=z.returncode,logSha256=sha(log)))
 save();print(name,z.returncode,flush=True)
 assert z.returncode==expected,log
 return z.stdout+z.stderr
run('compiler-version',[cc,'--version'])
for opt in ('O0','O2'):
 folder=out/opt;folder.mkdir();binary=folder/('fixture.exe'if os.name=='nt'else'fixture')
 command=[cc,'-std=c11','-'+opt,'-DTCC_DISABLE_BUILTIN_VFS','-I'+str(source/'lib/tcc'),
    source/'test/cases/tcc_literal_gc.c','-o',binary]
 if not a.expect_failure:command+=['-DTCC_LITERAL_GC_QUALIFY']
 command+=([source/'lib/tcc/tcc_utf8_io.c','-lws2_32']if os.name=='nt'else['-ldl','-lpthread','-lm'])
 run(opt+'-build',command)
 text=run(opt+'-run',[binary,source/'lib/tcc',folder],1 if a.expect_failure else 0)
 match=re.search(rb'LITERAL_GC_CHECKS=(\d+) FAILURES=(\d+)',text);assert match
 assert (int(match[2])>0)==a.expect_failure
 if a.expect_failure:
  assert b'FAIL dead literal follows its function reachability'in text
  assert b'FAIL dead float follows its function reachability'in text
 steps[-1].update(checks=int(match[1]),failures=int(match[2]),images=[dict(path=str(p),sha256=sha(p),bytes=p.stat().st_size)
    for p in folder.glob('*.bin')])
 if not a.expect_failure:
  for obj in (folder/'literal-first.o',folder/'literal-second.o'):
   data=obj.read_bytes();assert data[:6]==b'\x7fELF\x02\x01'
   offset=struct.unpack_from('<Q',data,40)[0];width,count,names=struct.unpack_from('<HHH',data,58)
   headers=[struct.unpack_from('<IIQQQQIIQQ',data,offset+i*width)for i in range(count)]
   strings=data[headers[names][4]:headers[names][4]+headers[names][5]];literals=[]
   for header in headers:
    name=strings[header[0]:].split(b'\0',1)[0]
    if b'.__tcc_literal.'in name:
     assert header[2]&0x10 and not header[2]&0x20
     assert header[5]>0 and header[5]==header[9]
     assert header[5]%header[8]==0
     literals.append(name.decode())
   assert literals
   report.setdefault('literalObjects',[]).append(dict(path=str(obj),sha256=sha(obj),records=literals))
  if os.name!='nt':
   caller=folder/'caller.c';caller.write_text('extern int entry(void);int main(void){return entry()==59?0:1;}\n')
   binary=folder/'gnu-client'
   run(opt+'-gnu-link',[cc,'-'+opt,'-no-pie',caller,folder/'literal-first.o',folder/'literal-second.o',
       '-Wl,--gc-sections','-o',binary])
   run(opt+'-gnu-run',[binary])
report.update(passed=True,inputsUnchanged=all(sha(source/name)==h for name,h in inputs.items()),
    workspaceInputsUnchanged=all(sha(root/name)==h for name,h in inputs.items()))
assert report['inputsUnchanged']and report['workspaceInputsUnchanged'];save()
