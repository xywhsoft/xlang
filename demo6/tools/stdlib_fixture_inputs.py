"""Explicit assertion-source updates against an immutable product inventory.
Replacement keeps existing names. Explicit additions grow the recorded test
inventory only within the approved family prefix. Never replace production
inputs or providers. Callers pin all selected assertion sources.
"""
from pathlib import Path
import re


def add_arguments(parser):
    parser.add_argument('--fixture-source',type=Path,action='append',default=[],
        help='Use this newer .xl assertion source for its existing inventory name; product inputs stay frozen')
    parser.add_argument('--additional-fixture-source',type=Path,action='append',default=[],
        help='Explicitly add this new approved-family .xl assertion and grow the recorded inventory; never replace an existing name')


def select(sources,updates):
    candidates={p.name for p in sources};replacements={}
    for path in updates:
        path=path.resolve()
        assert path.is_file() and path.suffix=='.xl' and path.name in candidates,path
        assert path.name not in replacements,'Duplicate fixture source: '+path.name
        replacements[path.name]=path
    return [replacements.get(p.name,p) for p in sources]


def extend(sources,additions,prefix):
    result=list(sources);names={p.name for p in result}
    for path in additions:
        path=path.resolve()
        assert path.is_file() and re.fullmatch(r'stdlib_[A-Za-z0-9_]+\.xl',path.name) and prefix.match(path.name),path
        assert path.name not in names,'Duplicate or existing added fixture: '+path.name
        names.add(path.name);result.append(path)
    return sorted(result,key=lambda p:p.name)
