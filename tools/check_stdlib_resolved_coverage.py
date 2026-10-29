"""Resolve three-group fixture calls against the actual compiled registration
catalog and exact parameter vectors. This is static binding coverage, not
branch execution, class completeness or full53/shipping acceptance.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess
import discover_stdlib_three_groups as inventory
from discover_stdlib_three_groups import PREFIX,NEGATIVE
from xlir_public_build_inputs import verify_public_build

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def type_text(node):
    text=node.get('text','');children=node.get('children',[])
    if text=='function' and len(children)==2:
        params=','.join(type_text(c) for c in children[0].get('children',[]))
        returns=','.join(type_text(c) for c in children[1].get('children',[]))
        return 'func('+params+')'+(' as '+returns if returns else '')
    if children and children[0].get('kind')=='list':
        args=','.join(type_text(c) for c in children[0].get('children',[]))
        if args:text+='<'+args+'>'
    return text
def declarations(node,name):
    if not node:return []
    result=[]
    children=node.get('children',[])
    if node.get('kind') in ('func','extern_func','async_func') and children and children[0].get('text')==name:
        params=children[1].get('children',[]) if len(children)>1 else []
        result.append([type_text(p['children'][0]) for p in params])
    for child in node.get('children',[]):result.extend(declarations(child,name))
    return result
def key(name,params):return name,tuple(t.replace(' ','') for t in params)
def generic_match(pattern,concrete):
    # Only these two declared type parameters occur in the three approved
    # generic array algorithms. Bind each once and require repeated equality.
    if len(pattern)!=len(concrete):return False
    bound={}
    for expected,actual in zip(pattern,concrete):
        expression='';at=0;captured=[]
        for token in re.finditer(r'\b[TK]\b|(?<=as)[TK]\b',expected):
            name=token.group();expression+=re.escape(expected[at:token.start()]);at=token.end()
            if name in bound:expression+=re.escape(bound[name])
            elif name in captured:expression+='(?P='+name+')'
            else:expression+='(?P<'+name+'>.+)';captured.append(name)
        expression+=re.escape(expected[at:]);match=re.fullmatch(expression,actual)
        if not match:return False
        bound.update(match.groupdict())
    return True
def self_test():
    sort=('array<T>','func(T,T)asint')
    assert generic_match(sort,('array<int>','func(int,int)asint'))
    assert generic_match(sort,('array<string>','func(string,string)asint'))
    assert not generic_match(sort,('array<int>','func(int,string)asint'))
    assert not generic_match(sort,('array<int>','func(int,int)asfloat'))
    assert not generic_match(sort,('array<int>',))
    by=('array<T>','func(T)asK','func(K,K)asint')
    assert generic_match(by,('array<string>','func(string)asint','func(int,int)asint'))
    assert not generic_match(by,('array<string>','func(string)asint','func(string,string)asint'))
    assert key('signal.on',('int','func()','int'))!=key('signal.on',('int','func(SignalEvent)','int'))
CLASS_NAMES={'Buffer','BufferIter','FixedStack','StringIterator','Utf8Validator','Regex','RegexMatch','RegexIterator',
    'RegexSplitIterator','RegexSplitPart','RegexReplacer','RegexSet','PatternOptions','PatternKey','PatternSpec','Pattern',
    'PatternMatch','PatternBuilder','DateTime','SignalSubscription','SignalEvent','SignalHandle','ProcessRunOptions',
    'ProcessResult','ProcessExitStatus','ProcessCommand','Process','ProcessPipeline','PipelineResult','ProcessStageResult'}
def class_members(catalog):
    rows=[];classes=set()
    for node in catalog['children']:
        owner=node.get('text')
        if node.get('kind')!='class' or owner not in CLASS_NAMES:continue
        classes.add(owner)
        for member in node['children'][1]['children']:
            kind=member['kind'];flags=member.get('flags',0);children=member.get('children',[])
            if kind=='field':
                rows.append(dict(Owner=owner,Name=member['text'],Kind=kind,ParameterTypes=[],Flags=flags,
                    Category='representation' if flags&128 else 'private' if flags&6 else 'field'));continue
            if kind not in ('func','async_func','extern_func','native_extern_func','native_extern_special','get','let','create'):continue
            name=member['text'] if kind=='create' else children[0]['text']
            params=children[0]['children'] if kind=='create' else children[1]['children']
            vector=[type_text(p['children'][0]).replace(' ','') for p in params]
            category='private' if flags&6 else 'lifecycle' if name=='destroy' else 'internal-composition' if any('__Pattern' in t for t in vector) else 'callable'
            rows.append(dict(Owner=owner,Name=name,Kind=kind,ParameterTypes=vector,Flags=flags,Category=category))
    assert classes==CLASS_NAMES,('Missing managed class declarations',CLASS_NAMES-classes)
    return rows
def class_call(call):
    owner=call['Owner'];base=owner.split('<',1)[0];vector=list(call['ParameterTypes'])
    if not call['DeclarationFlags']&8 and call['DeclarationKind']!='native_extern_special' and vector and vector[0]==owner:vector.pop(0)
    return base,call['Internal'],tuple(t.replace(' ','') for t in vector)
def main():
    self_test()
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--runtime-library',type=Path,help='Genuine current runtime containing the TCC core; no provider override')
    p.add_argument('--jobs',type=int,default=4);a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    assert not out.exists() and 1<=a.jobs<=8;cc=shutil.which('gcc');assert cc
    build,pinned,compared=verify_public_build(root,public)
    task=Path(__file__).resolve().parents[1];host=task/'test/cases/stdlib_resolved_coverage.c'
    runtime=a.runtime_library.resolve() if a.runtime_library else (public/'xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so')
    if os.name=='nt' and not runtime.exists():runtime=root/'release/xl.dll'
    assert runtime.exists();sources=sorted(s for s in (root/'test/cases').glob('stdlib_*.xl') if PREFIX.match(s.name) or s.stem=='stdlib_crt_algorithms')
    control=task/'test/cases/stdlib_resolved_coverage_control.xl'
    class_seed=task/'test/cases/stdlib_class_catalog.xl'
    pinned.update((Path(__file__).resolve(),Path(inventory.__file__),host,control,class_seed,Path(cc),runtime,*sources));inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(pinned)]
    out.mkdir();rows=[];env=dict(os.environ);ext='.exe' if os.name=='nt' else ''
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def run(label,command):
        r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=180)
        log=out/(label+'.log');log.write_bytes(r.stdout+r.stderr)
        row=dict(Name=label,Exit=r.returncode,Command=list(map(str,command)),Log=str(log),LogSha256=sha(log))
        return row,r.stdout,r.stderr
    flags=['-std=c11','-Wall','-Wextra','-Werror','-O0','-DXL_STATIC_EMBED',*['-I'+str(root/f) for f in ('','release/include','lib/xrt','lib/xruntime','lib/tcc')],'-I'+str(public/'include')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    binary=out/('resolved'+ext);row,data,error=run('build',[cc,*flags,host,*objects,runtime,*systems,'-o',binary]);rows.append(row);assert row['Exit']==0,error[-5000:]
    row,data,error=run('catalog',[binary,'--catalog']);rows.append(row);assert row['Exit']==0,error[-5000:]
    catalog=json.loads(data);expected={}
    row,data,error=run('class-catalog',[binary,'--class-catalog',class_seed]);rows.append(row);assert row['Exit']==0,error[-5000:]
    class_catalog=json.loads(data)
    member_catalog=class_members(class_catalog);class_observed={}
    for item in catalog['Registrations']:
        vectors=declarations(item['Ast'],item['Internal']) if item['Ast'] else [item['ParameterTypes']]
        assert vectors,('No actual declaration',item['Index'],item['Internal'])
        item['ResolvedParameterVectors']=vectors
        for vector in vectors:expected.setdefault(key(item['Internal'],vector),[]).append(item)
    row,data,error=run('overload-control',[binary,control]);rows.append(row);assert row['Exit']==0,error[-5000:]
    actual=json.loads(data)['Calls']
    controls={key(c['Internal'],c['ParameterTypes']) for c in actual if c['SourceCall'] and key(c['Internal'],c['ParameterTypes']) in expected}
    assert controls=={key('__xl_std_encoding_is_utf8',('string','int'))},controls
    class_controls={class_call(c) for c in actual if c['SourceCall'] and c['Owner'].split('<',1)[0] in CLASS_NAMES}
    assert class_controls=={('Buffer','create',('string','int'))},class_controls
    cases=[];bindings={};methods={};failures=[]
    def inspect(source):
        row,data,error=run(source.stem,[binary,source]);record=dict(Name=source.stem,Row=row,Calls=[])
        if source.stem in NEGATIVE:
            diagnostic=NEGATIVE[source.stem].removeprefix('[sem] ')
            record['ExpectedRejection']=row['Exit']!=0 and diagnostic.encode() in error+data;return record
        if row['Exit']!=0:record['Error']=error.decode(errors='replace')[-5000:];return record
        record['Calls']=json.loads(data)['Calls'];return record
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        for case in pool.map(inspect,sources):
            cases.append(case);rows.append(case['Row'])
            if case.get('Error') or ('ExpectedRejection' in case and not case['ExpectedRejection']):failures.append(case['Name'])
            for call in case['Calls']:
                identity=key(call['Internal'],call['ParameterTypes'])
                if identity in expected and call['SourceCall']:bindings.setdefault(identity,set()).add(case['Name'])
                elif call['SourceCall'] and call['Internal'] in ('__xl_std_array_sort','__xl_std_array_sort_by','__xl_std_array_binary_search'):
                    for template in expected:
                        if template[0]==identity[0] and generic_match(template[1],identity[1]):bindings.setdefault(template,set()).add(case['Name'])
                if call['Owner'] and call['SourceCall']:methods.setdefault((call['Owner'],call['Internal'],tuple(call['ParameterTypes'])),set()).add(case['Name'])
                if call['Owner'] and call['SourceCall']:
                    member=class_call(call)
                    if member[0] in CLASS_NAMES:class_observed.setdefault(member,set()).add(case['Name'])
    missing=[dict(Internal=k[0],ParameterTypes=list(k[1]),Registrations=[i['Index'] for i in v],Public=[i['Namespace']+'.'+i['Name'] for i in v]) for k,v in expected.items() if k not in bindings]
    class_missing=[]
    for member in member_catalog:
        if member['Category']!='callable':continue
        identity=(member['Owner'],member['Name'],tuple(member['ParameterTypes']))
        hits=set(class_observed.get(identity,()))
        if member['Owner']=='FixedStack':
            for concrete,fixtures in class_observed.items():
                if concrete[:2]==identity[:2] and generic_match(identity[2],concrete[2]):hits.update(fixtures)
        member['Fixtures']=sorted(hits)
        if not hits:class_missing.append(member)
    report=dict(Complete=not failures,InputsUnchanged=all(sha(r['Path'])==r['Sha256'] for r in inputs),Scope=__doc__,Inputs=inputs,
        Products=[dict(Path=str(binary),Sha256=sha(binary))],Rows=rows,Cases=cases,Catalog=catalog,ClassCatalog=class_catalog,ClassMembers=member_catalog,MissingClassBindings=class_missing,
        CandidateCount=len(sources),RegistrationCount=len(catalog['Registrations']),ExpectedBindingCount=len(expected),BoundCount=len(bindings),Missing=missing,
        ComparedSourceInputs=compared,StaticBindings=[dict(Internal=k[0],ParameterTypes=list(k[1]),Fixtures=sorted(v)) for k,v in sorted(bindings.items())],
        ObservedClassBindings=[dict(Owner=k[0],Method=k[1],ParameterTypes=list(k[2]),Fixtures=sorted(v)) for k,v in sorted(methods.items())],Failures=failures,
        AllStaticBindingsObserved=not missing,AllManagedClassBindingsObserved=not class_missing,ClassIdentityControls=True,
        GenericMatchingControls=True,NoNameOrDeclarationFalsePositives=True,RuntimeExecutionAcceptance=False,ClassCompletenessAcceptance=False,
        FunctionCoverageAcceptance=False,Full53Acceptance=False,ShippingArtifact=False)
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged'],failures
    print(json.dumps(dict(Candidates=len(sources),Registrations=report['RegistrationCount'],Expected=report['ExpectedBindingCount'],Bound=report['BoundCount'],MissingCount=len(missing)),ensure_ascii=False),flush=True)
if __name__=='__main__':main()
