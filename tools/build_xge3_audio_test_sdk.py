"""Build isolated real no-device audio SDK variants for language qualification.

The only production-source change is an aggregate suffix containing private
setup/inspection exports. Normal playback APIs are unchanged. These packages
are never shipping images and must not be installed in release/.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys, time


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'native-root', 'parent-source', 'host-parent', 'static-parent', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    root, native, parent, host, static, out = [v.resolve() for v in
        (a.root, a.native_root, a.parent_source, a.host_parent, a.static_parent, a.out)]
    assert not out.exists() and not out.is_relative_to(root / 'release')
    sha = lambda v: hashlib.sha256(Path(v).read_bytes()).hexdigest()
    helper = root / 'test/support/xge3_audio_headless.inc'
    inputs = {str(v): sha(v) for v in
        (Path(__file__).resolve(), helper, host / 'build-report.json', static / 'build-report.json')}
    # Hard links avoid copying large immutable dependency trees. Detach the
    # aggregate before modifying it; build outputs are excluded from the copy.
    def copy(source, target):
        try:
            os.link(source, target)
        except OSError:
            shutil.copy2(source, target)
        return target
    source = out / 'source'
    def ignore(folder, names):
        excluded = {'.git', '__pycache__'}
        if Path(folder).resolve() == native:
            excluded.add('build')
        return [name for name in names if name in excluded or name.endswith('.pyc')]
    shutil.copytree(native, source, copy_function=copy,
                    ignore=ignore)
    aggregate = source / 'single/xge.c'
    original = aggregate.read_bytes()
    aggregate.unlink()
    aggregate.write_bytes(original + b'\n#include "xlang_audio_test.inc"\n')
    shutil.copy2(helper, aggregate.with_name('xlang_audio_test.inc'))
    report = dict(passed=False, scope=__doc__, shippingDelivery=False,
                  audioDevice=False, productionAggregateSha256=sha(native / 'single/xge.c'),
                  privateAggregateSha256=sha(aggregate), inputs=inputs, rows=[])
    def save():
        (out / 'test-variant.json').write_text(json.dumps(report, indent=2) + '\n')
    def run(name, command):
        command = list(map(str, command))
        started = time.monotonic()
        with (out / (name + '.log')).open('wb') as log:
            result = subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT)
        report['rows'].append(dict(name=name, command=command, exitCode=result.returncode,
                                  seconds=time.monotonic()-started, logSha256=sha(out/(name+'.log'))))
        save()
        print(name, result.returncode, flush=True)
        assert not result.returncode, out/(name+'.log')
    save()
    run('host', [sys.executable, '-B', source/'tools/build/build.py', 'default', out/'host',
                 '--host-xrt', '--relink-from', host, '--relink-source', parent,
                 '--recompile-source', 'single/xge.c'])
    run('static', [sys.executable, '-B', source/'tools/build/static.py', '--root', source,
                   '--out', out/'static', '--reuse-build', static])
    for variant in ('host', 'static'):
        script = 'stage_xge3_packages.py' if variant == 'host' else 'stage_xge3_static_packages.py'
        run('stage-'+variant, [sys.executable, '-B', root/'tools'/script, '--root', root,
                               '--engine-root', source, '--engine-build', out/variant,
                               '--out', out/('packages-'+variant), '--report', out/('packages-'+variant+'.json')])
    report.update(passed=True, inputsUnchanged=all(sha(v)==h for v,h in inputs.items()))
    assert report['inputsUnchanged']
    save()


if __name__ == '__main__':
    main()
