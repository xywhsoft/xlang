"""Bind profiler compilation recipes to exact linked/archived object bytes."""
from pathlib import Path
from contextlib import contextmanager
import hashlib
import os
import shutil
import uuid

HOST_UNITS=frozenset(('language_snapshot','semantic_definition','language_check'))


def digest(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def verify_native_runs(build,root,receipt):
    """Bind external build-only acceptance to every selected native program.

    An explicit corrected-fixture override is permitted only at its recorded
    output path and link recipe. Its actual executable must match the separate
    acceptance digest; it never silently replaces the original build output.
    """
    if receipt.get('status')!='passed':raise ValueError('native acceptance did not pass')
    expected={};root=Path(root).resolve()
    inputs={Path(p).resolve():h for p,h in build['inputs'].items()}
    core=root/'xlang-analysis-core.a'
    for name,h in build['outputs'].items():
        path=Path(name)
        if path.suffix!='.exe' or path.name=='xlang-ls.exe':continue
        logical=path.stem
        if logical in expected:raise ValueError('duplicate native output: '+logical)
        path=path.resolve() if path.is_absolute() else (root/path).resolve()
        override=build.get('testOverrides',{}).get(logical)
        if override:
            command=override.get('compile',[]);link=override.get('link',[])
            if '-c' not in command or '-o' not in command or '-o' not in link:
                raise ValueError('native override has no compile/link recipe: '+logical)
            source=Path(command[command.index('-c')+1]).resolve()
            obj=Path(command[command.index('-o')+1]).resolve()
            path=Path(override['program']).resolve()
            if source not in inputs or digest(source)!=inputs[source] or Path(link[link.index('-o')+1]).resolve()!=path:
                raise ValueError('native override source/output mismatch: '+logical)
            objects={Path(p).resolve() for p in link if p.endswith('.o')}
            cores=[Path(p).resolve() for p in link if p.endswith('xlang-analysis-core.a')]
            if obj not in objects or len(cores)!=1 or digest(cores[0])!=digest(core):
                raise ValueError('native override uses another object/core: '+logical)
            h=digest(path)
        expected[logical]=(path,h)
    if not expected:raise ValueError('native acceptance has no fixture outputs')
    seen=set()
    for row in receipt.get('runs',[]):
        name=row.get('name')
        if name in seen or name not in expected:raise ValueError('duplicate/unknown native acceptance row: '+str(name))
        seen.add(name);path,h=expected[name]
        if (type(row.get('exit'))!=int or row['exit']!=0 or
                Path(row.get('program','')).resolve()!=path or row.get('programSha256')!=h or digest(path)!=h):
            raise ValueError('native acceptance identity/failure mismatch: '+name)
    if seen!=set(expected):raise ValueError('native acceptance is incomplete')


@contextmanager
def runtime_programs(programs,runtime=None):
    """Lease exclusively created executable aliases without installing images."""
    owned=[];selected={}
    try:
        if runtime is not None:
            runtime=Path(runtime).resolve(strict=True)
            if not runtime.is_dir():raise ValueError('runtime root is not a directory')
        for label,program in programs.items():
            source=Path(program['path']).resolve(strict=True);h=program['sha256']
            if digest(source)!=h:raise ValueError('profile program changed: '+str(source))
            target=source
            if runtime is not None:
                target=runtime/('.lsp-profile-'+uuid.uuid4().hex+'.exe')
                with target.open('xb') as out:
                    owned.append([target,None])
                    with source.open('rb') as stream:shutil.copyfileobj(stream,out)
                if os.name!='nt':shutil.copymode(source,target)
                owned[-1][1]=h
                if digest(target)!=h:raise ValueError('runtime alias copy mismatch')
            selected[label]={**program,'path':str(target),'sourcePath':str(source)}
        yield selected
    finally:
        problems=[]
        for path,h in reversed(owned):
            if not path.exists():
                problems.append('runtime alias disappeared: '+str(path));continue
            if h is not None and digest(path)!=h:
                problems.append('runtime alias changed; preserve it: '+str(path));continue
            path.unlink()
        if problems:raise ValueError('; '.join(problems))


def select_units(build,link,member_hashes,names):
    linked={Path(part).resolve() for part in link if part.endswith('.o')}
    recorded={}
    def admit(path,expected):
        path=Path(path).resolve()
        if path not in linked:return
        if path in recorded and recorded[path]!=expected:
            raise ValueError('Conflicting linked object identities: '+str(path))
        recorded[path]=expected
    for table in ('objectOutputs','linkedObjects'):
        for path,expected in build.get(table,{}).items():admit(path,expected)
    for row in build.get('dependencyAudit',[]):
        if row.get('actualObject') and row.get('actualObjectSha256'):
            admit(row['actualObject'],row['actualObjectSha256'])
    for path in linked:
        if path not in recorded:raise ValueError('Missing linked object identity: '+str(path))
        if not path.is_file() or digest(path)!=recorded[path]:
            raise ValueError('Linked object bytes changed: '+str(path))
    result={}
    for name in names:
        for command in reversed(build['commands']):
            if '-c' not in command or '-o' not in command:continue
            if Path(command[command.index('-c')+1]).stem!=name:continue
            output=Path(command[command.index('-o')+1]).resolve()
            if not output.is_file():continue
            if name in HOST_UNITS:
                if output not in linked:continue
            elif member_hashes.get(output.name)!=digest(output):continue
            result[name]=command.copy();break
        else:raise ValueError('No matching actual '+('linked' if name in HOST_UNITS else 'archived')+' object compile command: '+name)
    return result


def profile_inputs(build,root,output,names,run):
    links=[command for command in build['commands'] if '-c' not in command and '-MM' not in command and '-o' in command and
           Path(command[command.index('-o')+1]).name=='xlang-ls.exe']
    if not links:raise ValueError('Native language-server link command is missing')
    link=links[-1].copy();cores=[part for part in link if part.endswith('xlang-analysis-core.a')]
    if len(cores)!=1 or digest(cores[0])!=digest(root/'xlang-analysis-core.a'):
        raise ValueError('Linked core does not match the qualified archive')
    ar=str(Path(build['compiler']).with_name('ar.exe' if __import__('os').name=='nt' else 'ar'))
    result=run([ar,'t',cores[0]],'core-member-list')
    names_in_archive=result.stdout.decode('utf8').splitlines()
    if len(names_in_archive)!=len(set(names_in_archive)) or not names_in_archive or any(
        Path(name).name!=name or '/' in name or '\\' in name or not name.endswith('.o') for name in names_in_archive):
        raise ValueError('Qualified core must contain unique plain object members')
    members=output/'core-original-members';members.mkdir()
    if not members.resolve().is_relative_to(output.resolve()):raise ValueError('Core extraction directory escapes profiler output')
    # GNU ar stdout is in text mode on Windows and can insert CR before LF.
    # Extract binary files instead of hashing the output of `ar p`.
    run([ar,'x','--output='+str(members),cores[0]],'core-member-extract')
    hashes={name:digest(members/name) for name in names_in_archive}
    return link,cores[0],select_units(build,link,hashes,names),hashes
