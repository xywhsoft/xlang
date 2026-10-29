#!/usr/bin/env python3
"""Offline foreign graph lifecycle, allocation rollback and code residency tests."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--compiler', type=Path, required=True, help='GCC or Clang')
    args = parser.parse_args()
    root, out, cc = args.root.resolve(), args.out.resolve(), args.compiler.resolve()
    out.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    inputs = {str(p): sha(p) for folder in ['src', 'release/include', 'lib/xrt', 'lib/xruntime']
              for p in (root / folder).rglob('*') if p.is_file()}
    rows = []
    report = dict(complete=False, inputs=inputs, rows=rows, networkTests=False)
    exe = '.exe' if os.name == 'nt' else ''
    shared = '.dll' if os.name == 'nt' else '.so'
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(root),
             '-I' + str(root/'release'), '-I' + str(root/'release/include'), '-I' + str(out)]
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
    libraries = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else ['-pthread', '-ldl', '-lm']

    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

    def run(name, command):
        start = time.monotonic()
        command = list(map(str, command))
        result = subprocess.run(command, cwd=out, capture_output=True, timeout=180)
        log = out/(name+'.log')
        log.write_bytes(result.stdout+result.stderr)
        rows.append(dict(name=name, command=command, exit=result.returncode, log=str(log),
                         sha256=sha(log), seconds=time.monotonic()-start))
        save()
        print(name, result.returncode, flush=True)
        if result.returncode:
            raise RuntimeError(str(log))
        return (result.stdout+result.stderr).decode('utf-8', 'replace')

    plan = out/('plan'+exe)
    run('plan.build', [cc, *flags, '-O0', '-Wno-missing-field-initializers',
        '-ffunction-sections', '-fdata-sections', '-fno-jump-tables', '-fno-unwind-tables',
        '-fno-asynchronous-unwind-tables', root/'test/support/foreign_graph_plan.c',
        root/'src/compile/xl6_compile_unit.c', '-Wl,--gc-sections', '-o', plan])
    sources = run('plan.sources', [plan, '--sources']).splitlines()
    modules = run('plan.modules', [plan, '--modules'])
    assert '_MODULE_ALL' not in modules
    (out/'foreign-graph-modules.h').write_text(modules, encoding='utf-8')
    selected = [root/source for source in sources]
    for level in ['O0', 'O2']:
        product = out/(level+exe)
        run(level+'.build', [cc, *flags, '-'+level, root/'test/support/foreign_graph.c',
                            *selected, *libraries, '-o', product])
        run(level+'.run', [product])
    native = out/'native.c'
    native.write_text('#include "foreign-graph-modules.h"\n'
        '#define XRT_MODULE_NET_ENGINE\n#define XRT_MODULE_NET_PORT_SELECT\n'
        '#include "lib/xruntime/xruntime_features.h"\n#define XRT_IMPLEMENTATION\n'
        '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'
        '#include "lib/xruntime/xruntime.h"\n', encoding='utf-8')
    a, b = out/('producer'+shared), out/('adapter'+shared)
    for name, product in [('producer', a), ('adapter', b)]:
        run(name+'.build', [cc, *flags, '-O2', '-shared',
            *([] if os.name == 'nt' else ['-fPIC']),
            root/'test/cases/xlir_foreign_resource_module.c', '-o', product])
    host = out/('module-host'+exe)
    run('module-host.build', [cc, *flags, '-O2', native,
        root/'test/cases/xlir_foreign_resource_module_host.c', *selected, *libraries, '-o', host])
    run('leaf-code-residency', [host, a, b])
    run('graph-code-residency', [host, a, b, 'graph'])
    run('task-code-residency', [host, a, b, 'task'])
    report['inputsUnchanged'] = all(sha(Path(p)) == digest for p, digest in inputs.items())
    assert report['inputsUnchanged']
    report['complete'] = True
    save()


if __name__ == '__main__':
    main()
