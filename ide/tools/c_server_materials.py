"""Validate optional source/build materials bound to the selected C server."""
from pathlib import Path
import re

FORMAT = 'xlang-ide-c-server-materials-1'

def collect_c_server_materials(record, base, expected, *, add, load_json, plain, safe_name, sha, require):
    materials = record.get('providerMaterials')
    if materials is None:
        return {'status': 'upstream-runtime-manifest-only', 'copiedFiles': 0, 'inputTraceProtocol': 0}
    require(isinstance(materials, dict) and materials.get('format') == FORMAT, 'Invalid C server material format')
    require(type(materials.get('inputTraceProtocol')) is int and materials['inputTraceProtocol'] == 1 and materials.get('formalRelease') is False,
            'Unsupported C server input trace material declaration')
    commit = materials.get('upstreamCommit')
    require(isinstance(commit, str) and re.fullmatch(r'[0-9a-f]{40}', commit), 'Invalid C server upstream commit')
    digest = materials.get('executableSha256')
    require(isinstance(digest, str) and re.fullmatch(r'[0-9a-f]{64}', digest), 'Invalid C server binary binding')
    require(expected.get('bin/clangd.exe') == digest, 'C server material binary binding mismatch')
    relative_root = safe_name(materials.get('root', ''))
    root = Path(base) / relative_root
    parent = root.parent
    while parent != Path(base):
        plain(parent, True); parent = parent.parent
    plain(root, True)
    rows = materials.get('files')
    require(isinstance(rows, list) and 1 <= len(rows) <= 128, 'Invalid C server material inventory')
    originals, targets, total = set(), set(), 0
    copied = []
    for row in rows:
        require(isinstance(row, dict), 'Invalid C server material row')
        relative, target = safe_name(row.get('path', '')), safe_name(row.get('target', ''))
        require(relative.casefold() not in originals and target.casefold() not in targets, 'Duplicate C server material path')
        require(target.startswith(('sources/clangd-adaptation/', 'notices/clangd/', 'provenance/clangd-adaptation/')),
                'C server material target is outside its source/provenance namespace')
        originals.add(relative.casefold()); targets.add(target.casefold())
        source = root / relative
        parent = source.parent
        while parent != root:
            plain(parent, True); parent = parent.parent
        attributes = plain(source)
        length, expected_hash = row.get('size'), row.get('sha256')
        require(isinstance(length, int) and not isinstance(length, bool) and 0 <= length <= 16*1024*1024,
                'Invalid C server material length')
        require(isinstance(expected_hash, str) and re.fullmatch(r'[0-9a-f]{64}', expected_hash), 'Invalid C server material digest')
        total += length
        require(total <= 64*1024*1024 and attributes.st_size == length and sha(source) == expected_hash,
                'C server source/build material changed')
        copied.append((source, target, expected_hash))
    required = {'provenance/clangd-adaptation/source-build.json',
                'sources/clangd-adaptation/upstream-adaptation.patch',
                'notices/clangd/COPYING3', 'notices/clangd/COPYING.RUNTIME',
                'notices/clangd/COPYING.MinGW-w64-runtime.txt'}
    require({name.casefold() for name in required}.issubset(targets), 'C server source/build or runtime notice material is missing')
    receipt_row = next(row for row in copied if row[1] == 'provenance/clangd-adaptation/source-build.json')
    receipt = load_json(receipt_row[0])
    require(receipt.get('format') == 'xlang-ide-c-server-source-build-1' and type(receipt.get('actualBuildExitCode')) is int and receipt['actualBuildExitCode'] == 0 and
            receipt.get('executableSha256') == digest and receipt.get('upstreamCommit') == commit,
            'C server source-build receipt does not match the selected binary')
    by_target = {target: source for source, target, _ in copied}
    prefix = 'provenance/clangd-adaptation/original-build/'
    require(all((prefix + name).casefold() in targets for name in ('input-trace-prepared.json', 'input-trace-built.json',
            'configure-command.json', 'build-command.json', 'build.exit.txt', 'build.log')),
            'C server original source/build material is missing')
    prepared = load_json(by_target[prefix + 'input-trace-prepared.json'])
    built = load_json(by_target[prefix + 'input-trace-built.json'])
    require(prepared.get('commit') == commit and built.get('commit') == commit and built.get('sha256') == digest and
            sha(by_target[prefix + 'input-trace-prepared.json']) == built.get('preparedSha256') == receipt.get('preparedSha256') and
            sha(by_target['sources/clangd-adaptation/upstream-adaptation.patch']) == prepared.get('patchSha256') == receipt.get('adaptationPatchSha256') and
            by_target[prefix + 'build.exit.txt'].read_text(encoding='utf-8-sig').strip() == '0',
            'C server original source/build binding changed')
    for group, count in (('adapted', 6), ('added', 5)):
        entries = prepared.get(group)
        require(isinstance(entries, dict) and len(entries) == count, 'Incomplete C server adaptation inventory')
        for relative, expected_hash in entries.items():
            target = 'sources/clangd-adaptation/clang-tools-extra/clangd/' + safe_name(relative)
            require(target in by_target and sha(by_target[target]) == expected_hash,
                    'C server adaptation source is missing or changed')
    for source, target, expected_hash in copied:
        add(source, target, expected_hash)
    return {'status': 'frozen-adaptation-and-build-materials-copied', 'copiedFiles': len(copied),
            'upstreamCommit': commit, 'executableSha256': digest, 'inputTraceProtocol': 1,
            'sourceBuildReceipt': receipt_row[1], 'sourceScope': 'Exact adaptation delta and existing build/provenance records',
            'formalRelease': False, 'distributionReviewComplete': False}
