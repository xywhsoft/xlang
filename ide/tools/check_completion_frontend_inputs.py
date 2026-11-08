"""Verify the completion fix extends the final cohort without weakening its pins."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import native_final_c_interop_profile as final
import native_c_interop_profile as interop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frontend', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.frontend.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    fixture = output / 'frontend'
    shutil.copytree(source, fixture)
    report = dict(status='running', cases=[])

    def save():
        (output / 'result.json').write_bytes(
            (json.dumps(report, indent=2) + '\n').encode('utf-8'))

    def passed(name):
        report['cases'].append(name)
        save()

    try:
        original = dict(final.PINS)
        changed = [p for p in original if original[p] != final.COMPLETION_PINS[p]]
        assert changed == ['src/sem/xl6_sem.c']
        assert len(final.profile(fixture)) == len(original)
        assert interop.completion_facts(fixture)
        assert interop.async_corpus(fixture)
        assert interop.revision(fixture) == 'final-completion-facts-20261108'
        passed('complete-fixed-cohort-and-async-observers')

        # Only this one declaration producer differs. Never rewrite the prior
        # inventory or accept an unknown hash merely because the AST matches.
        semantic = fixture / changed[0]
        fixed = semantic.read_bytes()
        for relative in ('src/sem/xl6_sem.c', 'src/ir/xl6_ir.h',
                         'src/type/xl6_type_model.h', 'lib/xrt/xrt.h'):
            path = fixture / relative
            before = path.read_bytes()
            try:
                path.write_bytes(before + b'\n/* unreviewed input */\n')
                try:
                    final.profile(fixture)
                except ValueError:
                    passed('reject-unknown-' + relative)
                else:
                    raise AssertionError('unknown dependency admitted: ' + relative)
            finally:
                path.write_bytes(before)

        projected, metadata = final.prepare(fixture, output / 'projected')
        assert metadata['sourceRevision'] == 'final-completion-facts-20261108'
        assert final.profile(projected) == {
            str(projected / p): h for p, h in final.COMPLETION_PINS.items()}
        passed('complete-independent-projection')

        # A source edit during projection must not publish a valid-looking
        # mixed cohort. Change only this private fault-injection fixture.
        write_bytes = Path.write_bytes
        mutated = []

        def racing_write(path, data):
            result = write_bytes(path, data)
            if not mutated and path.is_relative_to(output / 'drifted'):
                write_bytes(semantic, fixed + b'\n/* source drift */\n')
                mutated.append(True)
            return result

        try:
            Path.write_bytes = racing_write
            try:
                final.prepare(fixture, output / 'drifted')
            except ValueError:
                assert mutated
                passed('reject-source-drift-during-projection')
            else:
                raise AssertionError('concurrent source drift admitted')
        finally:
            Path.write_bytes = write_bytes
            semantic.write_bytes(fixed)

        assert final.PINS == original
        assert all(hashlib.sha256((source / p).read_bytes()).hexdigest() == h
                   for p, h in final.COMPLETION_PINS.items())
        passed('original-pins-and-inputs-preserved')
        report['status'] = 'passed'
    finally:
        save()
    print(json.dumps(dict(status=report['status'], cases=len(report['cases']))))


if __name__ == '__main__':
    main()
