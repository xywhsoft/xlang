"""Check multi-generation header movement against the full native fresh oracle.

Compile an extended fixture with the intact normal build's exact flags, private
AST header and core archive. Do not replace the normal fixture or any runtime.
"""
from pathlib import Path
import argparse,json,re,subprocess
from build_lsp_lifecycle_profile import verify_build,sha


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();B=args.build.resolve(strict=True);O=args.output.resolve()
    R=Path(__file__).resolve().parents[2];normal=json.loads((B/'build-result.json').read_text());verify_build(normal,B)
    O.mkdir(parents=True,exist_ok=False)
    original=R/'ide/tests/analysis_native_borrow_test.c';text=original.read_text(encoding='utf8')
    start=text.index('static int geometryEdits(unsigned mode)');end=text.index('\nint main(int argc,char** argv)',start)
    body=text[start:end].replace('geometryEdits(unsigned mode)','geometryMoving(unsigned mode)',1)
    seam='for(unsigned v=0;v<3;++v){';assert body.count(seam)==1
    body=body.replace(seam,'for(unsigned generation=0;generation<3;++generation){\n        unsigned v=generation ? generation==1 ? 100 : 10000 : 0;',1)
    seam='geometry.reused==1 && !geometry.captured';assert body.count(seam)==1
    body=body.replace(seam,'geometry.captured==1 && !geometry.reused',1)
    text=text[:end]+'\n'+body+text[end:]
    seam='if(!expectEager && (!geometryEdits(0) || !geometryEdits(1) || !geometryEdits(2)))return 1;';assert text.count(seam)==1
    text=text.replace(seam,seam+'\n    if(!expectEager && !geometryMoving(0))return 1;',1)
    source=O/'moving.c';source.write_text(text,encoding='utf8',newline='\n')
    protected={**normal['inputs'],str(B/'build-result.json'):sha(B/'build-result.json'),str(Path(__file__)):sha(Path(__file__)),str(source):sha(source),str(original):sha(original)}
    record=dict(status='running',accepted=False,scope=__doc__,protected=protected,commands=[])

    def save():(O/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')

    def run(name,command):
        result=subprocess.run(command,cwd=O,capture_output=True,timeout=240)
        stdout,stderr=O/(name+'.stdout.log'),O/(name+'.stderr.log');stdout.write_bytes(result.stdout);stderr.write_bytes(result.stderr)
        record['commands'].append(dict(name=name,command=command,exit=result.returncode,stdout=str(stdout),stdoutSha256=sha(stdout),stderr=str(stderr),stderrSha256=sha(stderr)));save()
        assert not result.returncode,(name,result.returncode,result.stderr[-1800:])
        return result.stdout.decode('utf8')

    try:
        command=list(next(c for c in normal['commands'] if '-c' in c and c[-1]==str(B/'analysis_native_borrow_test.o')))
        command.insert(1,'-I'+str(R/'ide/tests'));command[command.index('-c')+1]=str(source);command[-1]=str(O/'moving.o');run('compile',command)
        command=list(next(c for c in normal['commands'] if c[-1]==str(B/'analysis_native_borrow_test.exe')))
        command=[str(O/'moving.o') if p==str(B/'analysis_native_borrow_test.o') else p for p in command];command[-1]=str(O/'moving.exe');run('link',command)
        output=run('run',[str(O/'moving.exe')]);rows=re.findall(r'geometry-mode0-v(100|10000) mapped=(\d+) asset-reused=(\d+) native=(\d+) retries=(\d+) copied=(\d+)',output)
        assert len(rows)==2 and all(int(r[1])>0 and int(r[2])==0 and int(r[3])==61 and int(r[4])==0 for r in rows),rows
        record['movingGenerations']=[dict(value=int(r[0]),mapped=int(r[1]),assetReused=int(r[2]),nativeReused=int(r[3]),retries=int(r[4]),copiedNodes=int(r[5])) for r in rows]
        record['nativeAssertions']=int(re.search(r'passed:(\d+) checks',output).group(1))
        verify_build(normal,B)
        for path,digest in protected.items():assert sha(path)==digest,path
        record['status']='passed'
    except BaseException as error:record.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=record['status'],assertions=record['nativeAssertions'],generations=record['movingGenerations'],report=str(O/'result.json'))))


if __name__=='__main__':main()
