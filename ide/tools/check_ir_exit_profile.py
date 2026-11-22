"""Check fail-closed native exit-producer admission without editing compiler inputs."""
from pathlib import Path
from unittest.mock import patch
import argparse
import json
import native_ir_exit_profile as policy


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frontend', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.frontend.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    record = dict(status='failed', cases=[])
    original_read, original_file = Path.read_bytes, Path.is_file

    def changed(path):
        return lambda value: original_read(value) + (b'\n/* changed admission input */\n' if value.resolve() == path else b'')

    def rejected_dependency(call):
        try:
            result = call()
        except (ValueError, RuntimeError):
            return True
        return not result['enabled']

    try:
        accepted = policy.qualify(root)
        assert accepted['enabled'] and len(accepted['functions']) == 44
        record['cases'].append('complete-real-cohort')
        for name in ('src/ir/xl6_ir.c', 'src/ir/xl6_ir.h'):
            with patch.object(Path, 'read_bytes', changed((root/name).resolve())):
                assert not policy.qualify(root)['enabled']
            record['cases'].append('changed-'+name)
        pins = dict(policy.pins_for(root))
        dependency = next(name for name in pins if name not in ('src/ir/xl6_ir.c', 'src/ir/xl6_ir.h'))
        altered = dict(pins)
        altered[dependency] = '0'*64
        with patch.object(policy, 'pins_for', return_value=altered):
            assert not policy.qualify(root)['enabled']
        record['cases'].append('changed-complete-dependency-contract')
        path = (root/dependency).resolve()
        with patch.object(Path, 'read_bytes', changed(path)):
            assert rejected_dependency(lambda: policy.qualify(root))
        record['cases'].append('changed-real-dependency-bytes')
        with patch.object(Path, 'is_file', lambda value: False if value.resolve() == path else original_file(value)):
            assert rejected_dependency(lambda: policy.qualify(root))
        record['cases'].append('missing-real-dependency')
        extract = policy.function
        with patch.object(policy, 'function', lambda text, name: extract(text, name)+'\n'):
            assert rejected_dependency(lambda: policy.qualify(root))
        record['cases'].append('changed-function-extraction')
        assert policy.qualify(root) == accepted
        record['status'] = 'passed'
        print(json.dumps(dict(status='passed', cases=len(record['cases']))))
    finally:
        (args.output/'result.json').write_text(json.dumps(record, indent=2)+'\n', encoding='utf8', newline='\n')


if __name__ == '__main__':
    main()
