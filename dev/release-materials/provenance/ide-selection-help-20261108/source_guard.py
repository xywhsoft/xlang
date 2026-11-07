import pathlib,json,hashlib,subprocess
root=pathlib.Path('D:/GIT/x-lang');run=root/'ide/build/Release/help-follow-20261108';m=json.loads((run/'objects/gui-candidate-build-inputs.json').read_text(encoding='utf-8-sig'));prefix=pathlib.Path('F:/xlang-validation/ide-help-follow-20261108/source');rev=(run/'source-revision.txt').read_text().strip();checked=[];exceptions=[]
for ps,h in m['inputs'].items():
 p=pathlib.Path(ps);data=p.read_bytes();assert hashlib.sha256(data).hexdigest()==h,ps
 try:rel=p.relative_to(prefix).as_posix()
 except ValueError:exceptions.append({'path':ps,'reason':'external runtime or build input','sha256':h});continue
 g=subprocess.run(['git','show',rev+':'+rel],capture_output=True)
 if g.returncode:exceptions.append({'path':rel,'reason':'not a Git blob','sha256':h});continue
 actual=data.replace(b'\r\n',b'\n');expected=g.stdout.replace(b'\r\n',b'\n')
 if rel=='ide/tools/build.ps1':
  expected=expected.replace(b"$objectRoot = Join-Path $ideRoot ('build\\' + $Configuration)",b"$objectRoot = 'D:/GIT/x-lang/ide/build/Release/help-follow-20261108/objects'")
 assert actual==expected,rel
 checked.append({'path':rel,'rawSha256':h,'normalizedSha256':hashlib.sha256(actual).hexdigest()})
out={'sourceRevision':rev,'exactInputsUnchanged':True,'checked':checked,'externalOrGeneratedInputs':exceptions}
(run/'committed-source-guard.json').write_text(json.dumps(out,indent=2)+'\n');print(len(checked),'verified;',exceptions)
