"""Package the latest successful source build with explicit pinned providers."""
import argparse
import json
from pathlib import Path
from types import SimpleNamespace

from package_portable import assemble, load_json, plain, require, safe_name, sha

IDE = Path(__file__).resolve().parents[1]


def package(profile_path, output=None):
    profile_path = profile_path.absolute()
    plain(profile_path)
    profile = load_json(profile_path)
    require(profile.get('format') == 'xlang-ide-portable-build-inputs-1', 'Invalid portable build input profile')
    pointer = load_json(IDE/'build/current-build.json')
    require(pointer.get('status') == 'current-source-build-passed' and pointer.get('exitCode') == 0,
            'Latest build failed or is unfinished; run build_current.bat before packaging')
    report = Path(pointer['report'])
    require(sha(report) == pointer['reportSha256'], 'Current build receipt changed')
    options = dict(output=str(output) if output else None, build_report=str(report))
    for key in ('compiler_root', 'c_server_root', 'compiler_manifest', 'c_server_manifest',
                'sdk_source_root', 'compiler_source_manifest', 'compiler_source_build_report'):
        relative = safe_name(profile[key])
        path = IDE/relative
        if key in ('compiler_manifest', 'c_server_manifest', 'compiler_source_manifest', 'compiler_source_build_report'):
            plain(path)
            require(sha(path) == profile[key + '_sha256'], 'Pinned provider material changed: ' + relative)
        options[key] = str(path)
    return assemble(SimpleNamespace(**options))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', type=Path, default=IDE/'data/portable-build-inputs.json')
    parser.add_argument('--output', type=Path, help='A new directory within ide/dist')
    args = parser.parse_args()
    try:
        path = package(args.profile, args.output)
        print(json.dumps(dict(status='current-source-portable-package-created', candidate=str(path)), ensure_ascii=False))
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, 'Current portable package failed: ' + str(error) + '\n')
