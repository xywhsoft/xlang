"""Inventory authoritative XGE/XUI declarations and validate direct language bindings.

All source symbols are accounted for, including disabled-feature branches. An
inventory entry is not a claim of managed ownership or executable coverage.
"""
from pathlib import Path
import argparse, ast, hashlib, json, re
try:
    from pycparser import c_parser, c_ast, c_generator
except ModuleNotFoundError:
    c_parser=c_ast=c_generator=None

KEYWORDS=set('void char short int long signed unsigned float double const volatile restrict struct union enum __restrict __restrict__'.split())
GEN=c_generator.CGenerator() if c_generator else None

def clean(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*',lambda m:'\n'*m.group().count('\n'),text,flags=re.S)
def split(text,delimiter=','):
    out=[];start=0;depth=0;quote=None;escaped=False
    for i,c in enumerate(text):
        if escaped:escaped=False;continue
        if quote:
            if c=='\\':escaped=True
            elif c==quote:quote=None
            continue
        if c in ('"',"'"):quote=c
        elif c in '([{':depth+=1
        elif c in ')]}':depth-=1
        elif c==delimiter and not depth:out.append(text[start:i].strip());start=i+1
    out.append(text[start:].strip());return [x for x in out if x]

def foreign_method_declaration(line):
    """Read one declaration without dropping nested defaults/function types."""
    native=re.match(r'\s*(?:export )?extern "([^"]+)"',line)
    if not native:return None
    quote=None;escaped=False;method=None
    for i in range(native.end(),len(line)):
        c=line[i]
        if escaped:escaped=False;continue
        if quote:
            if c=='\\':escaped=True
            elif c==quote:quote=None
            continue
        if c in ('"',"'"):quote=c;continue
        method=re.match(r'(func|get|let)\s+(\w+)\(',line[i:])
        if method:break
    if not method:return None
    start=i+method.end();depth=1;quote=None;escaped=False
    for end in range(start,len(line)):
        c=line[end]
        if escaped:escaped=False;continue
        if quote:
            if c=='\\':escaped=True
            elif c==quote:quote=None
            continue
        if c in ('"',"'"):quote=c
        elif c=='(':depth+=1
        elif c==')':
            depth-=1
            if not depth:break
    if depth or (line[end+1:].strip() and not line[end+1:].strip().startswith('as ')):
        raise ValueError('unclosed foreign method declaration: '+line)
    return native[1],method[1],method[2],line[start:end]

def macro_definitions(text):
    """Retain complete replacement lists, including backslash continuations."""
    lines=text.splitlines();i=0
    while i<len(lines):
        first=i;line=lines[i];i+=1
        m=re.match(r'\s*#\s*define\s+((?:XGE|XUI)\w*)(.*)',line)
        if not m:continue
        replacement=m[2]
        while replacement.rstrip().endswith('\\') and i<len(lines):
            replacement=replacement.rstrip()[:-1]+'\n'+lines[i];i+=1
        yield m[1],first+1,replacement

def native_function_sha(text,name):
    m=re.search(r'(?m)^[A-Za-z_][\w \t*]*?\b'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{',text)
    if not m:return None
    i=m.end();depth=1;quote=None;comment=None
    while depth and i<len(text):
        c=text[i];pair=text[i:i+2]
        if comment=='line':
            if c=='\n':comment=None
        elif comment=='block':
            if pair=='*/':comment=None;i+=2;continue
        elif quote:
            if c=='\\':i+=2;continue
            if c==quote:quote=None
        elif pair=='//':comment='line';i+=2;continue
        elif pair=='/*':comment='block';i+=2;continue
        elif c in ('"',"'"):quote=c
        elif c=='{':depth+=1
        elif c=='}':depth-=1
        i+=1
    return hashlib.sha256(text[m.start():i].encode()).hexdigest() if depth==0 else None
def typedefs(text):
    out=[];position=0
    while True:
        m=re.search(r'\btypedef\b',text[position:])
        if not m:break
        a=position+m.start();depth=0;end=None
        for i in range(a,len(text)):
            c=text[i]
            if c in '({[':depth+=1
            elif c in ')}]':depth-=1
            elif c==';' and not depth:end=i+1;break
        if end is None:raise ValueError('unterminated typedef')
        out.append((a,text[a:end]));position=end
    return out

def type_json(node):
    if isinstance(node,c_ast.TypeDecl):
        if isinstance(node.type,c_ast.IdentifierType):return dict(kind='named',names=node.type.names,qualifiers=node.quals)
        result=type_json(node.type)
        if node.quals:result['qualifiers']=list(node.quals)
        return result
    if isinstance(node,c_ast.PtrDecl):return dict(kind='pointer',qualifiers=node.quals,to=type_json(node.type))
    if isinstance(node,c_ast.ArrayDecl):return dict(kind='array',extent=GEN.visit(node.dim) if node.dim else None,of=type_json(node.type))
    if isinstance(node,c_ast.FuncDecl):return dict(kind='function',returns=type_json(node.type),parameters=[dict(name=getattr(p,'name',None),type=type_json(p.type)) if not isinstance(p,c_ast.EllipsisParam) else dict(variadic=True) for p in (node.args.params if node.args else [])])
    if isinstance(node,(c_ast.Struct,c_ast.Union)):return dict(kind='union' if isinstance(node,c_ast.Union) else 'struct',tag=node.name,fields=None if node.decls is None else [dict(name=x.name,type=type_json(x.type),bits=GEN.visit(x.bitsize) if x.bitsize else None) for x in node.decls])
    if isinstance(node,c_ast.Enum):return dict(kind='enum',tag=node.name,values=None if node.values is None else [dict(name=x.name,expression=GEN.visit(x.value) if x.value else None) for x in node.values.enumerators])
    raise ValueError(type(node).__name__)

def header_inventory(path,external_types=()):
    assert c_parser,'API inventory requires pycparser; acceptance does not'
    header_bytes=path.read_bytes()
    original=header_bytes.decode('utf-8-sig').replace('\r\n','\n');text=clean(original)
    conditions={};stack=[]
    for line,raw_line in enumerate(original.splitlines(),1):
        m=re.match(r'\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)',raw_line)
        if m:
            directive,expression=m[1],m[2].strip()
            if directive in ('if','ifdef','ifndef'):stack.append(expression if directive=='if' else ('!' if directive=='ifndef' else '')+'defined('+expression+')')
            elif directive=='endif' and stack:stack.pop()
            elif directive=='else' and stack:stack[-1]='!('+stack[-1]+')'
            elif directive=='elif' and stack:stack[-1]=expression
        conditions[line]=[x for x in stack if re.search(r'X(?:GE|UI)_(?:ENABLE|HAS)_',x)]
    type_text=re.sub(r'^[ \t]*#(?:[^\n\\]|\\\n)*\n',lambda m:'\n'*m.group().count('\n'),text,flags=re.M)
    td=typedefs(type_text)
    names=set(external_types)
    for _,raw in td:
        cb=re.search(r'\(\s*\*\s*(\w+)\s*\)',raw)
        last=re.search(r'\b(\w+)\s*(?:\[[^\]]*\])?\s*;\s*$',raw)
        if '}' in raw:
            names.update(re.findall(r'\b[A-Za-z_]\w*\b',raw[raw.rfind('}')+1:]))
        if last:names.add(last.group(1))
        elif cb:names.add(cb.group(1))
    parser=c_parser.CParser();types=[];errors=[]
    for pos,raw in td:
        symbols=set(re.findall(r'\b[A-Za-z_]\w*\b',raw))
        known={x for x in symbols if x in names or re.match(r'(?:u?int\d+_t|size_t|wchar_t|va_list|bool|xtime|xvalue|xerror|xstrview|xbytesview|xatomic\d+|xrt\w+)$',x)}
        pre='\n'.join('typedef int '+x+';' for x in known)
        try:
            tree=parser.parse(pre+'\n'+raw)
            for d in tree.ext[len(known):]:
                line=type_text.count('\n',0,pos)+1
                types.append(dict(name=d.name,line=line,conditions=conditions.get(line,[]),declaration=' '.join(raw.split()),type=type_json(d.type)))
        except Exception as error:errors.append(dict(declaration=' '.join(raw.split()),error=str(error)))
    # Complete forward typedefs from their separately declared public bodies.
    definitions={}
    declarations=re.sub(r'^[ \t]*#(?:[^\n\\]|\\\n)*\n',lambda m:'\n'*m.group().count('\n'),text,flags=re.M)
    for match in re.finditer(r'(?m)^(struct|union)\s+([A-Za-z_]\w*)\s*\{',declarations):
        depth=1;end=match.end()
        while depth and end<len(declarations):
            if declarations[end]=='{':depth+=1
            elif declarations[end]=='}':depth-=1
            end+=1
        raw=declarations[match.start():end]+';'
        known=(set(re.findall(r'\b[A-Za-z_]\w*\b',raw)) & names) | {'size_t','uintptr_t','intptr_t','bool','uint8_t','uint16_t','uint32_t','uint64_t','int8_t','int16_t','int32_t','int64_t','xtime','xstrview','xvalue','xerror'}
        try:
            tree=parser.parse('\n'.join('typedef int '+n+';' for n in known)+'\n'+raw)
            definitions[(match[1],match[2])]=type_json(tree.ext[-1].type)
        except Exception as error:errors.append(dict(declaration=' '.join(raw.split()),error=str(error)))
    for row in types:
        t=row['type']
        if t['kind'] in ('struct','union') and t.get('fields') is None and (t['kind'],t.get('tag')) in definitions:
            row['type']=dict(definitions[(t['kind'],t['tag'])])
    functions=[]
    for m in re.finditer(r'\b(X(?:GE|UI)\w*_API)\s+([^;{}]+?\([^;{}]*?\))\s*;',text,re.S):
        raw=m.group(2);before,args=raw.split('(',1);name=re.search(r'\w+\s*$',before).group().strip();ret=before[:before.rfind(name)]
        type_words=[ret]
        for arg in split(args[:-1]):type_words.append(re.sub(r'\b\w+\s*(?:\[[^\]]*\])?\s*$','',arg) if arg.strip() not in ('void','...') else '')
        known=set(re.findall(r'\b[A-Za-z_]\w*\b',' '.join(type_words)))-KEYWORDS
        pre='\n'.join('typedef int '+x+';' for x in known)
        try:
            line=text.count('\n',0,m.start())+1
            d=parser.parse(pre+'\n'+raw+';').ext[-1];functions.append(dict(name=name,line=line,conditions=conditions.get(line,[]),declaration=' '.join(raw.split()),type=type_json(d.type),state='requires-contract-review'))
        except Exception as error:errors.append(dict(declaration=' '.join(raw.split()),error=str(error)))
    enums=[];enum_constants=[]
    for match in re.finditer(r'(?m)^[ \t]*enum\s+([A-Za-z_]\w*)?\s*\{[^{}]*\}\s*;',declarations):
        raw=match.group();line=declarations.count('\n',0,match.start())+1
        try:
            definition=type_json(parser.parse(raw).ext[-1].type)
            enums.append(dict(name=match[1],line=line,conditions=conditions.get(line,[]),declaration=' '.join(raw.split()),type=definition))
            enum_constants.extend(dict(name=value['name'],expression=value['expression'],enum=match[1],line=line,conditions=conditions.get(line,[])) for value in definition['values'])
        except Exception as error:errors.append(dict(declaration=' '.join(raw.split()),error=str(error)))
    constants=[];function_macros=[]
    for name,line,replacement in macro_definitions(original):
        if replacement.startswith('('):
            signature=re.match(r'\(([^)]*)\)\s*(.*)',replacement,re.S)
            if signature:function_macros.append(dict(name=name,line=line,parameters=split(signature[1]),body=signature[2],state='requires-compile-time-mapping'))
            continue
        value=replacement.strip()
        if not value or value=='\\':continue
        constants.append(dict(name=name,line=line,expression=value))
    aliases={t['name']:t['type'] for t in types};callable_fields=[]
    for record in types:
        for member in record['type'].get('fields') or []:
            target=member['type'];seen=set();callback=None
            while target.get('kind')=='named' and len(target.get('names',[]))==1:
                alias=target['names'][0]
                if alias in seen or alias not in aliases:break
                seen.add(alias);callback=alias;target=aliases[alias]
            if target.get('kind')=='pointer' and target['to'].get('kind')=='function':
                callable_fields.append(dict(record=record['name'],member=member['name'],callbackTypedef=callback,signature=target['to'],globalSymbol=False,state='requires-callable-field-and-lifetime-contract'))
    return dict(header=str(path),sha256=hashlib.sha256(header_bytes).hexdigest(),functions=functions,types=types,constants=constants,enums=enums,enumConstants=enum_constants,functionMacros=function_macros,callableFields=callable_fields,parseErrors=errors)

def inventory(root,out):
    result=dict(schema=1,scope=__doc__,headers=[header_inventory(root/'include'/name) for name in ('xge.h','xui.h')])
    result['counts']={k:sum(len(h[k]) for h in result['headers']) for k in ('functions','types','constants','enums','enumConstants','functionMacros','callableFields','parseErrors')}
    # These are independent public headers, not reverse-included by xui.h.
    # Keep them explicit instead of silently counting the primary two headers
    # as the entire include/ surface. They are not auto-generated as caller
    # resources: each owning Document/Renderer handle needs a real contract.
    known={t['name'] for h in result['headers'] for t in h['types']}
    result['supplementaryHeaders']=[]
    for name in ('xui_document.h','xui_document_ui.h'):
        path=root/'include'/name
        if not path.exists():continue
        row=header_inventory(path,known);result['supplementaryHeaders'].append(row)
        known.update(t['name'] for t in row['types'])
    result['supplementaryCounts']={k:sum(len(h[k]) for h in result['supplementaryHeaders']) for k in result['counts']}
    result['combinedCounts']={k:result['counts'][k]+result['supplementaryCounts'][k] for k in result['counts']}
    out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(result['counts']));return result


SCALARS={'void':None,'int':'c_int','unsigned int':'c_uint','unsigned':'c_uint','char':'c_char','signed char':'c_schar','unsigned char':'c_uchar','short':'c_short','unsigned short':'c_ushort','long':'c_long','unsigned long':'c_ulong','long long':'c_longlong','unsigned long long':'c_ulonglong','float':'c_float','double':'c_double','size_t':'c_size','int8_t':'c_schar','uint8_t':'c_uchar','int16_t':'c_short','uint16_t':'c_ushort','int32_t':'c_int','uint32_t':'c_uint','int64_t':'int64','uint64_t':'uint64','xtime':'int64','intptr_t':'c_intptr','uintptr_t':'c_uintptr','ptrdiff_t':'c_ptrdiff'}
SCALARS.update(xtime='time',bool='bool',_Bool='bool')
def pascal(name):
    name=re.sub(r'^x(?:ge|ui)_','',name);name=re.sub(r'_t$','',name)
    return ''.join(x[:1].upper()+x[1:] for x in name.split('_'))
def integer(expression,known):
    expression=re.sub(r'/\*.*?\*/|//.*','',expression).strip()
    expression=re.sub(r'\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]+\b',r'\1',expression)
    expression=re.sub(r'(?<![\w.])((?:\d+\.\d*|\.\d+)(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+)[fFlL]\b',r'\1',expression)
    expression=re.sub(r'\((?:u?int\d+_t|unsigned|int|long|size_t)\)', '',expression)
    for symbol in set(re.findall(r'\b[A-Za-z_]\w*\b',expression)):
        if symbol not in known:raise ValueError(symbol)
        if not isinstance(known[symbol],(int,float)):raise ValueError('non-numeric '+symbol)
        expression=re.sub(r'\b'+re.escape(symbol)+r'\b',str(known[symbol]),expression)
    node=ast.parse(expression,mode='eval')
    allowed=(ast.Expression,ast.Constant,ast.UnaryOp,ast.BinOp,ast.USub,ast.UAdd,ast.Invert,ast.Add,ast.Sub,ast.Mult,ast.FloorDiv,ast.Div,ast.Mod,ast.LShift,ast.RShift,ast.BitAnd,ast.BitOr,ast.BitXor,ast.Load)
    if any(not isinstance(x,allowed) for x in ast.walk(node)):raise ValueError(expression)
    value=eval(compile(node,'<C constant>','eval'),{'__builtins__':{}},{})
    if not isinstance(value,(int,float)):raise ValueError(expression)
    return value

def expand_constant_macros(expression,macros):
    for _ in range(16):
        changed=False
        for name,m in macros.items():
            match=re.search(r'\b'+re.escape(name)+r'\s*\(',expression)
            if not match:continue
            depth=1;end=match.end()
            while depth and end<len(expression):
                if expression[end]=='(':depth+=1
                elif expression[end]==')':depth-=1
                end+=1
            if depth:raise ValueError('unterminated constant macro')
            args=split(expression[match.end():end-1])
            if len(args)!=len(m['parameters']):raise ValueError('constant macro arity')
            body=m['body']
            for parameter,argument in zip(m['parameters'],args):body=re.sub(r'\b'+re.escape(parameter)+r'\b','('+argument+')',body)
            expression=expression[:match.start()]+'('+body+')'+expression[end:];changed=True
        if not changed:return expression
    raise ValueError('recursive constant macro')

def is_build_macro(name):
    return name in ('XGE_DEBUG','XGE_RELEASE','XGE_DEBUGMODE','XGE_XRT_HOST') or name.endswith('_API') or bool(re.match(r'^X(?:GE|UI)(?:3D)?_(?:ENABLE|HAS|BUILD)_',name))

def generate(root,data):
    reserved=set(re.findall(r'\{\"([A-Za-z_]\w*)\", XL6_KW_', (root/'src/lexer/xl6_lexer_gen.c').read_text(encoding='utf8')))
    storage_path=root/'docs/xge3-xui3-api-map-storage.json'
    storage_inventory=json.loads(storage_path.read_text(encoding='utf8'));storage=storage_inventory['entries']
    field_storage=storage_inventory.get('fieldEntries',{})
    storage_sources={}
    def reviewed_span(f,pointer,count):
        # A matching C shape proves ABI, never non-escape. New signatures must
        # receive an explicit native storage review before an implicit span.
        review=storage.get(f['name'],{})
        shape='"'+pointer+'","'+count+'"'
        if review.get('storage') not in ('native-copy-or-synchronous-read','bounded-synchronous-output') or shape not in review.get('declaration',''):
            f.update(state='requires-pointer-storage-review',reason='Counted native pointer lacks a matching reviewed non-escape contract')
            raise ValueError('unreviewed pointer storage')
        native_root=Path(data['headers'][0]['header']).parents[1]
        source=native_root/review['sourceFile']
        if source not in storage_sources:storage_sources[source]=source.read_text(encoding='utf-8-sig')
        if native_function_sha(storage_sources[source],f['name'])!=review['functionSha256']:
            f.update(state='requires-pointer-storage-review',reason='Native pointer consumer changed after the recorded storage review')
            raise ValueError('native pointer storage review is stale')
        f['storageContract']=dict(inventory=str(storage_path.relative_to(root)).replace('\\','/'),storage=review['storage'],sourceFile=review['sourceFile'],functionSha256=review['functionSha256'])
    for header in data['headers']:
        package=Path(header['header']).stem;folder=root/'lib/bindings'/package;src=folder/'src';src.mkdir(parents=True,exist_ok=True)
        types={t['name']:t['type'] for t in header['types']};records={};pending={n:t for n,t in types.items() if t['kind']=='struct' and t.get('fields') is not None}
        def primitive(t):
            if t['kind']=='named':
                name=' '.join(t['names'])
                if name in SCALARS:return SCALARS[name]
                if name in records:return records[name]
                if name in types and types[name]['kind']=='enum':return 'c_int'
                raise ValueError(name)
            raise ValueError(t['kind'])
        def field(t):
            if t['kind']=='array':
                base,extent=field(t['of']);size=integer(t['extent'],{})
                if not isinstance(size,int) or size<=0:raise ValueError('array extent')
                return base,extent+'['+str(size)+']'
            return primitive(t),''
        declarations=[]
        while pending:
            progress=False
            for name,t in list(pending.items()):
                try:
                    fields=[]
                    for f in t['fields']:
                        if not f['name'] or f['bits']:raise ValueError('non-flat field')
                        ft,extent=field(f['type']);fields.append(' '+ft+' '+(f['name']+'Value' if f['name'] in reserved else f['name'])+extent+(' @c_name(\"'+f['name']+'\")' if f['name'] in reserved else ''))
                    if not fields:raise ValueError('opaque')
                except (ValueError,TypeError):continue
                language=pascal(name);records[name]=language
                declarations.append('export extern "'+name+'" struct '+language+' @header("xge3_runtime.h")\n'+'\n'.join(fields)+'\nend\n')
                del pending[name];progress=True
            if not progress:break
        debug_types={t['name'] for t in header['types'] if any('XGE_HAS_DEBUGMODE' in c for c in t.get('conditions',[]))}
        records3d=[d for d in declarations if 'extern "xge3d_' in d]
        records_debug=[d for d in declarations if any('extern "'+name+'"' in d for name in debug_types)]
        records2d=[d for d in declarations if d not in records3d and d not in records_debug]
        base_records={language for name,language in records.items() if name not in debug_types and not name.startswith('xge3d_')}
        def qualify_base_records(declaration):
            for name in base_records:declaration=re.sub(r'\b'+re.escape(name)+r'\b','xge.'+name,declaration)
            return declaration
        (src/'records.xl').write_text('// Exact, pointer-free native value records. Generated from the authoritative header.\n'+ '\n'.join(records2d),encoding='utf8')
        if package=='xge':
            optional=folder/'three_d';(optional/'src').mkdir(parents=True,exist_ok=True)
            (optional/'src/records.xl').write_text('// Requires a matching 3D-enabled native SDK; no feature macros are changed by this module.\n'+'\n'.join(records3d),encoding='utf8')
            debug=folder/'debug';(debug/'src').mkdir(parents=True,exist_ok=True)
            (debug/'src/records.xl').write_text('// Requires XGE_HAS_DEBUGMODE in the native SDK before its first header include.\n'+'\n'.join(map(qualify_base_records,records_debug)),encoding='utf8')
        known={};constants=[];seen=set()
        # Conditional preprocessing controls are selected by the actual SDK,
        # not by the first textual #define in an all-branches inventory.
        candidates=[c for c in header['constants'] if not is_build_macro(c['name'])]
        for t in header['types']:
            if t['type']['kind']=='enum' and t['type'].get('values'):
                previous=-1
                for v in t['type']['values']:
                    try: previous=integer(v['expression'],known) if v['expression'] is not None else previous+1
                    except (ValueError,SyntaxError,NameError):break
                    known[v['name']]=previous;candidates.append(dict(name=v['name'],expression=str(previous)))
        remaining=list(candidates)
        constant_macros={m['name']:m for m in header['functionMacros']}
        for _ in range(20):
            next_remaining=[]
            for c in remaining:
                if c['name'] in seen:continue
                try:
                    expression=expand_constant_macros(c['expression'],constant_macros)
                    value=ast.literal_eval(expression) if re.fullmatch(r'"(?:\\.|[^"\\])*"',expression) else known[expression] if expression in known else integer(expression,known)
                except (ValueError,SyntaxError,NameError,ZeroDivisionError):next_remaining.append(c);continue
                known[c['name']]=value;seen.add(c['name'])
                if isinstance(value,str):typ='string';literal=json.dumps(value,ensure_ascii=False)
                elif isinstance(value,float):typ='c_float' if re.search(r'(?:\d\.|\.\d|[eE][+-]?\d).*?[fF]\b',expression) else 'c_double';literal=repr(value)
                else:typ='uint64' if value>0xffffffff else 'uint32' if value>0x7fffffff else 'int64' if value < -0x80000000 else 'c_int';literal=str(value)
                constants.append('export const '+typ+' '+c['name']+' = '+literal)
            if len(next_remaining)==len(remaining):break
            remaining=next_remaining
        (src/'constants.xl').write_text('// Numeric and text constants preserve native values without runtime dispatch or C wrappers.\n'+'\n'.join(constants)+'\n',encoding='utf8')
        for c in header['constants']:
            if c['name'] in seen:c.update(state='compile-time-constant',language=package+'.'+c['name'],value=known[c['name']])
            elif is_build_macro(c['name']):c['state']='native-build-configuration'
            else:c['state']='requires-compile-time-mapping'
        # Core functions without resources, buffers or escaping pointers. The
        # native integer result is retained; status-vs-count is never guessed.
        functions=[];mapped={};special=set()
        def param(t,index):
            if t['kind']=='pointer' and t['to'].get('kind')=='named' and 'const' in t['to'].get('qualifiers',[]):
                pointee=t['to'];name=' '.join(pointee['names'])
                if name=='char':return 'string',['@c_cstr('+str(index)+')']
                if name in records:return records[name],['@c_ref('+str(index)+')']
            return primitive(t),[]
        if package=='xge':
            for f in header['functions']:
                if f['name'] in ('xgeUnit','xgeRequestQuit'):
                    f.update(state='replaced-checked-entry',language='xge.unit' if f['name']=='xgeUnit' else 'xge.requestQuit',replacement='xgeUnitChecked' if f['name']=='xgeUnit' else 'xgeRequestQuitChecked');continue
                sig=f['type'];attrs=[];params=[];outputs=[];out_types=[];consumed=set()
                try:
                    ret=primitive(sig['returns'])
                    for i,p in enumerate(sig['parameters']):
                        if i in consumed:continue
                        if p.get('variadic'):raise ValueError('variadic')
                        if p['type']=={'kind':'named','names':['void'],'qualifiers':[]}:continue
                        t=p['type']
                        if t['kind']=='pointer' and i+1<len(sig['parameters']) and re.search(r'(?:size|length|count|capacity|bytes)$',sig['parameters'][i+1].get('name') or '',re.I):
                            next_type=sig['parameters'][i+1]['type'];ct=' '.join(next_type.get('names',[])) if next_type['kind']=='named' else ''
                            element=t['to'];ename=' '.join(element.get('names',[])) if element['kind']=='named' else ''
                            if ct not in ('int','size_t') or ename not in SCALARS and ename not in records:raise ValueError('unreviewed counted input/output')
                            readonly='const' in element.get('qualifiers',[]);pointer=('const ' if readonly else '')+ename+'*'
                            pointer=pointer.replace('unsigned char','uint8_t').replace('signed char','int8_t')
                            if not re.fullmatch(r'(?:const )?[A-Za-z_]\w*\*',pointer):raise ValueError('counted pointer spelling')
                            reviewed_span(f,pointer,ct)
                            pt='string' if pointer=='const char*' else 'bytes' if pointer in ('const void*','const uint8_t*') else 'Buffer'
                            attrs.append('@c_span('+str(len(params))+',"'+pointer+'","'+ct+'")');params.append(pt+' '+p['name']);consumed.add(i+1);continue
                        if t['kind']=='pointer' and 'const' not in t['to'].get('qualifiers',[]) and re.search(r'Get|Default|Identity|Init$',f['name']):
                            pt=primitive(t['to'])
                            if not pt:raise ValueError('void output')
                            outputs.append(i);out_types.append(pt);continue
                        pt,at=param(t,len(params));attrs.extend(at);params.append(pt+' '+('arg'+str(i) if not p['name'] or p['name'] in reserved else p['name']))
                    if outputs:
                        if ret not in (None,'c_int'):raise ValueError('output producer has unreviewed native result')
                        attrs.append('@no_raise')
                        attrs.append('@c_output('+str(outputs[0])+(')' if ret is None else ',0)') if len(outputs)==1 else '@c_outputs('+','.join(map(str,outputs))+')'+(' @c_status(0)' if ret else ''))
                        ret=out_types[0] if len(out_types)==1 else '('+','.join(out_types)+')'
                    if f['name']=='xgeGetBuildInfo':attrs.append('@c_output_size(0,"size")')
                except ValueError:continue
                name=f['name'][3:];name='threeD'+name[2:] if name.startswith('3d') else name[:1].lower()+name[1:];name='native'+name[:1].upper()+name[1:] if name in reserved else name
                if f['name']=='xgeUnitChecked':name='unit';ret=None;attrs+=['@no_raise','@c_result("status",0)']
                if name in special:continue
                special.add(name)
                line='export extern "'+f['name']+'" @header("xge3_runtime.h") '+(' '.join(attrs)+' ' if attrs else '')+'func '+name+'('+','.join(params)+')'+(' as '+ret if ret else '')
                functions.append(line);mapped[f['name']]=('xge3d.' if f['name'].startswith('xge3d') else 'xgeDebug.' if any('XGE_HAS_DEBUGMODE' in c for c in f.get('conditions',[])) else 'xge.')+name
            functions3d=[f for f in functions if 'extern "xge3d' in f]
            debug_names={f['name'] for f in header['functions'] if any('XGE_HAS_DEBUGMODE' in c for c in f.get('conditions',[]))}
            functions_debug=[f for f in functions if any('extern "'+name+'"' in f for name in debug_names)]
            functions2d=[f for f in functions if f not in functions3d and f not in functions_debug]
            (src/'functions.xl').write_text('// Direct C declarations. Integer results retain the authoritative C status/count semantics.\n'+'\n'.join(functions2d)+'\n',encoding='utf8')
            (optional/'src/functions.xl').write_text('\n'.join(functions3d)+'\n',encoding='utf8')
            (debug/'src/functions.xl').write_text('\n'.join(map(qualify_base_records,functions_debug))+'\n',encoding='utf8')
            optional_manifest={'format':'xlang-package-1','name':'xge3d','version':'3.0.0','abiVersion':'2','kind':'xlang-extlib','artifact':{'kind':'source','cSources':[],'xlSources':['src/bootstrap.xl','src/records.xl','src/functions.xl']},'links':[],'loadlibs':[],'dependencies':[{'name':'xge','version':'3.0.0','abiVersion':'2'}],'requiresNativeFeatures':['XGE_ENABLE_3D'],'native':{'singleton':False,'loadName':'','version':''}}
            (optional/'xlang.package.json').write_text(json.dumps(optional_manifest,indent=2)+'\n',encoding='utf8')
            debug_manifest=dict(optional_manifest,name='xgeDebug',requiresNativeFeatures=['XGE_HAS_DEBUGMODE'])
            (debug/'xlang.package.json').write_text(json.dumps(debug_manifest,indent=2)+'\n',encoding='utf8')
            for extension in (optional,debug):(extension/'src/bootstrap.xl').write_text('// Normal module import executes the shared native initializer once.\nimport "xge"\n',encoding='utf8')
            object_mapping=generate_xge_objects(root,header,records,types,primitive,param,reserved,reviewed_span)
            mapped.update(object_mapping)
        if package=='xui':
            managed,mapping=generate_xui(root,header,records,types,primitive,param,reserved,reviewed_span)
            mapped.update(mapping)
        header['bindingSummary']=dict(pointerFreeRecords=len(records),numericConstants=sum(isinstance(v,(int,float)) for v in known.values()),textConstants=sum(isinstance(v,str) for v in known.values()),exportedConstants=len(constants),scalarFunctions=len(functions))
        header['bindingTypes']={name:dict(language=(('xge3d.' if name.startswith('xge3d_') else 'xgeDebug.' if name in debug_types else package+'.')+records[name]) if name in records else None,state='direct-value-record' if name in records else 'requires-lifetime-or-layout-contract') for name in types}
        for f in header['functions']:
            if f['name'] in mapped:f.update(state='direct-declaration',language=mapped[f['name']])
        (src/'bootstrap.xl').write_text('// Normal module import executes the shared native initializer once.\nimport "_xge_native"\n',encoding='utf8')
        manifest={'format':'xlang-package-1','name':package,'version':'3.0.0','abiVersion':'2','kind':'xlang-extlib','artifact':{'kind':'source','cSources':[],'xlSources':['src/bootstrap.xl','src/records.xl','src/constants.xl']+(['src/functions.xl','src/resources.xl','src/callbacks.xl','src/shapes.xl'] if package=='xge' else ['src/managed.xl','src/cell.xl','src/proxy.xl','src/draw.xl','src/text_shape.xl','src/managers.xl'])},'links':[],'loadlibs':[],'dependencies':[{'name':'_xge_native','version':'3.0.0','abiVersion':'2'}],'native':{'singleton':False,'loadName':'','version':''}}
        (folder/'xlang.package.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8')
    annotate_existing_bindings(root,data,strict_storage=True,generated=True)


def annotate_existing_bindings(root,data,strict_storage=False,generated=False):
    """Read existing public declarations without generating or editing sources.

    Presence is deliberately independent of compiler/header ABI proof and
    runtime qualification. Stale pointer reviews are reported; the generator
    remains fail-closed when strict_storage is selected.
    """
    storage_path=root/'docs/xge3-xui3-api-map-storage.json'
    field_storage=json.loads(storage_path.read_text(encoding='utf8')).get('fieldEntries',{})
    pins={}
    for header in data['headers']:
        package=Path(header['header']).stem;folder=root/'lib/bindings'/package
        folders=[folder]
        if package=='xge':folders += [folder/'three_d',folder/'debug']
        by_name={f['name']:f for f in header['functions']}
        callable_fields={(f['record'],f['member']):f for f in header['callableFields']}
        classes=[];records=[];unresolved=[]
        for public_folder in folders:
            manifest_path=public_folder/'xlang.package.json'
            if not manifest_path.is_file():continue
            manifest_bytes=manifest_path.read_bytes();manifest=json.loads(manifest_bytes.decode('utf-8-sig'));public_package=manifest['name']
            pins[manifest_path.relative_to(root).as_posix()]=hashlib.sha256(manifest_bytes).hexdigest()
            for relative in manifest['artifact']['xlSources']:
                path=public_folder/relative;source_bytes=path.read_bytes();source=source_bytes.decode('utf-8-sig');source_path=path.relative_to(root).as_posix()
                pins[source_path]=hashlib.sha256(source_bytes).hexdigest();owner=None;native_owner=None
                for number,line in enumerate(source.splitlines(),1):
                    cls=re.match(r'export extern "([^"]+)" class (\w+).*@lifetime\("([^"]+)"\)',line)
                    if cls:
                        owner=cls[2];native_owner=cls[1].removeprefix('const ').rstrip('*').strip()
                        classes.append(dict(language=public_package+'.'+owner,cName=cls[1],lifetime=cls[3],declaration=line,source=source_path,line=number));continue
                    record=re.match(r'export extern "([^"]+)" struct (\w+)',line)
                    if record:records.append(dict(language=public_package+'.'+record[2],cName=record[1],source=source_path,line=number))
                    if line=='end':owner=None;native_owner=None;continue
                    declaration=re.search(r'extern "([^"]+)".*\b(func|get|let) (\w+)\(',line)
                    if not declaration or not (owner or line.startswith('export extern')):continue
                    binding=dict(language=public_package+'.'+(owner+'.' if owner else '')+declaration[3],source=source_path,line=number,declaration=line.strip(),profile='default' if public_folder==folder else public_package,
                                 evidence=dict(declarationPresent=True,abiAccepted=False,runtimeAccepted=False))
                    binding['surface']='explicit-C-interop' if re.search(r'\bNative\w*Pointer\??\b',line) else 'typed-declaration'
                    if '@c_field_call' in line:
                        field=callable_fields.get((native_owner,declaration[1]))
                        if field is None:
                            unresolved.append(dict(native=declaration[1],owner=native_owner,**binding));continue
                        if '@c_span(' in line or '@c_region(' in line or re.search(r'\[[1-9][0-9]*\]',line):
                            review=field_storage.get(native_owner+'.'+declaration[1]);matching=False
                            if review:
                                source_file=Path(header['header']).parents[1]/review['sourceFile']
                                matching=source_file.is_file() and native_function_sha(source_file.read_text(encoding='utf-8-sig'),review['nativeHelper'])==review['functionSha256']
                            binding['storageReview']=dict(state='matching' if matching else 'changed' if review else 'missing',inventory=str(storage_path.relative_to(root)).replace('\\','/'))
                            if strict_storage and not matching:raise ValueError('callable-field pointer storage requires matching native review: '+native_owner+'.'+declaration[1])
                            if matching:field['storageContract']=dict(inventory=str(storage_path.relative_to(root)).replace('\\','/'),**review)
                        field.update(state='direct-callable-field-declaration' if generated else 'callable-field-declaration-present')
                        field.setdefault('bindings',[]).append(binding)
                    elif declaration[1] in by_name:
                        function=by_name[declaration[1]];function.update(state='direct-declaration' if generated else 'declaration-present',language=binding['language'],bindingSource=source_path)
                        function.setdefault('declarations',[]).append(binding)
                    elif not re.search(r'@c_(?:field|deref|control)\b',line):
                        unresolved.append(dict(native=declaration[1],owner=native_owner,authority='runtime-or-CRT' if declaration[1].startswith(('xl6','xrt')) or declaration[1] in ('strlen',) else 'not-in-primary-header',**binding))
        header.update(classBindings=classes,recordBindings=records,unresolvedDeclarations=unresolved)
        summary=header.setdefault('bindingSummary',{})
        summary.update(publicFunctionsDeclared=sum(bool(f.get('declarations')) for f in header['functions']),callableFieldsDeclared=sum(bool(f.get('bindings')) for f in header['callableFields']),foreignClasses=len(classes),recordDeclarations=len(records),nonPrimaryDeclarations=len(unresolved))
        if generated:summary.update(publicFunctionsMapped=summary['publicFunctionsDeclared'],callableFieldsMapped=summary['callableFieldsDeclared'])
        print(package,summary)
    unchanged=all(hashlib.sha256((root/path).read_bytes()).hexdigest()==digest for path,digest in pins.items())
    if not unchanged:raise ValueError('public binding source changed during inventory annotation')
    data['existingBindingAnnotation']=dict(declarationOnly=True,abiAccepted=False,runtimeAccepted=False,sourceFilesModified=False,inputsUnchanged=unchanged,generatorWroteSourcesBeforeAnnotation=generated,sourceSha256=pins)


def generate_xge_objects(root,header,records,types,primitive,param,reserved,reviewed_span):
    families={'xge_shape_ex':('Shape','xl6Xge3ShapePolicy','xgeShapeEx','xgeShapeExDestroy'),
              'xge_shape_ex_scene':('ShapeScene','xl6Xge3ShapeScenePolicy','xgeShapeExScene','xgeShapeExSceneDestroy'),
              'xge_shape_ex_path_measure':('PathMeasure','xl6Xge3PathMeasurePolicy','xgeShapeExPathMeasure','xgeShapeExPathMeasureDestroy')}
    mapped={};methods={n:[] for n,_,_,_ in families.values()};factories=[]
    def family(t):
        if t['kind']=='named' and len(t['names'])==1:return families.get(t['names'][0])
        return None
    for f in header['functions']:
        p=f['type']['parameters'];ret=f['type']['returns'];params=[];attrs=[];outputs=[];values=[];factory=None
        if not p:continue
        owner=family(p[0]['type'])
        if not owner:continue
        if f['name'] in [v[3] for v in families.values()] or f['name'].endswith('AddRef'):continue
        try:
            result=primitive(ret)
            i=1
            while i<len(p):
                a=p[i];t=a['type'];logical=len(params)
                if t['kind']=='pointer' and family(t['to']):
                    if not re.search(r'Clone$',f['name']):raise ValueError('unreviewed borrowed/resource output')
                    if factory or outputs:raise ValueError('mixed native outputs')
                    factory=family(t['to']);outputs.append(i);i+=1;continue
                resource=family(t)
                if resource:params.append(resource[0]+' '+a['name']);i+=1;continue
                if t['kind']=='pointer' and i+1<len(p) and re.search(r'(?:size|length|count|capacity|bytes)$',p[i+1].get('name') or '',re.I):
                    count=p[i+1]['type'];ct=' '.join(count.get('names',[])) if count['kind']=='named' else ''
                    el=t['to'];en=' '.join(el.get('names',[])) if el['kind']=='named' else ''
                    if ct not in ('int','size_t') or en not in SCALARS and en not in records:raise ValueError('array contract')
                    pointer=('const ' if 'const' in el.get('qualifiers',[]) else '')+en+'*'
                    pointer=pointer.replace('unsigned char','uint8_t').replace('signed char','int8_t')
                    if not re.fullmatch(r'(?:const )?[A-Za-z_]\w*\*',pointer):raise ValueError('array spelling')
                    reviewed_span(f,pointer,ct)
                    pt='string' if pointer=='const char*' else 'bytes' if pointer in ('const void*','const uint8_t*') else 'Buffer'
                    attrs.append('@c_span('+str(logical)+',"'+pointer+'","'+ct+'")');params.append(pt+' '+a['name']);i+=2;continue
                if t['kind']=='pointer' and 'const' not in t['to'].get('qualifiers',[]):
                    pt=primitive(t['to'])
                    if not pt:raise ValueError('opaque output')
                    outputs.append(i);values.append(pt);i+=1;continue
                pt,at=param(t,logical);attrs+=at;params.append(pt+' '+('arg'+str(i) if a['name'] in reserved else a['name']));i+=1
            if result!='c_int':raise ValueError('family methods must retain reviewed status semantics')
            if factory:
                if len(outputs)!=1 or values:raise ValueError('multiple resource outputs')
                attrs+=['@c_result("owned_view")','@factory('+str(outputs[0])+',0,rollback'+factory[0]+')'];result=factory[0]
            elif outputs:
                attrs.append('@c_output('+str(outputs[0])+',0)' if len(outputs)==1 else '@c_outputs('+','.join(map(str,outputs))+') @c_status(0)')
                result=values[0] if len(values)==1 else '('+','.join(values)+')'
            else:attrs.append('@c_result("status",0)');result=None
            name=f['name'][len(owner[2]):];name=name[:1].lower()+name[1:]
            if name=='close':name='closePath'
            if not name or name in reserved:name='native'+name[:1].upper()+name[1:]
            methods[owner[0]].append(' extern "'+f['name']+'" @no_raise '+ ' '.join(attrs)+' func '+name+'('+','.join(params)+')'+(' as '+result if result else ''))
            mapped[f['name']]='xge.'+owner[0]+'.'+name
        except ValueError:continue
    lines=['// Native-only shape graphs: own one exact native reference, no language callback slots.']
    for native,(name,policy,prefix,destroy) in families.items():
        lines+=['export extern "'+native+'" class '+name+' @header("xge3_runtime.h") @lifetime("owned") @policy("'+policy+'")',' native point handle',*methods[name],
                ' extern "xl6ForeignViewCloseV1" @no_raise @c_control(0) @c_result("bool_status") func close()','end',
                'extern "'+destroy+'" @no_raise func rollback'+name+'('+name+' value)']
        if name in ('Shape','ShapeScene'):
            cname=prefix+'Create';public='create'+name
            lines+=['export extern "'+cname+'" @no_raise @c_result("owned_view") @factory(0,0,rollback'+name+') func '+public+'() as '+name];mapped[cname]='xge.'+public
    lines+=['export extern "xgeShapeExPathMeasureCreate" @no_raise @c_ref(1) @c_result("owned_view") @factory(0,0,rollbackPathMeasure) func measurePath(Shape shape,ShapeExMatrix matrix,c_float tolerance) as PathMeasure']
    mapped['xgeShapeExPathMeasureCreate']='xge.measurePath'
    (root/'lib/bindings/xge/src/shapes.xl').write_text('\n'.join(lines)+'\n',encoding='utf8')
    return mapped


def generate_xui(root,header,records,types,primitive,param,reserved,reviewed_span):
    src=root/'lib/bindings/xui/src';functions=header['functions'];mapped={};raw={};callbacks={};views={};classes={'Widget':[]};context=[];free=[]
    # A resource-shaped C argument is not itself an owning storage proof.
    # These publishers/consumers have separately reviewed native owning slots
    # or synchronous use; do not broaden this list by matching a type name.
    surface_setters={
        'xuiBreadcrumbSetSeparatorIcon','xuiImageSetSurface','xuiQrCodeSetIcon',
        'xuiPanelSetIcon','xuiButtonSetIcon','xuiButtonSetBadgeSurface',
        'xuiCheckBoxSetIndicatorSurface','xuiCheckBoxSetMixedIndicatorSurface',
        'xuiRadioSetIndicatorSurface','xuiToggleSetIndicatorSurface',
        'xuiVirtualJoystickSetSurface','xuiWindowSetIcon',
        'xuiDockPanelSetWindowIcon','xuiToolbarSetItemIcon'}
    surface_getters={
        'xuiBreadcrumbGetSeparatorIcon','xuiImageGetSurface','xuiQrCodeGetIcon',
        'xuiPanelGetIconSurface','xuiButtonGetIconSurface','xuiWindowGetIconSurface',
        'xuiDockPanelGetWindowIconSurface','xuiCanvasGetSurface','xuiWidgetGetCacheSurface'}
    surface_outputs={
        'xuiButtonGetIcon','xuiButtonGetBadgeSurface',
        'xuiCheckBoxGetIndicatorSurface','xuiCheckBoxGetMixedIndicatorSurface',
        'xuiRadioGetIndicatorSurface','xuiToggleGetIndicatorSurface',
        'xuiVirtualJoystickGetSurface','xuiBuiltinAssetGetAtlas'}
    surface_consumers={'xuiRender','xuiWidgetRenderCache','xuiWidgetRenderTree',
                       'xuiCanvasDrawSurface','xuiCanvasDrawSurfaceQuad'}
    surface_allowed=surface_setters|surface_getters|surface_outputs|surface_consumers
    resource_allowed=surface_allowed|{'xuiWidgetUpdateBegin','xuiWidgetUpdateEnd'}
    current_function=None
    # Native ownership drafts remain in the authoritative inventory, but are
    # not public callable bindings until their consumer/lifetime gate passes.
    # Exact Grid owning output signatures come from the matching native header.
    pending_native=set()
    for f in functions:
        if f['name'] in pending_native:f.update(state='requires-native-lifecycle-acceptance',reason='Native owning output/adapter draft is not in the matched accepted SDK')
    sources={};native_bodies={}
    for path in Path(header['header']).parents[1].joinpath('src/xui').rglob('*.c'):
        sources[path]=clean(path.read_text(encoding='utf-8-sig'))
        text=sources[path]
        for m in re.finditer(r'(?m)^[A-Za-z_][\w \t*]*?\b(xui[A-Z]\w*)\s*\([^;{}]*\)\s*\{',text):
            depth=1;end=m.end();quote=None
            while depth and end<len(text):
                if quote:
                    if text[end]=='\\':end+=2;continue
                    if text[end]==quote:quote=None
                elif text[end] in ('"',"'"):quote=text[end]
                elif text[end]=='{':depth+=1
                elif text[end]=='}':depth-=1
                end+=1
            native_bodies[m.group(1)]=(path,text[m.end():end-1])
    def spelling(t):
        if t['kind']=='named':return (' '.join(t.get('qualifiers',[]))+' '+' '.join(t['names'])).strip()
        if t['kind']=='struct':return (' '.join(t.get('qualifiers',[]))+' struct '+t['tag']).strip()
        if t['kind']=='pointer':return spelling(t['to'])+'*'
        raise ValueError(t['kind'])
    def same(t,name):return t.get('kind')=='named' and t.get('names')==[name]
    def widget(t):return same(t,'xui_widget') or t.get('kind')=='pointer' and (same(t['to'],'xui_widget_t') or t['to'].get('kind')=='struct' and t['to'].get('tag')=='xui_widget_t')
    def root_type(t):return widget(t) or same(t,'xui_context') or t.get('kind')=='pointer' and t['to'].get('kind')=='struct' and t['to'].get('tag')=='xui_context_t'
    def pointer_alias(t):
        c=spelling(t)
        name='Native'+pascal(c.replace('const ','Const_').replace('struct ','').replace('*','_Pointer'))
        raw[c]=name;return name
    def simple(t,index,result=False):
        if widget(t):return 'Widget'+('?' if result else ''),[]
        if same(t,'xui_context'):return 'Context'+('?' if result else ''),[]
        if same(t,'xui_font'):return 'Font'+('?' if result else ''),[]
        if same(t,'xui_proxy'):return 'Proxy'+('?' if result else ''),[]
        if same(t,'xui_surface'):
            if current_function not in surface_allowed:raise ValueError('Surface ownership/consumer contract not reviewed')
            return 'Surface'+('?' if result and current_function!='xuiCanvasGetSurface' else ''),[]
        if same(t,'xui_draw_context'):
            if current_function not in resource_allowed:raise ValueError('Draw session contract not reviewed')
            return 'DrawContext',[]
        if result and t['kind']=='pointer' and same(t['to'],'char') and 'const' in t['to'].get('qualifiers',[]):return 'string?',[]
        return param(t,index) if not result else (primitive(t),[])
    def source_contract(f):
        # Require literal native success for status/output adaptation; otherwise
        # preserve its integer result and leave its pointer output unadapted.
        if f['name'] in native_bodies:
            path,body=native_bodies[f['name']]
            font_slot_status=f['name'].endswith('SetFont') and bool(re.search(r'\bxuiInternalFontSlot(?:Begin|Prepare)\(',body))
            return dict(file=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),statusZero=font_slot_status or bool(re.search(r'\breturn\s+XUI_OK\s*;',body)))
        return {}
    constructors=[]
    for f in functions:
        if f['name']=='xuiListViewSetSelectionBuffer':
            f.update(state='requires-owned-pinned-storage',reason='Native arrExternalSelected retains the mutable caller array beyond return');continue
        p=f['type']['parameters']
        if len(p)<2 or not same(p[0]['type'],'xui_context') or p[1]['type'].get('kind')!='pointer' or not same(p[1]['type']['to'],'xui_widget') or not f['name'].endswith('Create'):continue
        group=f['name'][3:-6];classes.setdefault(group,[]);params=[];attrs=[]
        raw_descriptor=False
        for i,a in enumerate(p[2:]):
            try:typ,at=simple(a['type'],i)
            except ValueError:
                raw_descriptor=True;typ=pointer_alias(a['type']);at=[]
                # NULL defaults require an actual validating native helper.
                dtype=' '.join(a['type'].get('to',{}).get('names',[]))
                null_default=False
                for text in sources.values():
                    helper=re.search(r'\b\w+DescValid\s*\(\s*const\s+'+re.escape(dtype)+r'\s*\*\s*pDesc\s*\)\s*\{',text)
                    if helper and re.search(r'if\s*\(\s*pDesc\s*==\s*NULL\s*\)\s*(?:\{\s*)?return\s+1\s*;',text[helper.end():helper.end()+140]):null_default=True;break
                if null_default:typ+='?'
            params.append(typ+' '+a['name']+('=null' if typ.endswith('?') else ''));attrs+=at
        method=group[:1].lower()+group[1:]
        if raw_descriptor:method+='Native'
        if method in reserved:method='create'+group
        declaration=' extern "'+f['name']+'" @no_raise @c_result("owner_view",0) @factory(1,0,rollback'+group+') '+(' '.join(attrs)+' ' if attrs else '')+'func '+method+'('+','.join(params)+') as '+group
        context.append(declaration);mapped[f['name']]='xui.Context.'+method;constructors.append(f['name'])
    groups=sorted(classes,key=len,reverse=True)
    callback_declarations=[];callback_spans={}
    def callback_view(t):
        c=spelling(t)
        if c in views:return views[c]
        name='Callback'+('Const' if c.startswith('const ') else '')+pascal(c.replace('const ','').replace('struct ','').replace('*',''))
        if c=='const char*':name='CString'
        if c=='const xbytesview*':name='ByteSlice'
        views[c]=name
        body=['export extern "'+c+'" class '+name+' @header("xge3_runtime.h") @lifetime("callback")',' native point handle']
        native=''
        if c=='const xbytesview*':
            body+=[' extern "Size" @c_field get size() as c_size',
                   ' extern "Data" @c_field @c_length("Size","size_t") @c_result("bytes_view") get bytes() as bytes']
        elif c=='const char*':
            body+=[' extern "strlen" @no_raise get length() as c_size',' extern "xrtStrDup" @owned func copy() as string']
        else:
            target=t['to'];native=' '.join(target.get('names',[])) if target['kind']=='named' else target.get('tag','');record=types.get(native,{})
            scalar=SCALARS.get(native)
            if scalar:
                body.append(' extern "value" @c_deref get value() as '+scalar)
                if 'const' not in target.get('qualifiers',[]):body.append(' extern "value" @c_deref let value('+scalar+' next)')
            for f in record.get('fields') or []:
                try:ft=primitive(f['type'])
                except ValueError:continue
                if not ft or f.get('bits') or not f.get('name') or f['name'].startswith('_'):continue
                language=f['name']+'Value' if f['name'] in reserved else f['name']
                body.append(' extern "'+f['name']+'" @c_field get '+language+'() as '+ft)
                if 'const' not in target.get('qualifiers',[]):body.append(' extern "'+f['name']+'" @c_field let '+language+'('+ft+' value)')
        if native in ('xui_tree_view_node_output_t','xui_table_view_cell_output_t'):
            prefix='xuiTreeViewNodeOutput' if native=='xui_tree_view_node_output_t' else 'xuiTableViewCellOutput'
            const='' if 'const' in target.get('qualifiers',[]) else '@c_const(0) '
            body+=[' extern "'+prefix+'GetText" @no_raise '+const+'@c_result("text_view") func text() as string']
            if 'const' not in target.get('qualifiers',[]):body+=[' extern "'+prefix+'SetText" @no_raise @c_text_view(0) @c_result("status",0) func setText(string text)']
            if native=='xui_table_view_cell_output_t':
                body+=[' extern "'+prefix+'GetTooltip" @no_raise '+const+'@c_result("text_view") func tooltip() as string',
                       ' extern "'+prefix+'GetValue" @no_raise '+const+'@c_result("borrowed_value") func value() as var']
                if 'const' not in target.get('qualifiers',[]):body+=[' extern "'+prefix+'SetTooltip" @no_raise @c_text_view(0) @c_result("status",0) func setTooltip(string text)',
                     ' extern "'+prefix+'SetValue" @no_raise @c_const(1) @c_result("status",0) func setValue(var value)',
                     ' extern "'+prefix+'Use" @no_raise @c_result("status",0) func use(TableCellOutput cached)']
        if native=='xui_widget_t':
            body+=[' extern "xuiWidgetGetRect" @no_raise func rect() as Rect']
        if native=='xui_table_grid_editor_config_output_t':
            const='' if 'const' in target.get('qualifiers',[]) else '@c_const(0) '
            prefix='xuiTableGridEditorConfigOutput'
            body+=[' extern "'+prefix+'GetEnumCount" @no_raise '+const+'func enumCount() as c_int',
                   ' extern "'+prefix+'GetEnumItem" @no_raise '+const+'@c_outputs(2,3,4,5,6) @c_status(0) @c_output_view(2,"text") func enumItem(c_int index) as (string,c_int,c_int,c_int,c_int)',
                   ' extern "'+prefix+'GetPaletteCount" @no_raise '+const+'func paletteCount() as c_int',
                   ' extern "'+prefix+'GetPaletteItem" @no_raise '+const+'@c_output(2,0) func paletteItem(c_int index) as c_uint']
            for field in ('DateFormat','ActionText','FileFilter','RangeSeparator'):
                accessor=field[:1].lower()+field[1:]
                body+=[' extern "'+prefix+'Get'+field+'" @no_raise '+const+'@c_result("text_view") func '+accessor+'() as string']
                if 'const' not in target.get('qualifiers',[]):
                    body+=[' extern "'+prefix+'Set'+field+'" @no_raise @c_text_view(0) @c_result("status",0) func set'+field+'(string text)']
            if 'const' not in target.get('qualifiers',[]):
                palette=next(f for f in functions if f['name']=='xuiTableGridEditorConfigOutputSetPalette')
                reviewed_span(palette,'const uint32_t*','int')
                body+=[' extern "'+prefix+'SetEnumCount" @no_raise @c_result("status",0) func setEnumCount(c_int count)',
                       ' extern "'+prefix+'SetEnumItem" @no_raise @c_text_view(1) @c_result("status",0) func setEnumItem(c_int index,string text,c_int value,c_int enabled,c_int separator,c_int icon)',
                       ' extern "'+prefix+'SetPaletteCount" @no_raise @c_result("status",0) func setPaletteCount(c_int count)',
                       ' extern "'+prefix+'SetPalette" @no_raise @c_span(0,"const uint32_t*","int") @c_result("status",0) func setPalette(Buffer colors)',
                       ' extern "'+prefix+'SetPaletteItem" @no_raise @c_result("status",0) func setPaletteItem(c_int index,c_uint color)',
                       ' extern "'+prefix+'Dispose" @no_raise func reset()']
        body+=['end'];callback_declarations.append('\n'.join(body));return name
    failures={'xui_widget_content_measure_proc':-16,'xui_widget_layout_prepare_proc':-16,'xui_widget_layout_children_proc':-16,
              'xui_widget_layout_complete_proc':-16,'xui_widget_event_proc':-16,'xui_widget_cursor_proc':-1,
              'xui_widget_ime_rect_proc':0,'xui_widget_cache_render_proc':-16,'xui_widget_update_proc':-16,
              'xui_tree_view_count_proc':-16,'xui_tree_view_node_owned_proc':-16,'xui_table_view_count_proc':-16,
              'xui_table_view_cell_owned_proc':-16,'xui_table_view_cell_render_owned_proc':-16}
    # These exact native consumers propagate negative event/merge failures.
    # Timeline's changing/opening consumers instead use C boolean veto: a
    # negative fallback would permit the action, so its failure value is zero.
    failures.update({name:-16 for name in ('xui_breadcrumb_context_proc','xui_tag_input_context_proc','xui_tabs_context_proc',
        'xui_window_context_proc','xui_dock_context_proc','xui_list_view_context_proc','xui_tree_view_context_proc',
        'xui_table_view_context_proc','xui_table_grid_context_proc','xui_property_grid_context_proc',
        'xui_toolbar_context_proc','xui_statusbar_context_proc','xui_flow_context_proc')})
    failures.update({name:0 for name in ('xui_timeline_current_frame_changing_proc','xui_timeline_layer_changing_proc',
        'xui_timeline_frame_changing_proc','xui_timeline_span_changing_proc','xui_timeline_context_opening_proc')})
    failures['xui_table_view_merge_proc']=-16
    failures.update({name:-16 for name in ('xui_table_grid_validate_proc','xui_table_grid_editor_owned_proc','xui_table_grid_editor_config_owned_proc')})
    # These Native records contain a retained Surface slot, not a trivial
    # language record. Flat inputs pack one stack descriptor; borrowed outputs
    # project POD fields plus one independently retained resource.
    nine_patch={}
    for native,owner,member,state in (
        ('xuiButtonSetPatch','Button','setPatch',True),('xuiButtonGetPatch','Button','getPatch',True),
        ('xuiProgressSetTrackPatch','Progress','setTrackPatch',False),('xuiProgressGetTrackPatch','Progress','getTrackPatch',False),
        ('xuiProgressSetFillPatch','Progress','setFillPatch',False),('xuiProgressGetFillPatch','Progress','getFillPatch',False)):
        setting='Set' in native;slot=2 if state else 1;first=1 if state else 0
        prefix='c_uint state,' if state else ''
        if setting:
            attrs='@c_result("status",0) @c_pack('+str(slot)+',"xui_nine_patch_t",'+str(first)+',5) @c_pack_size('+str(slot)+',"iSize","uint32_t")'
            for offset,field,kind in ((0,'pSurface','view_raw'),(1,'tSrc','value'),(2,'tSlice','value'),(3,'iColor','value'),(4,'iMode','value')):
                attrs+=' @c_pack_field('+str(slot)+',"'+field+'",'+str(first+offset)+',"'+kind+'")'
            signature='('+prefix+'Surface surface,Rect source,Thickness slice,c_uint color=0xFFFFFFFF,c_int mode=0)'
        else:
            attrs='@c_output_record('+str(slot)+',"xui_nine_patch_t","borrowed") @c_status(0) @c_output_field(0,"pSurface","retain",retainSurface,"zero",rollbackSurface)'
            for ordinal,field in ((1,'tSrc'),(2,'tSlice'),(3,'iColor'),(4,'iMode')):attrs+=' @c_output_field('+str(ordinal)+',"'+field+'")'
            signature='('+('c_uint state' if state else '')+') as (Surface?,Rect,Thickness,c_uint,c_int)'
        nine_patch[native]=(owner,member,' extern "'+native+'" @no_raise '+attrs+' func '+member+signature)
    def callback_type(native):
        if native in callbacks:return callbacks[native]
        t=types[native]
        if t['kind']!='pointer' or t['to']['kind']!='function':raise ValueError('not callback')
        signature=t['to'];args=signature['parameters']
        if not args or spelling(args[-1]['type'])!='void*':raise ValueError('userdata order')
        owned_text=native=='xui_table_view_cell_text_owned_proc'
        result='string' if owned_text else primitive(signature['returns']);params=[]
        if result and native not in failures and not owned_text:raise ValueError('nonvoid needs explicit failure review')
        spans=[];i=0
        while i<len(args)-1:
            a=args[i];t=a['type']
            if t['kind']=='pointer' and 'const' in t['to'].get('qualifiers',[]) and i+1<len(args)-1 and re.search(r'(?:size|length|count|bytes)$',args[i+1].get('name') or '',re.I):
                count=args[i+1]['type'];count_name=' '.join(count.get('names',[])) if count['kind']=='named' else ''
                pointer=spelling(t).replace('unsigned char','uint8_t')
                if count_name not in ('int','size_t') or pointer not in ('const void*','const uint8_t*','const char*'):raise ValueError('callback span needs exact byte count')
                pt=callback_view(dict(kind='pointer',qualifiers=[],to=dict(kind='named',names=['xbytesview'],qualifiers=['const'])))
                spans.append((len(params),pointer,count_name));params.append(pt);i+=2;continue
            if widget(t):
                pt=callback_view(dict(kind='pointer',qualifiers=[],to=dict(kind='named',names=['xui_widget_t'],qualifiers=[]))) if native in ('xui_table_view_cell_text_owned_proc','xui_table_view_cell_render_owned_proc') else 'Widget'
            elif same(t,'xui_context'):pt='Context'
            elif t['kind']=='pointer':pt=callback_view(t)
            elif t['kind']=='named' and types.get(t['names'][0],{}).get('kind')=='pointer':pt=callback_view(types[t['names'][0]])
            else:pt=primitive(t)
            if not pt:raise ValueError('callback void argument')
            params.append(pt);i+=1
        name=pascal(native);callbacks[native]=name
        callback_spans[native]=spans
        callback_declarations.append('export typedef '+name+' = func('+','.join(params)+')'+(' as '+result if result else ''))
        return name
    for f in functions:
        if f['name'] in nine_patch:
            owner,member,declaration=nine_patch[f['name']];p=f['type']['parameters'];setting='Set' in f['name'];state=owner=='Button'
            expected=(['uint32_t'] if state else [])+['const xui_nine_patch_t*' if setting else 'xui_nine_patch_t*']
            if not p or not widget(p[0]['type']) or [spelling(a['type']) for a in p[1:]]!=expected or spelling(f['type']['returns'])!='int':
                raise ValueError('NinePatch authoritative physical signature changed: '+f['name'])
            classes[owner].append(declaration);mapped[f['name']]='xui.'+owner+'.'+member;f['sourceContract']=source_contract(f);continue
        if f.get('state') in ('requires-owned-pinned-storage','requires-native-lifecycle-acceptance') or f['name'] in constructors or f['name'] in ('xuiCreate','xuiCreateOwned','xuiGetProxyOwned','xuiSetProxyOwned','xuiDestroy','xuiWidgetDestroy','xuiWidgetDestroyChecked','xuiContextLifetimeWatch','xuiWidgetLifetimeWatch'):continue
        current_function=f['name']
        p=f['type']['parameters'];sig=f['type'];receiver=False;owner=None;prefix='xui'
        if p and same(p[0]['type'],'xui_context'):owner='Context';receiver=True
        elif p and widget(p[0]['type']):
            owner=next((g for g in groups if f['name'].startswith('xui'+g)),None)
            if owner is None:owner='Widget';prefix='xui'
            else:prefix='xui'+owner
            receiver=True
        else:continue
        params=[];attrs=[];outputs=[];returns=[];callback_seen=False
        if p[0]['type']['kind']=='pointer' and 'const' in p[0]['type']['to'].get('qualifiers',[]):attrs.append('@c_const(0)')
        try:
            ret,at=simple(sig['returns'],0,True);attrs+=at
            if widget(sig['returns']) or same(sig['returns'],'xui_context'):attrs.append('@c_result("owner_view",0)')
            if same(sig['returns'],'xui_font'):attrs+=['@c_result("retained_view")','@c_retain(retainFont,"zero",rollbackFont)']
            if same(sig['returns'],'xui_surface'):attrs+=['@c_result("retained_view")','@c_retain(retainSurface,"zero",rollbackSurface)']
            explicit=p[1:] if receiver else p
            i=0
            while i<len(explicit):
                a=explicit[i];t=a['type'];logical=len(params)
                if f['name']=='xuiCanvasDrawSurfaceQuad' and spelling(t)=='const xui_surface_vertex_t*':
                    # The native consumer reads exactly four vertices. One
                    # const-record c_ref would compile but under-size storage.
                    params.append('SurfaceVertex[4] '+a['name']);attrs.append('@c_ref('+str(logical)+')');i+=1;continue
                if f['name']=='xuiTableViewGetCellOwned' and spelling(t)=='xui_table_view_cell_output_t*':
                    # This is an initialized owned object mutated transactionally,
                    # not a raw record output or a newly produced native pointer.
                    params.append('TableCellOutput '+a['name']);i+=1;continue
                if t['kind']=='pointer' and i+1<len(explicit) and re.search(r'(?:size|length|count|capacity|bytes)$',explicit[i+1].get('name') or '',re.I):
                    count=explicit[i+1]['type'];ct=' '.join(count.get('names',[])) if count['kind']=='named' else ''
                    if ct not in ('int','size_t'):raise ValueError('unsupported counted pointer ABI')
                    element=t['to'];ename=' '.join(element.get('names',[])) if element['kind']=='named' else element.get('tag','')
                    if ename not in SCALARS and ename not in records:raise ValueError('counted record contains unmanaged pointer fields')
                    readonly='const' in element.get('qualifiers',[]);pointer=spelling(t)
                    pointer=pointer.replace('unsigned char','uint8_t').replace('signed char','int8_t')
                    if not re.fullmatch(r'(?:const )?(?:struct )?[A-Za-z_]\w*\*',pointer):raise ValueError('unsupported native pointer spelling')
                    reviewed_span(f,pointer,ct)
                    pt='string' if readonly and ename=='char' else 'bytes' if readonly and ename in ('void','uint8_t','unsigned char','int8_t','signed char') else 'Buffer'
                    attrs.append('@c_span('+str(logical)+',"'+pointer+'","'+ct+'")');params.append(pt+' '+a['name']);i+=2;continue
                if t['kind']=='named' and t['names'][0] in types and types[t['names'][0]]['kind']=='pointer' and types[t['names'][0]]['to']['kind']=='function':
                    native=t['names'][0]
                    if not f['name'].endswith('Owned') or i+2>=len(explicit) or spelling(explicit[i+1]['type'])!='void*' or 'xui_callback_owner_t' not in spelling(explicit[i+2]['type']):raise ValueError('unowned callback')
                    pt=callback_type(native)+'?';params.append(pt+' '+a['name']);attrs.append('@c_owned_callback('+str(logical)+',0,"xui_callback_owner_t",'+str(failures.get(native,0))+')')
                    attrs+=['@c_callback_span('+str(logical)+','+str(arg)+',"'+pointer+'","'+count+'")' for arg,pointer,count in callback_spans.get(native,[])]
                    i+=3;callback_seen=True;continue
                if t['kind']=='pointer' and 'const' not in t['to'].get('qualifiers',[]):
                    if not re.search(r'Get|Query|Measure|Hit|OffsetTo|LineColumnTo',f['name']) and f['name']!='xuiWidgetUpdateBegin':raise ValueError('mutable pointer requires an explicit out/inout review')
                    if same(t['to'],'xui_surface'):
                        if f['name'] not in surface_outputs:raise ValueError('unreviewed retained Surface output')
                        pt='Surface?' if f['name']!='xuiBuiltinAssetGetAtlas' else 'Surface'
                        attrs.append('@c_output_retain('+str(i+1)+',retainSurface,"zero",rollbackSurface)')
                    elif same(t['to'],'xui_draw_context'):
                        if f['name']!='xuiWidgetUpdateBegin':raise ValueError('unreviewed retained Draw output')
                        pt='DrawContext';attrs.append('@c_output_retain('+str(i+1)+',retainDraw,"zero",rollbackDraw)')
                    else:pt=primitive(t['to'])
                    if not pt:raise ValueError('void output')
                    outputs.append(i+1);returns.append(pt);i+=1;continue
                pt,at=simple(t,logical);attrs+=at
                if same(t,'xui_font') and f['name']!='xuiRegisterFont':pt+='?'
                if same(t,'xui_surface') and f['name'] in surface_setters:pt+='?'
                if (widget(t) or same(t,'xui_context')) and t['kind']=='pointer' and 'const' in t['to'].get('qualifiers',[]):attrs.append('@c_const('+str(logical+int(receiver))+')')
                params.append(pt+' '+('arg'+str(i) if a['name'] in reserved else a['name']));i+=1
            contract=source_contract(f)
            if outputs:
                if ret!='c_int' or not (contract.get('statusZero') or f['name']=='xuiWidgetUpdateBegin') or callback_seen:raise ValueError('unreviewed output status')
                attrs.append(('@c_output('+str(outputs[0])+',0)') if len(outputs)==1 else '@c_outputs('+','.join(map(str,outputs))+') @c_status(0)')
                ret=returns[0] if len(returns)==1 else '('+','.join(returns)+')'
            # Literal XUI_OK guarantees the native int is a status, not a count.
            # Never infer this from a Set/Get spelling alone.
            elif ret=='c_int' and (callback_seen or f['name'] in ('xuiTableViewGetCellOwned','xuiSetDefaultFont','xuiRegisterFont','xuiSetProxyOwned') or contract.get('statusZero') and not re.search(r'Get|Count|Index|Find|Hit|Query|Is|Has|Contains|Compare',f['name'])):
                attrs.append('@c_result("status",0)');ret=None
            name=f['name'][len(prefix):];name=name[:1].lower()+name[1:]
            if name.endswith('Owned'):name=name[:-5]
            close_actions={'xuiMenuClose':'closeMenu','xuiMenuBarClose':'closeMenu','xuiComboBoxClose':'closePopup',
                           'xuiCascaderClose':'closePopup','xuiColorPickerClose':'closePopup','xuiIconPickerClose':'closePopup',
                           'xuiDatePickerClose':'closePopup','xuiWebViewClose':'closeBrowser'}
            name=close_actions.get(f['name'],name)
            if name in reserved:name='native'+name[:1].upper()+name[1:]
            declaration=' extern "'+f['name']+'" @no_raise '+(' '.join(attrs)+' ' if attrs else '')+'func '+name+'('+','.join(params)+')'+(' as '+ret if ret else '')
            (context if owner=='Context' else classes[owner]).append(declaration);mapped[f['name']]='xui.'+owner+'.'+name
            f['sourceContract']=contract
        except ValueError:continue
    # These callbacks belong to the independently owned Cell output root.
    for native in ('xui_table_view_cell_text_owned_proc','xui_table_view_cell_render_owned_proc'):
        if native in types:callback_type(native)
    # Pointer-free global utilities are ordinary direct declarations too. They
    # must not disappear merely because they do not have a Context/Widget slot.
    for f in functions:
        if f['name'] in mapped or f.get('state')!='requires-contract-review':continue
        p=f['type']['parameters']
        if p and root_type(p[0].get('type',{})):continue
        params=[];attrs=[];outputs=[];values=[]
        try:
            result=primitive(f['type']['returns'])
            for i,a in enumerate(p):
                if a.get('variadic'):raise ValueError('variadic')
                t=a['type']
                if same(t,'void'):continue
                if t['kind']=='pointer' and 'const' not in t['to'].get('qualifiers',[]):
                    if not re.search(r'Default$|Init$|Get|Query',f['name']):raise ValueError('unreviewed mutable pointer')
                    value=primitive(t['to'])
                    if not value:raise ValueError('void output')
                    outputs.append(i);values.append(value);continue
                typ,at=param(t,len(params));attrs+=at
                params.append(typ+' '+('arg'+str(i) if not a['name'] or a['name'] in reserved else a['name']))
            if outputs:
                if result not in (None,'c_int'):raise ValueError('unreviewed producer result')
                if result and not source_contract(f).get('statusZero'):raise ValueError('unreviewed producer status')
                attrs.append('@c_output('+str(outputs[0])+(')' if result is None else ',0)') if len(outputs)==1 else '@c_outputs('+','.join(map(str,outputs))+')'+(' @c_status(0)' if result else ''))
                result=values[0] if len(values)==1 else '('+','.join(values)+')'
            name=f['name'][3:];name=name[:1].lower()+name[1:]
            if name in reserved:name='native'+name[:1].upper()+name[1:]
            free.append('export extern "'+f['name']+'" @header("xge3_runtime.h") @no_raise '+(' '.join(attrs)+' ' if attrs else '')+'func '+name+'('+','.join(params)+')'+(' as '+result if result else ''))
            mapped[f['name']]='xui.'+name
        except ValueError:continue
    lines=['// Managed owner graph and direct native methods. Generated; no per-method C wrappers.']
    for c,n in sorted(raw.items()):lines+=['// Explicit C-interop-only descriptor pointer; this does not own the pointed-to data.','export extern "'+c+'" class '+n+' @header("xge3_runtime.h") @lifetime("caller")',' native point handle','end']
    lines+=callback_declarations
    lines+=['export extern "xui_context_t*" class Context @header("xge3_runtime.h") @lifetime("owned") @policy("xl6Xui3ContextPolicy") @scope("xui_call_scope_t") @watch("xui_lifetime_link_t") @domain("xl6Xui3OwnershipDomainV1")',' native point handle',*context,' extern "xuiGetProxyOwned" @no_raise @c_result("retained_view") @c_retain(retainProxy,"zero",rollbackProxy) func proxy() as Proxy',' extern "xl6ForeignViewCloseV1" @no_raise @c_control(0) @c_result("bool_status") func close()','end']
    for group,methods in classes.items():
        lines+=['export extern "xui_widget_t*" class '+group+' @header("xge3_runtime.h") @lifetime("owner") @owner(Context) @policy("xl6Xui3WidgetPolicy") @watch("xui_lifetime_link_t")'+(' extends Widget' if group!='Widget' else '')]
        if group=='Widget':lines+=[' native point handle']
        lines+=methods
        if group=='Widget':lines+=[' extern "xuiWidgetDestroyChecked" @no_raise @c_result("status",0) func close()']
        lines+=['end']
    lines+=['extern "xuiDestroy" @no_raise func rollback(Context value)']
    lines+=['extern "xuiWidgetDestroyChecked" @no_raise @c_result("status",0) func rollback'+group+'('+group+' value)' for group in classes]
    lines+=['export extern "xuiCreateOwned" @no_raise @c_result("owned_view") @factory(1,0,rollback) func create(Proxy proxy) as Context']
    lines+=free
    (src/'managed.xl').write_text('\n\n'.join(lines)+'\n',encoding='utf8')
    print('xui managed',len(mapped),'functions',len(classes),'classes',len(callbacks),'callbacks',len(views),'callback views')
    return lines,mapped


def probes(args):
    root=args.root.resolve();out=args.out.resolve();out.mkdir(parents=True,exist_ok=False)
    report={'schema':1,'compileOnly':True,'runtimeAccepted':False,'packageImportInitializationValidated':False,'rows':[]}
    for package,relative in (('xge','xge'),('xui','xui'),('xge3d','xge/three_d')):
        folder=root/'lib/bindings'/relative
        manifest=json.loads((folder/'xlang.package.json').read_text())
        source='\n'.join((folder/name).read_text(encoding='utf8') for name in manifest['artifact']['xlSources'] if name!='src/bootstrap.xl')
        callback_names=set(re.findall(r'(?:export )?typedef (\w+) = func\(',source))
        callback_signatures={n:split(t) for n,t in re.findall(r'(?:export )?typedef (\w+) = func\(([^\n]*)\)',source)}
        callback_results={n:r for n,r in re.findall(r'(?:export )?typedef (\w+) = func\([^\n]*\) as ([A-Za-z_]\w*)',source)}
        callback_views=set(re.findall(r'export extern "[^"]+" class (\w+)[^\n]*@lifetime\("callback"\)',source))
        view_operations={v:[] for v in callback_views}
        view_owner=None
        for line in source.splitlines():
            cls=re.match(r'export extern "[^"]+" class (\w+)',line)
            if cls:view_owner=cls.group(1);continue
            if line=='end':view_owner=None;continue
            if view_owner not in callback_views:continue
            member=re.search(r'\b(func|get|let) (\w+)\(([^()\n]*)\)(?: as .*)?$',line)
            if member:view_operations[view_owner].append(member.groups())
        methods=[];owner=None;index=0;array_storage={}
        native_records=dict(re.findall(r'export extern "([^"]+)" struct (\w+)',source))
        native_records={language:native for native,language in native_records.items()}
        for line in source.splitlines():
            cls=re.match(r'export extern "[^"]+" class (\w+)',line)
            if cls:owner=cls.group(1);continue
            if line=='end':owner=None;continue
            call=foreign_method_declaration(line)
            if not call or (not owner and not line.startswith('export extern')):continue
            if owner in callback_views:continue
            native,kind,name,parameters=call;params=[];arguments=[];locals=[];epoch_arguments=[]
            if owner:params.append(owner+' receiver')
            for i,p in enumerate(split(parameters)):
                p=p.split('=',1)[0].strip();pt=p.rsplit(' ',1)[0]
                if pt.rstrip('?') in callback_names:
                    args=[];body=[]
                    for ordinal,t in enumerate(callback_signatures[pt.rstrip('?')]):
                        variable='callbackArg'+str(ordinal);args.append(t+' '+variable)
                        for op,member,ps in view_operations.get(t,[]):
                            callargs=[]
                            for p in split(ps):
                                typ=p.rsplit(' ',1)[0]
                                if typ=='string':value='""'
                                elif typ=='var':value='null'
                                elif typ=='TableCellOutput':
                                    value='capturedCell'+str(i)+'_'+str(ordinal);declaration=typ+' '+value
                                    if declaration not in params:params.append(declaration)
                                else:value=typ+('(0)' if typ.startswith('c_') or typ in ('int64','uint64','time') else '()')
                                callargs.append(value)
                            body.append('  '+variable+'.'+member+('('+','.join(callargs)+')' if op=='func' else '='+callargs[-1] if op=='let' else ''))
                    result=callback_results.get(pt.rstrip('?'))
                    if result:body.append('  return '+('""' if result=='string' else result+'(0)' if result.startswith('c_') else result+'()'))
                    arguments.append('func('+','.join(args)+')'+(' as '+result if result else '')+'\n'+'\n'.join(body)+'\n end');continue
                if pt=='CallbackDrawContext':
                    # Exercise an epoch input only inside the real typed native
                    # cache-render callback, never through an escaping C export.
                    assert package=='xui' and not epoch_arguments
                    params.append('Widget callbackOwner');arguments.append('callbackDraw');epoch_arguments.append(pt);continue
                if pt in ('bytes?','Buffer?'):
                    # Typed nullable arguments preserve overload resolution,
                    # without inventing a nullable raw C export representation.
                    locals.append(pt+' arg'+str(i)+'=null');arguments.append('arg'+str(i));continue
                if re.fullmatch(r'[A-Za-z_]\w*\[[1-9][0-9]*\]',pt):
                    # Fixed native arrays have value semantics inside XLIR but
                    # are not raw C export parameters. Reach their call using
                    # initialized local storage instead of weakening exports.
                    element,extent=pt[:-1].split('[');key=(element,int(extent))
                    if key not in array_storage:
                        assert element in native_records,('probe fixed array native element',element)
                        array_storage[key]='BindingProbeStorage'+str(len(array_storage))
                    storage=array_storage[key];local='arrayStorage'+str(i)
                    locals.append(storage+' '+local+'='+storage+'()');arguments.append(local+'.values');continue
                if pt.endswith('?'):
                    arguments.append('null');continue
                params.append(pt+' arg'+str(i));arguments.append('arg'+str(i))
            expression=('receiver.' if owner else '')+name
            if kind=='func':expression+='('+','.join(arguments)+')'
            elif kind=='let':expression+='='+arguments[-1]
            if epoch_arguments:
                expression='callbackOwner.setCacheRenderCallback(func(Widget callbackWidget,CallbackDrawContext callbackDraw,c_uint callbackState) as c_int\n  '+expression+'\n  return c_int(0)\n end)'
            methods.append(dict(native=native,owner=owner,member=name,parameters=params,expression=expression,**({'locals':locals} if locals else {})))
        storage_declarations=[]
        storage_header=['#ifndef XL6_BINDING_PROBE_STORAGE_H','#define XL6_BINDING_PROBE_STORAGE_H','#include "xge3_runtime.h"']
        for (element,extent),storage in array_storage.items():
            storage_header.append('typedef struct '+storage+' { '+native_records[element]+' values['+str(extent)+']; } '+storage+';')
            storage_declarations.append('extern "'+storage+'" struct '+storage+' @header("binding_probe_storage_'+package+'.h")\n '+element+' values['+str(extent)+']\nend\n')
        storage_header.append('#endif')
        (out/('binding_probe_storage_'+package+'.h')).write_text('\n'.join(storage_header)+'\n',encoding='utf8')
        for first in range(0,len(methods),128):
            selected=methods[first:first+128];cases=[]
            for row in selected:
                index+=1;cases.append('export "binding_probe_'+package+'_'+str(index)+'" func probe'+str(index)+'('+','.join(row['parameters'])+')\n '+''.join(local+'\n ' for local in row.get('locals',[]))+row['expression']+'\nend\n')
            target=out/(package+'-'+str(first//128)+'.xl')
            target.write_text(source+'\n'+'\n'.join(storage_declarations)+'\n'+'\n'.join(cases)+'\nfunc main(...) as int\n return 0\nend\n',encoding='utf8')
            report['rows'].append(dict(package=package,source=str(target),sourceSha256=hashlib.sha256(target.read_bytes()).hexdigest(),methods=selected))
    (out/'probes.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    print('Compile-only probes:',sum(len(r['methods']) for r in report['rows']),'methods in',len(report['rows']),'batches')

def acceptance(args):
    import os,shutil,subprocess,time
    from compiler_response import command_with_response
    from xlir_public_build_inputs import verify_public_build
    from runtime_layout import runtime_directory
    root=args.root.resolve();driver=args.driver.resolve();out=args.out.resolve()
    fixtures=args.fixture_root.resolve() if args.fixture_root else root
    build,pinned,_=verify_public_build(root,driver.parent)
    out.mkdir(parents=True,exist_ok=False);rows=[]
    component=args.acceptance;stem={'fields':'field_alias','outputs':'outputs','exports':'export_alias','const':'const','header':'header','domain':'domain','value':'value','rollback':'rollback'}[component]
    source=args.source.resolve() if args.source else fixtures/('test/cases/foreign_xge3_binding_'+stem+'.xl')
    sdk=Path(build['SdkDirectory']) if build.get('SdkDirectory') else runtime_directory(root)
    runtime=Path(build['RuntimeLibrary']) if build.get('RuntimeLibrary') else sdk/('xl.dll' if os.name=='nt' else 'libxl.so')
    objects=[r['Path'] for r in build['Objects'] if Path(r['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    pinned.update([Path(__file__).resolve(),source,runtime]);inputs={str(p):sha(p) for p in pinned}
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(root));report=dict(complete=False,passed=False,component=component,sourceBuild=str(driver.parent/'build.json'),rows=rows,inputs=inputs,networkTests=False,desktopTests=False,gpuTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,accepted=True):
        command=list(map(str,command));command=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
        start=time.monotonic();r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);rows.append(dict(name=name,exitCode=r.returncode,accepted=accepted,log=str(log),sha256=sha(log),seconds=round(time.monotonic()-start,3)));save();print(name,r.returncode,flush=True)
        assert (r.returncode==0)==accepted,log
    generated=out/'generated.c';run('cgen',[driver,source,'cgen-default',generated]);run('ir',[driver,source,'ir-lowered',out/'lowered.json']);run('header',[driver,source,'emit',out/'public'])
    if component=='fields':
        assert 'offsetof(xge3d_fog_settings_t, end)' in generated.read_text(encoding='utf-8-sig')
        assert 'C record field offset' in (out/'public.h').read_text(encoding='utf-8-sig')
    # A development fixture overlay contains ONLY public test headers. Core
    # headers still come from the immutable compiler cohort before live src/.
    # This prevents both stale fixture headers and mixed XLIR struct layouts.
    overlay=out/'fixture-inputs';(overlay/'test/cases').mkdir(parents=True)
    fixture_headers=list((fixtures/'test/cases').glob('foreign_xge3_binding_*.h'))
    fixture_headers += [fixtures/'test/cases'/n for n in ('foreign_managed_view.h','foreign_expanded_calls.h','foreign_xui_declarative.h')]
    for header in fixture_headers:
        target=overlay/'test/cases'/header.name;shutil.copy2(header,target);inputs[str(header)]=sha(header);inputs[str(target)]=sha(target)
    includes=['-I'+str(x) for x in (overlay,root,fixtures,driver.parent/'include',sdk,sdk/'include')]
    if component=='fields':includes+=['-I'+str(args.native_root.resolve()),'-I'+str(args.native_root.resolve()/'include')]
    sidecars=[];native=[]
    if component=='const':sidecars=[fixtures/'test/cases/foreign_managed_view_native.c',fixtures/'test/cases/foreign_xge3_binding_const.c']
    if component=='header':sidecars=[fixtures/'test/cases/foreign_xge3_binding_header.c']
    if component in ('domain','value','rollback'):sidecars=[fixtures/('test/cases/foreign_xge3_binding_'+component+'.c')]
    if component=='outputs':
        assert args.engine_build
        engine=args.engine_build.resolve();sidecars=[root/'test/cases/foreign_xui_declarative_native.c',fixtures/'test/cases/foreign_xge3_binding_outputs.c']
        includes+=['-I'+str(x) for x in (args.native_root.resolve(),args.native_root.resolve()/'include',engine/'host-xrt')]
        includes+=['-DXGE_XRT_HOST=1','-include',str(engine/'xge_build_config.h')]
        native=[engine/('xge.lib' if os.name=='nt' else 'libxge.so')]
        if os.name=='nt':shutil.copy2(engine/'xge.dll',out/'xge.dll')
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+(':'+str(args.engine_build.resolve()) if args.engine_build else '')+':'+env.get('LD_LIBRARY_PATH','')
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    warnings=['-Wno-'+x for x in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
    common=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
    dependencies=set(sidecars)
    dependencies.update(p for p in (fixtures/'test/cases').glob('foreign_xge3_binding_'+stem+'*') if p.is_file())
    if component=='const':dependencies.update(fixtures/'test/cases'/n for n in ('foreign_managed_view.h','foreign_expanded_calls.h'))
    if component=='outputs':dependencies.update([root/'test/cases/foreign_xui_declarative.h',args.native_root.resolve()/'include/xge.h',args.native_root.resolve()/'include/xui.h',engine/'xge_build_config.h',*native])
    if component=='fields':dependencies.add(args.native_root.resolve()/'include/xge.h')
    inputs.update((str(p),sha(p)) for p in dependencies)
    if os.name=='nt' and component=='outputs':common+=['-DXGE_DLL','-DXUI_DLL']
    run('header-C',['gcc',*common,'-x','c','-fsyntax-only',out/'public.h'])
    if component=='fields':run('header-CXX',['g++','-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes,'-x','c++','-fsyntax-only',out/'public.h'])
    for opt in ('O0','O2'):
        exe=out/(opt+('.exe' if os.name=='nt' else '.out'))
        run(opt+'-build',['gcc',*common,*warnings,'-'+opt,generated,*sidecars,root/'test/cases/foreign_decl_module_host.c',*objects,runtime,*native,*systems,'-o',exe]);run(opt+'-run',[exe])
    original=source.read_text(encoding='utf8');negatives={}
    if component=='fields':
        needle='@c_name("end")'
        negatives={k:original.replace(needle,v) for k,v in {'duplicate':needle+' '+needle,'empty':'@c_name("")','not-identifier':'@c_name("end.other")','wrong-arity':'@c_name("end", "other")','wrong-type':'@c_name(3)','unknown':'@unknown("end")','duplicate-C-member':'@c_name("start")'}.items()}
        negatives['ordinary-class']='class Ordinary\n c_float value @c_name("end")\nend\n'
        negatives['ordinary-union']='union Ordinary\n c_float value @c_name("end")\nend\n'
    elif component=='exports':negatives['duplicate-export']=original.replace('export typedef Action','export export typedef Action')
    elif component=='header':
        needle='@header("test/cases/foreign_xge3_binding_header.h")'
        negatives={k:original.replace(needle,v,1) for k,v in {'duplicate':needle+' '+needle,'empty':'@header("")','injection':'@header("bad.h>injection")','wrong-type':'@header(3)','wrong-arity':'@header("native.h","other.h")'}.items()}
    elif component=='outputs':
        negatives={k:original.replace('@c_outputs(1,2) @c_status(0)',v,1) for k,v in {'descending':'@c_outputs(2,1) @c_status(0)','duplicate-slot':'@c_outputs(1,1) @c_status(0)','negative-slot':'@c_outputs(-1,2) @c_status(0)','missing-output':'@c_outputs(1) @c_status(0)','extra-output':'@c_outputs(1,2,3) @c_status(0)','duplicate-status':'@c_outputs(1,2) @c_status(0) @c_status(0)'}.items()}
    elif component=='const':
        negatives={k:original.replace('@c_const(0)',v,1) for k,v in {'duplicate':'@c_const(0) @c_const(0)','out-of-range':'@c_const(9)','wrong-type':'@c_const("zero")','wrong-arity':'@c_const(0,1)','empty':'@c_const()','overlap':'@c_const(0) @c_value(0)'}.items()}
    elif component=='domain':
        needle='@domain("bindingNativeDomain")'
        negatives={k:original.replace(needle,v,1) for k,v in {'duplicate':needle+' '+needle,'empty':'@domain("")','injection':'@domain("bindingNativeDomain;bad")','wrong-type':'@domain(3)','wrong-arity':'@domain("bindingNativeDomain","other")'}.items()}
        negatives.update({k:original.replace('@lifetime("owned")',v,1) for k,v in {'record':'@lifetime("record")','caller':'@lifetime("caller")','owner':'@lifetime("owner") @owner(DomainRoot)','callback':'@lifetime("callback")'}.items()})
    elif component=='value':
        needle='@c_result("borrowed_value")'
        negatives={k:original.replace(needle,v,1) for k,v in {'duplicate-result':needle+' '+needle,'result-with-output':needle+' @c_output(0)','result-extra-argument':'@c_result("borrowed_value",0)'}.items()}
        negatives['wrong-result-type']=original.replace('func read() as var','func read() as string',1)
        negatives['wrong-input-type']=original.replace('func set(var value)','func set(int value)',1)
        negatives['input-overlap']=original.replace('@c_const(1)','@c_const(1) @c_text_view(0)',1)
    elif component=='rollback':
        needle='@no_raise @c_result("status",0) func rollback(Root value)'
        negatives={k:original.replace(needle,v,1) for k,v in {
            'missing-no-raise':'@c_result("status",0) func rollback(Root value)',
            'logical-int-result':needle+' as c_int',
            'boolean-status':'@no_raise @c_result("bool_status") func rollback(Root value)',
            'nullable-owner':'@no_raise @c_result("status",0) func rollback(Root? value)',
            'extra-input':'@no_raise @c_result("status",0) func rollback(Root value,c_int other)',
            'input-rewrite':'@no_raise @c_const(0) @c_result("status",0) func rollback(Root value)',
            'invalid-success-code':'@no_raise @c_result("status","zero") func rollback(Root value)'}.items()}
    if component in ('fields','outputs','const','domain','value','header','rollback'):
        for opt in ('O0','O2'):
            exe=out/('ir-'+opt+('.exe' if os.name=='nt' else '.out'))
            run('ir-'+opt+'-build',['gcc',*common,'-'+opt,fixtures/('test/cases/foreign_xge3_binding_'+stem+'_ir.c'),*objects,runtime,*systems,'-o',exe]);run('ir-'+opt+'-run',[exe,source])
    for name,text in negatives.items():
        case=out/(name+'.xl');case.write_text(text,encoding='utf8');target=out/(name+'.c')
        run('reject-'+name,[driver,case,'cgen-default',target],False);assert not target.exists()
    report['inputsUnchanged']=all(sha(p)==h for p,h in inputs.items());assert report['inputsUnchanged']
    report.update(complete=True,passed=True);save()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--native-root',type=Path,default=Path('D:/GIT/xge'));p.add_argument('--inventory',type=Path);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    action=p.add_mutually_exclusive_group();action.add_argument('--generate',action='store_true');action.add_argument('--existing-bindings',action='store_true',help='Annotate current declarations without generating or editing library files; this does not certify ABI/runtime acceptance')
    p.add_argument('--acceptance',choices=('fields','outputs','exports','const','header','domain','value','rollback'));p.add_argument('--driver',type=Path);p.add_argument('--out',type=Path);p.add_argument('--engine-build',type=Path)
    p.add_argument('--fixture-root',type=Path)
    p.add_argument('--source',type=Path)
    p.add_argument('--probes',action='store_true')
    args=p.parse_args()
    if args.probes:
        assert args.out
        probes(args);return
    if args.acceptance:
        assert args.driver and args.out
        acceptance(args);return
    assert args.inventory
    data=inventory(args.native_root,args.inventory)
    if args.generate:
        generate(args.root,data);args.inventory.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    elif args.existing_bindings:
        annotate_existing_bindings(args.root,data);args.inventory.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()
