"""Real MSVCRT math through corrected package .def versus original DATA bug.

Loads an existing GCC-built libtcc provider and executes only JIT memory.
No TinyCC PE image is emitted or run; Native code is never recompiled.
"""
from pathlib import Path
import argparse,ctypes,hashlib,json,math,os,re,time


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','provider','distribution','new-definition','old-definition','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();assert os.name=='nt';out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    source=a.root.resolve()/'test/cases/tcc_static_import_math.c'
    paths=[Path(__file__).resolve(),source,a.provider.resolve(),a.new_definition.resolve(),a.old_definition.resolve()]
    report=dict(scope=__doc__,complete=False,passed=False,inputs={str(v):sha(v)for v in paths},rows=[],nativeCompilations=0,peEmitted=False,peExecuted=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    lib=ctypes.CDLL(str(a.provider.resolve()));ptr=ctypes.c_void_p
    lib.tcc_new.restype=ptr
    for name,args in [('tcc_set_lib_path',[ptr,ctypes.c_char_p]),('tcc_set_options',[ptr,ctypes.c_char_p]),('tcc_set_output_type',[ptr,ctypes.c_int]),('tcc_add_file',[ptr,ctypes.c_char_p]),('tcc_compile_string',[ptr,ctypes.c_char_p]),('tcc_relocate',[ptr]),('tcc_get_symbol',[ptr,ctypes.c_char_p]),('tcc_delete',[ptr])]:
        getattr(lib,name).argtypes=args
    lib.tcc_get_symbol.restype=ptr
    error_type=ctypes.CFUNCTYPE(None,ptr,ctypes.c_char_p);diagnostics=[]
    callback=error_type(lambda unused,text:diagnostics.append(text.decode('utf-8',errors='replace')))
    lib.tcc_set_error_func.argtypes=[ptr,ptr,error_type]
    cases=[('sin',0.5,0.0,0.479425538604203),('cos',0.5,0.0,0.8775825618903728),('sqrt',9.0,0.0,3.0),('pow',2.0,3.0,8.0),('fmod',5.5,2.0,1.5)]
    old=a.old_definition.read_text();new=a.new_definition.read_text()
    for name,_,_,_ in cases:
        assert re.search(r'^'+name+r'(?:=[^\s]+)? DATA$',old,re.M),'Old negative control must genuinely mark '+name+' DATA'
        assert re.search(r'^'+name+r'(?:=[^\s]+)?$',new,re.M),'Corrected function metadata missing: '+name
    for index,(name,first,second,expected)in enumerate(cases):
        for corrected,definition in [(False,a.old_definition),(True,a.new_definition)]:
            diagnostics.clear();state=lib.tcc_new();assert state
            row=dict(name=name,corrected=corrected,definition=str(definition.resolve()),inputs=[first,second],expected=expected)
            started=time.monotonic()
            try:
                lib.tcc_set_lib_path(state,str(a.distribution.resolve()).encode())
                lib.tcc_set_error_func(state,None,callback)
                flags='-nostdlib -ffunction-sections -fdata-sections -Wl,--gc-sections,--undefined=import_math_entry -DIMPORT_MATH_CASE='+str(index)
                assert lib.tcc_set_options(state,flags.encode())==0
                assert lib.tcc_set_output_type(state,1)==0
                row['flags']=flags;row['definitionAdded']=lib.tcc_add_file(state,str(definition.resolve()).encode());assert row['definitionAdded']==0
                row['compiled']=lib.tcc_compile_string(state,source.read_bytes());assert row['compiled']==0
                row['relocated']=lib.tcc_relocate(state)
                if not corrected:
                    message='\n'.join(diagnostics)
                    assert row['relocated']<0 and 'DATA import'in message and name in message,'Old metadata was not explicitly rejected as DATA: '+message
                    row['passed']=True;row['called']=False
                else:
                    assert row['relocated']==0,'Corrected relocation failed: '+'\n'.join(diagnostics)
                    address=lib.tcc_get_symbol(state,b'import_math_entry');assert address
                    entry=ctypes.CFUNCTYPE(ctypes.c_double,ctypes.c_double,ctypes.c_double)(address)
                    value=entry(first,second);row['actual']=value;row['called']=True
                    assert math.isfinite(value)and abs(value-expected)<=1e-12,(name,value,expected)
                    row['passed']=True
            except Exception as error:
                row.update(passed=False,error=str(error));raise
            finally:
                row['diagnostics']=list(diagnostics);row['seconds']=round(time.monotonic()-started,6)
                report['rows'].append(row);lib.tcc_delete(state);save()
    report.update(complete=True,passed=all(v['passed']for v in report['rows'])and all(sha(v)==h for v,h in report['inputs'].items()),checks=len(report['rows']),allInputsUnchanged=all(sha(v)==h for v,h in report['inputs'].items()));save();assert report['passed'];print(out/'results.json')
if __name__=='__main__':main()
