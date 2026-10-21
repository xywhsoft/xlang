"""Exercise real extension imports, native execution and the IDE C session."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def check(compiler, server, fixture, output):
    compiler, server, fixture = (Path(p).resolve(strict=True) for p in (compiler, server, fixture))
    root = Path(output).absolute() / ('c-extensions-' + uuid.uuid4().hex)
    root.mkdir(parents=True)
    private = root / 'toolchain'
    private.mkdir()
    protected = {str(p): sha(p) for p in (server, fixture, Path(__file__))}
    runtime = [compiler, compiler.with_name('xl.dll'), compiler.with_name('xlc.dll')]
    for directory in ('include', 'extlibs'):
        runtime.extend(p for p in (compiler.parent / directory).rglob('*') if p.is_file())
    for source in runtime:
        for parent in (source, *source.parents):
            assert not parent.is_symlink() and not parent.stat().st_file_attributes & 0x400, str(parent)
        relative = source.relative_to(compiler.parent)
        target = private / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        digest = sha(source)
        shutil.copy2(source, target)
        assert sha(target) == digest
        protected[str(source)] = digest
        protected[str(target)] = digest
    compiler = private / compiler.name
    records = []
    status = 'failed'
    receipt = root / 'evidence.json'
    env = dict(os.environ, PATH=str(private) + os.pathsep + os.environ.get('PATH', ''))

    def publish():
        receipt.write_text(json.dumps(dict(status=status, root=str(root), records=records,
            protected=protected, formalRelease=False, cPreprocessorGraphComplete=False,
            abiEquivalenceGuaranteed=False), indent=2) + '\n', encoding='utf8')

    def run(label, args, cwd):
        args = list(map(str, args))
        result = subprocess.run(args, cwd=cwd, env=env, capture_output=True, timeout=180)
        out, err = root / (label + '.stdout'), root / (label + '.stderr')
        out.write_bytes(result.stdout)
        err.write_bytes(result.stderr)
        records.append(dict(label=label, arguments=args, exitCode=result.returncode,
            stdout=str(out), stdoutSha256=sha(out), stderr=str(err), stderrSha256=sha(err),
            finishedUtc=datetime.now(timezone.utc).isoformat()))
        publish()
        assert result.returncode == 0, (label, result.returncode, result.stderr[-2500:])
        return result

    cases = [('crc32', 'xcrc32_hash_str', '__hashStr', 'long long', '3421780262',
        'long long value = xcrc32_hash_str("123456789",9);',
        'value == 3421780262LL', 'crc32.checksum("123456789") == 3421780262'),
        ('md5', 'xmd5_hash_str_hex', '__hexStr', 'const char *',
        hashlib.md5(b'123456789').hexdigest(),
        'const char* value = xmd5_hash_str_hex("123456789",9);',
        'value && !strcmp(value,"' + hashlib.md5(b'123456789').hexdigest() + '")',
        'md5.hashHex("123456789") == "' + hashlib.md5(b'123456789').hexdigest() + '"')]
    try:
        for name, symbol, selection, hover, expected, call, condition, assertion in cases:
            work = root / (name + ' 源码 工程😀')
            work.mkdir()
            entry, native, project, program = (work / n for n in ('main.xl', 'native.c', 'app.xlp', 'program.exe'))
            entry.write_text('import ' + name + '\nexport "xl_add" func add(c_int a, c_int b) as c_int\n return a+b\nend\n'
                'extern "extension_answer" func extensionAnswer() as c_int\nfunc main(...) as int\n assert ' + assertion +
                '\n return extensionAnswer()\nend\n', encoding='utf8')
            native.write_text('#include <stdio.h>\n#include <stdarg.h>\n#include <string.h>\n#include "' + name + '_native.h"\n'
                'static int format(char* out, size_t size, const char* fmt, ...) {\n va_list args; va_start(args,fmt);\n'
                ' int (*function)(char*,size_t,const char*,va_list) = vsnprintf;\n int result=function(out,size,fmt,args);\n'
                ' va_end(args); return result;\n}\nint extension_answer(void) {\n ' + call + '\n char buffer[64],other[64];\n'
                ' if(format(buffer,sizeof(buffer),"%s-%d","stdio",42)!=8 || strcmp(buffer,"stdio-42")) return 4;\n'
                ' if(snprintf(other,sizeof(other),"%s-%d","stdio",42)!=8 || strcmp(other,buffer)) return 5;\n'
                ' printf("native extension and stdio passed; sizeof(size_t)=%llu\\n",(unsigned long long)sizeof(size_t));\n'
                ' return (' + condition + ') && xl_add(19,23)==42 ? 0 : 3;\n}\n', encoding='utf8')
            project.write_text(json.dumps(dict(schema=1, name='Real ' + name + ' extension', kind='exe',
                entry='main.xl', files=['main.xl', 'native.c'], cSources=['native.c'])), encoding='utf8')
            # The fixture has a narrow C runtime argv. Keep its control paths
            # outside the Unicode project; project paths travel as UTF-8 JSON.
            config = root / (name + '-fixture.json')
            definition = private / 'extlibs' / name / 'src' / (name + '.xl')
            config.write_text(json.dumps(dict(ownedRoot=str(root), directory=str(work), entry=str(entry), native=str(native),
                guard=str(private / 'extlibs' / name / 'include' / (name + '_native.h')),
                probe=symbol + '(', prefix=symbol.rsplit('_', 1)[0] + '_', hover=hover, completion=symbol,
                definition=str(definition), selection=selection)), encoding='utf8')
            for p in (entry, native, project, config):
                protected[str(p)] = sha(p)
            run(name + '-mixed-build', [compiler, 'build', project, '-o', program], work)
            executed = run(name + '-mixed-execute', [program], work)
            assert b'native extension and stdio passed; sizeof(size_t)=8' in executed.stdout
            run(name + '-ide-context', [fixture, config, server, compiler, root / (name + '-native-evidence')], work)
            generated = run(name + '-compiler-query', [compiler, 'c-bridge', project, '--json', '--c-environment'], work)
            data = json.loads(generated.stdout)
            (work / 'compiler-query.json').write_bytes(generated.stdout)
            assert data['ok'] and data['cBridge']['complete']
            environment = data['cBridge']['nativeEnvironment']
            assert environment['cPreprocessorGraphComplete'] is False and environment['abiEquivalenceGuaranteed'] is False
            rows = [r for r in data['cBridge']['declarationMap']['records'] if r['cName'] == symbol]
            assert len(rows) == 1 and rows[0]['target']
            target = rows[0]['target']
            source = data['sourceReport']['files'][target['sourceId'] - 1]
            assert Path(source['path']).resolve() == definition.resolve() and source['textSha256'] == sha(definition)
            span = target['selectionRange']
            assert definition.read_bytes()[span['startOffset']:span['endOffset']].decode('utf8') == selection
            header = work / 'module.h'
            run(name + '-ordinary-header', [compiler, 'cgen', entry, '--emit-header', header], work)
            assert header.read_bytes() == data['cBridge']['declarations'].encode('utf8')
        assert all(sha(Path(p)) == digest for p, digest in protected.items())
        status = 'actual-extension-import-execution-and-ide-context-passed'
    finally:
        publish()
    print(json.dumps(dict(status=status, extensions=len(cases), actualCommands=len(records), evidence=str(receipt))))
    return receipt


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('compiler', 'server', 'fixture', 'output'):
        parser.add_argument('--' + option, type=Path, required=True)
    args = parser.parse_args()
    check(args.compiler, args.server, args.fixture, args.output)
