"""Associate genuine-SO public bindings with passing Windows/Linux product cases.

This read-only WSL audit rehashes reports, their inputs, children and logs, then
requires an identical source fixture that passed JIT/module and relocated AOT on
both platforms for every public signature. Reachable functions are not runtime
branch instrumentation: this is execution-witness association, not full53,
branch completeness, arbitrary GUI success or scheduler-wide OOM acceptance.
"""
from pathlib import Path, PureWindowsPath
import argparse
import hashlib
import json
import os
import re
import sys


LANES = ('windows-products', 'windows-aot', 'linux-products', 'linux-aot')


def physical(value):
    """Resolve evidence paths without altering or rewriting hashed reports."""
    if re.match(r'^[A-Za-z]:[\\/]', str(value)):
        windows = PureWindowsPath(value)
        return Path('/mnt') / windows.drive[0].lower() / Path(*windows.parts[1:])
    path = Path(value)
    assert path.is_absolute(), ('Evidence path must be absolute', value)
    return path


class Evidence:
    def __init__(self):
        self.files = {}
        self.reports = {}

    def hash(self, value):
        path = physical(value)
        stat = path.stat()
        key = (str(path), stat.st_size, stat.st_mtime_ns)
        if key not in self.files:
            self.files[key] = hashlib.sha256(path.read_bytes()).hexdigest()
        return self.files[key]

    def record(self, value, expected):
        assert self.hash(value) == expected.lower(), ('Evidence changed', value)

    def report(self, value, population=True):
        path = physical(value)
        if path in self.reports:
            if population:
                assert not self.reports[path].get('ExplicitSubset'), ('Subset is not a complete population', path)
            return self.reports[path]
        digest = self.hash(path)
        raw = path.read_bytes()
        assert hashlib.sha256(raw).hexdigest() == digest, ('Report changed while reading', path)
        data = json.loads(raw.decode('utf-8-sig'))
        assert data['Complete'] and data['InputsUnchanged'], path
        assert not data.get('Failures') and not data.get('Errors'), path
        if population:
            assert not data.get('ExplicitSubset'), ('Subset is not a complete population', path)
        for item in data.get('Inputs', []) + data.get('Products', []) + data.get('Objects', []):
            self.record(item['Path'], item['Sha256'])
        for row in data.get('Rows', []):
            if row.get('Log'):
                self.record(row['Log'], row.get('LogSha256', row.get('Sha256')))
            if row.get('Stderr'):
                self.record(row['Stderr'], row['StderrSha256'])
        for case in data.get('Cases', []):
            if case.get('Image'):
                self.record(case['Image']['Path'], case['Image']['Sha256'])
        self.reports[path] = data
        for child in data.get('Children', []):
            assert child['Passed'] and child['Exit'] == 0, child
            self.record(child['Report'], child['Sha256'])
            if child['Name'] != 'headers':
                self.report(child['Report'], population=False)
        return data


def source_inputs(data):
    result = {}
    for row in data['Inputs']:
        path = physical(row['Path'])
        if path.suffix == '.xl':
            result.setdefault(path.stem, set()).add(row['Sha256'].lower())
    return result


def product_hashes(data, windows):
    names = ('xl.exe', 'xl.dll', 'xlc.dll') if windows else ('xl', 'libxl.so', 'libxlc.so')
    result = {}
    for name in names:
        matches = {r['Sha256'].lower() for r in data['Inputs']
                   if physical(r['Path']).name == name and physical(r['Path']).parent.name == 'release'}
        assert len(matches) == 1, ('Ambiguous or absent genuine product', name, matches)
        result[name] = matches.pop()
    return result


def passing_cases(data, aot):
    assert data['AllExecutablePassed']
    assert data['CandidateCount'] == len(data['Cases'])
    assert not data['Full53Acceptance'] and not data['FunctionCoverageAcceptance']
    if aot:
        assert data['RelocatedAotInventoryAcceptance'] and data['SelectedCount'] == data['CandidateCount']
    else:
        assert data['SourceProductAcceptance'] and data['EmbeddedVfsWithoutFallback'] and not data['ProviderOverrides']
    passed = {}
    for case in data['Cases']:
        if case['Status'] != 'passed':
            continue
        assert case['Executed'] and case['Passed'], case
        rows = case['Rows']
        assert all(r['Exit'] == 0 for r in rows), case
        assert any(r['Name'] == case['Name'] + '-run' for r in rows), case
        assert any(r['Name'] == case['Name'] + ('-build' if aot else '-module') for r in rows), case
        if aot:
            assert case['Image'] and not any('libxl' in n.lower() or n.lower() in ('xl.dll', 'xlc.dll')
                                           for n in case['Dependencies']), case
        assert case['Name'] not in passed, case
        passed[case['Name']] = case
    return passed


