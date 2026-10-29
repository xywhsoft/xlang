"""Native ranged-AST API/CLI gate; checks real bytes, not diagnostic-point guesses."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def position(data, offset):
    text = data[:offset].decode('utf-8')
    line, column, i = 1, 1, 0
    while i < len(text):
        ch = text[i]
        if ch == '\r':
            if i + 1 < len(text) and text[i + 1] == '\n':
                i += 1
            line, column = line + 1, 1
        elif ch == '\n':
            line, column = line + 1, 1
        else:
            column += 1
        i += 1
    return line, column


def nodes(root):
    yield root
    for child in root.get('children', []):
        yield from nodes(child)


def legacy(root):
    return {key: ([legacy(x) for x in value] if key == 'children' else value)
            for key, value in root.items() if key not in ('source', 'range', 'tokenRange')}


def validate(value, raw, source):
    assert value['schema'] == 1 and value['scope'] == 'entry-syntax'
    assert value['offsetEncoding'] == 'utf-8-after-bom'
    assert value['columnEncoding'] == 'unicode-scalar-1-based'
    assert value['rangeConvention'] == 'half-open'
    data = raw.removeprefix(b'\xef\xbb\xbf')
    count = 0
    for node in nodes(value['ast']):
        count += 1
        assert isinstance(node['source'], str) and Path(node['source']).name == source
        for key in ('range', 'tokenRange'):
            span = node[key]
            if span is None:
                continue
            start, end = span['startOffset'], span['endOffset']
            assert isinstance(start, int) and 0 <= start <= end <= len(data), (node, len(data))
            assert position(data, start) == (span['startLine'], span['startColumn']), (key, node)
            assert position(data, end) == (span['endLine'], span['endColumn']), (key, node)
            data[start:end].decode('utf-8')
        if node['range'] and node['tokenRange']:
            a, b = node['range'], node['tokenRange']
            assert a['startOffset'] <= b['startOffset'] <= b['endOffset'] <= a['endOffset'], node
    return count


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--baseline-compiler', type=Path)
    parser.add_argument('--gcc', default='gcc')
    args = parser.parse_args()
    if os.name != 'nt':
        raise RuntimeError('This native DLL gate requires Windows')
    root = args.root.resolve()
    compiler = (args.compiler or root / 'release/xl.exe').resolve()
    gcc = shutil.which(args.gcc)
    if not gcc:
        raise RuntimeError('MinGW GCC is required')
    work = root / 'build/test' / ('source-ranges-' + str(uuid.uuid4()))
    inputs = work / '源码 Ω'
    inputs.mkdir(parents=True)
    core = [compiler, compiler.with_name('xlc.dll'), compiler.with_name('xl.dll')]
    protected = {str(path): digest(path) for path in core}
    results, checked_nodes = [], 0
    environment = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ.get('PATH', ''))
    started = datetime.datetime.now(datetime.timezone.utc).isoformat()
    status = 'failed'

    def run(name, command, expected=0):
        completed = subprocess.run([str(x) for x in command], cwd=inputs, env=environment,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90)
        (work / (name + '.stdout')).write_bytes(completed.stdout)
        (work / (name + '.stderr')).write_bytes(completed.stderr)
        results.append({'name': name, 'exitCode': completed.returncode,
                        'stdoutBytes': len(completed.stdout), 'stderrBytes': len(completed.stderr)})
        if completed.returncode != expected:
            raise RuntimeError(f'{name}: exit {completed.returncode}, expected {expected}\n' +
                               completed.stderr.decode('utf-8', errors='replace'))
        return completed

    def parse(name, source, raw, extra=()):
        nonlocal checked_nodes
        path = inputs / source
        path.write_bytes(raw)
        protected[str(path)] = digest(path)
        value = json.loads(run(name, [compiler, 'parse', path, '--json', '--source-ranges', *extra]).stdout)
        checked_nodes += validate(value, raw, source)
        old = json.loads(run(name + '-legacy', [compiler, 'parse', path, '--json', *extra]).stdout)
        assert old == legacy(value['ast'])
        if args.baseline_compiler:
            baseline = json.loads(run(name + '-baseline', [args.baseline_compiler, 'parse', path, '--json']).stdout)
            assert baseline == old, name
        return value

    try:
        source = ('func 计算(int 值) as int\r\n'
                  '    string 提示 = f"前😀 {计算(值 + 2)} 后"\r\n'
                  '    return (值 + 1)\r\nend')
        raw = b'\xef\xbb\xbf' + source.encode()
        value = parse('unicode-bom-crlf-no-final-newline', 'unicode.xl', raw)
        data = raw[3:]
        function = value['ast']['children'][0]
        span = function['range']
        assert data[span['startOffset']:span['endOffset']].decode() == source
        name = function['children'][0]
        span = name['tokenRange']
        assert data[span['startOffset']:span['endOffset']].decode() == '计算'
        expressions = [n for n in nodes(function) if n['kind'] == 'binary']
        slices = [data[n['range']['startOffset']:n['range']['endOffset']].decode() for n in expressions]
        assert '值 + 2' in slices and '(值 + 1)' in slices, slices
        nested = 'int 值 = 3\nstring 提示 = f"外😀 {f`内{值 + 3}`}"\n'
        parse('nested-fstring', 'nested.xl', nested.encode())
        quoted_nested = 'int 值 = 3\nstring 提示 = f"外😀 {f"内{值 + 3}"}"\n'
        parse('nested-quoted-fstring', 'nested-quoted.xl', quoted_nested.encode())
        parse('empty-source', 'empty.xl', b'')
        multiline = 'int 值 = 3\r\nstring 提示 = f"""第一😀\r\n{值 +\r\n 2}\r\n末尾"""\r\n'
        parse('multiline-fstring-crlf', 'multiline.xl', multiline.encode())
        fixtures = ['anonymous_functions.xl', 'cgen_fstring.xl', 'cgen_unicode_identifiers.xl',
                    'cgen_multiline_anonymous_call_arg.xl', 'cgen_generic_class_deep_function_fields.xl',
                    'cgen_generic_multi_return.xl', 'cgen_generic_alias.xl', 'cgen_generic_class_overloads.xl',
                    'cgen_generic_enum_match_top_level.xl', 'async_generic_class_method.xl',
                    'cgen_optional_nullable_function_default.xl', 'cgen_nested_anonymous_metadata.xl']
        for fixture in fixtures:
            parse('fixture-' + Path(fixture).stem, fixture, (root / 'test/cases' / fixture).read_bytes())
        virtual = inputs / 'virtual.xl'
        snapshot = inputs / 'overlay.utf8'
        snapshot.write_bytes(raw)
        protected[str(snapshot)] = digest(snapshot)
        value = json.loads(run('virtual-overlay', [compiler, 'parse', virtual, '--json', '--source-ranges',
                                                  '--source-overlay', virtual, snapshot]).stdout)
        checked_nodes += validate(value, raw, 'virtual.xl')
        assert not virtual.exists()
        option_snapshot = inputs / '--source-ranges'
        option_snapshot.write_bytes(raw)
        protected[str(option_snapshot)] = digest(option_snapshot)
        ordinary = json.loads(run('option-named-snapshot', [compiler, 'parse', virtual, '--json',
                                '--source-overlay', virtual, '--source-ranges']).stdout)
        assert ordinary['kind'] == 'module' and 'schema' not in ordinary
        json_snapshot = inputs / '--json'
        json_snapshot.write_bytes(raw)
        protected[str(json_snapshot)] = digest(json_snapshot)
        assert not run('snapshot-json-is-not-mode', [compiler, 'parse', virtual, '--source-ranges',
                       '--source-overlay', virtual, '--json'], 1).stdout
        ordinary = json.loads(run('range-after-double-dash-is-not-flag',
                                 [compiler, 'parse', inputs / 'unicode.xl', '--json', '--', '--source-ranges']).stdout)
        assert ordinary['kind'] == 'module' and 'schema' not in ordinary
        emitted = inputs / 'output ranges.json'
        run('explicit-output', [compiler, 'parse', inputs / 'unicode.xl', '--json', '--source-ranges', '--emit', emitted])
        checked_nodes += validate(json.loads(emitted.read_bytes()), raw, 'unicode.xl')
        run('reject-text-mode', [compiler, 'parse', inputs / 'unicode.xl', '--source-ranges'], 1)
        run('reject-wrong-command', [compiler, 'check', inputs / 'unicode.xl', '--json', '--source-ranges'], 1)
        broken = inputs / 'broken.xl'
        broken.write_bytes(b'func incomplete(\n')
        protected[str(broken)] = digest(broken)
        assert not run('reject-parse-error', [compiler, 'parse', broken, '--json', '--source-ranges'], 1).stdout
        native = work / 'source_ranges_host.exe'
        run('compile-native-host', [gcc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-municode',
                                   '-I' + str(root / 'release'), root / 'test/cases/source_ranges_host.c', '-o', native])
        run('native-api', [native, compiler.with_name('xlc.dll'), inputs])
        ast_host = work / 'source_ranges_ast_host.exe'
        run('compile-ast-host', [gcc, '-std=c11', '-Wall', '-Wextra', '-Werror',
                                '-Wno-pointer-sign', '-finput-charset=UTF-8', '-fexec-charset=UTF-8',
                                '-I' + str(root), '-I' + str(root / 'release'),
                                root / 'test/cases/source_ranges_ast_host.c', root / 'src/ast/xl6_ast.c',
                                '-L' + str(compiler.parent), '-l:xl.dll', '-o', ast_host])
        run('ast-copy-source-lifetime', [ast_host])
        memory = 'func 计算(int 值) as int\r\n    return (值 + 1)\r\nend'.encode()
        checked_nodes += validate(json.loads((inputs / 'native-memory.json').read_bytes()), memory, 'memory.xl')
        assert json.loads((inputs / 'native-memory.json').read_bytes()) == json.loads((inputs / 'native-string-authoritative.json').read_bytes())
        assert legacy(json.loads((inputs / 'native-memory.json').read_bytes())['ast']) == json.loads((inputs / 'native-legacy.json').read_bytes())
        replacement = b'func other() as int\n    return 2\nend\n'
        checked_nodes += validate(json.loads((inputs / 'native-virtual.json').read_bytes()), replacement, 'virtual.xl')
        checked_nodes += validate(json.loads((inputs / 'native-recovered.json').read_bytes()), replacement, 'recovered.xl')
        assert not (inputs / 'native-invalid.json').exists() and not (inputs / 'native-missing.json').exists()
        assert not (inputs / 'native-wide.json').exists()
        assert (inputs / 'native-deep.json').read_bytes() == b'prior-output', 'Rejected output must retain prior output'
        assert {path: digest(Path(path)) for path in protected} == protected
        status = 'passed'
    finally:
        evidence = {'status': status, 'startedUtc': started,
                    'completedUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    'compiler': str(compiler), 'checkedNodes': checked_nodes, 'results': results,
                    'protectedSha256': protected}
        (work / 'evidence.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
        print(work / 'evidence.json')
    print(f'ranged AST gate: PASS ({len(results)} commands, {checked_nodes} nodes)')


if __name__ == '__main__':
    main()
