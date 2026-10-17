"""Verify that the actual TCC VFS consumes borrowed immutable read bytes."""
from pathlib import Path
import argparse,ctypes,hashlib,json,os,subprocess,uuid,shutil
p=argparse.ArgumentParser();p.add_argument('--compiler-root',required=True);p.add_argument('--work-root',required=True);a=p.parse_args()
compiler=Path(a.compiler_root).resolve();work=Path(a.work_root).resolve()/('tcc-input-transport-'+str(uuid.uuid4()));work.mkdir(parents=True)
shutil.copy2(__file__,work/'check_tcc_input_transport.py')
header=work/'changing.h';header.write_bytes(b'#define VALUE 1\n')
dll=ctypes.CDLL(str(compiler/'release/xlc.dll'));buffers={};reads=[];errors=[]
byte_pointer=ctypes.POINTER(ctypes.c_ubyte)
Reader=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_char_p,ctypes.POINTER(byte_pointer),ctypes.POINTER(ctypes.c_size_t))
@Reader
def reader(opaque,name,data,size):
 try:
  path=Path(name.decode('utf8')).resolve();reads.append(str(path))
  if path not in buffers:
   if not path.exists(): return 0
   raw=path.read_bytes();buffers[path]=(ctypes.create_string_buffer(raw),len(raw))
   if path==header: header.write_bytes(b'#define VALUE 2\n')
  buffer,n=buffers[path];data[0]=ctypes.cast(buffer,byte_pointer);size[0]=n;return 1
 except Exception as e: errors.append(str(e));return -1
for name,types,result in [('tcc_new',[],ctypes.c_void_p),('tcc_delete',[ctypes.c_void_p],None),('tcc_set_lib_path',[ctypes.c_void_p,ctypes.c_char_p],None),('tcc_set_output_type',[ctypes.c_void_p,ctypes.c_int],ctypes.c_int),('tcc_add_include_path',[ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),('tcc_compile_string',[ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),('tcc_output_file',[ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),('tcc_vfs_set_disk_reader',[Reader,ctypes.c_void_p],None)]:
 fn=getattr(dll,name);fn.argtypes=types;fn.restype=result
old=os.getcwd();os.chdir(work);state=None;checks=[]
def check(label,value):
 checks.append({'label':label,'passed':bool(value)})
 if not value: raise AssertionError(label)
try:
 dll.tcc_vfs_set_disk_reader(reader,None);state=dll.tcc_new();check('actual TCC state',bool(state))
 dll.tcc_set_lib_path(state,b'.');check('EXE output',dll.tcc_set_output_type(state,2)==0)
 check('include path',dll.tcc_add_include_path(state,str(work).encode())==0)
 text=b'#include "changing.h"\nint first(void) {return VALUE;}\n#undef VALUE\n#include "changing.h"\nint main(void) {return first()*10+VALUE;}\n'
 (work/'source.c').write_bytes(text)
 check('actual compile',dll.tcc_compile_string(state,text)==0)
 exe=work/'snapshot.exe';check('actual link',dll.tcc_output_file(state,str(exe).encode())==0)
 check('both header reads',reads.count(str(header))>=2)
 check('disk was changed during first read',header.read_bytes()==b'#define VALUE 2\n')
 check('no callback failures',not errors)
 check('compiled both immutable snapshots',subprocess.run([str(exe)],cwd=work,timeout=15).returncode==11)
except Exception as e: error=str(e)
else: error=None
finally:
 if state: dll.tcc_delete(state)
 dll.tcc_vfs_set_disk_reader(Reader(),None);os.chdir(old)
report={'status':'passed' if error is None else 'failed','scope':'tcc-disk-reader-transport','error':error,'checks':checks,'reads':reads,'callbackErrors':errors,'headerOriginalSha256':hashlib.sha256(b'#define VALUE 1\n').hexdigest(),'headerChangedSha256':hashlib.sha256(header.read_bytes()).hexdigest(),'compilerDllSha256':hashlib.sha256((compiler/'release/xlc.dll').read_bytes()).hexdigest()}
(work/'evidence.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print(work,report['status'],error or '');raise SystemExit(0 if error is None else 1)
