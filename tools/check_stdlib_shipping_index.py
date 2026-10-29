"""Read-only final index/product provenance for a shipping batch.
Compiled inputs must equal selected index blobs (LF/CRLF normalized only).
Documentation and test-runner-only differences are recorded, never hidden.
Actual product/test/build inputs are rehashed. No release promotion/full53 claim.
"""
from pathlib import Path
import argparse, hashlib, json, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def normal(data): return data.replace(b'\r\n',b'\n')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--report',type=Path,action='append',required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--test-only-change',action='append',default=[],
        help='Explicit repo-relative test/check-runner difference; requires current report hash proof')
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve(); assert not out.exists()
    project=Path(__file__).resolve().parents[2]
    index=subprocess.check_output(['git','write-tree'],cwd=project,text=True).strip()
    names=[name.decode() for name in subprocess.check_output(['git','ls-files','-z','--',
        'demo6/src','demo6/lib','demo6/tools','demo6/res','demo6/release/include',
        'demo6/include','demo6/release/examples/*.c','demo6/test/cases','demo6/test/support',
        'demo6/test/fixtures/native_stack_batch_rejected',
        'demo6/build.bat','demo6/build_dll.bat','demo6/build_linux.sh'],cwd=project).split(b'\0') if name]
    objects=subprocess.check_output(['git','cat-file','--batch'],cwd=project,
        input=b''.join(b':'+name.encode()+b'\n' for name in names))
    blobs={}; offset=0
    for name in names:
        boundary=objects.index(b'\n',offset); header=objects[offset:boundary].split()
        assert len(header)==3 and header[1]==b'blob'; count=int(header[2]); offset=boundary+1
        blobs[name]=objects[offset:offset+count]; offset+=count; assert objects[offset:offset+1]==b'\n'; offset+=1
    assert offset==len(objects)
    # The compiler snapshot may precede new test runners. No compiled source,
    # resource, header, manifest, generator or product-build script may differ.
    optional={'demo6/tools/check_stdlib_thread_shipping.py','demo6/tools/check_stdlib_shipping_index.py',
        'demo6/tools/check_stdlib_runtime_header_vfs.py'}
    test_only=set(args.test_only_change)
    assert all(name in blobs and (name.startswith(('demo6/test/','demo6/tools/check_'))
        or name=='demo6/tools/stdlib_fixture_inputs.py')
        and '..' not in Path(name).parts for name in test_only)
    comparisons=[]; differences=[]
    for name,blob in blobs.items():
        path=root/Path(name).relative_to('demo6')
        if not path.exists():
            assert name in optional or name in test_only or name.startswith(('demo6/test/','demo6/tools/check_')),name
            differences.append(name+': absent test-only input'); continue
        if normal(path.read_bytes())!=normal(blob):
            # The tool index is prose, not a compiler source/build input. Keep
            # its late documentation edit visible just like docs/ (which is
            # outside the compiled-input population), never exempt scripts.
            documentation=name=='demo6/tools/README.md'
            assert documentation or name in optional or name in test_only,name
            differences.append(name+(': documentation-only' if documentation else ''))
        comparisons.append({'Path':name,'Sha256':sha(path)})
    reports=[]; rechecked=0; current=set()
    for report_path in args.report:
        report_path=report_path.resolve(); report=json.loads(report_path.read_text(encoding='utf-8-sig'))
        assert report['Complete'] and report['InputsUnchanged'],report_path
        assert not report.get('Errors',[]) and not report.get('Failures',[]),report_path
        for row in report['Inputs']+report.get('Products',[])+report.get('Objects',[]):
            # PowerShell emits uppercase hexadecimal and Python lowercase.
            # They encode the same digest; never normalize file contents here.
            path=Path(row['Path']); assert sha(path)==row['Sha256'].lower(),path; rechecked+=1
            if path.is_relative_to(project):
                name=path.relative_to(project).as_posix()
                if name in blobs: assert normal(path.read_bytes())==normal(blobs[name]),name; current.add(name)
        reports.append({'Path':str(report_path),'Sha256':sha(report_path),'Rows':len(report.get('Rows',[]))})
    # An old frozen fixture is never silently counted as the current test.
    # Every explicitly permitted test difference must appear byte-identical
    # to the final index in a completed, rehashed acceptance report.
    assert test_only<=current,sorted(test_only-current)
    assert subprocess.check_output(['git','write-tree'],cwd=project,text=True).strip()==index
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps({'Scope':__doc__,'Complete':True,'IndexTree':index,
        'ComparedIndexInputs':len(comparisons),'Comparisons':comparisons,
        'TestOnlyDifferences':differences,'Reports':reports,'RecheckedInputs':rechecked,
        'CurrentTestInputs':sorted(current),'ShippingLibraryAcceptance':False,'Full53Acceptance':False},
        indent=2)+'\n',encoding='utf-8')
    print('Shipping index: '+str(len(comparisons))+' snapshot inputs and '+str(rechecked)+' report input hashes verified')
if __name__=='__main__': main()
