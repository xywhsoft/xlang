"""Compare native analysis over real stdio, or calibrate against the same image.

Pairs remain alive together; query order alternates for every generation.
This measures configured synthetic checks, not UI latency or full LSP coverage.
Windows supplies process CPU, cycles, memory and system commit counters.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes as w
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import sys
import time

from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import source, initialize, checked


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def distribution(values):
    ordered = sorted(values)
    if not ordered:
        raise ValueError('empty measurement distribution')
    return dict(samples=len(ordered), p50=ordered[math.ceil(.5*len(ordered))-1],
                p95=ordered[math.ceil(.95*len(ordered))-1], maximum=ordered[-1])


class SystemMemory(ctypes.Structure):
    _fields_ = [('length', w.DWORD), ('load', w.DWORD)] + [
        (name, ctypes.c_ulonglong) for name in (
            'totalPhysical', 'availablePhysical', 'totalCommit', 'availableCommit',
            'totalVirtual', 'availableVirtual', 'availableExtended')]


class ProcessMemory(ctypes.Structure):
    _fields_ = [('cb', w.DWORD), ('pageFaults', w.DWORD)] + [
        (name, ctypes.c_size_t) for name in (
            'peakWorkingSet', 'workingSet', 'peakPagedPool', 'pagedPool',
            'peakNonPagedPool', 'nonPagedPool', 'pagefile', 'peakPagefile', 'private')]


class Counters:
    def __init__(self):
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        self.psapi = ctypes.WinDLL('psapi', use_last_error=True)
        self.kernel.GetProcessTimes.argtypes = [w.HANDLE] + [ctypes.POINTER(w.FILETIME)]*4
        self.kernel.GetProcessTimes.restype = w.BOOL
        self.kernel.QueryProcessCycleTime.argtypes = [w.HANDLE, ctypes.POINTER(ctypes.c_ulonglong)]
        self.kernel.QueryProcessCycleTime.restype = w.BOOL
        self.kernel.GlobalMemoryStatusEx.argtypes = [ctypes.POINTER(SystemMemory)]
        self.kernel.GlobalMemoryStatusEx.restype = w.BOOL
        self.psapi.GetProcessMemoryInfo.argtypes = [w.HANDLE, ctypes.POINTER(ProcessMemory), w.DWORD]
        self.psapi.GetProcessMemoryInfo.restype = w.BOOL

    @staticmethod
    def require(okay):
        if not okay:
            raise ctypes.WinError(ctypes.get_last_error())

    def cpu(self, client):
        handle = int(client.process._handle)
        fields = [w.FILETIME() for _ in range(4)]
        self.require(self.kernel.GetProcessTimes(handle, *[ctypes.byref(v) for v in fields]))
        cycles = ctypes.c_ulonglong()
        self.require(self.kernel.QueryProcessCycleTime(handle, ctypes.byref(cycles)))
        milliseconds = sum((v.dwHighDateTime << 32)+v.dwLowDateTime for v in fields[2:])*.0001
        return milliseconds, cycles.value

    def memory(self, client):
        value = ProcessMemory()
        value.cb = ctypes.sizeof(value)
        self.require(self.psapi.GetProcessMemoryInfo(int(client.process._handle), ctypes.byref(value), value.cb))
        return {name: int(getattr(value, name)) for name in
                ('workingSet', 'peakWorkingSet', 'private', 'peakPagefile')}

    def qualify(self, rows, minimum, **details):
        value = SystemMemory()
        value.length = ctypes.sizeof(value)
        self.require(self.kernel.GlobalMemoryStatusEx(ctypes.byref(value)))
        row = dict(time=time.time(), requiredGiB=minimum, **details,
                   **{name: getattr(value, name) for name, _ in value._fields_})
        rows.append(row)
        if min(value.availablePhysical, value.availableCommit) < minimum*1024**3:
            raise RuntimeError('system memory/commit floor: '+json.dumps(row))


def make_body(count, generation, topology, edit):
    body = source(count, generation, True)
    if edit == 'declaration' and generation:
        needle = ' int local = value+0\r\n'
        if body.count(needle) != 1:
            raise ValueError('shared source fixture changed')
        body = body.replace(needle, needle+f' int inserted = {generation}\r\n', 1)
        body = body.replace(' return local\r\n', ' return local+inserted\r\n', 1)
    if topology == 'single':
        body += '\nfunc main(...) as int\n return compute(3)\nend\n'
    return body


def summarize(rows, cases):
    summary = {}
    for case in cases:
        selected = [row for row in rows if row['case'] == case]
        data = {phase: {variant: {metric: distribution([
            row[metric] for row in selected if row['variant'] == variant and row['phase'] == phase])
            for metric in ('ms', 'cpuMs', 'cycles')} for variant in ('baseline', 'candidate')}
            for phase in ('cold', 'first', 'hot', 'exact')}
        summary[case] = data
    return summary


def evaluate(rows, cases, declaration_ratios):
    summary, gates = summarize(rows, cases), []

    def gate(name, actual, limit, **details):
        gates.append(dict(name=name, actual=actual, limit=limit, passed=actual <= limit, **details))

    for case, (count, _, edit) in cases.items():
        selected, data = [row for row in rows if row['case'] == case], summary[case]
        for phase in data:
            before, after = [data[phase][v]['ms'] for v in ('baseline', 'candidate')]
            ratio = declaration_ratios[count] if phase == 'hot' and edit == 'declaration' else 1.05
            benefit = ratio < 1
            limit = before['p50']*ratio if benefit else max(before['p50']*ratio, before['p50']+.5)
            gate(case+'/'+phase+'/wireP50', after['p50'], limit, baseline=before['p50'], ratio=ratio)
            if phase == 'hot':
                gate(case+'/hot/wireP95', after['p95'], max(before['p95']*1.05, before['p95']+.5), baseline=before['p95'])
                a, b = [data[phase][v]['cpuMs']['p50'] for v in ('baseline', 'candidate')]
                gate(case+'/hot/cpuP50', b, a*1.05, baseline=a)
            if phase == 'exact':
                gate(case+'/exact/absoluteP50', after['p50'], 10)
        a, b = [max(row['memory']['private'] for row in selected if row['variant'] == v)
                for v in ('baseline', 'candidate')]
        gate(case+'/privateExtra', b-a, 192*1024*1024, baseline=a, candidate=b)
    return summary, gates


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--calibrate', action='store_true', help='Require identical images; failed relative gates expose variation; absolute budgets remain separate requirements.')
    modes.add_argument('--functional-only', action='store_true', help='Check transport, results, counters and cleanup without evaluating performance gates.')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--pairs', type=int, default=4, help='Independent process pairs per case, 2..32.')
    parser.add_argument('--generations', type=int, default=14, help='Cold plus changed versions, 4..64; then one exact repeat.')
    parser.add_argument('--functions', default='32,512')
    parser.add_argument('--topologies', default='single,imported')
    parser.add_argument('--edits', default='literal,declaration')
    parser.add_argument('--declaration-ratio-32', type=float, default=1.05)
    parser.add_argument('--declaration-ratio-512', type=float, default=1.05)
    parser.add_argument('--minimum-available-gib', type=float, default=3)
    parser.add_argument('--initial-available-gib', type=float, default=5)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('this process-counter backend requires Windows')
    if not 2 <= args.pairs <= 32 or not 4 <= args.generations <= 64:
        parser.error('use 2..32 pairs and 4..64 generations')
    try:
        functions = [int(v) for v in args.functions.split(',')]
    except ValueError:
        parser.error('functions must be comma-separated integers')
    topologies, edits = args.topologies.split(','), args.edits.split(',')
    if not functions or len(set(functions)) != len(functions) or any(v not in (32, 512) for v in functions):
        parser.error('current fixtures support distinct 32,512 counts')
    if not topologies or len(set(topologies)) != len(topologies) or set(topologies)-{'single', 'imported'}:
        parser.error('use distinct single,imported topologies')
    if not edits or len(set(edits)) != len(edits) or set(edits)-{'literal', 'declaration'}:
        parser.error('use distinct literal,declaration edits')
    ratios = {32: args.declaration_ratio_32, 512: args.declaration_ratio_512}
    if any(not math.isfinite(v) or not 0 < v <= 1.05 for v in ratios.values()):
        parser.error('declaration ratios must be in (0,1.05]')
    if not (math.isfinite(args.minimum_available_gib) and math.isfinite(args.initial_available_gib)
            and 0 < args.minimum_available_gib <= args.initial_available_gib):
        parser.error('require finite positive memory floor and initial headroom >= floor')
    if not args.functional_only and (args.minimum_available_gib < 3 or args.initial_available_gib < 5):
        parser.error('performance runs require >=3 GiB per-query floor and >=5 GiB initial headroom')
    if (args.calibrate or args.functional_only) and any(v != 1.05 for v in ratios.values()):
        parser.error('benefit ratios apply only to comparison mode')
    if not args.calibrate and not args.functional_only and not args.candidate:
        parser.error('comparison requires --candidate')
    servers = dict(baseline=args.baseline.resolve(strict=True),
                   candidate=(args.candidate or args.baseline).resolve(strict=True))
    if args.calibrate and sha(servers['baseline']) != sha(servers['candidate']):
        parser.error('A/A calibration requires byte-identical executable images')
    # Record the known xlang runtime DLLs as well as executable identity.
    for name in ('xge.dll', 'xl.dll', 'xlc.dll'):
        a, b = [p.with_name(name) for p in servers.values()]
        if args.calibrate and (a.exists() != b.exists() or (a.exists() and sha(a) != sha(b))):
            parser.error('A/A runtime mismatch: '+name)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    protected = {str(p): sha(p) for p in [*servers.values(), Path(__file__), *[
        Path(__file__).with_name(n) for n in ('check_lsp_automatic_diagnostics.py',
            'check_lsp_module_semantics.py', 'check_language_snapshots.py',
            'check_language_definitions.py', 'check_lsp_analysis_configuration.py')]]}
    for server in servers.values():
        for name in ('xge.dll', 'xl.dll', 'xlc.dll'):
            dll = server.with_name(name)
            if dll.is_file():
                protected[str(dll)] = sha(dll)
    cases = {f'{count}-{topology}-{edit}': (count, topology, edit)
             for count in functions for topology in topologies for edit in edits}
    prepared = {}
    for name, (_, topology, _) in cases.items():
        folder = output/name
        folder.mkdir()
        entry = folder/'main.xl'
        dep = folder/'dep.xl' if topology == 'imported' else entry
        members = list(dict.fromkeys((entry, dep)))
        for p in members:
            p.write_bytes(b'// real analysis uses immutable unsaved overlays\n')
            protected[str(p)] = sha(p)
        prepared[name] = folder, entry, dep, members
    plan = dict(format='xlang-lsp-analysis-pairs-1', registeredUtc=time.time(),
        mode='functional-only' if args.functional_only else 'calibration' if args.calibrate else 'comparison', cases=cases,
        pairs=args.pairs, generations=list(range(args.generations))+[args.generations-1],
        declarationRatios=ratios, protected=protected,
        expectedQueries=len(cases)*args.pairs*2*(args.generations+1),
        expectedProcesses=len(cases)*args.pairs*2,
        order='Start AB/BA by pair parity. Alternate AB/BA before EVERY query generation.',
        clock='didOpen/didChange through complete checked stdio reply; source generation excluded',
        classifications=dict(cold='generation0', first='generation1', hot='generation2..last', exact='unchanged last-generation repeat'),
        resourceFloorGiB=args.minimum_available_gib, initialHeadroomGiB=args.initial_available_gib,
        gates='Original 5percent P50/P95/CPU nonregression,0.5ms wall tolerance,10ms cached P50,192MiB additional private. Optional declared hot-edit benefit ratios. Cycles are diagnostic,not a replacement CPU gate.',
        limits=['Synthetic native xlang/check;not UI latency,whole project orfullLSP qualification.',
                'Two idle servers retain memory;this paired environment differs from sequential-process screens.',
                'Shared nonexclusive host. No own build,other QA orsource/artifact hashing during requests.',
                'All successful slow samples retained. Failed resource/protocol/input check invalidates whole run.',
                'CPU counters may have approximately15.625ms resolution. Memory samples are after replies;OS lifetime peakworking set also reported.',
                'Failed A/A relative gates expose variation;an absolute-budget failure can also indicate a slow baseline. Comparison alone doesnot establish causalattribution or final feature acceptance.',
                'Functional-only mode evaluates no performance gates and supports no performance conclusion.'],
        machine=dict(platform=platform.platform(), processor=platform.processor(), logicalCpus=os.cpu_count()))
    plan_path = output/'plan.json'
    plan_path.write_text(json.dumps(plan, indent=2)+'\n', encoding='utf8', newline='\n')
    record = dict(status='running', accepted=False, plan=str(plan_path), planSha256=sha(plan_path),
                  resources=[], queries=[], processes=[])

    def publish():
        (output/'result.json').write_text(json.dumps(record, ensure_ascii=False, indent=2)+'\n', encoding='utf8', newline='\n')

    counters, answers = Counters(), {}
    removed = {name: os.environ.pop(name, None) for name in ('XLS_CHECK_PHASE_TRACE', 'XLS_FUNCTION_STAGE_VALIDATE')}
    try:
        counters.qualify(record['resources'], args.initial_available_gib, phase='initial')
        publish()
        for case, (count, topology, edit) in cases.items():
            folder, entry, dep, members = prepared[case]
            for pair in range(args.pairs):
                variants = ('baseline', 'candidate')
                clients = {}
                try:
                    for variant in variants[::1 if pair%2 == 0 else -1]:
                        counters.qualify(record['resources'], args.minimum_available_gib, phase='process', case=case, pair=pair, variant=variant)
                        client = AutomaticClient(servers[variant], output/'absent/xl.exe', folder)
                        clients[variant] = client
                        initialize(client, entry, members, folder)
                        if topology == 'imported':
                            client.open(entry, 'import dep\nfunc main(...) as int\n return dep.compute(3)\nend\n', 1)
                    for step, generation in enumerate(plan['generations']):
                        repeat = step == args.generations
                        phase = 'exact' if repeat else 'cold' if not generation else 'first' if generation == 1 else 'hot'
                        if not repeat:
                            body = make_body(count, generation, topology, edit)
                        for variant in variants[::1 if (step+pair)%2 == 0 else -1]:
                            client = clients[variant]
                            counters.qualify(record['resources'], args.minimum_available_gib, phase='query', case=case, pair=pair, variant=variant, generation=generation, repeat=repeat)
                            before_cpu, before_cycles = counters.cpu(client)
                            started = time.perf_counter_ns()
                            if not repeat:
                                if generation:
                                    client.change(dep, body, generation+1)
                                else:
                                    client.open(dep, body, 1)
                            value = checked(client, entry, generation+1 if topology == 'single' else 1)
                            elapsed = (time.perf_counter_ns()-started)/1e6
                            after_cpu, after_cycles = counters.cpu(client)
                            if not (value['ok'] and not value['count'] and value['semanticDefinitions']):
                                raise RuntimeError('native semantic check failed: '+json.dumps(value))
                            if value['analysis']['workerThreadId'] == value['analysis']['captureThreadId']:
                                raise RuntimeError('check did not use independent native worker')
                            facts = {k: v for k, v in value.items() if k not in ('analysis', 'durationMs')}
                            key = case, generation, repeat
                            if key in answers and answers[key] != facts:
                                raise RuntimeError(f'standard result mismatch: {key}, {variant}')
                            answers.setdefault(key, facts)
                            record['queries'].append(dict(case=case, pair=pair, variant=variant,
                                generation=generation, phase=phase, ms=elapsed, cpuMs=after_cpu-before_cpu,
                                cycles=after_cycles-before_cycles, memory=counters.memory(client), result=value))
                finally:
                    original_error = sys.exc_info()[1]
                    cleanup_errors = []
                    for variant, client in clients.items():
                        stderr, errors = '', []
                        try:
                            stderr = client.stop()
                        except BaseException as error:
                            errors.append(repr(error))
                            # Close every owned process even when another
                            # client's shutdown or transcript write failed.
                            if client.process.poll() is None:
                                try:
                                    client.process.kill()
                                    client.process.wait(timeout=5)
                                except BaseException as kill_error:
                                    errors.append(repr(kill_error))
                        sp, tp = [folder/f'{pair}-{variant}-{suffix}' for suffix in ('stderr.log', 'transcript.json')]
                        try:
                            sp.write_text(stderr, encoding='utf8', newline='\n')
                            tp.write_text(json.dumps(client.transcript, ensure_ascii=False, indent=2)+'\n', encoding='utf8', newline='\n')
                        except BaseException as write_error:
                            errors.append(repr(write_error))
                        record['processes'].append(dict(case=case, pair=pair, variant=variant,
                            pid=client.process.pid, exit=client.process.returncode,
                            stderr=str(sp), transcript=str(tp), cleanupErrors=errors))
                        cleanup_errors.extend(errors)
                    publish()
                    if cleanup_errors and original_error is None:
                        raise RuntimeError('client cleanup failed: '+repr(cleanup_errors))
                if any(client.process.returncode for client in clients.values()):
                    raise RuntimeError('server process exit failure')
                print(case, pair, 'pair complete', flush=True)
        if len(record['queries']) != plan['expectedQueries'] or len(record['processes']) != plan['expectedProcesses']:
            raise RuntimeError('incomplete measurement set')
        for row in record['processes']:
            row['stderrSha256'], row['transcriptSha256'] = sha(row['stderr']), sha(row['transcript'])
        if any(sha(p) != digest for p, digest in protected.items()) or sha(plan_path) != record['planSha256']:
            raise RuntimeError('protected source, executable, fixture or plan changed')
        if args.functional_only:
            record['summary'], record['gates'] = summarize(record['queries'], cases), []
        else:
            record['summary'], record['gates'] = evaluate(record['queries'], cases, ratios)
        failed = sum(not row['passed'] for row in record['gates'])
        record['status'] = ('calibration-failed' if args.calibrate else 'gates-failed') if failed else 'passed'
        record['failedGates'] = failed
        publish()
        print(json.dumps(dict(status=record['status'], queries=len(record['queries']),
            processes=len(record['processes']), gates=len(record['gates']), failed=failed,
            result=str(output/'result.json'))), flush=True)
        return 2 if failed else 0
    except BaseException as error:
        record.update(status='failed', error=repr(error))
        publish()
        raise
    finally:
        for name, value in removed.items():
            if value is not None:
                os.environ[name] = value


if __name__ == '__main__':
    sys.exit(main())
