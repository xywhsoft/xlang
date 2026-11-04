"""Read an actual full source build or an explicitly partial frontend overlay.

This helper never writes or synthesizes a build manifest. The selected frozen
compiler inputs are independently hashed; a separate fixture tree is allowed.
"""
from pathlib import Path
import hashlib,json
from xlir_public_build_inputs import verify_public_build


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def add_build_arguments(parser):
    choice=parser.add_mutually_exclusive_group(required=True)
    choice.add_argument('--build',type=Path,help='Actual complete source build.json')
    choice.add_argument('--overlay',type=Path,help='Explicit partial frontend overlay.json')


def load_build(build=None,overlay=None):
    assert (build is None)!=(overlay is None)
    manifest=Path(build or overlay).resolve()
    description=json.loads(manifest.read_text(encoding='utf-8-sig'))
    inputs={str(manifest):sha(manifest),str(Path(__file__).resolve()):sha(__file__)}
    if build is not None:
        base=description;basePath=manifest
        driverSource=[Path(row['Source']) for row in base['Objects']
            if Path(row['Source']).name in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        assert len(driverSource)==1
        cohort=driverSource[0].parents[2]
        checked,pinned,_=verify_public_build(cohort,basePath.parent)
        assert checked==base
        inputs.update((str(p),sha(p)) for p in pinned)
        objects=[row['Path'] for row in base['Objects']
            if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        driver=Path(base['Driver'])
        runtime=Path(base['RuntimeLibrary']) if base.get('RuntimeLibrary') else driver.parent/'xl.dll'
        full=True
    else:
        assert description['passed'] and not description['fullRebuild']
        cohort=Path(description['sourceRoot']);basePath=Path(description['parentBuild'])
        assert sha(basePath)==description['parentBuildSha256']
        base=json.loads(basePath.read_text(encoding='utf-8-sig'))
        assert base['Complete'] and base['InputsUnchanged']
        inputs[str(basePath)]=sha(basePath)
        replacements={row['ParentObject']:row for row in description['Objects']}
        assert len(replacements)==len(description['Objects'])
        objects=[]
        for original in base['Objects']:
            row=replacements.get(original['Path'],original)
            assert sha(row['Path'])==row['Sha256'].lower()
            inputs[row['Path']]=sha(row['Path'])
            if Path(original['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c'):
                objects.append(row['Path'])
        driver=Path(description['Driver']);runtime=Path(description['RuntimeLibrary'])
        assert sha(driver)==description['DriverSha256']
        full=False
    assert driver.is_file() and runtime.is_file()
    return dict(cohort=cohort,basePath=basePath,base=base,objects=objects,inputs=inputs,
        driver=driver,runtime=runtime,fullSourceRebuild=full,includeOverlayRoot=description.get('includeOverlayRoot',str(cohort)),
        buildManifest=str(manifest) if full else None,overlay=str(manifest) if not full else None)
