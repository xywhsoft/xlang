"""Compare the record parser to a pinned source parser on all root fixtures.

Each parser is compiled against its own generated token header and lexer. This
matters when adding a token: mixing a baseline parser with today's Lemon token
numbers would test a broken baseline rather than an actual regression.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--baseline', required=True)
    parser.add_argument('--profile', type=Path, help='Immutable current source profile; defaults to root')
    parser.add_argument('--runtime', type=Path, help='Matched runtime library; avoids changing release/')
    parser.add_argument('--sdk', type=Path, help='Matched declaration SDK directory')
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    current = (args.profile or root).resolve()
    out.mkdir(parents=True, exist_ok=False)
    baseline = out/'baseline'
    files = ('src/parser/xl6_parser.c', 'src/parser/xl6_parser.h',
             'src/parser/xl6_parser_api.c', 'src/lexer/xl6_lexer_gen.c',
             'src/lexer/xl6_lexer_gen.h', 'src/lexer/xl6_token.c', 'src/lexer/xl6_token.h')
    inputs = []
    for name in files:
        data = subprocess.check_output(['git', 'show', args.baseline+':'+name], cwd=root)
        target = baseline/name; target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        inputs.append(dict(path=str(target), sha256=hashlib.sha256(data).hexdigest()))
    common = ['test/cases/xlir_parser_corpus.c', 'src/lexer/xl6_lexer_api.c',
              'src/diag/xl6_diag.c', 'src/ast/xl6_ast.c', 'src/common/xl6_output.c',
              'src/common/xl6_console.c', 'src/sem/xl6_completion.c']
    fixtures = sorted((root/'test/cases').glob('*.xl'), key=lambda p:p.name)
    feed = ''.join(str(p)+'\n' for p in fixtures).encode('utf-8')
    rows, outcomes = [], {}
    runtime = (args.runtime or root/'release'/('win_x64/xl.dll' if os.name=='nt' else 'linux_x64/libxl.so')).resolve()
    if os.name == 'nt':
        shutil.copy2(runtime, out/'xl.dll')
    env = dict(os.environ)
    if os.name != 'nt':env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    for version in ('baseline', 'current'):
        profile = baseline if version == 'baseline' else current
        executable = out/(version+('.exe' if os.name == 'nt' else ''))
        command = ['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
                   '-I'+str(profile), '-I'+str(current),
                   *(['-I'+str(args.sdk.resolve()),'-I'+str(args.sdk.resolve()/'include')] if args.sdk else []),
                   *[current/name for name in common],
                   *[profile/name for name in files if name.endswith('.c')],
                   runtime, '-o', executable]
        result = subprocess.run(list(map(str,command)), cwd=root, capture_output=True, timeout=180)
        (out/(version+'-build.log')).write_bytes(result.stdout+result.stderr)
        assert result.returncode == 0, (version, 'build')
        result = subprocess.run([str(executable)], cwd=root, env=env,input=feed, capture_output=True, timeout=180)
        log = out/(version+'-corpus.log'); log.write_bytes(result.stdout+result.stderr)
        assert result.returncode == 0, (version, 'corpus')
        values = result.stdout.decode('utf-8-sig').splitlines()
        assert len(values) == len(fixtures), version
        outcomes[version] = [line.split('|',2) for line in values]
        rows.append(dict(version=version, fixtures=len(values), accepted=sum(v[0]=='1' for v in outcomes[version]),
                         log=str(log), sha256=hashlib.sha256(log.read_bytes()).hexdigest()))
    regressions, additions = [], []
    for source, prior, current in zip(fixtures,outcomes['baseline'],outcomes['current']):
        assert prior[2] == current[2] == str(source), source
        if prior[0] == '1' and current[:2] != prior[:2]:
            regressions.append(str(source))
        elif prior[0] == '0' and current[0] == '1':
            additions.append(str(source))
    report = dict(baseline=args.baseline, inputs=inputs, rows=rows, regressions=regressions,
                  newlyAccepted=additions, complete=not regressions, scope=__doc__)
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert not regressions, regressions
    print(json.dumps(dict(fixtures=len(fixtures), regressions=0,newlyAccepted=additions)))


if __name__ == '__main__':
    main()
