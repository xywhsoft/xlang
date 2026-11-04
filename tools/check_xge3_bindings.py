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
    out=[];start=0;depth=0
    for i,c in enumerate(text):
        if c in '([{':depth+=1
        elif c in ')]}':depth-=1
        elif c==delimiter and not depth:out.append(text[start:i].strip());start=i+1
    out.append(text[start:].strip());return [x for x in out if x]
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

def header_inventory(path):
    assert c_parser,'API inventory requires pycparser; acceptance does not'
    original=path.read_text(encoding='utf-8-sig');text=clean(original)
    td=typedefs(re.sub(r'^\s*#(?:[^\n\\]|\\\n)*\n','\n',text,flags=re.M))
    names=set()
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
        known={x for x in symbols if x in names or re.match(r'(?:u?int\d+_t|size_t|wchar_t|va_list|bool|xtime|xvalue|xstrview|xbytesview|xatomic\d+|xrt\w+)$',x)}
        pre='\n'.join('typedef int '+x+';' for x in known)
        try:
            tree=parser.parse(pre+'\n'+raw)
            for d in tree.ext[len(known):]:
                types.append(dict(name=d.name,declaration=' '.join(raw.split()),type=type_json(d.type)))
        except Exception as error:errors.append(dict(declaration=' '.join(raw.split()),error=str(error)))
    functions=[]
    for m in re.finditer(r'\b(X(?:GE|UI)\w*_API)\s+([^;{}]+?\([^;{}]*?\))\s*;',text,re.S):
        raw=m.group(2);before,args=raw.split('(',1);name=re.search(r'\w+\s*$',before).group().strip();ret=before[:before.rfind(name)]
        type_words=[ret]
        for arg in split(args[:-1]):type_words.append(re.sub(r'\b\w+\s*(?:\[[^\]]*\])?\s*$','',arg) if arg.strip() not in ('void','...') else '')
        known=set(re.findall(r'\b[A-Za-z_]\w*\b',' '.join(type_words)))-KEYWORDS
        pre='\n'.join('typedef int '+x+';' for x in known)
        try:
            d=parser.parse(pre+'\n'+raw+';').ext[-1];functions.append(dict(name=name,line=text.count('\n',0,m.start())+1,declaration=' '.join(raw.split()),type=type_json(d.type),state='requires-contract-review'))
        except Exception as error:errors.append(dict(declaration=' '.join(raw.split()),error=str(error)))
    constants=[]
    for m in re.finditer(r'^\s*#define\s+((?:XGE|XUI)\w*)([^\n]*)',original,re.M):
        if m.group(2).startswith('('):continue
        value=m.group(2).strip()
        if not value or value=='\\':continue
        constants.append(dict(name=m.group(1),line=original.count('\n',0,m.start())+1,expression=value))
    return dict(header=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),functions=functions,types=types,constants=constants,parseErrors=errors)

def inventory(root,out):
    result=dict(schema=1,scope=__doc__,headers=[header_inventory(root/'include'/name) for name in ('xge.h','xui.h')])
    result['counts']={k:sum(len(h[k]) for h in result['headers']) for k in ('functions','types','constants','parseErrors')}
    out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(result['counts']));return result


SCALARS={'void':None,'int':'c_int','unsigned int':'c_uint','unsigned':'c_uint','char':'c_char','signed char':'c_schar','unsigned char':'c_uchar','short':'c_short','unsigned short':'c_ushort','long':'c_long','unsigned long':'c_ulong','long long':'c_longlong','unsigned long long':'c_ulonglong','float':'c_float','double':'c_double','size_t':'c_size','int8_t':'c_schar','uint8_t':'c_uchar','int16_t':'c_short','uint16_t':'c_ushort','int32_t':'c_int','uint32_t':'c_uint','int64_t':'int64','uint64_t':'uint64','xtime':'int64','intptr_t':'c_intptr','uintptr_t':'c_uintptr','ptrdiff_t':'c_ptrdiff'}
SCALARS['xtime']='time'
def pascal(name):
    name=re.sub(r'^x(?:ge|ui)_','',name);name=re.sub(r'_t$','',name)
    return ''.join(x[:1].upper()+x[1:] for x in name.split('_'))
