"""Check literal collection through a complete CLI and its embedded compiler.

Real, benign C executables are emitted and run. Their live NUL/interior-pointer
behavior is checked; a deliberately unused string/float must disappear only
with GC. Parent executables and shared libraries use their own embedded TCC.
"""
from pathlib import Path
import argparse,ctypes,hashlib,json,os,struct,subprocess,sys,time
from full_compiler_qualification import verify_full_compiler


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('compiler','compiler-report','out'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expect-retained',action='store_true')
    a=p.parse_args();compiler,cohort,out=[v.resolve()for v in(a.compiler,a.compiler_report,a.out)]
    metadata=verify_full_compiler(cohort,compiler)
    out.mkdir(parents=True,exist_ok=False)
    sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    inputs={str(v):sha(v)for v in(Path(__file__),Path(__file__).with_name('full_compiler_qualification.py'),
                                  cohort,*[Path(row['path'])for row in metadata['products'].values()])}
    env={k:v for k,v in os.environ.items()if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
    report=dict(passed=False,scope=__doc__,fullCompilerBuild=True,shippingDelivery=False,
                beforeControl=a.expect_retained,inputs=inputs,rows=[],children=[])
    save=lambda:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    suffix='.exe'if os.name=='nt'else''
    shared='.dll'if os.name=='nt'else'.so'
    marker='UNREACHABLE_NESTED_LITERAL_PAYLOAD_'
    dead=marker+'q'*16000
    source=('static const char*unused_text(void){return "'+dead+'";}\n'
            'static double unused_number(void){return 0x1.56789abcdefp+20;}\n'
            'int main(void){const char*s="a\\0b";const char*p="prefix_tail"+7;'
            'return s[0]==97 && s[1]==0 && s[2]==98 && p[0]==116?0:1;}\n')

    def run(name,command):
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=out,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,
                                   seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert z.returncode==0,log
        return z.stdout+z.stderr

    for kind in ('jit','exe','shared'):
        paths={mode:out/(kind+'-'+mode+suffix)for mode in('all','gc')}
        lines=['import tcc','export "generate" func generate() as int']
        for mode,path in paths.items():
            options='-ffunction-sections -fdata-sections'+(' -Wl,--gc-sections'if mode=='gc'else'')
            lines.extend([' auto '+mode+'=TccContext(c_int(tcc.EXE))',
                ' assert int('+mode+'.setOptions('+json.dumps(options)+'))==0',
                ' assert int('+mode+'.compileString('+json.dumps(source)+'))==0',
                ' assert int('+mode+'.outputFile('+json.dumps(path.as_posix())+'))==0'])
        lines+=[' return 0','end']
        if kind!='shared':lines+=['assert generate()==0']
        parent=out/(kind+'.xl');parent.write_text('\n'.join(lines)+'\n',encoding='utf-8')
        if kind=='jit':run(kind+'-parent',[compiler,'run',parent])
        else:
            binary=out/(kind+'-parent'+(suffix if kind=='exe'else shared))
            run(kind+'-parent-build',[compiler,'build',parent,'-o',binary,*(['--shared']if kind=='shared'else[])])
            if kind=='exe':run(kind+'-parent-run',[binary])
            else:
                # A separate process releases the loaded image and reports errors normally.
                run(kind+'-parent-call',[sys.executable,'-c',
                    'import ctypes,sys; v=ctypes.CDLL(sys.argv[1]); v.generate.restype=ctypes.c_int64; sys.exit(v.generate())',binary])
        for mode,path in paths.items():
            run(kind+'-'+mode+'-child',[path])
            data=path.read_bytes();text=marker.encode()in data
            number=struct.pack('<d',float.fromhex('0x1.56789abcdefp+20'))in data
            expected=mode=='all'or a.expect_retained
            assert text==expected and number==expected,(kind,mode,text,number,expected)
            report['children'].append(dict(host=kind,mode=mode,path=str(path),sha256=sha(path),
                bytes=len(data),unusedStringPresent=text,unusedFloatPresent=number))
            save()
    report.update(passed=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()))
    assert report['inputsUnchanged'];save()


if __name__=='__main__':main()
