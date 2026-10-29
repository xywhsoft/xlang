"""Parse local Python runner references without importing or executing them."""
from pathlib import Path
import ast
import json
import sys


LIMIT = 4096


def strings(node, environment):
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return {node.value}
    if isinstance(node, ast.Name):
        return environment.get(node.id, set())
    if isinstance(node, ast.Subscript) and isinstance(node.slice, ast.Constant) and isinstance(node.slice.value, int):
        return strings(node.value, environment)
    if isinstance(node, (ast.List, ast.Tuple, ast.Set)):
        return set().union(*(strings(item, environment) for item in node.elts))
    if isinstance(node, ast.IfExp):
        return strings(node.body, environment) | strings(node.orelse, environment)
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
        left, right = strings(node.left, environment), strings(node.right, environment)
        if len(left) * len(right) <= LIMIT:
            return {a + b for a in left for b in right if len(a) + len(b) <= 2048}
    if isinstance(node, ast.JoinedStr):
        result = {''}
        for part in node.values:
            values = strings(part.value if isinstance(part, ast.FormattedValue) else part, environment)
            if not values or len(result) * len(values) > LIMIT:
                return set()
            result = {a + b for a in result for b in values}
        return result
    if isinstance(node, (ast.ListComp, ast.SetComp, ast.GeneratorExp)):
        environments = [dict(environment)]
        for generator in node.generators:
            expanded = []
            for current in environments:
                for row in bindings(generator.target, generator.iter, current):
                    expanded.append(dict(current, **row))
                    if len(expanded) > LIMIT:
                        return set()
            if len(expanded) > LIMIT:
                return set()
            environments = expanded
        return set().union(*(strings(node.elt, current) for current in environments))
    return set()


def bindings(target, iterable, environment):
    if isinstance(iterable, ast.BinOp) and isinstance(iterable.op, ast.Add):
        return bindings(target, iterable.left, environment) + bindings(target, iterable.right, environment)
    if isinstance(target, ast.Name):
        return [{target.id: {value}} for value in strings(iterable, environment)]
    if isinstance(target, (ast.List, ast.Tuple)) and isinstance(iterable, (ast.List, ast.Tuple)):
        rows = []
        for item in iterable.elts:
            if isinstance(item, (ast.List, ast.Tuple)) and len(item.elts) == len(target.elts):
                rows.append({name.id: strings(value, environment)
                             for name, value in zip(target.elts, item.elts) if isinstance(name, ast.Name)})
        return rows
    return []


def scope_literals(scope, inherited):
    nodes, children, pending = [], [], list(ast.iter_child_nodes(scope))
    while pending:
        node = pending.pop()
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.Lambda, ast.ClassDef)):
            children.append(node)
            continue
        nodes.append(node)
        pending.extend(ast.iter_child_nodes(node))
    environment = {name: set(values) for name, values in inherited.items()}
    if isinstance(scope, (ast.FunctionDef, ast.AsyncFunctionDef, ast.Lambda)):
        arguments = scope.args
        for argument in [*arguments.posonlyargs, *arguments.args, *arguments.kwonlyargs,
                         *([arguments.vararg] if arguments.vararg else []),
                         *([arguments.kwarg] if arguments.kwarg else [])]:
            environment.pop(argument.arg, None)
        for node in nodes:
            if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Store):
                environment.pop(node.id, None)
    # Resolve only finite literal tables/concatenations. Never import a runner,
    # invoke a call, read a file named by it, or infer arbitrary CLI inputs.
    for _ in range(8):
        changed = False
        for node in nodes:
            rows = []
            if isinstance(node, ast.Assign):
                rows = [{target.id: strings(node.value, environment)}
                        for target in node.targets if isinstance(target, ast.Name)]
            elif isinstance(node, (ast.For, ast.comprehension)):
                rows = bindings(node.target, node.iter, environment)
            for row in rows:
                for name, values in row.items():
                    previous = environment.setdefault(name, set())
                    if len(previous | values) <= LIMIT and not values <= previous:
                        previous.update(values)
                        changed = True
        if not changed:
            break
    values = set()
    for node in nodes:
        if isinstance(node, ast.expr):
            values.update(value for value in strings(node, environment) if len(value) <= 2048)
        if isinstance(node, ast.ImportFrom) and node.module:
            values.add(node.module.replace('.', '/') + '.py')
        elif isinstance(node, ast.Import):
            values.update(item.name.replace('.', '/') + '.py' for item in node.names)
        elif (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
              and isinstance(node.func.value, ast.Name) and node.func.value.id == 'importlib'
              and node.func.attr == 'import_module' and len(node.args) == 1):
            values.update(value.replace('.', '/') + '.py' for value in strings(node.args[0], environment))
    for child in children:
        values.update(scope_literals(child, environment))
    return values


def literals(text, filename='<runner>'):
    return sorted(scope_literals(ast.parse(text, filename=filename), {}))


def main():
    root = Path(sys.argv[1]).resolve()
    paths = set((root / 'tools').glob('*.py'))
    if (root / 'release/extlibs').is_dir():
        for package in (root / 'release/extlibs').iterdir():
            if package.is_dir():
                for folder in ('test', 'tests', 'examples'):
                    paths.update((package / folder).glob('*.py'))
    records = {str(path): literals(path.read_text(encoding='utf-8-sig'), str(path)) for path in sorted(paths)}
    print(json.dumps(records))


if __name__ == '__main__':
    main()
