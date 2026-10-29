"""Explicit DER/PEM keys: offline public vectors, exact native entries and source ABI.
No network traffic, malformed-input/security campaigns, or shipping certification.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import time

from check_key_format_wire import vectors, header
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec, ed25519, rsa, x25519, x448
import math


FAMILIES = ['RsaPublic', 'RsaPrivate', 'X25519', 'X448', 'Ed25519', 'P256', 'P384']
VECTOR_INDICES = [0, 0, 4, 5, 3, 1, 2]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def entries(root):
    names = re.findall(r'XL6_TEXT_EXPORT\((\w+),', (root/'src/runtime/xl6_key_symbols.inc').read_text())
    declared = set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(\w+)\(', (root/'src/runtime/xl6_key_abi.h').read_text()))
    contracted = set(re.findall(r'^\{"(\w+)"', (root/'src/ir/xl6_ir_key_contracts.inc').read_text(), re.M))
    assert len(names) == 74 and set(names) == declared == contracted, (len(names), len(declared), len(contracted), set(names)-declared, set(names)-contracted)
    result = []
    for name in names:
        family = next(i for i, prefix in enumerate(FAMILIES) if name.startswith('xl6Crypto'+prefix))
        method = name[len('xl6Crypto'+FAMILIES[family]):-2]
        pem = method.endswith('Pem')
        bare = method[:-3] if pem else method
        flag = bare in ('ToPkcs8', 'ToSec1')
        if bare.startswith('PublicFrom'):
            arguments = 'state->spkiPem' if pem else 'state->spki'
            kind, fmt = 'BYTES', 'PUBLIC'
        elif bare.startswith('PublicTo'):
            arguments = 'state->publicBytes'
            kind, fmt = ('TEXT' if pem else 'BYTES'), 'SPKI'
        elif bare.startswith('From'):
            arguments = {'FromPkcs8': 'state->vector->pem' if pem else 'state->pkcs8',
                         'FromSpki': 'state->spkiPem' if pem else 'state->spki',
                         'FromSec1': 'state->innerPem' if pem else 'state->inner',
                         'FromPkcs1': 'state->innerPem' if pem else 'state->inner'}[bare]
            kind, fmt = 'KEY', 'PUBLIC'
        else:
            arguments = 'state->raw'+(',flag' if flag else '')
            kind = 'TEXT' if pem else 'BYTES'
            fmt = 'PKCS8' if bare == 'ToPkcs8' else ('RSA_PUBLIC' if bare == 'ToPkcs1' and family == 0 else
                  'INNER' if bare in ('ToSec1', 'ToPkcs1') else 'SPKI')
        label = 'PUBLIC KEY' if fmt == 'SPKI' else 'PRIVATE KEY' if fmt == 'PKCS8' else (
                'RSA PUBLIC KEY' if family == 0 else 'RSA PRIVATE KEY' if family == 1 else 'EC PRIVATE KEY')
        result.append(dict(name=name, family=family, method=method, bare=bare, pem=pem,
                           arguments=arguments, kind=kind, fmt=fmt, label=label, flag=flag))
    return result


def calls(path, rows):
    lines = ['static const Entry entries[]={']
    for row in rows:
        lines.append('{"%(name)s",%(family)d,RESULT_%(kind)s,FORMAT_%(fmt)s,"%(label)s",%(flag)s},' %
                     dict(row, flag=str(row['flag']).lower()))
    lines += ['};', 'static void invoke(size_t index,bool flag,Result* result)', '{',
              'State* state=&states[entries[index].family];switch(index){']
    for index, row in enumerate(rows):
        field = {'KEY': 'key', 'BYTES': 'bytes', 'TEXT': 'text'}[row['kind']]
        lines.append(f'case {index}:result->{field}={row["name"]}({row["arguments"]});break;')
    lines += ['default:assert(0);', '}}']
    path.write_text('\n'.join(lines)+'\n', encoding='utf8')


def alternate_public(path, cases):
    parts = cases[0]['key'].private_numbers();phi = (parts.p-1)*(parts.q-1)
    exponent = next(e for e in (3, 17, 257) if math.gcd(e, phi) == 1 and e != parts.public_numbers.e)
    keys = [rsa.RSAPublicNumbers(exponent, parts.public_numbers.n).public_key(),
            ec.derive_private_key(2, ec.SECP256R1()).public_key(),
            ec.derive_private_key(2, ec.SECP384R1()).public_key(),
            ed25519.Ed25519PrivateKey.from_private_bytes(bytes([2])*32).public_key(),
            x25519.X25519PrivateKey.from_private_bytes(bytes([2])*32).public_key(),
            x448.X448PrivateKey.from_private_bytes(bytes([2])*56).public_key()]
    lines, spans = [], []
    for index, key in enumerate(keys):
        encoding, fmt = ((serialization.Encoding.DER, serialization.PublicFormat.PKCS1) if index == 0 else
                         (serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint) if index < 3 else
                         (serialization.Encoding.Raw, serialization.PublicFormat.Raw))
        data = key.public_bytes(encoding, fmt);assert data != cases[index]['public']
        name = 'alternate_'+str(index)
        lines.append('static const uint8 '+name+'[]={'+','.join(hex(byte) for byte in data)+'};')
        spans.append('{'+name+',sizeof('+name+')}')
    lines.append('static const xbytesview alternatePublic[]={'+','.join(spans)+'};')
    path.write_text('\n'.join(lines)+'\n', encoding='utf8')


def literal(data, text=False):
    return ('"' if text else 'b"')+''.join('\\x%02x' % byte for byte in data)+'"'


def oracle(directory, cases):
    for vector in cases:
        prefix = directory/vector['name']
        data = Path(str(prefix)+'.spki.der').read_bytes();assert data == vector['spki']
        assert serialization.load_der_public_key(data).public_bytes(serialization.Encoding.DER,
            serialization.PublicFormat.SubjectPublicKeyInfo) == vector['spki']
        for suffix, loader in [('pkcs8.der', serialization.load_der_private_key), ('pkcs8.pem', serialization.load_pem_private_key)]:
            key = loader(Path(str(prefix)+'.'+suffix).read_bytes(), None)
            assert key.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo) == vector['spki']
        if vector['algorithm'] <= 3:
            key = serialization.load_der_private_key(Path(str(prefix)+'.inner.der').read_bytes(), None)
            assert key.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo) == vector['spki']


def fixtures(directory, cases, rows):
    """All native entries through real language declarations, plus isolated pruning roots."""
    all_lines = ['// Fixed public keys; generated deterministically from independent DER/PEM oracle.', 'int probe = 1', 'assert probe == 1']
    paths = []
    for family, class_prefix in enumerate(FAMILIES):
        v = cases[VECTOR_INDICES[family]]
        cls = class_prefix+'Key'
        variable = 'k'+str(family)
        lines = [f'bytes der = {literal(v["spki"] if family == 0 else v["pkcs8"])}',
                 f'auto {variable} = {cls}.from'+('Spki' if family == 0 else 'Pkcs8')+'(der)']
        expected = literal(v['public'])
        public = variable+'.publicKey' if family > 1 else variable+'.toPkcs1()' if family == 0 else variable+'.publicKey.toPkcs1()'
        lines.append(f'assert {public} == {expected}')
        original_args = ','.join(literal(part) for part in v['rsa'][:2 if family == 0 else 8]) if family < 2 else literal(v['raw'])
        lines.append('auto original'+str(family)+' = '+cls+'('+original_args+')')
        lines.append('array<'+cls+'> owners'+str(family)+' = [original'+str(family)+','+variable+']')
        check = '.publicKey' if family > 1 else '.toPkcs1()' if family == 0 else '.publicKey.toPkcs1()'
        lines.append('assert owners'+str(family)+'[0]'+check+' == '+expected)
        lines.append('owners'+str(family)+'[0] = owners'+str(family)+'[1]')
        lines.append('assert owners'+str(family)+'[0]'+check+' == '+expected)
        for row in (item for item in rows if item['family'] == family):
            bare, pem = row['bare'], row['pem']
            if bare.startswith('PublicFrom'):
                arg = literal(v['key'].public_key().public_bytes(serialization.Encoding.PEM,
                    serialization.PublicFormat.SubjectPublicKeyInfo), True) if pem else literal(v['spki'])
                expression = 'crypto.'+class_prefix.lower()+row['method']+'('+arg+')'
                lines.append('assert '+expression+' == '+expected)
            elif bare.startswith('PublicTo'):
                expression = 'crypto.'+class_prefix.lower()+row['method']+'('+expected+')'
                if pem:
                    lines.append(f'assert crypto.{class_prefix.lower()}PublicFromSpkiPem({expression}) == {expected}')
                else:
                    lines.append('assert '+expression+' == '+literal(v['spki']))
            elif bare.startswith('From'):
                fmt = bare[4:]
                output = variable+'.to'+fmt+('Pem' if pem else '')+'()'
                imported = cls+'.'+row['method'][0].lower()+row['method'][1:]+'('+output+')'
                check = '.publicKey' if family > 1 else '.toPkcs1()' if family == 0 else '.publicKey.toPkcs1()'
                lines.append('assert ('+imported+')'+check+' == '+expected)
            else:
                method = row['method'][0].lower()+row['method'][1:]
                expression = variable+'.'+method+'()'
                if row['fmt'] == 'SPKI':
                    if pem:
                        restored = 'RsaPublicKey.fromSpkiPem('+expression+').toPkcs1()' if family < 2 else (
                            'crypto.'+class_prefix.lower()+'PublicFromSpkiPem('+expression+')')
                        lines.append('assert '+restored+' == '+expected)
                    else:
                        lines.append('assert '+expression+' == '+literal(v['spki']))
                else:
                    fmt = 'Pkcs8' if row['fmt'] == 'PKCS8' else 'Pkcs1' if family < 2 else 'Sec1'
                    reader = cls+'.from'+fmt+('Pem' if pem else '')
                    check = '.publicKey' if family > 1 else '.toPkcs1()' if family == 0 else '.publicKey.toPkcs1()'
                    for argument in ('', 'false', 'true') if row['flag'] else ('',):
                        lines.append('assert ('+reader+'('+variable+'.'+method+'('+argument+')))'+check+' == '+expected)
        path = directory/('key.'+class_prefix.lower()+'.xl')
        path.write_text('\n'.join(lines)+'\n', encoding='utf8');paths.append(path)
        all_lines += [line.replace('der', 'der'+str(family)) for line in lines]
    path = directory/'key.all.xl';path.write_text('\n'.join(all_lines)+'\n', encoding='utf8');paths.append(path)
    return paths


def pruning_fixtures(directory, cases):
    paths = []
    for family in range(2, 7):
        name = FAMILIES[family];vector = cases[VECTOR_INDICES[family]];cls = name+'Key'
        for mode in ['public', 'der']+(['sec1'] if family >= 5 else []):
            if mode == 'public':
                body = f'assert crypto.{name.lower()}PublicToSpki({literal(vector["public"])}) == {literal(vector["spki"])}\n'
            elif mode == 'der':
                body = f'auto key = {cls}.fromPkcs8({literal(vector["pkcs8"])})\nassert key.publicSpki() == {literal(vector["spki"])}\n'
            else:
                body = f'auto key = {cls}.fromSec1({literal(vector["inner"])})\nassert key.publicKey == {literal(vector["public"])}\n'
            path = directory/('prune.'+name.lower()+'.'+mode+'.xl');path.write_text(body, encoding='utf8')
            paths.append((path, name.lower(), mode))
    return paths


def contract_fixture(path, rows):
    parameters = ','.join(prefix+'Key k'+str(index) for index, prefix in enumerate(FAMILIES))
    lines = ['// Metadata-only: valid typed signatures, never executed.', 'int marker = 1',
             'func keyContracts('+parameters+')']
    for index, row in enumerate(rows):
        method = row['method'][0].lower()+row['method'][1:]
        material = '"pem"' if row['pem'] and not row['bare'].startswith('PublicTo') else 'b"key"'
        if row['bare'].startswith('PublicFrom') or row['bare'].startswith('PublicTo'):
            call = 'crypto.'+FAMILIES[row['family']].lower()+row['method']+'('+material+')'
        elif row['bare'].startswith('From'):
            call = FAMILIES[row['family']]+'Key.'+method+'('+material+')'
        else:
            call = 'k'+str(row['family'])+'.'+method+'('+('true' if row['flag'] else '')+')'
        lines.append(' auto v'+str(index)+' = '+call)
    lines.append('end');path.write_text('\n'.join(lines)+'\n', encoding='utf8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--source-build', type=Path)
    parser.add_argument('--native-only', action='store_true')
    parser.add_argument('--source-contract-only', action='store_true')
    args = parser.parse_args();root = Path(__file__).resolve().parents[1];out = args.out.resolve()
    assert not out.exists();out.mkdir(parents=True)
    gcc = Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    tcc = Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    selected_entries = entries(root);cases = vectors(root);header(out/'key_vectors.h', cases);calls(out/'key_calls.inc', selected_entries)
    alternate_public(out/'key_alternate_public.h', cases)
    fixture_dir = out/'fixtures';fixture_dir.mkdir()
    test_paths = fixtures(fixture_dir, cases, selected_entries)
    contract_source = fixture_dir/'key.contract.xl';contract_fixture(contract_source, selected_entries)
    report = dict(Complete=False, Rows=[], Failures=[], Products=[], ResidentEntries=len(selected_entries),
                  TestSelection='native' if args.native_only else 'contracts' if args.source_contract_only else 'source' if args.source_build else 'native',
                  SourceProductAcceptance=False, ShippingLibraryAcceptance=False, NetworkTraffic=False, NetworkSecurityTests=False,
                  Inputs={str(path):sha(path) for path in [Path(__file__), gcc, tcc, root/'test/support/key_format_objects.c',
                          root/'test/support/key_formats_plan.c', root/'test/cases/xlir_key_formats_contract.c',
                          root/'test/support/container_projection_artifact.c', *root.glob('src/runtime/xl6_key*'),
                          *root.glob('src/runtime/xl6_crypto_*'), root/'tools/check_key_format_wire.py'] if path.is_file()})
    env = dict(os.environ)
    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    def run(name, command, reject=False):
        command = list(map(str, command));invocation = command;started = time.monotonic()
        if command[0] in (str(gcc), str(tcc)):
            response = out/(name+'.rsp')
            response.write_text('\n'.join('"'+arg.replace('\\', '/').replace('"', '\\"')+'"' for arg in command[1:])+'\n', encoding='utf8')
            invocation = [command[0], '@'+str(response)]
        log = out/(name+'.log')
        with log.open('wb') as stream:
            try:
                code = subprocess.run(invocation, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=300).returncode
            except subprocess.TimeoutExpired:
                code = None
        report['Rows'].append(dict(Name=name, Command=command, Invocation=invocation, Exit=code, Log=str(log), Sha256=sha(log), Seconds=time.monotonic()-started));save()
        print(name, code, flush=True)
        assert code == (1 if reject else 0), (name, str(log))
        return log.read_text(encoding='utf8', errors='replace')
    flags = ['-std=c11', '-DXL_STATIC_EMBED', '-I'+str(root), '-I'+str(root/'release'), '-I'+str(root/'release/include'), '-I'+str(out)]
    if os.name != 'nt':flags += ['-D_GNU_SOURCE']
    strict = ['-Wall', '-Wextra', '-Werror', '-fmax-errors=4']
    libs = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else ['-pthread', '-ldl', '-lm']
    suffix = '.exe' if os.name == 'nt' else ''
    if args.native_only or not args.source_build:
        try:
            planner = out/('planner'+suffix)
            run('planner.build', [gcc, *flags, *strict, '-O0', '-Wno-missing-field-initializers',
                '-ffunction-sections', '-fdata-sections', '-fno-jump-tables', '-fno-asynchronous-unwind-tables', '-fno-unwind-tables',
                root/'test/support/key_formats_plan.c', root/'src/compile/xl6_compile_unit.c', '-Wl,--gc-sections', '-o', planner])
            sources = run('planner.sources', [planner, '--sources']).splitlines()
            modules = run('planner.modules', [planner, '--modules'])
            assert len(sources) == len(set(sources)) and '_MODULE_ALL' not in modules
            report['NativeAdapterClosure'] = dict(Sources=sources, Modules=modules)
            implementation = out/'native.c'
            implementation.write_text(modules+'\n#define XRT_MODULE_MEMORY_DEBUG\n'+
                ('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n' if '#define XRUNTIME_MODULE_' in modules else
                 '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'), encoding='utf8')
            modes = [('gcc.O0', gcc, ['-O0']), ('gcc.O2', gcc, ['-O2']), ('tcc', tcc, [])]
            if os.name != 'nt':modes += [('asan', gcc, ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie'])]
            for name, cc, options in modes:
                try:
                    binary = out/(name+suffix)
                    run(name+'.build', [cc, *flags, *options, *(strict if cc == gcc else []),
                        root/'test/support/key_format_objects.c', implementation, *[root/source for source in sources], *libs, '-o', binary])
                    directory = out/(name+'.oracle');directory.mkdir()
                    assert 'Key objects: 74 entries,' in run(name+'.run', [binary, directory])
                    oracle(directory, cases)
                except Exception as error:
                    report['Failures'].append(dict(Name=name, Error=repr(error)));save()
        except Exception as error:
            report['Failures'].append(dict(Name='native-plan', Error=repr(error)));save()
    if args.source_build and not args.native_only:
        build = json.loads(args.source_build.read_text(encoding='utf-8-sig'))
        assert build.get('Complete', True) and build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path']) == row['Sha256'].lower(), row['Path']
        driver = Path(build['Driver']);assert sha(driver) == build['DriverSha256'].lower()
        env['XL6_SOURCE_NATIVE_ROOT'] = build.get('NativeRoot', '')
        runtime = root/'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot'])/'release/libxl.so'
        if os.name == 'nt':shutil.copy2(runtime, out/'xl.dll')
        compat = ['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-but-set-variable',
                  '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-missing-field-initializers', '-Wno-pointer-sign']
        count = int(re.search(r'#define XL6_STDLIB_FUNC_COUNT (\d+)', (root/'src/stdlib/xl6_stdlib_data.h').read_text()).group(1))
        index = (root/'src/stdlib/xl6_stdlib_func_index.inc').read_text()
        assert int(re.search(r'#if XL6_STDLIB_FUNC_COUNT != (\d+)', index).group(1)) == count
        for wrong in (count-1, count+1):
            try:
                path = out/f'bad.index.{wrong}.c'
                path.write_text(f'#define XL6_STDLIB_FUNC_COUNT {wrong}\n#define XL6_STDLIB_FUNC_INDEX_CAPACITY 4096\n#include "src/stdlib/xl6_stdlib_func_index.inc"\n', encoding='utf8')
                assert 'stdlib function index is stale' in run(f'bad.index.{wrong}', [gcc, *flags, '-fsyntax-only', path], True)
            except Exception as error:
                report['Failures'].append(dict(Name=f'bad.index.{wrong}', Error=repr(error)));save()
        for source in ([] if args.source_contract_only else test_paths):
            name = source.stem
            try:
                generated = out/(name+'.c');run(name+'.emit', [driver, source, 'cgen-default', generated])
                text = generated.read_text(encoding='utf8')
                assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Crypto\w*(?:Pkcs|Spki|Sec1)\w*\([^;]*\)\s*\{', text, re.M)
                run(name+'.jit', [driver, source, 'run', out/(name+'.jit')])
                for stage in ('typed', 'lowered'):
                    run(name+'.'+stage, [driver, source, 'ir-'+stage, out/(name+'.'+stage+'.json')])
                for opt in ('O0', 'O2'):
                    run(name+'.'+opt, [gcc, *flags, '-I'+str(root/'release'), '-I'+str(driver.parent/'include'),
                        *strict, *compat, '-'+opt, '-c', generated, '-o', out/(name+'.'+opt+'.o')])
                run(name+'.module', [driver, source, 'module', out/(name+'.module')])
                binary = out/(name+'.aot'+suffix);target = binary.with_suffix('') if os.name == 'nt' else binary
                run(name+'.aot.build', [driver, source, 'exe', target]);run(name+'.aot.run', [binary])
                imports = run(name+'.imports', ['E:/software/w64devkit/bin/objdump.exe', '-p', binary] if os.name == 'nt' else ['readelf', '-d', binary])
                assert not any(name in imports.lower() for name in ('xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so'))
                implementation = Path(str(target)+'.temps')/'xl6_xrt_impl.c'
                assert '_MODULE_ALL' not in implementation.read_text(encoding='utf8')
            except Exception as error:
                report['Failures'].append(dict(Name=name, Error=repr(error)));save()
        objects = [row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
                   ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
        assert len(objects) == len(build['Objects'])-1
        source_libs = [runtime, *libs]
        if os.name != 'nt':source_libs += ['-Wl,--export-dynamic', '-Wl,-rpath,'+str(runtime.parent)]
        for opt in ('O0', 'O2'):
            try:
                obj = out/('contract.'+opt+'.o');binary = out/('contract.'+opt+suffix)
                run('contract.'+opt+'.compile', [gcc, *flags, *strict, '-'+opt, '-c', root/'test/cases/xlir_key_formats_contract.c', '-o', obj])
                run('contract.'+opt+'.link', [gcc, *objects, obj, *source_libs, '-o', binary])
                assert 'Key formats XLIR:' in run('contract.'+opt+'.run', [binary, contract_source])
            except Exception as error:
                report['Failures'].append(dict(Name='contract.'+opt, Error=repr(error)));save()
        try:
            planner = out/('artifact-plan'+suffix);obj = out/'artifact-plan.o'
            run('artifact-plan.compile', [gcc, *flags, *strict, '-O0', '-c', root/'test/support/container_projection_artifact.c', '-o', obj])
            run('artifact-plan.link', [gcc, *objects, obj, *source_libs, '-o', planner])
            plans = [];report['Plans'] = plans
            for path, family, mode in pruning_fixtures(fixture_dir, cases):
                prefix = out/path.stem
                run(path.stem+'.plan', [planner, path, prefix])
                plan = json.loads(Path(str(prefix)+'.plan.json').read_text())
                sources = plan['Sources'];modules = plan['NativeModules']
                assert not any('_pem' in s for s in sources) and not any('PEM' in m for m in modules)
                assert not any('_MODULE_ALL' in m for m in modules)
                assert not any('crypto_rsa_' in s for s in sources)
                for other in ('x25519', 'x448', 'ed25519', 'p256', 'p384'):
                    if other != family:assert not any('xl6_crypto_'+other in s for s in sources)
                if mode == 'public':
                    assert not any('xl6_crypto_'+family+'.c' in s or '_domain.c' in s and 'crypto' in s for s in sources)
                    assert not any('CRYPTO_'+algorithm in m for m in modules for algorithm in ('X25519', 'X448', 'ED25519', 'P256', 'P384'))
                if mode == 'sec1':assert not any('pkcs8' in s for s in sources)
                plans.append(dict(Family=family, Mode=mode, **plan));save()
                run(path.stem+'.jit', [driver, path, 'run', out/(path.stem+'.jit')])
                binary = out/(path.stem+'.aot'+suffix);target = binary.with_suffix('') if os.name == 'nt' else binary
                run(path.stem+'.aot.build', [driver, path, 'exe', target]);run(path.stem+'.aot.run', [binary])
        except Exception as error:
            report['Failures'].append(dict(Name='pruning', Error=repr(error)));save()
        negatives = {
            'text-der': 'auto key = X25519Key.fromPkcs8("text")\n',
            'bytes-pem': 'auto key = P256Key.fromPkcs8Pem(b"text")\n',
            'wrong-flag': 'func bad(P384Key key)\n key.toPkcs8(1)\nend\n',
            'private-field': 'func bad(Ed25519Key key)\n auto raw = key.handle\nend\n',
            'raw-public-text': 'auto value = crypto.ed25519PublicToSpki("text")\n',
            'rsa-wrong-result': 'X25519Key key = RsaPublicKey.fromSpki(b"data")\n',
            'no-key-size': 'auto value = X448Key.fromPkcs8(b"data",1)\n',
        }
        for name, body in negatives.items():
            try:
                path = fixture_dir/('bad.'+name+'.xl');path.write_text(body, encoding='utf8');target = out/('bad.'+name+'.c')
                message = run('bad.'+name, [driver, path, 'cgen-default', target], True)
                assert any(tag in message for tag in ('[sem]', '[ir]', '[checked]', '[stdlib]')) and not target.exists()
            except Exception as error:
                report['Failures'].append(dict(Name='bad.'+name, Error=repr(error)));save()
        report['SourceProductAcceptance'] = not report['Failures']
    report['InputsUnchanged'] = all(sha(path) == digest for path, digest in report['Inputs'].items())
    if args.source_build:
        report['SourceInputsUnchanged'] = all(sha(row['Path']) == row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
        report['InputsUnchanged'] = report['InputsUnchanged'] and report['SourceInputsUnchanged'] and sha(driver) == build['DriverSha256'].lower()
    report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in report['Rows'])
    report['Complete'] = not report['Failures'] and report['InputsUnchanged'] and report['LogsUnchanged']
    for path in out.rglob('*'):
        if path.is_file() and path.name != 'results.json':report['Products'].append(dict(Path=str(path), Sha256=sha(path)))
    save();assert report['Complete'], report['Failures']


if __name__ == '__main__':
    main()
