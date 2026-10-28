"""Execute the explicit supplemental compiler/rejection fixture inventory."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def accepted(row, code, text, target):
    if code!=row['ExpectedExit']:return False
    if row['ExpectedExit']:
        invalid_evidence = r'\[(lex|parse)\]|out of memory|cannot allocate memory|allocation failed|internal compiler error|assertion failed|segmentation fault|\u5185\u5b58\u4e0d\u8db3|\u5185\u5b58\u5206\u914d\u5931\u8d25'
        return not target.exists() and bool(re.search(row['DiagnosticRegex'],text)) and not re.search(invalid_evidence,text,re.I)
    return target.is_file() and target.stat().st_size>0


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--runtime-directory',type=Path)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--case')
    args=parser.parse_args()
    root,out=args.root.resolve(),args.out.resolve()
    manifest=Path(__file__).resolve().parents[1]/'test/cases/delivery_case_matrix.json'
    runtime=args.runtime_directory.resolve() if args.runtime_directory else root/'release'
    driver=runtime/('xl.exe' if os.name=='nt' else 'xl')
    data=json.loads(manifest.read_text(encoding='utf-8-sig'))
    cases=data['Cases'];seen=set();files={Path(__file__).resolve(),manifest,driver}
    files.update(runtime/name for name in (('xl.dll','xlc.dll') if os.name=='nt' else ('libxl.so','libxlc.so')))
    for row in cases:
        relative=Path(row['Source']);assert not relative.is_absolute() and '..' not in relative.parts
        assert row['Source'] not in seen;seen.add(row['Source'])
        path=(root/'test/cases'/relative).resolve()
        assert path.is_relative_to(root/'test/cases') and path.is_file()
        assert row['ExpectedExit'] in (0,1)
        if row['ExpectedExit']:assert row['DiagnosticRegex']
        files.add(path)
    if args.case:
        cases=[row for row in cases if row['Source']==args.case];assert len(cases)==1
    assert not out.exists();out.mkdir(parents=True)
    inputs={str(path):sha(path) for path in files}
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=[],Failures=[],Scope=data['Scope'])
    env=dict(os.environ,XLANG_PACKAGE_PATH=str(root/'release/extlibs')+os.pathsep+str(root/'test/fixtures'))
    def save():
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    try:
        for index,row in enumerate(cases):
            name=row['Source'].replace('/','__').removesuffix('.xl')
            target=out/(name+'.c');log=out/(name+'.log')
            command=[str(driver),'cgen',str(root/'test/cases'/row['Source']),'--emit',str(target)]
            try:
                result=subprocess.run(command,cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
                log.write_bytes(result.stdout)
                text=result.stdout.decode('utf-8-sig',errors='replace')
                ok=accepted(row,result.returncode,text,target)
                code=result.returncode
            except subprocess.TimeoutExpired as error:
                log.write_bytes(error.stdout or b'');ok=False;code='timeout'
            report['Rows'].append(dict(Source=row['Source'],Command=command,Exit=code,
                Expected=row['ExpectedExit'],Passed=ok,Log=str(log),Sha256=sha(log)))
            if not ok:report['Failures'].append(row['Source'])
            save();print(f'[{index+1}/{len(cases)}] {row["Source"]}: {"PASS" if ok else "FAIL"}',flush=True)
        report['InputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
        report['Complete']=report['InputsUnchanged'] and not report['Failures']
    finally:save()
    if not report['Complete']:raise SystemExit(1)


if __name__=='__main__':
    main()
