"""Run the owned Timeline render fixture against an explicit compiler cohort."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from foreign_compiler_cohort import load_compiler_cohort


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    keys = ('root', 'compiler', 'compiler-report', 'native-root', 'dynamic-packages', 'static-packages', 'out')
    for key in keys:
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--render-example', action='store_true', help='Check the visible highlight in a two-frame capture')
    args = parser.parse_args()
    root, compiler, build, native, dynamic, static, out = (
        getattr(args, key.replace('-', '_')).resolve() for key in keys
    )
    sdk, compiler_inputs = load_compiler_cohort(root, compiler, build)
    win = os.name == 'nt'
    tools = {name: Path(shutil.which(name)).resolve() for name in ('gcc', 'g++', 'objdump')}
    case = root / 'test/cases/foreign_xui_timeline_renderers.xl'
    files = [Path(__file__), compiler, build, *compiler_inputs, *tools.values(), case,
        case.with_suffix('.c'), case.with_suffix('.h')]
    files.extend(case.with_name('foreign_xui_render_' + name) for name in ('backend.c', 'backend.h', 'adapters.inc'))
    files.extend(native / 'tests/xui' / name for name in ('xui_test_proxy.c', 'xui_test_proxy.h'))
    files.extend(compiler.parent / name for name in (('xl.dll', 'xlc.dll') if win else ('libxl.so', 'libxlc.so')))
    example = dynamic / 'xui/examples/timeline_renderers.xl'
    font = root / 'lib/bindings/xge/examples/data/font_fixture.ttf'
    if args.render_example:
        files.append(font)
    for directory in (sdk, dynamic, static):
        files.extend(path for path in directory.rglob('*') if path.is_file())

    def digest(path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()

    inputs = {str(path): digest(path) for path in files}
    out.mkdir(parents=True)
    report = dict(complete=False, passed=False, networkTests=False, inputs=inputs, rows=[], programs=[])

    def save():
        temporary = out / 'results.json.tmp'
        temporary.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        temporary.replace(out / 'results.json')

    def run(label, command, packages, render=False):
        env = {key: value for key, value in os.environ.items() if not key.startswith('XL6_')}
        env['XLANG_PACKAGE_PATH'] = str(packages)
        key = 'PATH' if win else 'LD_LIBRARY_PATH'
        env[key] = os.pathsep.join((str(packages/'_xge_native/native'), str(compiler.parent), env.get(key, '')))
        if render:
            env.update(XUI_EXAMPLE_FRAMES='2', XUI_EXAMPLE_FONT=str(font), XUI_EXAMPLE_DUMP=str(out/'example.rgba'),
                XDG_CACHE_HOME=str(out/'cache'))
            if not win:
                env.update(LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe', LP_NUM_THREADS='1')
                command = ['xvfb-run', '-a', '-s', '-screen 0 1024x768x24 -nolisten tcp', *command]
        command = list(map(str, command))
        process = subprocess.run(command, cwd=out, env=env, capture_output=True, timeout=300)
        output = process.stdout + process.stderr
        log = out / (label + '.log')
        log.write_bytes(output)
        report['rows'].append(dict(name=label, command=command, exitCode=process.returncode, logSha256=digest(log)))
        save()
        print(label, process.returncode, flush=True)
        if process.returncode:
            print(output.decode('utf-8', errors='replace')[-4000:], flush=True)
            raise RuntimeError(label)
        return output

    includes = ['--inc-path', root, '--inc-path', sdk, '--inc-path', native]
    try:
        source = out / 'timeline_renderers.c'
        header = source.with_suffix('.h')
        run('emit', [compiler, 'cgen', case, '--emit', source, '--emit-header', header, *includes], dynamic)
        generated = source.read_text(encoding='utf-8-sig')
        symbols = ('xuiTimeLineViewSetRenderersOwned', 'xuiTimeLineViewGetScrollSnapshot')
        report['directCalls'] = {symbol: len(re.findall(r'\b' + symbol + r'\(', generated)) for symbol in symbols}
        assert all(report['directCalls'].values())
        c_include = ['-I' + str(path) for path in (root, sdk, sdk/'include', *sorted(dynamic.glob('*/include')))]
        for standard, tool, language in (('c11', 'gcc', 'c'), ('c++11', 'g++', 'c++'), ('c++17', 'g++', 'c++')):
            run('header-' + standard, [tools[tool], '-std=' + standard, '-Wall', '-Wextra', '-Werror',
                *c_include, '-x', language, '-fsyntax-only', header], dynamic)
        for mode, packages in (('jit', dynamic), ('dynamic', dynamic), ('static', static)):
            if mode == 'jit':
                output = run(mode, [compiler, 'run', case, *includes], packages)
            else:
                binary = out / (mode + ('.exe' if win else ''))
                run(mode + '-build', [compiler, 'build', case, '-o', binary, *includes], packages)
                output = run(mode + '-execute', [binary], packages)
                table = run(mode + '-imports', [tools['objdump'], '-p', binary], packages)
                pattern = rb'DLL Name:\s*(\S+)' if win else rb'\bNEEDED\s+(\S+)'
                imports = [name.decode().lower() for name in re.findall(pattern, table)]
                assert any('xge3_host' in name for name in imports) == (mode == 'dynamic')
                assert not {'xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so', 'libtcc.dll', 'libtcc.so'}.intersection(imports)
                report['programs'].append(dict(mode=mode, bytes=binary.stat().st_size, sha256=digest(binary), imports=imports))
            assert b'TIMELINE_RENDERERS_OK' in output and b'TIMELINE_FRAME_FIELDS_OK' in output
        if args.render_example:
            output = run('render-example', [compiler, 'run', example, *includes], dynamic, render=True)
            match = re.search(rb'RGBA (\d+) (\d+)', output)
            assert match, 'The example did not capture a frame'
            width, height = map(int, match.groups())
            pixels = out / 'example.rgba'
            data = pixels.read_bytes()
            assert len(data) == width * height * 4
            color = bytes.fromhex('3B82F6FF')
            highlighted = sum(data[index:index+4] == color for index in range(0, len(data), 4))
            assert highlighted >= 100, 'The timeline highlight is missing from the captured frame'
            report['graphicalExample'] = dict(width=width, height=height, highlightedPixels=highlighted, sha256=digest(pixels))
        report['inputsUnchanged'] = all(digest(path) == sha for path, sha in inputs.items())
        assert report['inputsUnchanged']
        report['passed'] = True
    except Exception as error:
        report['error'] = repr(error)
        raise
    finally:
        report['complete'] = True
        save()


if __name__ == '__main__':
    main()
