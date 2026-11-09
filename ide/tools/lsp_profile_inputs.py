"""Bind profiler compilation recipes to exact linked/archived object bytes."""
from pathlib import Path
import hashlib

HOST_UNITS=frozenset(('language_snapshot','semantic_definition','language_check'))


def digest(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


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