def integer(expression,known):
    expression=re.sub(r'/\*.*?\*/|//.*','',expression).strip()
    expression=re.sub(r'\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]+\b',r'\1',expression)
    expression=re.sub(r'\((?:u?int\d+_t|unsigned|int|long|size_t)\)', '',expression)
    for symbol in set(re.findall(r'\b[A-Za-z_]\w*\b',expression)):
        if symbol not in known:raise ValueError(symbol)
        expression=re.sub(r'\b'+re.escape(symbol)+r'\b',str(known[symbol]),expression)
    node=ast.parse(expression,mode='eval')
    allowed=(ast.Expression,ast.Constant,ast.UnaryOp,ast.BinOp,ast.USub,ast.UAdd,ast.Invert,ast.Add,ast.Sub,ast.Mult,ast.FloorDiv,ast.Div,ast.Mod,ast.LShift,ast.RShift,ast.BitAnd,ast.BitOr,ast.BitXor,ast.Load)
    if any(not isinstance(x,allowed) for x in ast.walk(node)):raise ValueError(expression)
    value=eval(compile(node,'<C constant>','eval'),{'__builtins__':{}},{})
    if not isinstance(value,(int,float)):raise ValueError(expression)
    return value

def generate(root,data):
    reserved=set(re.findall(r'\{\"([A-Za-z_]\w*)\", XL6_KW_', (root/'src/lexer/xl6_lexer_gen.c').read_text(encoding='utf8')))
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
        records3d=[d for d in declarations if 'extern "xge3d_' in d]
        records2d=[d for d in declarations if d not in records3d]
        (src/'records.xl').write_text('// Exact, pointer-free native value records. Generated from the authoritative header.\n'+ '\n'.join(records2d),encoding='utf8')
        if package=='xge':
            optional=folder/'three_d';(optional/'src').mkdir(parents=True,exist_ok=True)
            (optional/'src/records.xl').write_text('// Requires a matching 3D-enabled native SDK; no feature macros are changed by this module.\n'+'\n'.join(records3d),encoding='utf8')
        known={};constants=[];seen=set()
        candidates=list(header['constants'])
        for t in header['types']:
            if t['type']['kind']=='enum' and t['type'].get('values'):
                previous=-1
                for v in t['type']['values']:
                    try: previous=integer(v['expression'],known) if v['expression'] is not None else previous+1
                    except (ValueError,SyntaxError,NameError):break
                    known[v['name']]=previous;candidates.append(dict(name=v['name'],expression=str(previous)))
        remaining=list(candidates)
        for _ in range(20):
            next_remaining=[]
            for c in remaining:
                if c['name'] in seen:continue
                try:value=integer(c['expression'],known)
                except (ValueError,SyntaxError,NameError,ZeroDivisionError):next_remaining.append(c);continue
                known[c['name']]=value;seen.add(c['name'])
                if isinstance(value,float):typ='float';literal=repr(value)
                else:typ='uint64' if value>0xffffffff else 'uint32' if value>0x7fffffff else 'int64' if value < -0x80000000 else 'c_int';literal=str(value)
                constants.append('export const '+typ+' '+c['name']+' = '+literal)
            if len(next_remaining)==len(remaining):break
            remaining=next_remaining
        (src/'constants.xl').write_text('// Numeric constants preserve their native values; no runtime storage or C wrapper.\n'+'\n'.join(constants)+'\n',encoding='utf8')
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
                sig=f['type'];attrs=[];params=[]
                try:
                    ret=primitive(sig['returns'])
                    for i,p in enumerate(sig['parameters']):
                        if p.get('variadic'):raise ValueError('variadic')
                        if p['type']=={'kind':'named','names':['void'],'qualifiers':[]}:continue
                        pt,at=param(p['type'],i);attrs.extend(at);params.append(pt+' '+('arg'+str(i) if not p['name'] or p['name'] in reserved else p['name']))
                except ValueError:continue
                name=f['name'][3:];name='threeD'+name[2:] if name.startswith('3d') else name[:1].lower()+name[1:];name='native'+name[:1].upper()+name[1:] if name in reserved else name
                if name in special:continue
                special.add(name)
                line='export extern "'+f['name']+'" @header("xge3_runtime.h") '+(' '.join(attrs)+' ' if attrs else '')+'func '+name+'('+','.join(params)+')'+(' as '+ret if ret else '')
                functions.append(line);mapped[f['name']]='xge.'+name
            functions3d=[f for f in functions if 'extern "xge3d' in f]
            functions2d=[f for f in functions if f not in functions3d]
            (src/'functions.xl').write_text('// Direct C declarations. Integer results retain the authoritative C status/count semantics.\n'+'\n'.join(functions2d)+'\n',encoding='utf8')
            (optional/'src/functions.xl').write_text('\n'.join(functions3d)+'\n',encoding='utf8')
            optional_manifest={'format':'xlang-package-1','name':'xge3d','version':'3.0.0','abiVersion':'2','kind':'xlang-extlib','artifact':{'kind':'source','cSources':[],'xlSources':['src/records.xl','src/functions.xl']},'links':[],'loadlibs':[],'dependencies':[{'name':'xge','version':'3.0.0','abiVersion':'2'}],'requiresNativeFeatures':['XGE_ENABLE_3D'],'native':{'singleton':False,'loadName':'','version':''}}
            (optional/'xlang.package.json').write_text(json.dumps(optional_manifest,indent=2)+'\n',encoding='utf8')
        if package=='xui':
            managed,mapping=generate_xui(root,header,records,types,primitive,param,reserved)
            mapped.update(mapping)
        header['bindingSummary']=dict(pointerFreeRecords=len(records),numericConstants=len(constants),scalarFunctions=len(functions))
        header['bindingTypes']={name:dict(language=records.get(name),state='direct-value-record' if name in records else 'requires-lifetime-or-layout-contract') for name in types}
        for f in header['functions']:
            if f['name'] in mapped:f.update(state='direct-declaration',language=mapped[f['name']])
        manifest={'format':'xlang-package-1','name':package,'version':'3.0.0','abiVersion':'2','kind':'xlang-extlib','artifact':{'kind':'source','cSources':[],'xlSources':['src/records.xl','src/constants.xl']+(['src/functions.xl','src/resources.xl'] if package=='xge' else ['src/managed.xl'])},'links':[],'loadlibs':[],'dependencies':[{'name':'_xge_native','version':'3.0.0','abiVersion':'2'}],'native':{'singleton':False,'loadName':'','version':''}}
        (folder/'xlang.package.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8')
        print(package,header['bindingSummary'])