def associate(census, reports):
    assert census['DiscoveryComplete'] and census['UsesActualCompilerSO'] and not census['CompilerSourceObjects']
    assert census['ReachableFunctionBindingAcceptance'] and census['CallReachabilityControl']
    assert not census['RuntimeExecutionAcceptance'] and not census['Full53Acceptance']
    for key in ('MissingNativeBindings', 'UnexpectedNativeBindings', 'MissingStaticBindings', 'MissingClassBindings',
                'UnreachableNativeBindings', 'UnreachableStaticBindings', 'UnreachableClassBindings'):
        assert not census[key], key
    assert product_hashes(reports['windows-products'], True) == product_hashes(reports['windows-aot'], True)
    assert product_hashes(reports['linux-products'], False) == product_hashes(reports['linux-aot'], False) == product_hashes(census, False)
    populations = [{c['Name'] for c in d['Cases']} for d in reports.values()]
    assert all(p == populations[0] for p in populations), 'Different execution populations'
    sources = {lane: source_inputs(data) for lane, data in reports.items()}
    passed = {lane: passing_cases(data, lane.endswith('-aot')) for lane, data in reports.items()}
    common = set.intersection(*(set(v) for v in passed.values()))
    catalog = {case['Name']: case for case in census['Cases']}
    inputs = {r['Path']: r['Sha256'].lower() for r in census['Inputs']}
    groups = {'native': census['NativeBindings'], 'static': census['StaticBindings'],
              'class': [m for m in census['ClassMembers'] if m['Category'] == 'callable']}
    assert len(groups['native']) == census['NativeExpectedCount'] == census['NativeBoundCount']
    assert len(groups['static']) == census['StaticExpectedCount'] == census['StaticBoundCount']
    assert len(groups['class']) == census['ClassCallableCount'] == census['ClassReachableCount']
    assert all(groups.values()), 'Empty signature population'
    witnesses = []; missing = []
    for category, bindings in groups.items():
        for binding in bindings:
            identity = {k: v for k, v in binding.items() if k not in ('Fixtures', 'ReachableFunctionFixtures')}
            candidates = sorted(common & set(binding['ReachableFunctionFixtures']))
            accepted = []
            for name in candidates:
                source = catalog[name]['Row']['Command'][-1]
                source_hash = inputs[source]
                if all(source_hash in sources[lane].get(name, ()) for lane in LANES):
                    accepted.append(dict(Name=name, SourceSha256=source_hash))
            row = dict(Category=category, Binding=identity, Witnesses=accepted)
            (witnesses if accepted else missing).append(row)
    return witnesses, missing, {k: len(v) for k, v in groups.items()}, len(populations[0])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('census-report', *LANES, 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    assert os.name != 'nt', 'Run this read-only cross-platform evidence audit inside WSL'
    assert not args.out.exists()
    evidence = Evidence()
    inputs = [dict(Path=str(Path(__file__).resolve()), Sha256=evidence.hash(Path(__file__).resolve())),
              dict(Path=str(Path(sys.executable).resolve()), Sha256=evidence.hash(Path(sys.executable).resolve()))]
    result = dict(Complete=False, InputsUnchanged=False, Scope=__doc__, Inputs=inputs, Failures=[],
                  Full53Acceptance=False, RuntimeBranchCoverageAcceptance=False, Witnesses=[], MissingWitnesses=[])
    try:
        census = evidence.report(args.census_report)
        reports = {lane: evidence.report(getattr(args, lane.replace('-', '_'))) for lane in LANES}
        for path in (args.census_report, *(getattr(args, lane.replace('-', '_')) for lane in LANES)):
            inputs.append(dict(Path=str(path.resolve()), Sha256=evidence.hash(path.resolve())))
        witnesses, missing, counts, population = associate(census, reports)
        result.update(Witnesses=witnesses, MissingWitnesses=missing, SignatureCounts=counts, CandidateCount=population,
                      PassingProductWitnessAcceptance=not missing,
                      PlatformAndHostedConditions={k: [c for c in d['Cases'] if c['Status'] != 'passed'] for k, d in reports.items()})
        result['InputsUnchanged'] = all(evidence.hash(i['Path']) == i['Sha256'] for i in inputs)
        for key, digest in list(evidence.files.items()):
            evidence.record(key[0], digest)
        # Record every transitive artifact checked, not just JSON filenames.
        result['VerifiedFiles'] = [dict(Path=k[0], Size=k[1], MtimeNs=k[2], Sha256=v) for k, v in sorted(evidence.files.items())]
        result['Complete'] = result['InputsUnchanged'] and not missing
    except (AssertionError, OSError, KeyError, ValueError) as error:
        result['Failures'].append(repr(error))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k in
                     ('Complete', 'InputsUnchanged', 'Failures', 'SignatureCounts', 'CandidateCount')}, ensure_ascii=False), flush=True)
    return 0 if result['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
