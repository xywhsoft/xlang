"""Independently recheck this platform's completed CRL source/runtime receipts.
No tests are rerun and no failed/preparation report is rewritten. Source-driver
evidence never certifies shipping binaries, the latest mixed HEAD or full TLS.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['source', 'public', 'resident', 'prune', 'out']:
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--client', type=Path)
    args = parser.parse_args()
    out = args.out.resolve()
    assert not out.exists()
    out.mkdir(parents=True)
    paths = set()
    summaries = []
    for role in ['source', 'public', 'resident', 'prune']+(['client'] if args.client else []):
        path = getattr(args, role).resolve()
        data = json.loads(path.read_text(encoding='utf-8-sig'))
        assert data['Complete'] and data['InputsUnchanged'], path
        assert not data.get('Failures'), path
        inputs = data['Inputs']
        if isinstance(inputs, list):
            inputs = {row['Path']: row['Sha256'].lower() for row in inputs}
        for original, expected in inputs.items():
            assert sha(original) == expected.lower(), original
            paths.add(Path(original))
        for row in data.get('Objects', [])+data.get('Headers', []):
            assert sha(row['Path']) == row['Sha256'].lower(), row
            paths.add(Path(row['Path']))
        products = data.get('Products', {})
        if isinstance(products, list):
            products = {row['Path']: row['Sha256'] for row in products}
        for product, expected in products.items():
            assert sha(product) == expected.lower(), product
            paths.add(Path(product))
        for row in data.get('Rows', []):
            assert row['Exit'] == row.get('Expected', 0), row
            if row.get('Log'):
                assert sha(row['Log']) == row['Sha256'], row
                paths.add(Path(row['Log']))
        if role == 'source':
            assert data['SourceXrt'] and not data['RuntimeOnly']
            assert sha(data['Driver']) == data['DriverSha256'].lower()
            paths.add(Path(data['Driver']))
        summaries.append(dict(Role=role, Path=str(path), Sha256=sha(path),
            Stages=len(data.get('Rows', [])), TranslationUnits=len(data.get('Objects', []))))
        paths.add(path)
    # Keep project-owned test controls independently of later workspace work.
    project = Path(__file__).resolve().parents[1]
    snapshots = []
    for source in sorted(paths):
        if not source.is_relative_to(project):
            continue
        target = out/'controls'/source.relative_to(project)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        assert sha(source) == sha(target)
        snapshots.append(dict(Original=str(source), Path=str(target), Sha256=sha(target)))
    report = dict(Complete=True, Platform=os.name, Reports=summaries,
        SuccessfulStages=sum(row['Stages'] for row in summaries if row['Role'] != 'source'),
        VerifiedFiles=len(paths), ControlSnapshots=snapshots,
        NetworkTraffic=False, SecurityTests=False, SourceCohortOnly=True,
        ShippingAccepted=False, LatestMixedHeadAccepted=False, FullTlsAccepted=False, FullGoalComplete=False)
    (out/'proof.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    print(json.dumps({key: value for key, value in report.items() if key not in ['Reports', 'ControlSnapshots']}, indent=2))


if __name__ == '__main__':
    main()
