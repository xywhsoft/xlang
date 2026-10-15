"""Audit exact resident linkage ownership against separately compiled objects.
Shared by Windows artifact execution and the Linux source gate. No spelling
prefix is used for production selection. The nm family filter is only a test
oracle and includes checked host xlSync exports as well as private xl6 exports.
This proves the resident catalog/closures, not shipping or full53 acceptance.
"""
from pathlib import Path
import argparse, copy, hashlib, json, os, re, shutil, subprocess
from xlir_public_build_inputs import verify_public_build


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def declarations(root):
    header = (root / 'src/compile/xl6_artifact_runtime.h').read_text(encoding='utf-8')
    match = re.search(r'typedef\s+enum\s+XL6_ArtifactRuntimeId\s*\{(.*?)\}\s*XL6_ArtifactRuntimeId\s*;', header, re.S)
    assert match, 'Missing explicit component identities'
    names = re.findall(r'\bXL6_ARTIFACT_(?!RUNTIME_COUNT\b)([A-Z_][A-Z_0-9]*)\b', match[1])
    assert len(names) == len(set(names)) and 'NETWORK_SERVICE' in names
    ids = {name: index for index, name in enumerate(names)}
    entries = []
    for path, pattern in (
        ('src/compile/xl6_artifact_runtime_symbols.inc', r'XL6_ARTIFACT_SYMBOL\("(\w+)", (\w+)\)'),
        ('src/runtime/xl6_log_symbols.inc', r'XL6_LOG_EXPORT\((\w+), (\w+)\)'),
        ('src/runtime/xl6_text_symbols.inc', r'XL6_TEXT_EXPORT\((\w+), (\w+)\)'),
    ):
        entries.extend(re.findall(pattern, (root / path).read_text(encoding='utf-8')))
    domains = (root / "src/runtime/xl6_ownership_domains.inc").read_text(encoding="utf-8")
    policies = re.findall(r"XL6_OWNERSHIP_DOMAIN_ENTRY\((\w+),(\w+),(\w+)\)", domains)
    prototypes = re.findall(r'\b(xl6OwnershipDomain\w+V1Get)\(void\);',
        (root / 'src/runtime/xl6_ownership_domains.h').read_text(encoding='utf-8'))
    assert len(prototypes)==len(set(prototypes)) and set(prototypes)=={getter for _, _, getter in policies}
    entries.extend((getter, owner) for _, owner, getter in policies)
    return ids, entries


def inspect(ids, entries, components):
    errors, declared, actual = [], {}, {}
    for symbol, owner in entries:
        if symbol in declared:
            errors.append('Duplicate declaration: ' + symbol)
        if owner not in ids:
            errors.append('Unknown declaration owner: ' + owner)
        else:
            declared[symbol] = ids[owner]
    if sorted(row['Id'] for row in components) != list(range(len(ids))):
        errors.append('Incomplete or duplicate component population')
    for row in components:
        if row['Build'] != 0:
            errors.append('Failed provider build: ' + row['Source'])
        for symbol in row['Defined']:
            if symbol in actual:
                errors.append('Duplicate actual export: ' + symbol)
            actual[symbol] = row['Id']
    for symbol, owner in actual.items():
        if declared.get(symbol) != owner:
            errors.append('Unregistered or misowned actual export: ' + symbol)
    for symbol in declared:
        if symbol not in actual:
            errors.append('Declared export missing: ' + symbol)
    links = []
    by_id = {row['Id']: row for row in components}
    for row in components:
        reached, pending = set(), [row['Id']]
        while pending:
            item = pending.pop()
            if item in reached:
                continue
            reached.add(item)
            if item not in by_id:
                errors.append('Unknown component dependency: ' + str(item))
            else:
                pending.extend(by_id[item]['Dependencies'])
        if reached != set(row['Closure']) or len(row['Closure']) != len(set(row['Closure'])):
            errors.append('Incorrect component closure: ' + str(row['Id']))
        missing = [symbol for symbol in row['Undefined'] if actual.get(symbol) not in row['Closure']]
        links.append({'Id': row['Id'], 'Source': row['Source'], 'Build': row['Build'],
                      'Missing': missing, 'Complete': row['Build'] == 0 and not missing})
        errors.extend('Missing resident dependency: ' + symbol for symbol in missing)
    return {'RegistryErrors': errors, 'DeclaredExports': len(declared),
            'ActualExports': len(actual), 'ComponentCount': len(ids), 'Linkage': links}


