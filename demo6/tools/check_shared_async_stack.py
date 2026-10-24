"""Bound generated async automatic storage using the Windows TCC consumer.

This is a native object/prologue measurement, not a substitute for actual
JIT/module/AOT execution. No stack reserve/commit flags are changed.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['root','build','language','compiler','objdump','out']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();assert os.name=='nt';root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build=json.loads(a.build.read_text(encoding='utf-8-sig'));language=json.loads(a.language.read_text())
    assert build['Complete'] and language['Complete'] and language['InputsUnchanged']
    assert language['SourceBuildSha256']==sha(a.build)
    cases=[q for q in language['Cases'] if 'shared_async_owners_' in q['Path'] or 'resident_tls_dial_direct' in q['Path']]
    files={Path(__file__).resolve(),a.build,a.language,a.compiler,a.objdump}
    sources=[]
    for case in cases:
        matches=[q for q in language['Rows'] if q['Name'].endswith('.emit') and q['Command'][1]==case['Path']];assert len(matches)==1
        source=Path(matches[0]['Command'][-1]);files.add(source);sources.append(source)
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in files},Rows=[],Products=[],Frames=[],Failures=[],
        NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,StackFlagsChanged=False,ShippingAccepted=False,FullTlsAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,command):
        command=list(map(str,command));log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name,code,flush=True);assert not code,str(log);return log.read_text(errors='replace')
    try:
        for index,source in enumerate(sources):
            obj=out/(str(index)+'.o')
            run(str(index)+'.compile',[a.compiler,'-std=c11','-I'+str(root),'-I'+str(root/'release'),
                '-I'+str(root/'release/include'),'-I'+str(Path(build['Driver']).parent/'include'),'-c',source,'-o',obj])
            symbols=re.findall(r'^static void (xlir_async_machine_program_\d+)\(',source.read_text(),re.M);assert symbols
            for symbol in symbols:
                assembly=run(str(index)+'.'+symbol,[a.objdump,'-d','--disassemble='+symbol,obj])
                match=re.search(r'<'+re.escape(symbol)+r'>:\n((?:[^\n]*\n){1,12})',assembly);assert match,(source,symbol)
                prologue=match[1]
                size=re.search(r'(?:mov\s+\$0x([0-9a-f]+),%eax|sub\s+\$0x([0-9a-f]+),%rsp)',prologue)
                assert size,(source,symbol,prologue)
                count=int(size[1] or size[2],16);assert count<=262144,(source,symbol,count)
                r['Frames'].append(dict(Source=str(source),Symbol=symbol,Bytes=count,Maximum=262144));save()
        r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items())
        r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json']
        r['Complete']=r['InputsUnchanged'];save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
