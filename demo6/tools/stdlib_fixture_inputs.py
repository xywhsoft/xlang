"""Explicit assertion-source updates against an immutable product inventory.
Only existing language fixtures may be replaced; never production inputs,
providers, candidate names or inventory size. Callers pin the selected files.
"""
from pathlib import Path


def add_arguments(parser):
    parser.add_argument('--fixture-source',type=Path,action='append',default=[],
        help='Use this newer .xl assertion source for its existing inventory name; product inputs stay frozen')


def select(sources,updates):
    candidates={p.name for p in sources};replacements={}
    for path in updates:
        path=path.resolve()
        assert path.is_file() and path.suffix=='.xl' and path.name in candidates,path
        assert path.name not in replacements,'Duplicate fixture source: '+path.name
        replacements[path.name]=path
    return [replacements.get(p.name,p) for p in sources]