def negative_controls(ids, entries, components):
    """Mutate the actual full passing population, never an invented toy catalog."""
    checks = []

    def reject(name, changed_entries, changed_components):
        result = inspect(ids, changed_entries, changed_components)
        assert result['RegistryErrors'], 'Audit accepted negative control: ' + name
        checks.append({'Name': name, 'Rejected': True, 'Errors': result['RegistryErrors']})

    symbol, owner = entries[0]
    reject('undeclared-export', entries[1:], components)
    reject('wrong-owner', [(symbol, next(name for name in ids if name != owner)), *entries[1:]], components)
    reject('duplicate-declaration', [*entries, entries[0]], components)
    reject('unknown-owner', [(symbol, 'NOT_A_COMPONENT'), *entries[1:]], components)
    for required in ('xl6SequenceBeginErasedRoleV1', 'xl6SequenceBindProjectionRoleV1',
                     'xl6SequenceBindStackResultArrayV1', 'xl6SequenceStackResultArrayV1', 'xl6StackEraseV1'):
        selected = next((item for item in entries if item[0] == required), None)
        assert selected, 'Missing required role/Stack ownership contract: ' + required
        reject('missing-role-' + required, [item for item in entries if item != selected], components)
        wrong = next(name for name in ids if name != selected[1])
        reject('misowned-role-' + required,
               [(name, wrong if name == required else owner) for name, owner in entries], components)
    mutated = copy.deepcopy(components)
    next(row for row in mutated if symbol in row['Defined'])['Defined'].remove(symbol)
    reject('missing-definition', entries, mutated)
    mutated = copy.deepcopy(components)
    next(row for row in mutated if symbol not in row['Defined'])['Defined'].append(symbol)
    reject('duplicate-definition', entries, mutated)
    mutated = copy.deepcopy(components)
    mutated[0]['Build'] = 1
    reject('provider-build-failure', entries, mutated)
    reject('missing-component', entries, components[1:])
    mutated = copy.deepcopy(components)
    edge = next(row for row in mutated if any(
        name in other['Defined'] and other['Id'] != row['Id']
        for name in row['Undefined'] for other in mutated))
    edge['Closure'] = [edge['Id']]
    reject('missing-resident-dependency', entries, mutated)
    for name in ('xlSyncRetirementSnapshotCheckedV1', 'xlSyncRetirementAcquireCheckedV1',
                 'xlSyncRetirementUnlockCheckedV1', 'xlSyncRetirementRetryCheckedV1'):
        mutated = copy.deepcopy(components)
        next(row for row in mutated if name in row['Defined'])['Defined'].remove(name)
        reject('missing-host-' + name, entries, mutated)
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--catalog-dir', type=Path, help='Reuse actual objects from the enclosing artifact gate')
    mode.add_argument('--compiler', type=Path, help='Build contract and every resident provider independently')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists(), 'Preserve prior catalog evidence'
    cc, nm = shutil.which('gcc'), shutil.which('nm')
    assert cc and nm
    build_dir = args.catalog_dir.resolve() if args.catalog_dir else out.parent / (out.stem + '-objects')
    files = {Path(__file__).resolve(), Path(nm)}
    files.update(root / path for path in ('src/compile/xl6_artifact_runtime.h',
        'src/compile/xl6_artifact_runtime_symbols.inc', 'src/runtime/xl6_log_symbols.inc',
        'src/runtime/xl6_text_symbols.inc'))
    # Include all native/header inputs, not just the exported registry rows.
    files.update(path for folder in ('src', 'lib/xrt', 'lib/xruntime', 'release/include')
                 for path in (root / folder).rglob('*') if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    rows = []
    report = {'Complete': False, 'InputsUnchanged': False, 'Scope': __doc__,
              'Platform': os.name, 'Rows': rows, 'ShippingLibraryAcceptance': False,
              'Full53Acceptance': False, 'StandaloneArtifactExecutionAcceptance': False}

    def run(name, command):
        command = list(map(str, command))
        result = subprocess.run(command, cwd=root, capture_output=True, timeout=180)
        (build_dir / (name + '.log')).write_bytes(result.stdout + result.stderr)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode})
        assert result.returncode == 0, (name, result.stderr[-6000:].decode(errors='replace'))
        return result.stdout.decode(errors='strict')

    windows = os.name == 'nt'
    flags = ['-std=c11', '-O0', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
        '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-label',
        '-Wno-unused-but-set-variable', '-Wno-unused-const-variable', '-Wno-pointer-sign',
        '-Wno-format-truncation', '-Wno-missing-field-initializers', '-DXL_STATIC_EMBED',
        '-I.', '-Irelease', '-Irelease/include', '-Ilib/tcc']
    if not windows:
        flags.append('-D_GNU_SOURCE')
    if args.compiler:
        assert not build_dir.exists()
        build_dir.mkdir(parents=True)
        manifest = args.compiler.resolve() / 'build.json'
        build, public_inputs, compared = verify_public_build(root, args.compiler)
        files.update(public_inputs)
        report['PublicBuildVerified'] = True
        report['ComparedSources'] = compared
        files.update((manifest, Path(build['Driver']), Path(cc), root / 'test/cases/xlir_artifact_runtime_contract.c'))
        objects = [row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
                   ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
        if windows:
            runtime = root / 'release/xl.dll'
            files.add(runtime)
            shutil.copy2(runtime, build_dir / 'xl.dll')
            libraries = [runtime, '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
        else:
            native = Path(build['NativeRoot']) / 'release'
            files.add(native / 'libxl.so')
            libraries = ['-L' + str(native), '-lxl', '-Wl,-rpath,' + str(native),
                         '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm']
        binary = build_dir / ('contract.exe' if windows else 'contract')
        inputs = [{'Path': str(path), 'Sha256': sha(path)} for path in sorted(files)]
        run('contract-build', [cc, *flags, 'test/cases/xlir_artifact_runtime_contract.c',
                              *objects, *libraries, '-o', binary])
        # Actual independent exhaustive pairwise closure and exact-owner tests.
        run('contract', [binary, 'test/cases/cgen_basic.xl',
            'test/cases/stdlib_persistence_restore_json_prune.xl',
            'test/cases/stdlib_persistence_restore_xlon_prune.xl',
            'test/cases/stdlib_persistence_restore_array_json_prune.xl'])
        catalog_text = run('catalog', [binary, '--catalog'])
        (build_dir / 'catalog.jsonl').write_text(catalog_text, encoding='utf-8')
    else:
        assert build_dir.is_dir()
        files.add(build_dir / 'catalog.jsonl')
        inputs = [{'Path': str(path), 'Sha256': sha(path)} for path in sorted(files)]
        catalog_text = (build_dir / 'catalog.jsonl').read_text(encoding='utf-8-sig')
    report['Inputs'] = inputs
    try:
        catalog = [json.loads(line) for line in catalog_text.splitlines() if line.strip()]
        components = []
        for component in catalog:
            obj = build_dir / ('component-' + str(component['Id']) + '.o')
            if args.compiler:
                run('component-' + str(component['Id']), [cc, *flags, '-c', component['Source'], '-o', obj])
            assert obj.is_file(), 'Missing actual provider object: ' + str(obj)
            defined = re.findall(r'\b[A-Za-z]\s+(xl\w+)\s*$',
                run('defined-' + str(component['Id']), [nm, '-g', '--defined-only', obj]), re.M)
            undefined = re.findall(r'\bU\s+(xl\w+)\s*$',
                run('undefined-' + str(component['Id']), [nm, '-u', obj]), re.M)
            report.setdefault('Objects', []).append({'Path': str(obj), 'Sha256': sha(obj)})
            components.append(dict(component, Build=0, Defined=defined, Undefined=undefined))
        ids, entries = declarations(root)
        report.update(inspect(ids, entries, components))
        assert not report['RegistryErrors'], report['RegistryErrors']
        report['NegativeControls'] = negative_controls(ids, entries, components)
        report['DependencyPairs'] = len(ids) ** 2
        report['HostCheckedExports'] = sorted(symbol for row in components for symbol in row['Defined'] if symbol.startswith('xlSync'))
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs + report['Objects'])
        report['Complete'] = report['InputsUnchanged'] and all(row['Exit'] == 0 for row in rows)
        assert report['Complete']
    finally:
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Resident catalog accepted: ' + str(report['ComponentCount']) + ' components, '
          + str(report['ActualExports']) + ' exact exports, '
          + str(len(report['NegativeControls'])) + ' rejecting controls', flush=True)


if __name__ == '__main__':
    main()
