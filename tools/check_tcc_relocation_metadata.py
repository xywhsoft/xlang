"""Frozen CPU relocation-metadata identity, output and actual-object admission gate.

The shared library is loaded by the normal OS loader and returns 42. No window,
device or network work is performed. Captured project objects are admitted only;
full nested compiler execution remains a separate complete-build acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,struct,subprocess,sys,time


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--provider-root',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--capture',type=Path)
    p.add_argument('--expect-failure',action='store_true')
    a=p.parse_args();root,out=a.provider_root.resolve(),a.out.resolve()
    case_root=Path(__file__).resolve().parents[1]
    out.mkdir(parents=True,exist_ok=False);source=out/'source'
    shutil.copytree(root/'lib/tcc',source/'lib/tcc',ignore=shutil.ignore_patterns('__pycache__'))
    for name in ('test/cases/tcc_relocation_metadata.c','tools/check_tcc_relocation_metadata.py'):
        q=source/name;q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(case_root/name,q)
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs={str(root/name.relative_to(source)):sha(name)for name in(source/'lib/tcc').rglob('*')if name.is_file()}
    inputs.update({str(case_root/name):sha(source/name)for name in('test/cases/tcc_relocation_metadata.c','tools/check_tcc_relocation_metadata.py')})
    frozen={v.relative_to(source).as_posix():sha(v)for v in source.rglob('*')if v.is_file()}
    windows=os.name=='nt';cc=Path(shutil.which('gcc')).resolve();steps=[]
    report=dict(passed=False,scope=__doc__,beforeControl=a.expect_failure,shippingDelivery=False,
                fullCompilerQualified=False,compiler=dict(path=str(cc),sha256=sha(cc)),inputs=inputs,steps=steps)
    save=lambda:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,expected=0):
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=out,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        steps.append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert z.returncode==expected,log
        return z.stdout+z.stderr

    def metadata_object(path,second=False,allocated=False,writable=False):
        data=bytearray(b'\0'*64);strings=bytearray(b'\0');names=bytearray(b'\0')
        def word(table,s):
            n=len(table);table.extend(s.encode()+b'\0');return n
        def payload(value,align=1):
            while len(data)%align:data.append(0)
            n=len(data);data.extend(value);return n
        syms=[b'\0'*24,struct.pack('<IBBHQQ',word(strings,'second'if second else'first'),0x11,0,1,0,8),
              struct.pack('<IBBHQQ',word(strings,'probe_absolute'),0x10,0,0 if second else 0xfff1,0 if second else 0x1234,0)]
        if second:syms.append(struct.pack('<IBBHQQ',word(strings,'relocation_anchor'),0x11,0,2,0,24))
        headers=[[0]*10,
            [word(names,'.data'),1,3,0,payload(b'\0'*8,8),8,0,0,8,0],
            [word(names,'.rela.data'),4,2 if allocated else 1 if writable else 0,0,payload(struct.pack('<QQq',0,(2<<32)|1,0),8),24,3,1,8,24],
            [word(names,'.symtab'),2,0,0,payload(b''.join(syms),8),len(syms)*24,4,1,8,24],
            [word(names,'.strtab'),3,0,0,payload(strings),len(strings),0,0,1,0]]
        name=word(names,'.shstrtab');headers.append([name,3,0,0,payload(names),len(names),0,0,1,0])
        while len(data)%8:data.append(0)
        offset=len(data)
        for h in headers:data.extend(struct.pack('<IIQQQQIIQQ',*h))
        ident=b'\x7fELF'+bytes((2,1,1))+b'\0'*9
        data[:64]=struct.pack('<16sHHIQQQIHHHHHH',ident,1,62,1,0,0,offset,0,64,0,0,64,len(headers),5)
        path.write_bytes(data)

    capture=[]
    if a.capture:
        original=a.capture.resolve();c=json.loads(original.read_text());paths=[Path(v['path'])for v in c['objects']]
        assert all(sha(v['path'])==v['sha256']for v in c['objects'])
        # This gate admits the actual unique object population, not the full
        # language caller's load protocol. Preserve repeated calls separately;
        # blindly loading an object twice is a multiple-definition error.
        repeated=[];seen=set();selected=[]
        for v in c['objects']:
            identity=(str(Path(v['path']).resolve()),v['sha256'])
            if identity in seen:repeated.append(v)
            else:seen.add(identity);selected.append(Path(v['path']))
        paths=selected
        f=out/'captured.list';f.write_text(str(c['seed'])+'\n'+'\n'.join(v.as_posix()for v in paths)+'\n')
        capture=[f];inputs[str(original)]=sha(original);inputs.update({str(v['path']):v['sha256']for v in c['objects']})
        report['capture']=dict(path=str(original),sha256=sha(original),objects=len(paths),originalCalls=len(c['objects']),repeatedCallsNotReplayed=repeated,scope='Unique actual object population admission only; not caller-protocol execution')
    run('compiler-version',[cc,'--version'])
    for opt in ('O0','O2'):
        folder=out/opt;folder.mkdir()
        metadata_object(folder/'meta-first.o');metadata_object(folder/'meta-second.o',second=True)
        metadata_object(folder/'meta-allocated.o',allocated=True)
        metadata_object(folder/'meta-write.o',writable=True)
        binary=folder/('fixture.exe'if windows else'fixture')
        command=[cc,'-std=c11','-'+opt,'-DTCC_DISABLE_BUILTIN_VFS','-I'+str(source/'lib/tcc'),source/'test/cases/tcc_relocation_metadata.c','-o',binary]
        command+=([source/'lib/tcc/tcc_utf8_io.c','-lws2_32']if windows else['-ldl','-lpthread','-lm'])
        run(opt+'-build',command)
        text=run(opt+'-run',[binary,source/'lib/tcc',folder,*capture],1 if a.expect_failure else 0)
        result=re.search(rb'RELOCATION_METADATA_CHECKS=(\d+) FAILURES=(\d+)',text);assert result
        steps[-1].update(checks=int(result[1]),failures=int(result[2]))
        if a.expect_failure:
            labels=[line.split(b': ',1)[0]for line in text.splitlines()if line.startswith(b'FAIL ')]
            allowed={b'FAIL ordinary source relocations do not consume numbered slots',
                     b'FAIL unaddressed imported relocation records are private',b'FAIL first ordinary metadata is private',
                     b'FAIL entire unique captured object population admitted'}
            assert labels and set(labels)<=allowed and len(labels)==int(result[2]),labels
        else:assert int(result[2])==0
        shared=folder/('metadata.dll'if windows else'metadata.so')
        run(opt+'-shared-load',[sys.executable,'-c','import ctypes,sys;v=ctypes.CDLL(sys.argv[1]);v.metadata_answer.restype=ctypes.c_int;sys.exit(0 if v.metadata_answer()==42 else 1)',shared])
        if not windows:
            caller=folder/'caller.c';caller.write_text('int host_value(int x){return x*4;}extern int entry(void);int main(void){return entry()==42?0:1;}\n')
            client=folder/'gnu-client'
            run(opt+'-gnu-link',[cc,'-no-pie',caller,folder/'first.o',folder/'second.o','-Wl,--gc-sections','-o',client])
            run(opt+'-gnu-run',[client])
    report.update(passed=True,checks=sum(v.get('checks',0)for v in steps),frozenInputsUnchanged=all(sha(source/n)==h for n,h in frozen.items()),
                  sourceInputsUnchanged=all(sha(n)==h for n,h in inputs.items()))
    assert report['frozenInputsUnchanged']and report['sourceInputsUnchanged'];save()


if __name__=='__main__':main()