def generate_xui(root,header,records,types,primitive,param,reserved):
    src=root/'lib/bindings/xui/src';functions=header['functions'];mapped={};raw={};callbacks={};views={};classes={'Widget':[]};context=[];free=[]
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
        if result and t['kind']=='pointer' and same(t['to'],'char') and 'const' in t['to'].get('qualifiers',[]):return 'string?',[]
        return param(t,index) if not result else (primitive(t),[])
    def source_contract(f):
        # Require literal native success for status/output adaptation; otherwise
        # preserve its integer result and leave its pointer output unadapted.
        if f['name'] in native_bodies:
            path,body=native_bodies[f['name']]
            return dict(file=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),statusZero=bool(re.search(r'\breturn\s+XUI_OK\s*;',body)))
        return {}
    constructors=[]
    for f in functions:
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
    callback_declarations=[]
    def callback_view(t):
        c=spelling(t)
        if c in views:return views[c]
        name='Callback'+pascal(c.replace('const ','').replace('struct ','').replace('*',''))
        if name=='CallbackChar':name='CString'
        views[c]=name
        body=['export extern "'+c+'" class '+name+' @header("xge3_runtime.h") @lifetime("callback")',' native point handle']
        if c=='const char*':
            body+=[' extern "strlen" @no_raise get length() as c_size',' extern "xrtStrDup" @owned func copy() as string']
        else:
            target=t['to'];native=' '.join(target.get('names',[])) if target['kind']=='named' else target.get('tag','');record=types.get(native,{})
            for f in record.get('fields') or []:
                try:ft=primitive(f['type'])
                except ValueError:continue
                if not ft or f.get('bits') or not f.get('name'):continue
                language=f['name']+'Value' if f['name'] in reserved else f['name']
                body.append(' extern "'+f['name']+'" @c_field get '+language+'() as '+ft)
                if 'const' not in target.get('qualifiers',[]):body.append(' extern "'+f['name']+'" @c_field let '+language+'('+ft+' value)')
        body+=['end'];callback_declarations.append('\n'.join(body));return name
    def callback_type(native):
        if native in callbacks:return callbacks[native]
        t=types[native]
        if t['kind']!='pointer' or t['to']['kind']!='function':raise ValueError('not callback')
        signature=t['to'];args=signature['parameters']
        if not args or spelling(args[-1]['type'])!='void*':raise ValueError('userdata order')
        result=primitive(signature['returns']);params=[]
        if result:raise ValueError('nonvoid needs explicit failure review')
        for a in args[:-1]:
            t=a['type']
            if widget(t):pt='Widget'
            elif t['kind']=='pointer':pt=callback_view(t)
            else:pt=primitive(t)
            if not pt:raise ValueError('callback void argument')
            params.append(pt)
        name=pascal(native);callbacks[native]=name
        callback_declarations.append('export typedef '+name+' = func('+','.join(params)+')')
        return name
    for f in functions:
        if f['name'] in constructors or f['name'] in ('xuiCreate','xuiDestroy','xuiWidgetDestroy','xuiContextLifetimeWatch','xuiWidgetLifetimeWatch'):continue
        p=f['type']['parameters'];sig=f['type'];receiver=False;owner=None;prefix='xui'
        if p and same(p[0]['type'],'xui_context'):owner='Context';receiver=True
        elif p and widget(p[0]['type']):
            owner=next((g for g in groups if f['name'].startswith('xui'+g)),None)
            if owner is None:owner='Widget';prefix='xui'
            else:prefix='xui'+owner
            receiver=True
        else:continue
        params=[];attrs=[];outputs=[];returns=[];callback_seen=False
        try:
            ret,at=simple(sig['returns'],0,True);attrs+=at
            if widget(sig['returns']) or same(sig['returns'],'xui_context'):attrs.append('@c_result("owner_view",0)')
            explicit=p[1:] if receiver else p
            i=0
            while i<len(explicit):
                a=explicit[i];t=a['type'];logical=len(params)
                if t['kind']=='pointer' and i+1<len(explicit) and re.search(r'(?:size|length|count|capacity|bytes)$',explicit[i+1].get('name') or '',re.I):
                    count=explicit[i+1]['type'];ct=' '.join(count.get('names',[])) if count['kind']=='named' else ''
                    if ct not in ('int','size_t'):raise ValueError('unsupported counted pointer ABI')
                    element=t['to'];ename=' '.join(element.get('names',[])) if element['kind']=='named' else element.get('tag','')
                    if ename not in SCALARS and ename not in records:raise ValueError('counted record contains unmanaged pointer fields')
                    readonly='const' in element.get('qualifiers',[]);pointer=spelling(t)
                    pointer=pointer.replace('unsigned char','uint8_t').replace('signed char','int8_t')
                    if not re.fullmatch(r'(?:const )?(?:struct )?[A-Za-z_]\w*\*',pointer):raise ValueError('unsupported native pointer spelling')
                    pt='string' if readonly and ename=='char' else 'bytes' if readonly and ename in ('void','uint8_t','unsigned char','int8_t','signed char') else 'Buffer'
                    attrs.append('@c_span('+str(logical)+',"'+pointer+'","'+ct+'")');params.append(pt+' '+a['name']);i+=2;continue
                if t['kind']=='named' and t['names'][0] in types and types[t['names'][0]]['kind']=='pointer' and types[t['names'][0]]['to']['kind']=='function':
                    native=t['names'][0]
                    if not f['name'].endswith('Owned') or i+2>=len(explicit) or spelling(explicit[i+1]['type'])!='void*' or 'xui_callback_owner_t' not in spelling(explicit[i+2]['type']):raise ValueError('unowned callback')
                    pt=callback_type(native)+'?';params.append(pt+' '+a['name']);attrs.append('@c_owned_callback('+str(logical)+',0,"xui_callback_owner_t",0)');i+=3;callback_seen=True;continue
                if t['kind']=='pointer' and 'const' not in t['to'].get('qualifiers',[]):
                    if not re.search(r'Get|Query|Measure|Hit|OffsetTo|LineColumnTo',f['name']):raise ValueError('mutable pointer requires an explicit out/inout review')
                    pt=primitive(t['to'])
                    if not pt:raise ValueError('void output')
                    outputs.append(i+1);returns.append(pt);i+=1;continue
                pt,at=simple(t,logical);attrs+=at;params.append(pt+' '+('arg'+str(i) if a['name'] in reserved else a['name']));i+=1
            contract=source_contract(f)
            if outputs:
                if ret!='c_int' or not contract.get('statusZero') or callback_seen:raise ValueError('unreviewed output status')
                attrs.append(('@c_output('+str(outputs[0])+',0)') if len(outputs)==1 else '@c_outputs('+','.join(map(str,outputs))+') @c_status(0)')
                ret=returns[0] if len(returns)==1 else '('+','.join(returns)+')'
            # Literal XUI_OK guarantees the native int is a status, not a count.
            # Never infer this from a Set/Get spelling alone.
            elif ret=='c_int' and contract.get('statusZero') and not re.search(r'Get|Count|Index|Find|Hit|Query|Is|Has|Contains|Compare',f['name']):
                attrs.append('@c_result("status",0)');ret=None
            name=f['name'][len(prefix):];name=name[:1].lower()+name[1:]
            if name.endswith('Owned'):name=name[:-5]
            if name in reserved:name='native'+name[:1].upper()+name[1:]
            declaration=' extern "'+f['name']+'" @no_raise '+(' '.join(attrs)+' ' if attrs else '')+'func '+name+'('+','.join(params)+')'+(' as '+ret if ret else '')
            (context if owner=='Context' else classes[owner]).append(declaration);mapped[f['name']]='xui.'+owner+'.'+name
            f['sourceContract']=contract
        except ValueError:continue
    lines=['// Managed owner graph and direct native methods. Generated; no per-method C wrappers.']
    for c,n in sorted(raw.items()):lines+=['// Explicit C-interop-only descriptor pointer; this does not own the pointed-to data.','export extern "'+c+'" class '+n+' @header("xge3_runtime.h") @lifetime("caller")',' native point handle','end']
    lines+=callback_declarations
    lines+=['export extern "xui_context_t*" class Context @header("xge3_runtime.h") @lifetime("owned") @policy("xl6Xui3ContextPolicy") @scope("xui_call_scope_t") @watch("xui_lifetime_link_t")',' native point handle',*context,' extern "xl6ForeignViewCloseV1" @no_raise @c_control(0) @c_result("bool_status") func close()','end']
    for group,methods in classes.items():
        lines+=['export extern "xui_widget_t*" class '+group+' @header("xge3_runtime.h") @lifetime("owner") @owner(Context) @policy("xl6Xui3WidgetPolicy") @watch("xui_lifetime_link_t")'+(' extends Widget' if group!='Widget' else '')]
        if group=='Widget':lines+=[' native point handle']
        lines+=methods
        if group=='Widget':lines+=[' extern "xl6ForeignViewCloseV1" @no_raise @c_control(0) @c_result("bool_status") func close()']
        lines+=['end']
    lines+=['extern "xuiDestroy" @no_raise func rollback(Context value)']
    lines+=['extern "xuiWidgetDestroy" @no_raise func rollback'+group+'('+group+' value)' for group in classes]
    lines+=['export extern "xuiCreate" @no_raise @c_result("owned_view") @factory(0,0,rollback) func create() as Context']
    (src/'managed.xl').write_text('\n\n'.join(lines)+'\n',encoding='utf8')
    print('xui managed',len(mapped),'functions',len(classes),'classes',len(callbacks),'callbacks',len(views),'callback views')
    return lines,mapped


