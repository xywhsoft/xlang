"""Audit exact resident linkage ownership against separately compiled objects.
Shared by Windows artifact execution and the Linux source gate. No spelling
prefix is used for production selection. The nm family filter is only a test
oracle and includes checked host recovery exports as well as private xl6 exports.
This proves the resident catalog/closures, not shipping or full53 acceptance.
"""
from pathlib import Path
import argparse, copy, hashlib, json, os, re, shutil, subprocess
from xlir_public_build_inputs import verify_public_build


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def symbol_tables(root):
    """Follow the production registry's modular literal symbol tables.

    This is not a C preprocessor: native-policy/domain macros are read below.
    Repeated includes remain repeated declarations, so duplicate registration
    cannot disappear through a global visited set. Include cycles are rejected.
    """
    root = root.resolve()
    def walk(path, active):
        path = path.resolve()
        assert path.is_relative_to(root) and path.is_file(), path
        assert path not in active, ('symbol table include cycle', path)
        yield path
        active = (*active, path)
        for reference in re.findall(r'^\s*#include\s+"([^"]+_symbols\.inc)"',
                                     path.read_text(encoding='utf-8'), re.M):
            child = root/reference
            if not child.is_file(): child = path.parent/reference
            yield from walk(child, active)
    return list(walk(root/'src/compile/xl6_artifact_runtime_symbols.inc', ()))


def declarations(root):
    header = (root / 'src/compile/xl6_artifact_runtime.h').read_text(encoding='utf-8')
    match = re.search(r'typedef\s+enum\s+XL6_ArtifactRuntimeId\s*\{(.*?)\}\s*XL6_ArtifactRuntimeId\s*;', header, re.S)
    assert match, 'Missing explicit component identities'
    def expand_ids(body, active):
        pieces = []
        for line in body.splitlines(keepends=True):
            include = re.fullmatch(r'\s*#include\s+"([^"]+_component_ids\.inc)"\s*', line)
            if not include:
                pieces.append(line)
                continue
            child = (root / include[1]).resolve()
            if not child.is_file(): child = (root / 'src/compile' / include[1]).resolve()
            assert child.is_relative_to(root.resolve()) and child.is_file(), child
            assert child not in active, ('component identity include cycle', child)
            pieces.append(expand_ids(child.read_text(encoding='utf-8'), (*active, child)))
        return ''.join(pieces)
    names = re.findall(r'\bXL6_ARTIFACT_(?!RUNTIME_COUNT\b)([A-Z_][A-Z_0-9]*)\b',
                       expand_ids(match[1], ()))
    assert len(names) == len(set(names)) and 'NETWORK_SERVICE' in names
    ids = {name: index for index, name in enumerate(names)}
    entries = []
    for path in symbol_tables(root):
        body = path.read_text(encoding='utf-8')
        for pattern in (r'^\s*XL6_ARTIFACT_SYMBOL\("(\w+)",\s*(\w+)\)',
                        r'^\s*XL6_LOG_EXPORT\((\w+),\s*(\w+)\)',
                        r'^\s*XL6_TEXT_EXPORT\((\w+),\s*(\w+)\)'):
            entries.extend(re.findall(pattern, body, re.M))
    resources=(root/'src/runtime/xl6_native_resource_policies.inc').read_text(encoding='utf-8')
    resource_policies = re.findall(
        r'XL6_NATIVE_RESOURCE_POLICY\((?:"\w+"|NULL),(\w+),(\w+)\)', resources)
    domains = (root / "src/runtime/xl6_ownership_domains.inc").read_text(encoding="utf-8")
    policies = re.findall(r"XL6_OWNERSHIP_DOMAIN_ENTRY\((\w+),(\w+),(\w+)\)", domains)
    # domains.h includes catalog.h, whose public forward declarations are
    # part of the same declaration-only SDK. Keep exact duplicate/set checks.
    prototype_headers = ('xl6_ownership_domains.h', 'xl6_ownership_catalog.h')
    prototypes = [name for header in prototype_headers for name in re.findall(
        r'\b(xl6OwnershipDomain\w+V1Get)\(void\);',
        (root / 'src/runtime' / header).read_text(encoding='utf-8'))]
    assert len(prototypes)==len(set(prototypes)) and set(prototypes)=={getter for _, _, getter in policies}
    # The primary C table expands policy/domain getters as well as ordinary
    # runtime exports. The same getter may serve several roles; join those exact
    # identities only when their component owners agree. The compiled oracle
    # below independently checks the resulting registration map.
    owners = dict(entries)
    assert len(owners) == len(entries), 'Duplicate primary symbol declaration'
    derived = [(getter, owner) for _, owner, getter in policies] + [
        (getter, owner) for owner, getter in resource_policies]
    for getter, owner in derived:
        if getter in owners:
            assert owners[getter] == owner, ('Conflicting resource policy owner', getter)
        else:
            entries.append((getter, owner))
            owners[getter] = owner
    return ids, entries


