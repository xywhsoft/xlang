"""Build a declaration-only consumer of unchanged shared compiler frontend sources."""
import json
from pathlib import Path
import shutil
import subprocess
import sys

from catalog_sources import sha

SOURCES = ('src/lexer/xl6_token.c', 'src/lexer/xl6_lexer_api.c', 'src/lexer/xl6_lexer_gen.c',
           'src/parser/xl6_parser.c', 'src/parser/xl6_parser_api.c', 'src/ast/xl6_ast.c',
           'src/ast/xl6_ast_dump.c', 'src/diag/xl6_diag.c', 'src/common/xl6_output.c',
           'src/common/xl6_console.c', 'src/sem/xl6_completion.c')


def build(root, output, gcc=None):
    root, output = Path(root).resolve(), Path(output).resolve()
    gcc = Path(gcc or shutil.which('gcc') or '').resolve(strict=True)
    output.mkdir(parents=True)
    windows = sys.platform == 'win32'
    runtime = root / ('release/xlc.dll' if windows else 'release/libxlc.so')
    inputs = [root / p for p in SOURCES] + list((root / 'src').rglob('*.h'))
    inputs += [root / 'lib/xrt/xrt.h', root / 'lib/xruntime/xruntime.h', runtime,
               Path(__file__), Path(__file__).with_name('dump_extension_catalog.c'), gcc]
    before = {str(p): sha(p) for p in inputs}
    calls = []
    report = dict(status='running', inputs=before, calls=calls)
    report_file = output / 'build-report.json'

    def run(arguments):
        result = subprocess.run([str(gcc), *arguments], capture_output=True, timeout=180)
        calls.append(dict(arguments=arguments, exitCode=result.returncode,
                          stdout=result.stdout.decode('utf-8', errors='replace'), stderr=result.stderr.decode('utf-8', errors='replace')))
        report_file.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        if result.returncode:
            raise ValueError('Catalog frontend build failed: ' + calls[-1]['stderr'])

    flags = ['-std=c11', '-O2', '-ffunction-sections', '-fdata-sections', '-I' + str(root), '-I' + str(root / 'lib/xrt')]
    objects = []
    for i, source in enumerate([root / p for p in SOURCES] + [Path(__file__).with_name('dump_extension_catalog.c')]):
        target = output / (str(i) + '.o')
        strict = ['-Wall', '-Wextra', '-Werror'] if source.name == 'dump_extension_catalog.c' else []
        run(flags + strict + ['-c', str(source), '-o', str(target)])
        objects.append(str(target))
    executable = output / ('dump-extension-catalog.exe' if windows else 'dump-extension-catalog')
    run(objects + [str(runtime), '-Wl,--gc-sections', '-lm'] + (['-Wl,--no-insert-timestamp', '-lbcrypt', '-lole32', '-lws2_32', '-ladvapi32'] if windows else ['-ldl', '-pthread', '-Wl,-rpath,$ORIGIN']) + ['-o', str(executable)])
    shutil.copy2(runtime, output / runtime.name)
    if any(sha(Path(p)) != value for p, value in before.items()):
        raise ValueError('Catalog frontend build inputs changed')
    report.update(status='passed', executableSha256=sha(executable), runtimeSha256=sha(runtime))
    report_file.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return executable, report
