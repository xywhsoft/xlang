"""Derive a private DLL import name without changing SDK exports or metadata."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def runtime_basename(ide):
    text = (Path(ide) / 'src/platform/runtime_library.h').read_text(encoding='utf-8')
    names = re.findall(r'^#define XIDE_RUNTIME_DLL_BASENAME "([a-z][a-z0-9_]*\.dll)"$', text, re.M)
    if len(names) != 1 or names[0] == 'xge.dll':
        raise ValueError('Expected one private IDE runtime basename')
    return names[0]


def archive_members(data):
    """Read GNU ar members, including its long-name table; exclude indexes."""
    if not data.startswith(b'!<arch>\n'):
        raise ValueError('Expected a GNU import archive')
    at, names, result = 8, b'', {}
    while at < len(data):
        header = data[at:at + 60]
        if len(header) != 60 or header[58:] != b'`\n':
            raise ValueError('Invalid archive member')
        name = header[:16].decode('ascii').strip()
        length = int(header[48:58])
        body = data[at + 60:at + 60 + length]
        if len(body) != length:
            raise ValueError('Truncated archive')
        at += 60 + length + length % 2
        if name == '//':
            names = body
            continue
        if name in ('/', '/SYM64/'):
            continue
        if name.startswith('/'):
            offset = int(name[1:])
            end = names.find(b'/\n', offset)
            if end < 0:
                raise ValueError('Invalid long archive name')
            name = names[offset:end].decode('ascii')
        else:
            name = name.rstrip('/')
        if name in result or Path(name).name != name:
            raise ValueError('Duplicate or unsafe archive member')
        result[name] = body
    if at != len(data):
        raise ValueError('Invalid archive alignment')
    return result


def derive(ide, sdk, output, cc):
    ide, sdk, output, cc = map(Path, (ide, sdk, output, cc))
    name = runtime_basename(ide)
    source = sdk / 'xge.lib'
    before = source.read_bytes()
    members = archive_members(before)
    hits = [key for key, body in members.items() if b'xge.dll\0' in body]
    if len(hits) != 1:
        raise ValueError('Expected exactly one SDK DLL-name member')
    member = hits[0]
    output.parent.mkdir(parents=True, exist_ok=True)
    tools = {key: cc.with_name(key + '.exe') for key in ('ar', 'objcopy', 'dlltool')}
    for tool in tools.values():
        if not tool.is_file():
            raise ValueError('Missing compiler companion: ' + str(tool))
    with tempfile.TemporaryDirectory(prefix='ide-import-', dir=output.parent) as work:
        work = Path(work)
        obj, section, candidate = work / member, work / 'name.bin', work / 'xge.lib'
        obj.write_bytes(members[member])
        subprocess.run([str(tools['objcopy']), '--dump-section', '.idata$7=' + str(section), str(obj)], check=True)
        if section.read_bytes().rstrip(b'\0') != b'xge.dll':
            raise ValueError('Unexpected DLL-name section')
        section.write_bytes(name.encode('ascii') + b'\0')
        subprocess.run([str(tools['objcopy']), '--update-section', '.idata$7=' + str(section), str(obj)], check=True)
        candidate.write_bytes(before)
        subprocess.run([str(tools['ar']), 'rcs', str(candidate), str(obj)], check=True)
        after = candidate.read_bytes()
        changed = archive_members(after)
        if set(changed) != set(members) or [key for key in members if members[key] != changed[key]] != [member]:
            raise ValueError('Import symbols changed outside the DLL-name member')
        observed = subprocess.check_output([str(tools['dlltool']), '-I', str(candidate)], text=True).strip()
        if observed != name or source.read_bytes() != before:
            raise ValueError('Private import validation failed')
        # Publish only after validating every member. The SDK remains untouched.
        shutil.copyfile(candidate, output)
    sha = lambda data: hashlib.sha256(data).hexdigest()
    return dict(runtimeBasename=name, sdkImportSha256=sha(before), importSha256=sha(after),
                sdkDllSha256=sha((sdk / 'xge.dll').read_bytes()), changedMember=member,
                unchangedMembers=len(members) - 1, importLibrary=str(output.resolve()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ide', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cc', type=Path, required=True)
    args = parser.parse_args()
    result = derive(args.ide, args.sdk, args.output, args.cc)
    receipt = args.output.with_suffix('.json')
    receipt.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('Private IDE runtime:', result['runtimeBasename'])


if __name__ == '__main__':
    main()
