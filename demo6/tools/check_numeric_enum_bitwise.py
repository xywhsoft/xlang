"""Offline full-source numeric enum operators: JIT/module/AOT, strict C and corrupt IR.
This validates source-built compiler products, not shipping libraries. No network I/O.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--native-root', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest = args.source_build.resolve()
    build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    out = args.out.resolve()
    assert not out.exists() and build['SourceXrt'] and not build['RuntimeOnly']
    assert build['EnableExeCompiler'] == 1
    assert all(sha(row['Path']) == row['Sha256'].lower()
               for row in build['Inputs'] + build['Objects'] + build['Headers'])
    driver = Path(build['Driver'])
    assert sha(driver) == build['DriverSha256'].lower()
    cc = Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    sdk = driver.parent / 'include'
    cases = ('enum_numeric_bitwise', 'enum_numeric_bitwise_lifetime',
             'enum_numeric_bitwise_contract', 'enum_numeric_bitwise_stdlib',
             'enum_numeric_runtime', 'operators', 'enum_dynamic_domains',
             'dynamic_scalar_container_domains', 'dynamic_primitive_operators', 'stdlib_callable_native_scalars')
    contracts = (
        ('xlir_numeric_enum_bitwise_contract.c', 'enum_numeric_bitwise_contract.xl',
         'XLIR: nominal enum bitwise:'),
        ('xlir_enum_scalar_domain_contract.c', 'enum_scalar_domain_contract.xl',
         'XLIR: enum scalar domain:'),
    )
    inputs = {str(path): sha(path) for path in
              (Path(__file__), manifest, driver, cc,
               *[root / 'test/cases' / name for row in contracts for name in row[:2]],
               *[root / ('test/cases/' + case + '.xl') for case in cases])}
    out.mkdir(parents=True)
    env = dict(os.environ)
    if os.name != 'nt':
        assert args.native_root
        env['XL6_SOURCE_NATIVE_ROOT'] = str(args.native_root.resolve())
    rows, products, failures = [], [], []
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False,
                  Inputs=inputs, Rows=rows, Products=products, Failures=failures,
                  SourceBuild=str(manifest), SourceProductAcceptance=True,
                  ShippingLibraryAcceptance=False, NetworkTests=False, OperatorProofs=[], Scope=__doc__)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def product(path):
        products.append(dict(Path=str(path), Sha256=sha(path)))
        save()

    def run(name, command, reject=False):
        log = out / (name + '.log')
        command = list(map(str, command))
        with log.open('wb') as stream:
            result = subprocess.run(command, cwd=root, env=env, stdout=stream,
                                    stderr=subprocess.STDOUT, timeout=240)
        rows.append(dict(Name=name, Command=command, Exit=result.returncode,
                         ExpectedRejection=reject, Log=str(log), Sha256=sha(log)))
        save()
        print(name + ': ' + str(result.returncode), flush=True)
        assert result.returncode == (1 if reject else 0), (name, str(log))
        return log.read_text(encoding='utf-8', errors='replace')

    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(root),
             '-I' + str(root / 'release'), '-I' + str(root / 'release/include'), '-I' + str(sdk)]
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
    compat = ['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
              '-Wno-unused-but-set-variable', '-Wno-unused-label', '-Wno-missing-field-initializers']
    save()
    for case in cases:
        try:
            source = root / ('test/cases/' + case + '.xl')
            generated = out / (case + '.c')
            run(case + '.emit', [driver, source, 'cgen-default', generated])
            product(generated)
            if case in ('enum_dynamic_domains','dynamic_scalar_container_domains','dynamic_primitive_operators'):
                code=generated.read_text(encoding='utf-8')
                assert 'static xvalue* xlir_xvalue_binary' not in code and 'static xvalue* xlir_xvalue_unary' not in code
                assert 'xl6ValueNumeric' in code
                if case=='enum_dynamic_domains': assert 'xl6ValueEnumBindTakeV1(' in code
            for stage in ('typed', 'lowered'):
                ir = out / (case + '.' + stage + '.json')
                run(case + '.' + stage, [driver, source, 'ir-' + stage, ir])
                product(ir)
            if case == 'enum_numeric_bitwise':
                typed = json.loads((out / (case + '.typed.json')).read_text())
                lowered = json.loads((out / (case + '.lowered.json')).read_text())
                types = {row['id']: row for row in typed['types']}
                enum_ids = {row['type'] for row in typed['enumLayouts']}
                assert {types[key]['integerTargetName'] for key in enum_ids} == {
                    'i8', 'u8', 'i16', 'u16', 'i32', 'u32', 'i64', 'u64'}
                nodes = {row['id']: row for row in typed['nodes']}
                code = generated.read_text()
                counts = dict(Typed=0, Lowered=0, WidthLimitedComplements=0)
                for node in typed['nodes']:
                    if node['opcodeName'] not in ('bit-and', 'bit-or', 'bit-xor', 'bit-not'):
                        continue
                    result = node['storageType']
                    if node['sourceKindName'] == 'assign':
                        left = typed['nodeChildren'][node['firstChild']]
                        result = nodes[left]['type']
                    if result not in enum_ids:
                        continue
                    operation = [row for row in lowered['instructions'] if row['origin'] == node['id'] and
                                 row['kindName'] in ('binary', 'unary')]
                    assert len(operation) == 1
                    instruction = operation[0]
                    assert instruction['type'] == result and instruction['runtimeOpName'] == 'none'
                    narrow = node['opcodeName'] == 'bit-not' and types[result]['integerTargetName'] in ('u8', 'u16', 'u32')
                    expected = 'bit-xor' if narrow else node['opcodeName']
                    assert instruction['opcodeName'] == expected
                    if narrow:
                        mask_id = instruction['operands'][1]
                        mask = {'u8': '255', 'u16': '65535', 'u32': '4294967295'}[types[result]['integerTargetName']]
                        assert re.search(r'\bv' + str(mask_id) + r'\s*=\s*' + mask + r'LL;', code)
                        counts['WidthLimitedComplements'] += 1
                    counts['Typed'] += 1
                    counts['Lowered'] += 1
                assert counts['Typed'] >= 64 and counts['WidthLimitedComplements'] >= 12
                report['OperatorProofs'].append(counts)
                save()
            for opt in ('O0', 'O2'):
                obj = out / (case + '.' + opt + '.o')
                run(case + '.' + opt, [cc, *flags, *compat, '-' + opt, '-c', generated, '-o', obj])
                product(obj)
            for mode in ('run', 'module'):
                run(case + '.' + mode, [driver, source, mode, out / (case + '.' + mode)])
            exe = out / (case + ('.aot.exe' if os.name == 'nt' else '.aot.out'))
            target = out / (case + '.aot') if os.name == 'nt' else exe
            run(case + '.aot.build', [driver, source, 'exe', target])
            product(exe)
            run(case + '.aot.run', [exe])
            native = Path(str(target) + '.temps') / 'xl6_xrt_impl.c'
            product(native)
            text = native.read_text()
            assert '_MODULE_ALL' not in text
            assert not any('#define XRT_MODULE_' + module + '\n' in text
                           for module in ('NET_SOCKET', 'NET_TCP', 'NET_UDP', 'TLS', 'NET_ENGINE'))
            imports = run(case + '.imports', ['E:/software/w64devkit/bin/objdump.exe', '-p', exe]
                          if os.name == 'nt' else ['readelf', '-d', exe])
            assert not any(name in imports.lower() for name in ('xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so'))
        except Exception as error:
            failures.append(dict(Name=case, Error=repr(error)))
            save()

    prefix = ('enum A as uint8\n zero = 0\n one = 1\nend\n'
              'enum B as uint8\n one = 1\nend\nA a = A.one\nB b = B.one\n')
    negatives = {}
    for operator in ('&', '|', '^'):
        label = {'&': 'and', '|': 'or', '^': 'xor'}[operator]
        for kind, expression in (('cross', 'a ' + operator + ' b'),
                                 ('integer', 'a ' + operator + ' 1'),
                                 ('reversed', '1 ' + operator + ' a'),
                                 ('dynamic', 'a ' + operator + ' value')):
            negatives[label + '.' + kind] = (('var value = 1\n' if kind == 'dynamic' else '') +
                                            'auto bad = ' + expression + '\n',
                                            'numeric enum bitwise operands must have the same enum type')
        negatives[label + '.compound'] = ('a ' + operator + '= b\n', 'numeric enum bitwise operands must have the same enum type')
    negatives.update(shift=('auto bad = a << a\n', 'numeric enum bitwise operands'),
                     modulo=('auto bad = a % a\n', 'numeric enum bitwise operands'),
                     arithmetic=('auto bad = a + a\n', 'numeric operator operands must be numeric'),
                     unary=('auto bad = -a\n', 'unary numeric operand must be numeric'),
                     tagged=('enum Tagged\n one\nend\nauto bad = ~Tagged.one()\n', 'bitwise not operand'),
                     nullable=('A? optional = null\nauto bad = ~optional\n', 'typed XLIR value expression has no canonical type: unary ~'),
                     nominal=('B bad = a | A.one\n', 'target B, value A'),
                     integer_result=('int bad = a | A.one\n', 'target int, value A'))
    for name, (body, reason) in negatives.items():
        try:
            source = out / ('bad.' + name + '.xl')
            source.write_text(prefix + body, encoding='utf-8')
            product(source)
            generated = out / ('bad.' + name + '.c')
            log = run('bad.' + name, [driver, source, 'cgen-default', generated], True)
            assert reason in log and not generated.exists(), (name, log)
        except Exception as error:
            failures.append(dict(Name='bad.' + name, Error=repr(error)))
            save()

    objects = [row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
               ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
    assert len(objects) == len(build['Objects']) - 1
    for contract_name, source_name, proof_text, opt in (
        (name, source, proof, opt)
        for name, source, proof in contracts
        for opt in ('O0', 'O2')
    ):
        contract = root / 'test/cases' / contract_name
        label = contract.stem + '.' + opt
        try:
            obj = out / (label + '.o')
            run(label + '.build', [cc, *flags, '-' + opt, '-c', contract, '-o', obj])
            product(obj)
            binary = out / (label + ('.exe' if os.name == 'nt' else ''))
            if os.name == 'nt':
                links = [*objects, obj, root / 'release/xl.dll', '-lws2_32', '-liphlpapi',
                         '-lshell32', '-lole32', '-ladvapi32', '-o', binary]
                shutil.copy2(root / 'release/xl.dll', out / 'xl.dll')
            else:
                lib = args.native_root.resolve() / 'release'
                links = [*objects, obj, '-L' + str(lib), '-lxl', '-Wl,-rpath,' + str(lib),
                         '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm', '-o', binary]
            response = out / (label + '.rsp')
            response.write_text('\n'.join('"' + str(item).replace('\\', '/') + '"' for item in links) + '\n', encoding='utf-8')
            product(response)
            run(label + '.link', [cc, '@' + str(response)])
            product(binary)
            proof = run(label + '.run', [binary, root / 'test/cases' / source_name])
            assert proof_text in proof
        except Exception as error:
            failures.append(dict(Name=label, Error=repr(error)))
            save()
    report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items()) and all(
        sha(row['Path']) == row['Sha256'].lower() for row in build['Inputs'])
    report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
    report['Complete'] = not failures and report['InputsUnchanged'] and report['LogsUnchanged']
    save()
    assert report['Complete'], failures


if __name__ == '__main__':
    main()