def inspect(ids, entries, components, host_symbols, host_abis, compiled_owners):
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
    for symbol, owner in compiled_owners.items():
        if declared.get(symbol) != owner:
            errors.append('Missing or misowned compiled Cgen entry: ' + symbol)
    for symbol, owner in declared.items():
        if compiled_owners.get(symbol) != owner:
            errors.append('Declaration differs from compiled Cgen entry: ' + symbol)
    for symbol, owner in actual.items():
        if symbol in declared and declared[symbol] != owner:
            errors.append('Misowned actual Cgen export: ' + symbol)
    for symbol in declared:
        if symbol not in actual:
            errors.append('Declared export missing: ' + symbol)
    links = []
    by_id = {row['Id']: row for row in components}
    allowed_bits=0
    for bit in host_abis:allowed_bits|=bit
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
        host_abi=row['HostAbi'];closed_host=0
        if host_abi & ~allowed_bits:errors.append('Unknown host ABI: '+str(row['Id']))
        for identifier in reached:
            if identifier in by_id:closed_host|=by_id[identifier]['HostAbi']
        if row['ClosureHostAbi']!=closed_host:errors.append('Incorrect host closure: '+str(row['Id']))
        imports={symbol for bit,symbol in host_abis.items() if host_abi & bit}
        missing = [symbol for symbol in row['Undefined'] if actual.get(symbol) not in row['Closure'] and
            not (symbol in imports and symbol in host_symbols)]
        links.append({'Id': row['Id'], 'Source': row['Source'], 'Build': row['Build'],
                      'Missing': missing, 'Complete': row['Build'] == 0 and not missing})
        errors.extend('Missing resident dependency: ' + symbol for symbol in missing)
    return {'RegistryErrors': errors, 'DeclaredExports': len(declared),
            'ActualExports': len(actual), 'ComponentCount': len(ids), 'Linkage': links}


