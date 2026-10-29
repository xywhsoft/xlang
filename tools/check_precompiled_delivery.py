#!/usr/bin/env python3
"""Check source-free native and nested compiler EXE/shared-library delivery.

Uses ordinary local compilation and a millisecond timestamp roundtrip only.
Run in a dedicated output directory, away from the producer's source tree.
"""
from pathlib import Path
import argparse
import ctypes
import hashlib
import json
import os
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xl', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    xl, out = args.xl.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    rows = []
    report = dict(complete=False, compiler=str(xl),
                  compilerSha256=hashlib.sha256(xl.read_bytes()).hexdigest(), rows=rows)
    exe = '.exe' if os.name == 'nt' else ''
    shared = '.dll' if os.name == 'nt' else '.so'

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(name, command):
        start = time.perf_counter()
        result = subprocess.run(list(map(str, command)), cwd=out, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=180)
        output = result.stdout.decode('utf-8', 'replace')
        rows.append(dict(name=name, exit=result.returncode, seconds=time.perf_counter()-start, output=output))
        save()
        if result.returncode:
            raise RuntimeError(name + '\n' + output)

    try:
        source = out / 'native.c'
        source.write_text('#define XRT_MODULE_TIME\n#include <xrt.h>\n'
            '#ifdef _WIN32\n__declspec(dllexport)\n#endif\n'
            'int native_time(void){xtime t;int64 ms;return '
            'xrtTimeFromUnixMs(1234,&t)&&xrtTimeToUnixMs(t,&ms)&&ms==1234?34:-1;}\n'
            'int main(void){return native_time()==34?0:1;}\n', encoding='utf-8')
        run('native-build', [xl, 'build', source, '-o', out / ('native' + exe)])
        run('native-independent-process', [out / ('native' + exe)])
        run('native-shared-build', [xl, 'build', source, '--shared', '-o', out / ('native' + shared)])
        native = ctypes.CDLL(str(out / ('native' + shared)))
        native.native_time.restype = ctypes.c_int
        if native.native_time() != 34 or not native.xrtErrorFree:
            raise RuntimeError('native shared library did not retain its own XRT')
        rows.append(dict(name='native-shared-call', exit=0))
        child = out / ('child' + exe)
        child_source = 'import xlang\nassert xlang.run("assert 6 * 7 == 42\\n")\n'
        c_source = '#include <xlc.h>\n#include <stdio.h>\nint main(void){' \
            'XLC_Context *ctx=xlcCreateContext(NULL);if(!ctx)return 1;' \
            'if(!xlcAddSourceString(ctx,"child.xl",' + json.dumps(child_source) + \
            ')||!xlcCompile(ctx)||!xlcEmitExe(ctx,' + json.dumps(child.as_posix()) + \
            ')){fprintf(stderr,"%s\\n",xlcGetLastError(ctx));xlcDestroyContext(ctx);return 2;}' \
            'xlcDestroyContext(ctx);return 0;}\n'
        source = out / 'parent.xl'
        source.write_text('import xlang.compiler\nimport tcc\nassert tcc.run(' +
                          json.dumps(c_source) + ') == 0\n', encoding='utf-8')
        run('parent-build', [xl, 'build', source, '-o', out / ('parent' + exe)])
        run('parent-independent-process', [out / ('parent' + exe)])
        run('child-independent-process', [child])
        source = out / 'shared.xl'
        source.write_text('import xlang.compiler\nimport tcc\n'
            'export "nested_compile" func check() as int\n    return tcc.run(' +
            json.dumps(c_source) + ')\nend\n', encoding='utf-8')
        run('compiler-shared-build', [xl, 'build', source, '--shared', '-o', out / ('compiler' + shared)])
        compiler = ctypes.CDLL(str(out / ('compiler' + shared)))
        compiler.nested_compile.restype = ctypes.c_int64
        if compiler.nested_compile() != 0 or not compiler.xrtErrorFree:
            raise RuntimeError('shared compiler could not generate an independent child')
        rows.append(dict(name='compiler-shared-call', exit=0))
        run('shared-child-independent-process', [child])
        report['complete'] = True
        print('native and nested compiler EXE/shared delivery: ok')
    finally:
        save()


if __name__ == '__main__':
    main()
