"""Copy a coherent source checkout for isolated, ordinary delivery builds."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    source, out = args.source.resolve(), args.out.resolve()
    assert (source / '.git').exists() and not out.exists()
    head = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    directories = ['.github'] + ['' + item for item in (
        'src', 'lib', 'tools', 'res', 'docs', 'cgenrule', 'dev', 'test',
        'release/include', 'release/examples', 'release/extlibs', 'release/projects')]
    files = {path for folder in directories for path in (source / folder).rglob('*') if path.is_file()}
    print('Pinning source inputs:', len(files), flush=True)
    files.update(path for parent in (source, source / 'demo6') for path in parent.iterdir() if path.is_file())
    files.update(path for path in (source/'release').iterdir() if path.is_file() and
                 path.suffix.lower() not in ('.exe','.dll','.so','.def') and path.name!='xl')
    records = [{'Relative': path.relative_to(source).as_posix(), 'Sha256': digest(path)} for path in sorted(files)]
    out.mkdir(parents=True)
    tree = out / 'tree'
    subprocess.run(['git', 'clone', '--shared', '--no-checkout', str(source), str(tree)], check=True)
    subprocess.run(['git', '-C', str(tree), 'read-tree', head], check=True)
    print('Copying pinned source inputs:', len(records), flush=True)
    for row in records:
        original, target = source / row['Relative'], tree / row['Relative']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
        assert digest(target) == row['Sha256'], ('Copy input changed', str(original))
    changed = [row['Relative'] for row in records if digest(source / row['Relative']) != row['Sha256']]
    latest = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    report = dict(Complete=not changed and head == latest, Source=str(source), Root=str(tree / 'demo6'),
                  Head=head, FinalHead=latest, Changed=changed, Inputs=records,
                  ExcludedUnrelatedTrees=['ide'])
    (out / 'snapshot.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    assert report['Complete'], 'Source changed while copying; use a new snapshot directory'
    # The snapshot's index models the new feature inventory without touching
    # the user's index or absorbing unrelated untracked work into a commit.
    additions = [row['Relative'] for row in records if
                 'compiled_package' in Path(row['Relative']).name or
                 '/compiled_' in row['Relative'] or
                 '/embedded_package/' in row['Relative'] or
                 row['Relative'].endswith(('xl_package.h', 'xl6_compiled_interface.inc',
                     'xl6_compiled_packages.inc', 'xl6_compiled_package.h',
                     'compiled-host-packages.md', 'embedded-package-delivery-2026-10-28.md',
                     'create_embedded_delivery_snapshot.py', 'update_docs_reference_index.py',
                     'cli_shell_mixed.xl', 'cli_shell_native.c', 'xl6_ir_cgen_tcc_runtime.c',
                     'stdlib_tcc_runtime_contract.xl', 'xl6_tcc_context_policy.c',
                     'xl6_tcc_context_policy.h', 'xl6_ownership_domain_tcc_context.c',
                     'xl6_ownership_domain_tcc_handler.c', 'tcc_runtime_generated_host.c',
                     'stage_runtime_headers.py', 'test_runtime_header_staging.py'))]
    subprocess.run(['git', '-C', str(tree), 'add', '--', *additions], check=True)
    print('Coherent delivery snapshot:', tree / 'demo6', 'files=', len(records), flush=True)


if __name__ == '__main__':
    main()
