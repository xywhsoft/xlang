"""Normal manifest/index snapshot tests. Keeps all private fixture repositories.
No network or shipping-product acceptance, and no mutation of the calling index.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys
from stdlib_manifest_inputs import root_rows,index_manifest_inputs,local_manifest_inputs

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();out=args.out.resolve();assert not out.exists();out.mkdir(parents=True)
    controls=Path(__file__).resolve().parent
    inputs={str(p):sha(p) for p in [Path(__file__).resolve(),controls/'stdlib_manifest_inputs.py',controls/'freeze_stdlib_source.py']}
    rows=[]
    text='''# ordinary source selection
file|one.h|%XL6_ROOT%\\release\\native\\one.h
dir|provider|%XL6_ROOT%/release/native|*.c;*.h
file|generated.c|%XL6_VFS_OUTPUT_DIR%/resources.c
file|lib.a|%XL6_EMBED_TCC_DIST%/lib/libtcc1.a
'''
    manifest='tools/tcc_vfs_manifest_fixture.txt'
    names={manifest,'release/native/one.h','release/native/nested/with space.c','release/native/not-selected.json',
        'test/fixtures/extra/helper.c','release/xl.dll','build/tmp/xrt.o'}
    selected,closure=index_manifest_inputs(names,lambda p:text)
    assert selected=={manifest,'release/native/one.h','release/native/nested/with space.c'}
    assert len(closure)==2
    rows.append(dict(Name='exact-indexed-root-and-recursive-filename-patterns',Passed=True))
    assert len(root_rows('FILE|v.h|relative/file.h'))==1
    assert root_rows('file|v.h|%XL6_XRT_OBJ%\nfile|a.h|%XL6_EMBED_TCC_DIST_X64%/include/a.h')==[]
    rows.append(dict(Name='relative-source-and-generated-input-separation',Passed=True))
    invalid=['file|v|%XL6_ROOT%/../outside.h','file|v|%XL6_ROOT%_bad/a.h',
        'file|v|%XL6_ROOT%/C:/outside.h','file|v|/absolute.h','file|v|%UNKNOWN%/a.h',
        'dir|v|.','unknown|v|relative','file|v|relative|*.h','file||relative','file|v|']
    for value in invalid:
        try:root_rows(value)
        except ValueError:pass
        else:raise AssertionError(value)
    rows.append(dict(Name='invalid-root-source-and-row-refusals',Passed=True,Refusals=len(invalid)))
    for missing in ['file|v|%XL6_ROOT%/missing.h','dir|v|%XL6_ROOT%/missing']:
        try:index_manifest_inputs(names,lambda p:missing)
        except ValueError:pass
        else:raise AssertionError(missing)
    rows.append(dict(Name='unindexed-manifest-dependencies-refused',Passed=True,Refusals=2))

    repository=out/'private-repository';project=repository/'demo6';project.mkdir(parents=True)
    files={'src/main.c':'/* INDEX SOURCE */\n','lib/decl.h':'/* INDEX DECL */\n',
        manifest:text,'release/native/one.h':'/* INDEX HEADER */\n',
        'release/native/nested/with space.c':'/* INDEX NATIVE */\n',
        'release/native/not-selected.json':'{}\n','test/fixtures/extra/helper.c':'/* FULL TEST CONTROLS */\n',
        'test/fixtures/extra/payload.bin':'fixture data\n','release/xl.dll':'NOT AN INPUT\n',
        'release/xl.exe':'NOT AN INPUT\n','build/tmp/xrt.o':'NOT AN INPUT\n'}
    for relative,value in files.items():
        path=project/relative;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(value.encode('utf8'))
    for name in ['freeze_stdlib_source.py','stdlib_manifest_inputs.py']:
        target=project/'tools'/name;shutil.copy2(controls/name,target);assert sha(target)==sha(controls/name)
    environment={k:v for k,v in os.environ.items() if k.upper() not in ['GIT_DIR','GIT_WORK_TREE','GIT_INDEX_FILE']}
    commands=[]
    def run(command,cwd=repository,failure=False):
        command=list(map(str,command));result=subprocess.run(command,cwd=cwd,env=environment,capture_output=True,timeout=90)
        commands.append(dict(Command=command,Exit=result.returncode,Stdout=result.stdout.decode(errors='replace'),
            Stderr=result.stderr.decode(errors='replace')))
        assert (result.returncode!=0 if failure else result.returncode==0),commands[-1]
        return result.stdout
    def git(*arguments):return run(['git','-c','core.autocrlf=false','-c','core.safecrlf=false',*arguments])
    git('init','--initial-branch=fixture');git('add','--','demo6')
    git('-c','user.name=Snapshot Fixture','-c','user.email=snapshot@example.invalid',
        '-c','commit.gpgsign=false','commit','-m','Private snapshot source fixture')
    tree=git('write-tree');index=git('ls-files','--stage','-z')
    # Deliberately incompatible working manifest and bytes. Neither may enter
    # a selected-index export, nor may an untracked native header enter it.
    (project/manifest).write_text('file|bad|%XL6_ROOT%/untracked.h\n',encoding='utf8')
    (project/'src/main.c').write_text('/* WORKING COPY, NOT INDEX */\n',encoding='utf8')
    (project/'release/native/one.h').write_text('/* WORKING HEADER, NOT INDEX */\n',encoding='utf8')
    (project/'release/native/untracked.h').write_text('/* NOT INDEXED */\n',encoding='utf8')
    exported=out/'index-export'
    run([sys.executable,project/'tools/freeze_stdlib_source.py','--out',exported])
    report=json.loads((exported/'freeze.json').read_text(encoding='utf8'));frozen=exported/'frozen/demo6'
    assert report['Complete'] and report['CompleteIndexedTestControls'] and len(report['ManifestSourceClosure'])==2
    for name in ['src/main.c','release/native/one.h','release/native/nested/with space.c',
        'test/fixtures/extra/helper.c','test/fixtures/extra/payload.bin']:
        assert (frozen/name).read_bytes()==files[name].encode('utf8')
    for name in ['release/xl.dll','release/xl.exe','build/tmp/xrt.o','release/native/untracked.h',
        'release/native/not-selected.json']:
        assert not (frozen/name).exists(),name
    assert git('write-tree')==tree and git('ls-files','--stage','-z')==index
    assert (project/'src/main.c').read_text()=='/* WORKING COPY, NOT INDEX */\n'
    rows.append(dict(Name='actual-git-index-export-preserves-dirty-working-copy-and-index',Passed=True))
    local=local_manifest_inputs(frozen)
    assert {p.relative_to(frozen).as_posix() for p in local}==selected
    rows.append(dict(Name='ordinary-builder-local-source-closure-agrees',Passed=True))
    # Create a symlink-mode INDEX entry without creating a Windows symlink or
    # following any filesystem link. The exporter must reject before checkout.
    oid=git('hash-object','-w','demo6/src/main.c').decode().strip()
    git('update-index','--add','--cacheinfo','120000',oid,'demo6/src/link.h')
    nonregular_index=git('ls-files','--stage','-z');refused=out/'symlink-export'
    run([sys.executable,project/'tools/freeze_stdlib_source.py','--out',refused],failure=True)
    assert 'Only regular source files may be exported' in commands[-1]['Stderr']
    assert not refused.exists() and git('ls-files','--stage','-z')==nonregular_index
    rows.append(dict(Name='nonregular-index-source-refused-before-export',Passed=True,Refusals=1))
    for entry in report['Files']:
        assert sha(exported/'frozen'/entry['Path'])==entry['FrozenSha256']
    assert all(sha(p)==h for p,h in inputs.items())
    (out/'results.json').write_text(json.dumps(dict(Complete=True,InputsUnchanged=True,Inputs=inputs,
        Rows=rows,Commands=commands,PrivateFixtureRetained=True,NetworkTraffic=False,ShippingAccepted=False),indent=2)+'\n')
    print(len(rows),'snapshot source gates; 13 expected refusals; actual index and dirty files preserved',flush=True)

if __name__=='__main__':main()
