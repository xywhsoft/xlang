"""Execute orphaned native contracts, package probes and interoperability tests.

Wiring is not acceptance: every selected compile/run must succeed. Incomplete
parallel package migrations remain failures, never implicit skips.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import importlib
import importlib.metadata
import json
import os
import re
import shutil
import subprocess
import sys
import time

from compiler_response import command_with_response
from xlir_public_build_inputs import verify_public_build


CONTRACTS = {
    'xlir_tcc_implementation_contract.c': ['test/cases/stdlib_tcc_runtime_contract.xl'],
    'xlir_bytes_collection_iteration_contract.c': ['test/cases/xlir_bytes_collection_iteration.xl'],
    'xlir_class_scalar_defaults_contract.c': [],
    'xlir_modular_symbol_contract.c': [],
    'xlir_tls_verify_link_catalog.c': [],
    'stdlib_crypto_chacha20_oom_host.c': [],
    'xlir_tls_resume_peer_contract.c': ['$TLS_PEER'],
}
PACKAGES = [
    ('bloom', 'test/bench_bloom.c'),
    ('dotenv', 'test/bench_dotenv.c'),
    ('imagehash', 'test/bench_phash.c'),
    ('siphash128', 'test/test_siphash.c'),
    ('bson', 'test/external_python_c.c'),
    ('cbor', 'test/external_python_c.c'),
    ('msgpack', 'test/external_python_c.c'),
    ('xhttp.client', 'test/buffer_transfer.c'),
    ('xhttp.protocol', 'test/buffer_transfer.c'),
    ('xhttp', 'test/xhttp_buffer_transfer.c'),
]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def report_inputs(inputs):
    normalized = {}
    for path, digest in inputs.items():
        key = os.path.normcase(os.path.normpath(path))
        if key in normalized and normalized[key] != digest:
            raise ValueError('Conflicting hashes for the same input path: ' + path)
        normalized[key] = digest
    return normalized


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'public', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--group', choices=('all', 'contracts', 'packages'), default='all')
    args = parser.parse_args()
    root, public, out = args.root.resolve(), args.public.resolve(), args.out.resolve()
    if os.name != 'nt' or out.exists():
        parser.error('Windows and a fresh output directory are required')
    build, pinned, _ = verify_public_build(root, public)
    out.mkdir(parents=True)
    cc, ar = shutil.which('gcc'), shutil.which('ar')
    assert cc and ar
    rows, inputs = [], {str(path): sha(path) for path in pinned}
    for path in (Path(__file__), Path(__file__).with_name('compiler_response.py'), Path(cc), Path(ar),
                 Path(__file__).resolve().parents[1]/'test/requirements-delivery.txt'):
        inputs[str(path)] = sha(path)
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows,
                  Scope=__doc__, Group=args.group, FullRegressionAcceptance=False)

    def save():
        report['Inputs'] = report_inputs(inputs)
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(name, command, expected=0):
        command = list(map(str, command))
        start = time.monotonic()
        log = out / (name + '.log')
        try:
            actual = command_with_response(command, cc, out / (name + '.rsp'))
            result = subprocess.run(actual, cwd=root, capture_output=True, timeout=600)
            log.write_bytes(result.stdout + result.stderr)
            code = result.returncode
        except (OSError, subprocess.TimeoutExpired) as error:
            log.write_text(repr(error), encoding='utf-8')
            code = 125
        passed = code == expected
        rows.append(dict(Name=name, Command=command, Exit=code, ExpectedExit=expected,
                         Complete=passed, Log=str(log), Sha256=sha(log), Seconds=time.monotonic()-start))
        save()
        print(name + ': ' + ('PASS' if passed else 'FAIL'), flush=True)
        return passed

    objects = [row['Path'] for row in build['Objects'] if Path(row['Path']).name != 'xlir_public_source_driver.o']
    archive = out / 'compiler.a'
    response = out / 'archive.rsp'
    response.write_text('\n'.join('"' + str(path).replace('\\', '/') + '"' for path in objects), encoding='utf-8')
    if not run('archive', [ar, 'rcs', archive, '@' + str(response)]):
        return 1
    shutil.copy2(root / 'release/xl.dll', out / 'xl.dll')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-DXRT_MODULE_ALL=',
             '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-label',
             '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers',
             '-Wno-pointer-sign', '-Wno-format-truncation']
    for folder in (root, root/'release', root/'release/include', root/'lib/xrt', root/'lib/xruntime',
                   root/'lib/tcc', public/'include'):
        flags.append('-I' + str(folder))
    links = [archive, root/'release/xl.dll', '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32', '-lm']

    if args.group in ('all', 'contracts'):
        tls = out / 'tls-peer.xl'
        tls.write_text('''func inspectPeer(TlsResume source, array<Certificate> certificates)
 auto copy = source.withPeerCertificates(certificates)
 auto count = copy.peerCertificateCount
 auto first = copy.peerCertificateAt(0)
end
assert 6 * 7 == 42
''', encoding='utf-8')
        for filename, arguments in CONTRACTS.items():
            source = root / 'test/cases' / filename
            inputs[str(source)] = sha(source)
            for level in ('O0', 'O2'):
                name = source.stem + '-' + level
                binary = out / (name + '.exe')
                if run(name + '.build', [cc, *flags, '-'+level, source, *links, '-o', binary]):
                    run(name, [binary, *[tls if value == '$TLS_PEER' else root/value for value in arguments]])
        source = root / 'release/examples/stdlib_tls_session_pair_c.c'
        inputs[str(source)] = sha(source)
        for level in ('O0', 'O2'):
            binary = out / ('tls-pair-' + level + '.exe')
            if run('tls-pair-'+level+'.build', [cc, *flags, '-'+level, source, *links, '-o', binary]):
                run('tls-pair-'+level, [binary])
        source = root / 'test/cases/xlir_serialization_visit_expect_host.c'
        inputs[str(source)] = sha(source)
        run('serialization-native-assertions', [root/'release/xl.exe', 'run',
            root/'test/cases/stdlib_json_stream.xl', source])
        source = root / 'test/cases/xlir_network_source_host.c'
        inputs[str(source)] = sha(source)
        generated = out / 'network.c'
        if run('network-emit', [build['Driver'], root/'test/cases/stdlib_net_tcp_udp.xl', 'cgen-default', generated]):
            for level in ('O0', 'O2'):
                binary = out / ('network-'+level+'.exe')
                if run('network-'+level+'.build', [cc, *flags, '-'+level, '-include', 'stdio.h',
                        '-include', generated, source, *links, '-o', binary]):
                    run('network-'+level, [binary])

    def package_sources(package, visited, included):
        if package in visited:
            return [], []
        visited.add(package)
        directory = root / 'release/extlibs' / package
        manifest = directory / 'xlang.package.json'
        inputs[str(manifest)] = sha(manifest)
        metadata = json.loads(manifest.read_text(encoding='utf-8-sig'))
        for path in directory.rglob('*'):
            if path.is_file() and path.suffix in ('.c', '.h', '.inc'):
                inputs[str(path)] = sha(path)
        sources, includes = [], ['-I'+str(directory)]
        for dependency in metadata.get('dependencies', []):
            child_sources, child_includes = package_sources(dependency['name'], visited, included)
            sources.extend(child_sources)
            includes.extend(child_includes)
        for item in metadata['artifact'].get('cSources', []):
            path = (directory/item).resolve()
            if path not in included:
                sources.append(path)
                inputs[str(path)] = sha(path)
        includes.extend('-I'+str(directory/item) for item in metadata['artifact'].get('includeDirs', []))
        return sources, includes

    if args.group in ('all', 'packages'):
        payloads = {}
        try:
            bson, cbor, msgpack = [importlib.import_module(name) for name in ('bson', 'cbor2', 'msgpack')]
            common = dict(name='Python \u4f60\u597d', count=2**40, nested=dict(ok=True), items=[1,2,3], raw=b'\0\xff')
            payloads['bson'] = bson.BSON.encode(dict(common, when=datetime.datetime.fromtimestamp(-0.001, datetime.timezone.utc)))
            payloads['cbor'] = cbor.dumps(dict(common, maximum=2**64-1,
                when=datetime.datetime.fromtimestamp(1786930200, datetime.timezone.utc).replace(microsecond=123456),
                epoch=cbor.CBORTag(1, 1786930200.25)))
            payloads['msgpack'] = msgpack.packb(dict(common, maximum=2**64-1, when=msgpack.Timestamp(-1,500000000)))
            report['ExternalEncoderVersions'] = {name: importlib.metadata.version(name)
                for name in ('pymongo', 'cbor2', 'msgpack')}
            for name, module in list(sys.modules.items()):
                path = getattr(module, '__file__', None)
                if path and name.split('.')[0] in ('bson', 'cbor2', '_cbor2', 'msgpack') and Path(path).is_file():
                    inputs[str(Path(path))] = sha(path)
        except ImportError as error:
            report['ExternalEncoderError'] = repr(error)
        for package, relative in PACKAGES:
            source = root / 'release/extlibs' / package / relative
            name = package.replace('.', '-') + '-' + source.stem
            if package == 'imagehash':
                asset = root / 'release/examples/extlibs/assets/tiny.png'
                inputs[str(asset)] = sha(asset)
            if source.is_file():
                inputs[str(source)] = sha(source)
            included = {source}
            if source.is_file():
                included.update((source.parent/match).resolve() for match in re.findall(
                    r'(?m)^\s*#\s*include\s+"([^"\r\n]+\.c)"', source.read_text(encoding='utf-8-sig')))
            sources, includes = package_sources(package, set(), included)
            for level in (('O2',) if source.stem.startswith('bench_') else ('O0','O2')):
                binary = out / (name+'-'+level+'.exe')
                if run(name+'-'+level+'.build', [cc, *flags, '-'+level, *includes, source, *sources, *links, '-o', binary]):
                    if source.name == 'external_python_c.c':
                        data = out / (package+'.bin')
                        if package not in payloads:
                            run(name+'-'+level+'.missing-encoder', [sys.executable, '-c', 'raise SystemExit(1)'])
                            continue
                        data.write_bytes(payloads[package])
                        run(name+'-'+level, [binary, data])
                        invalid = out / (package+'.invalid.bin')
                        invalid.write_bytes(b'\xff')
                        run(name+'-'+level+'.invalid', [binary, invalid], expected=1)
                    else:
                        run(name+'-'+level, [binary])
        source = root / 'release/extlibs/_expat/vendor/expat/lib/random_rand_s.c'
        inputs[str(source)] = sha(source)
        run('retained-expat-win32-alternative-syntax', [cc, *flags, '-fsyntax-only', source])
    report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items())
    report['Complete'] = report['InputsUnchanged'] and bool(rows) and all(row['Complete'] for row in rows)
    save()
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
