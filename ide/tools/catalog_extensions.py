"""Extract extension API from compiler-owned UTF-8 AST ranges, never guessed syntax."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
import textwrap

PUBLIC, PRIVATE, PROTECTED, STATIC, NATIVE = 1, 2, 4, 8, 128
FUNCTIONS = {'func', 'async_func', 'extern_func', 'export_func',
             'native_extern_func', 'native_extern_special', 'create', 'get', 'let'}
TYPES = {'class', 'protocol', 'enum', 'union', 'type_alias', 'extern_type_alias'}


def child(node, kind):
    return next((n for n in node.get('children', []) if n['kind'] == kind), None)


def name_of(node):
    name = child(node, 'name')
    return name.get('text', '') if name else node.get('text', '')


def public(node):
    flags = node.get('flags', 0)
    return not flags & (PRIVATE | PROTECTED) and (flags & PUBLIC or node['kind'] in ('export_func', 'export_const', 'export_var'))


def type_references(node):
    if node.get('flags', 0) & (PRIVATE | PROTECTED):
        return set()
    result = set(re.findall(r'[^\W\d]\w*', node.get('text', ''), re.UNICODE)) if node['kind'] == 'type' else set()
    if node['kind'] in ('class', 'protocol'):
        for base in (child(node, 'list') or {}).get('children', []):
            result.update(re.findall(r'[^\W\d]\w*', base.get('text', ''), re.UNICODE))
    for item in node.get('children', []):
        result.update(type_references(item))
    return result


def doc_fields(value):
    # 顶层标题之外都是正文，缩进的代码和围栏不再次解析。
    body = value.splitlines()[1:-1] if value else []
    fields, key, lines = {}, None, []
    for line in textwrap.dedent('\n'.join(body)).splitlines():
        match = re.fullmatch(r'(desc|param\s+[^:]+|return(?:\s+[^:]+)?|note|example):\s*', line)
        if match:
            if key is not None:
                fields[key] = textwrap.dedent('\n'.join(lines)).strip()
            key, lines = match[1].strip(), []
        elif key is not None:
            lines.append(line)
    if key is not None:
        fields[key] = textwrap.dedent('\n'.join(lines)).strip()
    return fields


class DeclarationText:
    # 使用编译器给出的半开 UTF-8 范围，括号、逗号、原始串都不另写解析器。
    def __init__(self, text):
        self.raw = text.encode('utf-8')

    def slice(self, begin, end):
        if not 0 <= begin <= end <= len(self.raw):
            raise ValueError('Compiler source range outside captured bytes')
        return self.raw[begin:end].decode('utf-8').strip()

    def span(self, node, token=False):
        if node is None:
            return ''
        region = node.get('tokenRange' if token else 'range')
        if region is None:
            if not node.get('children') and not node.get('text'):
                return ''
            raise ValueError('Missing compiler source range: ' + node['kind'])
        return self.slice(region['startOffset'], region['endOffset'])

    def callable(self, node, label):
        params = (child(node, 'list') or {}).get('children', [])
        parameters, parts = [], []
        for param in params:
            if param['kind'] != 'param':
                raise ValueError('Unknown compiler parameter kind: ' + label)
            name = param.get('text', '')
            marker = self.span(param, True)
            variadic = name == 'arg' and marker in ('...', '***')
            if marker != name and not variadic:
                raise ValueError('Parameter source differs from AST: ' + label + '.' + name)
            children = param.get('children', [])
            default = children[1] if len(children) > 1 else None
            param_type = child(param, 'type')['text'] if variadic else self.slice(param['range']['startOffset'], param['tokenRange']['startOffset'])
            parameters.append(dict(name=name, type=param_type,
                                   optional=default is not None, default=self.span(default), description=''))
            parts.append(self.span(param))
        result = self.span(child(node, 'return_types'))
        if result and (result[:2] != 'as' or not result[2:3].isspace()):
            raise ValueError('Unexpected return signature range: ' + label)
        returns = result[2:].strip() if result else ''
        signature = label + self.span(child(node, 'type_params')) + '(' + ', '.join(parts) + ')' + (' as ' + returns if returns else '')
        return signature, parameters, returns

    def declaration(self, node, qualified):
        name = child(node, 'name') or node
        region = name['tokenRange']
        type_node = child(node, 'type')
        begin = type_node['range']['startOffset'] if type_node else region['startOffset']
        return self.slice(begin, region['startOffset']) + (' ' if type_node else '') + qualified + self.raw[region['endOffset']:node['range']['endOffset']].decode('utf-8').rstrip()

    def type_declaration(self, node, label):
        kind = node['kind']
        if kind in ('type_alias', 'extern_type_alias'):
            return 'typedef ' + label + ' = ' + self.span(child(node, 'type'))
        signature = kind + ' ' + label + self.span(child(node, 'type_params'))
        if kind in ('class', 'protocol'):
            bases = (child(node, 'list') or {}).get('children', [])
            if bases:
                signature += ' extends ' + ', '.join(self.span(n) for n in bases)
        elif kind == 'enum' and child(node, 'type'):
            signature += ' as ' + self.span(child(node, 'type'))
        return signature


def extract(root, compiler, scratch, capture):
    """capture(path) returns (logical source key, immutable UTF-8 bytes)."""
    root, compiler, scratch = Path(root).resolve(), Path(compiler).resolve(), Path(scratch)
    scratch.mkdir(parents=True, exist_ok=True)
    entries, documents, packages, excluded, nodes = [], [], [], [], Counter()
    seen = set()

    def add(label, module, kind, signature, origin, line, version, params=None, returns='', doc=None):
        fields = doc_fields((doc or {}).get('doc', ''))
        identity = 'extension:' + label + ':' + hashlib.sha256((kind + '|' + signature).encode()).hexdigest()[:16]
        entry = dict(id=identity, name=label, language='xlang', category='extension', module=module,
                     kind=kind, signature=signature, parameters=params or [], returns=returns,
                     description=fields.get('desc', ''), example=fields.get('example', ''), insert=label,
                     version=version, requiredImport=module, source=origin, sourceLine=line, sections=[])
        extra = [fields[k] for k in fields if k == 'note' or k.startswith('return')]
        if extra:
            entry['description'] += ('\n\n' if entry['description'] else '') + '\n\n'.join(extra)
        for p in entry['parameters']:
            p['description'] = fields.get('param ' + p['name'], '')
        if identity not in seen:
            entries.append(entry)
            seen.add(identity)
        return entry

    for manifest_path in sorted(root.glob('*/xlang.package.json')):
        origin, raw = capture(manifest_path)
        manifest = json.loads(raw.decode('utf-8-sig'))
        name, version = manifest['name'], manifest.get('version', '')
        if manifest.get('kind') == 'demo6-private-extlib' or name.startswith('_'):
            excluded.append(name)
            continue
        if name in packages:
            raise ValueError('Duplicate extension name: ' + name)
        packages.append(name)
        module = add(name, name, 'module', 'import ' + name, origin, 1, version)
        module.update(description='公开扩展声明；需显式 import ' + name + '。目录覆盖不代表包已通过发布验收。', example='import ' + name + '\n')
        readmes = [p for p in manifest_path.parent.iterdir() if p.name.lower() == 'readme.md']
        if len(readmes) > 1:
            raise ValueError('Ambiguous README: ' + name)
        section = []
        if readmes:
            path, raw_doc = capture(readmes[0])
            if raw_doc.strip():
                section = ['package:' + name]
                documents.append(dict(id=section[0], title=name + ' 使用说明', markdown=raw_doc.decode('utf-8-sig'), source=path, sourceLine=1))
        units = []
        for relative in manifest.get('artifact', {}).get('xlSources', []):
            if not isinstance(relative, str) or Path(relative).is_absolute() or '..' in Path(relative).parts:
                raise ValueError('Invalid declared extension source: ' + name)
            path = manifest_path.parent / relative
            if not path.resolve().is_relative_to(manifest_path.parent.resolve()):
                raise ValueError('Extension source escapes package: ' + name)
            origin, raw_source = capture(path)
            text = raw_source.decode('utf-8-sig')
            work = scratch / hashlib.sha256(origin.encode()).hexdigest()[:20]
            work.mkdir()
            source = work / 'source.xl'
            source.write_bytes(raw_source)
            target = work / 'parse.json'
            # 相对快照路径使范围输出可搬迁，也避免将很长的工作树绝对路径重复到每个节点。
            result = subprocess.run([str(compiler), source.name, target.name], cwd=work, capture_output=True, timeout=30)
            (work / 'parse.stderr').write_bytes(result.stderr)
            if result.returncode or result.stderr:
                raise ValueError(f'{origin}: parse failed ({result.returncode}): ' + result.stderr.decode('utf-8', errors='replace'))
            records = json.loads(target.read_text(encoding='utf-8-sig'))
            parsed, parents = [], []
            for record in records:
                syntax = record['syntax']
                if syntax.get('schema') != 1 or syntax.get('offsetEncoding') != 'utf-8-after-bom' or syntax.get('rangeConvention') != 'half-open':
                    raise ValueError('Unsupported compiler source-range schema')
                node, parent = syntax['ast'], record['parent']
                if parent == -1:
                    parsed.append(node)
                elif isinstance(parent, int) and 0 <= parent < len(parents):
                    owner = parents[parent]
                    lists = [n for n in owner.get('children', []) if n['kind'] == 'list']
                    members = lists[-1]
                    members['children'].append(node)
                else:
                    raise ValueError('Invalid declaration parent identity')
                parents.append(node)
            units.append((origin, text, parsed))

        # 工厂返回的未导出类仍有可用的实例方法；只沿公开签名/字段传播可达类型。
        roots = [n for _, _, items in units for n in items]
        visible = [n for n in roots if public(n)]
        candidates = {name_of(n): n for n in roots if n['kind'] in TYPES and not n.get('flags', 0) & (PRIVATE | PROTECTED)}
        reachable = set()
        while visible:
            referenced = set().union(*(type_references(n) for n in visible))
            new = (referenced & candidates.keys()) - reachable
            reachable.update(new)
            visible = [candidates[n] for n in new]
        for origin, text, parsed in units:
            spelling = DeclarationText(text)

            def walk(items, prefix, top=True):
                for node in items:
                    kind, flags = node['kind'], node.get('flags', 0)
                    label = name_of(node)
                    exposed = top and not public(node) and kind in TYPES and label in reachable
                    if flags & (PRIVATE | PROTECTED) or (top and not public(node) and not exposed):
                        continue
                    if label.startswith('__') and (top or kind not in FUNCTIONS):
                        continue
                    if label == 'destroy' or kind == 'derive' or (kind == 'field' and flags & NATIVE):
                        continue
                    qualified = prefix + label
                    start = len(entries)
                    line = (child(node, 'name') or node).get('line', 1)
                    nodes[kind] += 1
                    if kind in TYPES:
                        if not label and kind == 'union':
                            walk((child(node, 'list') or {}).get('children', []), prefix, False)
                            continue
                        add(qualified, name, 'type', spelling.type_declaration(node, qualified), origin, line, version, doc=node)
                        lists = [n for n in node.get('children', []) if n['kind'] == 'list']
                        members = lists[-1] if lists else {}
                        walk(members.get('children', []), qualified + '.', False)
                    elif kind in FUNCTIONS:
                        signature, params, returns = spelling.callable(node, qualified)
                        if kind == 'async_func':
                            signature = 'async ' + signature
                        if kind in ('get', 'let'):
                            signature = kind + ' ' + signature
                        if flags & STATIC:
                            signature = 'static ' + signature
                        add(qualified, name, 'function' if top else 'method', signature, origin, line, version, params, returns, node)
                    elif kind == 'enum_variant' and (child(node, 'list') or {}).get('children'):
                        signature, params, _ = spelling.callable(node, qualified)
                        add(qualified, name, 'function', signature, origin, line, version, params, doc=node)
                    elif kind in ('export_const', 'export_var', 'extern_var', 'field', 'const_decl', 'var_decl', 'enum_variant'):
                        category = 'constant' if kind in ('export_const', 'const_decl', 'enum_variant') else 'variable' if top else 'field'
                        declared = spelling.declaration(node, qualified)
                        if category == 'constant' and kind != 'enum_variant':
                            declared = 'const ' + declared
                        if flags & STATIC:
                            declared = 'static ' + declared
                        add(qualified, name, category, declared, origin, line, version, doc=node)
                    else:
                        raise ValueError(f'Unrecognized public AST declaration: {origin}:{line}: {kind}')
                    if exposed:
                        for entry in entries[start:]:
                            entry['insert'] = ''
                            entry['description'] += ('\n\n' if entry['description'] else '') + '此类型未直接导出；经公开 API 获得实例后访问，不能按模块名直接构造。'

            walk(parsed, name + '.')
        for entry in entries:
            if entry['module'] == name:
                entry['sections'] = section
    return entries, documents, dict(packages=packages, excludedPrivate=excluded, publicAstKinds=dict(nodes))
