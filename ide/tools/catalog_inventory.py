"""Public API coverage independent of catalog rendering and display signatures."""
import hashlib,json,re

def name(node):
    return next((c.get('text','') for c in node.get('children',[]) if c['kind']=='name'),node.get('text',''))

def inventory(raw,ast):
    functions=sorted({(r['namespace']+'.' if r['namespace'] else '')+r['name'] for r in raw['functions'] if not r['namespace'].startswith('__') and not r['name'].startswith('__')})
    constants=sorted({(r['namespace']+'.' if r['namespace'] else '')+r['name'] for r in raw['constants']})
    types=sorted({n for row in raw['sourceChunks'] for n in re.findall(r'(?m)^\s*(?:public\s+)?(?:class|protocol|enum)\s+(\w+)',row['source']) if not n.startswith('__')})
    members=set();hidden=set()
    def walk(nodes,prefix=''):
        for node in nodes:
            kind=node['kind'];n=name(node);flags=node.get('flags',0);qualified=prefix+n
            if not n:continue
            if n.startswith('__') or flags&6 or (kind=='field' and flags&128):
                if prefix:hidden.add(qualified)
                continue
            if kind in ('class','enum','protocol'):
                lists=[c for c in node.get('children',[]) if c['kind']=='list']
                if lists:walk(lists[-1]['children'],qualified+'.')
            elif prefix and kind in ('func','async_func','extern_func','export_func','native_extern_func','native_extern_special','get','field','enum_variant') and n!='destroy':members.add(qualified)
    walk(ast['children'])
    return {'functions':functions,'constants':constants,'types':types,'members':sorted(members),'hiddenMembers':sorted(hidden)}

def api_digest(data):
    rows=[{k:e.get(k) for k in ('name','kind','signature','parameters','returns','requiredImport','declarationKind')} for e in data['entries'] if e['category']=='standard']
    rows.sort(key=lambda e:(e['name'],e['kind'],e['signature']))
    return hashlib.sha256(json.dumps(rows,sort_keys=True,ensure_ascii=False,separators=(',',':')).encode()).hexdigest()

def validate(data,expected):
    rows=[r for r in data['entries'] if r['category']=='standard']
    actual_functions={r['name'] for r in rows if r['kind']=='function'}
    actual_constants={r['name'] for r in rows if r['kind']=='constant'}
    actual_types={r['name'] for r in rows if r['kind']=='type'}
    actual_names={r['name'] for r in rows}
    actual_members={r['name'] for r in rows if r['kind']=='method' or (r['kind']=='constant' and r['name'] not in expected['constants'])}
    failures={
      'missingFunctions':sorted(set(expected['functions'])-actual_functions),
      'staleFunctions':sorted(actual_functions-set(expected['functions'])),
      'missingConstants':sorted(set(expected['constants'])-actual_constants),
      'missingTypes':sorted(set(expected['types'])-actual_types),
      'staleTypes':sorted(actual_types-set(expected['types'])),
      'missingMembers':sorted(set(expected['members'])-actual_names),
      'staleMembers':sorted(actual_members-set(expected['members'])),
      'hiddenMembers':sorted(set(expected['hiddenMembers'])&actual_names)}
    # A class field may deliberately shadow a private base member; its public
    # declaration is authoritative for that qualified name.
    failures['hiddenMembers']=[n for n in failures['hiddenMembers'] if n not in expected['members']]
    if any(failures.values()):raise ValueError('Catalog API coverage differs: '+json.dumps(failures,ensure_ascii=True))
    if data.get('publicApiSha256') and api_digest(data)!=data['publicApiSha256']:raise ValueError('Public API signatures/defaults changed')
    return {key:len(expected[key]) for key in ('functions','constants','types','members')}