def probes(args):
    root=args.root.resolve();out=args.out.resolve();out.mkdir(parents=True,exist_ok=False)
    report={'schema':1,'compileOnly':True,'runtimeAccepted':False,'rows':[]}
    for package,relative in (('xge','xge'),('xui','xui'),('xge3d','xge/three_d')):
        folder=root/'lib/bindings'/relative
        manifest=json.loads((folder/'xlang.package.json').read_text())
        source='\n'.join((folder/name).read_text(encoding='utf8') for name in manifest['artifact']['xlSources'])
        callback_names=set(re.findall(r'(?:export )?typedef (\w+) = func\(',source))
        callback_signatures={n:split(t) for n,t in re.findall(r'(?:export )?typedef (\w+) = func\(([^\n]*)\)',source)}
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
        methods=[];owner=None;index=0
        for line in source.splitlines():
            cls=re.match(r'export extern "[^"]+" class (\w+)',line)
            if cls:owner=cls.group(1);continue
            if line=='end':owner=None;continue
            call=re.search(r'extern "([^"]+)".*\b(func|get|let) (\w+)\(([^()\n]*)\)(?: as .*)?$',line)
            if not call or (not owner and not line.startswith('export extern')):continue
            if owner in callback_views:continue
            native,kind,name,parameters=call.groups();params=[];arguments=[]
            if owner:params.append(owner+' receiver')
            for i,p in enumerate(split(parameters)):
                p=p.split('=',1)[0].strip();pt=p.rsplit(' ',1)[0]
                if pt.rstrip('?') in callback_names:
                    args=[];body=[]
                    for ordinal,t in enumerate(callback_signatures[pt.rstrip('?')]):
                        variable='callbackArg'+str(ordinal);args.append(t+' '+variable)
                        for op,member,ps in view_operations.get(t,[]):
                            callargs=[p.rsplit(' ',1)[0]+('(0)' if p.startswith(('c_','int64 ','uint64 ')) else '()') for p in split(ps)]
                            body.append('  '+variable+'.'+member+('('+','.join(callargs)+')' if op=='func' else '='+callargs[-1] if op=='let' else ''))
                    arguments.append('func('+','.join(args)+')\n'+'\n'.join(body)+'\n end');continue
                if pt.endswith('?'):
                    arguments.append('null');continue
                params.append(pt+' arg'+str(i));arguments.append('arg'+str(i))
            expression=('receiver.' if owner else '')+name
            if kind=='func':expression+='('+','.join(arguments)+')'
            elif kind=='let':expression+='='+arguments[-1]
            methods.append(dict(native=native,owner=owner,member=name,parameters=params,expression=expression))
        for first in range(0,len(methods),128):
            selected=methods[first:first+128];cases=[]
            for row in selected:
                index+=1;cases.append('export "binding_probe_'+package+'_'+str(index)+'" func probe'+str(index)+'('+','.join(row['parameters'])+')\n '+row['expression']+'\nend\n')
            target=out/(package+'-'+str(first//128)+'.xl')
            target.write_text(source+'\n'+'\n'.join(cases)+'\nfunc main(...) as int\n return 0\nend\n',encoding='utf8')
            report['rows'].append(dict(package=package,source=str(target),methods=selected))
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
    p.add_argument('--native-root',type=Path,default=Path('D:/GIT/xge'));p.add_argument('--inventory',type=Path);p.add_argument('--generate',action='store_true');p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
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
if __name__=='__main__':main()