def negative_controls(ids, entries, components, host_symbols, host_abis, compiled_owners):
    """Mutate the actual full passing population, never an invented toy catalog."""
    checks = []

    def reject(name, changed_entries, changed_components):
        result = inspect(ids, changed_entries, changed_components,host_symbols,host_abis,compiled_owners)
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
                 'xlSyncRetirementUnlockCheckedV1', 'xlSyncRetirementRetryCheckedV1',
                 'xlCancellationRetirementSnapshotCheckedV1', 'xlCancellationRetirementRetryCheckedV1'):
        mutated = copy.deepcopy(components)
        next(row for row in mutated if name in row['Defined'])['Defined'].remove(name)
        reject('missing-host-' + name, entries, mutated)
    row=next(item for item in components if item['HostAbi'])
    changed=copy.deepcopy(components);changed[row['Id']]['HostAbi']=0
    reject('undeclared-host-abi',entries,changed)
    changed=copy.deepcopy(components);changed[row['Id']]['HostAbi']=1<<31
    reject('unknown-host-abi',entries,changed)
    assert inspect(ids,entries,components,set(),host_abis,compiled_owners)['RegistryErrors']
    checks.append(dict(Name='missing-actual-host-definition',Rejected=True))
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--catalog-dir', type=Path, help='Reuse actual objects from the enclosing artifact gate')
    mode.add_argument('--compiler', type=Path, help='Build contract and every resident provider independently')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--host-object',type=Path,help='Actual host object/library for a reused provider catalog')
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists(), 'Preserve prior catalog evidence'
    cc, nm = shutil.which('gcc'), shutil.which('nm')
    assert cc and nm
    build_dir = args.catalog_dir.resolve() if args.catalog_dir else out.parent / (out.stem + '-objects')
    files = {Path(__file__).resolve(), Path(nm)}
    host_provider=None;host_inspector=None
    if args.catalog_dir:
        host_provider=args.host_object.resolve() if args.host_object else root/'release'/('xl.dll' if os.name=='nt' else 'libxl.so')
        assert host_provider.is_file(),'A host capability must be checked against an actual provider'
        host_inspector=shutil.which('objdump') if os.name=='nt' and host_provider.suffix=='.dll' else nm
        assert host_inspector;files.update((host_provider,Path(host_inspector)))
    files.update(root / path for path in ('src/compile/xl6_artifact_runtime.h','src/compile/xl6_artifact_host_abis.inc',
        'src/compile/xl6_artifact_runtime_symbols.inc', 'src/runtime/xl6_log_symbols.inc',
        'src/runtime/xl6_text_symbols.inc'))
    # Include all native/header inputs, not just the exported registry rows.
    files.update(path for folder in ('src', 'lib/xrt', 'lib/xruntime', 'release/include')
                 for path in (root / folder).rglob('*') if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    rows, responses = [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Scope': __doc__,
              'Platform': os.name, 'Rows': rows, 'ShippingLibraryAcceptance': False,
              'Full53Acceptance': False, 'StandaloneArtifactExecutionAcceptance': False}

    def run(name, command, input_path=None):
        command = list(map(str, command))
        actual = command
        if command[0] == str(cc):
            response = build_dir / (name + '.rsp')
            response.write_text('\n'.join('"'+arg.replace('\\', '/').replace('"', '\\"')+'"'
                for arg in command[1:])+'\n', encoding='utf-8')
            files.add(response)
            responses.append({'Path': str(response), 'Sha256': sha(response)})
            actual = [str(cc), '@'+str(response)]
        result = subprocess.run(actual, cwd=root, capture_output=True, timeout=180,
            input=None if input_path is None else Path(input_path).read_bytes())
        (build_dir / (name + '.log')).write_bytes(result.stdout + result.stderr)
        rows.append({'Name': name, 'Command': command, 'ActualCommand': actual,
            'InputPath': None if input_path is None else str(input_path), 'Exit': result.returncode})
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
        ids, entries = declarations(root)
        registered = {symbol for symbol, _ in entries}
        components = []
        for component in catalog:
            obj = build_dir / ('component-' + str(component['Id']) + '.o')
            if args.compiler:
                run('component-' + str(component['Id']), [cc, *flags, '-c', component['Source'], '-o', obj])
            assert obj.is_file(), 'Missing actual provider object: ' + str(obj)
            defined = re.findall(r'\b[A-Za-z]\s+(\w+)\s*$',
                run('defined-' + str(component['Id']), [nm, '-g', '--defined-only', obj]), re.M)
            undefined = re.findall(r'\bU\s+(\w+)\s*$',
                run('undefined-' + str(component['Id']), [nm, '-u', obj]), re.M)
            report.setdefault('Objects', []).append({'Path': str(obj), 'Sha256': sha(obj)})
            components.append(dict(component, Build=0, Defined=defined, Undefined=undefined))
        # Ask the real compiled catalog which names are Cgen entry points.
        # All other native globals keep their physical source ownership and
        # participate in the same full transitive undefined-symbol checks.
        observer = Path(__file__).resolve().parents[1] / 'test/support/xlir_resident_symbol_owners.c'
        observer_hash = sha(observer)
        owner_binary = build_dir / ('symbol-owners.exe' if windows else 'symbol-owners')
        run('symbol-owners-build', [cc, *flags, '-ffunction-sections', '-fdata-sections',
            '-fno-jump-tables', '-fno-asynchronous-unwind-tables', '-fno-unwind-tables',
            observer, 'src/compile/xl6_compile_unit.c', '-Wl,--gc-sections', '-o', owner_binary])
        names = sorted(registered | {name for row in components for name in row['Defined']})
        query = build_dir / 'symbol-owner-queries.txt'
        query.write_text(''.join(name+'\n' for name in names), encoding='utf-8')
        owner_text = run('symbol-owners', [owner_binary], input_path=query)
        records = [line.split('\t') for line in owner_text.splitlines()]
        assert all(len(row) == 3 and row[0] in ('D', 'A') for row in records)
        owner_rows = [row[1:] for row in records if row[0] == 'A']
        assert [row[0] for row in owner_rows] == names
        owner_ids = {name: int(owner) for name, owner in owner_rows}
        assert all(0 <= owner <= len(ids) for owner in owner_ids.values())
        compiled_owners = {}
        for _, name, raw_owner in (row for row in records if row[0] == 'D'):
            owner = int(raw_owner)
            assert 0 <= owner < len(ids)
            assert name not in compiled_owners or compiled_owners[name] == owner
            compiled_owners[name] = owner
        assert all(compiled_owners.get(name, len(ids)) == owner for name, owner in owner_ids.items())
        # The textual inventory is a subset check only. C preprocessing also
        # expands grouped engine/buffer declarations and all future additions.
        assert all(compiled_owners.get(name) == ids[owner] for name, owner in entries)
        report['ParsedDeclarationSubset'] = len(entries)
        owner_names = {owner: name for name, owner in ids.items()}
        entries = [(name, owner_names[owner]) for name, owner in compiled_owners.items()]
        for row in components:
            row['Defined'] = [name for name in row['Defined'] if name.startswith('xl') or name in compiled_owners]
            row['Undefined'] = [name for name in row['Undefined'] if name.startswith('xl') or name in compiled_owners]
        report['CompiledCgenEntryOwners'] = compiled_owners
        report['InternalCExports'] = {name: row['Id'] for row in components for name in row['Defined']
            if owner_ids[name] == len(ids)}
        observer_inputs = [{'Path': str(observer), 'Sha256': observer_hash}] + [
            {'Path': str(path), 'Sha256': sha(path)} for path in (owner_binary, query)]
        inputs.extend(observer_inputs)
        abi_text=(root/'src/compile/xl6_artifact_host_abis.inc').read_text(encoding='utf-8')
        declared_host=re.findall(r'XL6_ARTIFACT_HOST_ABI\((\w+),(\d+),"(\w+)"\)',abi_text)
        assert declared_host and len({int(bit) for _,bit,_ in declared_host})==len(declared_host)
        host_abis={int(bit):symbol for _,bit,symbol in declared_host}
        assert all(bit and not bit&(bit-1) for bit in host_abis)
        if args.compiler:
            host_object=next(Path(row['Path']) for row in build['Objects'] if Path(row['Path']).stem=='xl_api')
            host_text=run('host-definitions',[nm,'-g','--defined-only',host_object])
            host_symbols=set(re.findall(r'\b[A-Za-z]\s+(xl\w+)\s*$',host_text,re.M))
            report['ActualHostObject']=dict(Path=str(host_object),Sha256=sha(host_object))
        else:
            if os.name=='nt' and host_provider.suffix=='.dll':
                host_text=run('host-definitions',[host_inspector,'-p',host_provider])
                # Binutils can print either the compact name-pointer table or
                # the +base/ordinal form. Accept only complete export rows.
                host_symbols=set(re.findall(r'^\s*\[\s*\d+\]\s+(?:\+base\[\s*\d+\]\s+[0-9a-fA-F]+\s+)?(xl\w+)\s*$',host_text,re.M))
            else:
                host_text=run('host-definitions',[nm,*(['-D'] if host_provider.suffix=='.so' else []),'-g','--defined-only',host_provider])
                host_symbols=set(re.findall(r'\b[A-Za-z]\s+(xl\w+)\s*$',host_text,re.M))
            report['ActualHostObject']=dict(Path=str(host_provider),Sha256=sha(host_provider))
        report['DeclaredHostAbis']=declared_host
        report['ActualRequiredHostExports']=sorted(host_symbols & set(host_abis.values()))
        report.update(inspect(ids, entries, components,host_symbols,host_abis,compiled_owners))
        assert not report['RegistryErrors'], (len(report['RegistryErrors']), report['RegistryErrors'][:10])
        report['NegativeControls'] = negative_controls(ids, entries, components,host_symbols,host_abis,compiled_owners)
        report['DependencyPairs'] = len(ids) ** 2
        report['HostCheckedExports'] = sorted(symbol for row in components for symbol in row['Defined']
            if symbol.startswith(('xlSyncRetirement', 'xlCancellationRetirement')))
        report['CompilerResponses'] = responses
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256']
            for row in inputs + responses + report['Objects'])
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
