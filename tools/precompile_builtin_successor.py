"""Honest incremental serialized-object manifest over an unchanged target ABI.

Only unchanged literal C/include closures may inherit an existing object.
Compiler identities are per object. Schema 1 remains a single-producer input;
schema 2 never describes inherited bytes as freshly compiled target code.
"""
from pathlib import Path,PurePosixPath
import argparse,copy,hashlib,json,os,subprocess,time
from precompile_builtin import source_dependencies,source_fingerprint
from runtime_layout import source_directories,runtime_directory


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def header_inventory(root):
    return {p.relative_to(root).as_posix():sha(p)
        for folder in source_directories(root) for p in folder.rglob('*')
        if p.is_file() and p.suffix in ('.h','.inc','.def')}


def sdk_hash(directory):
    digest=hashlib.sha256()
    for path in sorted((directory/'include').rglob('*')):
        if path.is_file():
            digest.update(path.relative_to(directory).as_posix().encode()+b'\0'+bytes.fromhex(sha(path)))
    return digest.hexdigest()


def unit(row):
    if row.get('product')=='xrt':return None,None
    prefix,profile,relative=row['virtual'].split('/',2)
    if prefix!='__xl_objects' or not relative.endswith('.c.o') or '..'in PurePosixPath(relative).parts:
        raise ValueError('Unknown fixed target object: '+row['virtual'])
    return profile,relative[:-2]


def flags_for(parent,row):
    if parent['schema']==2:return row['compileFlags']
    profile,relative=unit(row)
    flags=list(parent['flags'])
    if relative is None:
        return flags+['-DXL_STATIC_EMBED']+(['-DXL6_BUILD_SHARED']if'/shared-native/'in row['virtual']else[])
    shared=profile.startswith('shared-')
    flags+=['-DXL_BUILD_DLL','-DLIBTCC_AS_DLL']if shared else['-DXL_STATIC_EMBED']
    if profile.endswith('-runtime'):flags+=['-DXL_RUNTIME_ONLY_DLL','-DXL_RUNTIME_ONLY_API']
    return flags


def actual_producer(parent,row):
    return row['producerSha256']if parent['schema']==2 else parent['nativeCompiler']


def plan(parent_path,old_root,new_root,distribution):
    parent=json.loads(parent_path.read_text());old_root=old_root.resolve();new_root=new_root.resolve()
    if parent['schema']not in (1,2):raise ValueError('Unsupported parent manifest')
    if parent['sources']!=source_fingerprint(old_root):raise ValueError('Parent source fingerprint changed')
    if parent['nativeSystemHeaders']!=hashlib.sha256(b'').hexdigest():
        raise ValueError('Native system-header digest requires external recomputation; this CLI qualifies TCC SDK headers only')
    old_headers=header_inventory(old_root);new_headers=header_inventory(new_root)
    if old_headers!=new_headers:raise ValueError('Target ABI/include headers changed; full qualification required')
    if sdk_hash(distribution)!=parent['sdk']:raise ValueError('Compiler SDK changed')
    # The unit universe comes from the same literal API arrays and component
    # catalog. A body-only API change must not silently add/remove target TUs.
    import re
    c_files=lambda root:{p.relative_to(root).as_posix():sha(p)
        for folder in source_directories(root)for p in folder.rglob('*.c')if p.is_file()}
    old_c,new_c=c_files(old_root),c_files(new_root)
    changed_c={name for name in old_c.keys()|new_c.keys()if old_c.get(name)!=new_c.get(name)}
    # Literal .c closures are conservative only when a header does not hide a
    # changed C include. Reject that envelope instead of pretending the old
    # producer's limited dependency walk proves arbitrary header-to-C edges.
    for relative in old_headers:
        header=old_root/relative
        for name in re.findall(r'^\s*#\s*include\s*["<]([^">\n]+\.c)[">]',header.read_text(encoding='utf-8'),re.M):
            candidates=[header.parent/name,old_root/name,old_root/'src'/name,runtime_directory(old_root)/name]
            for candidate in candidates:
                if not candidate.is_file():continue
                selected=Path(os.path.abspath(candidate)).relative_to(old_root).as_posix()
                if selected in changed_c:
                    raise ValueError('Changed C body included by a header; full qualification required: '+selected)
                break
    for name in ('runtimeSources','compilerSources'):
        pattern=r'static const char\* '+name+r'\[\] = \{(.*?)\};'
        a=re.search(pattern,(old_root/'src/api/xl_api.c').read_text(encoding='utf-8'),re.S)
        b=re.search(pattern,(new_root/'src/api/xl_api.c').read_text(encoding='utf-8'),re.S)
        if not a or not b or a[1]!=b[1]:raise ValueError('Target source universe changed')
    if source_dependencies(old_root,'src/compile/xl6_compile_unit.c')!=source_dependencies(new_root,'src/compile/xl6_compile_unit.c'):
        raise ValueError('Component catalog changed')
    native_files=lambda root:{p.relative_to(root).as_posix():sha(p)
        for name in ('lib/xrt','lib/xruntime')for p in(root/name).rglob('*')
        if p.is_file()and p.suffix in('.c','.h','.inc','.def')}
    if native_files(old_root)!=native_files(new_root):raise ValueError('Native product source changed')
    rows=[];closures={}
    for row in parent['objects']:
        if sha(row['source'])!=row['sha256']:raise ValueError('Parent object changed: '+row['source'])
        profile,relative=unit(row)
        if relative is None:
            rows.append(dict(virtual=row['virtual'],recompile=False,sourceRelative=None,
                closureSha256=None,literalSourceSha256=None))
            continue
        if relative not in closures:
            closures[relative]=(source_dependencies(old_root,relative),source_dependencies(new_root,relative))
        before,after=closures[relative]
        rows.append(dict(virtual=row['virtual'],recompile=before!=after,
            sourceRelative=relative,closureSha256=after,literalSourceSha256=sha(new_root/relative)))
    return parent,dict(oldRoot=str(old_root),newRoot=str(new_root),
        headers=old_headers,headerInventorySha256=hashlib.sha256(json.dumps(old_headers,sort_keys=True).encode()).hexdigest(),
        sdk=parent['sdk'],parentManifestSha256=sha(parent_path),rows=rows,
        recompiled=sum(r['recompile']for r in rows),inherited=sum(not r['recompile']for r in rows))


def validate_mixed_manifest(path,root=None,producer=None,verify_sources=True):
    path=Path(path);m=json.loads(path.read_text())
    if m.get('schema')!=2:raise ValueError('Not a mixed schema-2 manifest')
    unsigned={key:value for key,value in m.items()if key!='signature'}
    if hashlib.sha256(json.dumps(unsigned,sort_keys=True).encode()).hexdigest()!=m.get('signature'):
        raise ValueError('Mixed manifest content signature differs')
    parent_path=Path(m['parentManifest']['path'])
    if sha(parent_path)!=m['parentManifest']['sha256']:raise ValueError('Parent manifest changed')
    parent=json.loads(parent_path.read_text());original={r['virtual']:r for r in parent['objects']}
    for key in ('target','sdk','flags','components','nativeTranslationUnitSources','nativeSystemHeaders'):
        if m[key]!=parent[key]:raise ValueError('Inherited target configuration changed: '+key)
    if len(original)!=len(parent['objects']):raise ValueError('Duplicate parent object')
    rows=m['objects'];seen=set();counts={'recompiled':0,'inherited':0}
    if m.get('fullRecompile')is not False:raise ValueError('Mixed set cannot claim full recompilation')
    if root is not None and source_fingerprint(Path(root))!=m['sources']:raise ValueError('New source fingerprint differs')
    if producer is not None and sha(producer)!=m['currentProducer']['sha256']:raise ValueError('Current producer mismatch')
    if verify_sources:
        _,proof=plan(parent_path,Path(m['parentManifest']['sourceRoot']),Path(root or m['sourceRoot']),Path(m['distribution']))
        if proof!=m['inheritanceProof']:raise ValueError('Inheritance source proof differs')
    for identity,actual in m['producers'].items():
        if actual['sha256']!=identity or sha(actual['path'])!=identity:
            raise ValueError('Actual producer binary changed')
    proof_rows={r['virtual']:r for r in m['inheritanceProof']['rows']}
    for row in rows:
        name=row['virtual']
        if name in seen or name not in original:raise ValueError('Unexpected/duplicate target object')
        seen.add(name);previous=original[name];p=row['provenance'];kind=p['kind']
        if kind not in counts:raise ValueError('Unknown provenance kind')
        counts[kind]+=1
        if p['parentVirtual']!=name or p['parentManifestSha256']!=m['parentManifest']['sha256']:
            raise ValueError('Wrong parent reference')
        if sha(row['source'])!=row['sha256']or Path(row['source']).stat().st_size!=row['bytes']:
            raise ValueError('Target object changed')
        if row['producerSha256']not in m['producers']:raise ValueError('Unregistered actual producer')
        if row['compileFlags']!=flags_for(parent,previous):raise ValueError('Target flags changed')
        proof=proof_rows[name]
        if p['sourceRelative']!=proof['sourceRelative']or p['sourceClosureSha256']!=proof['closureSha256']:
            raise ValueError('Wrong target source closure')
        if (kind=='recompiled')!=proof['recompile']:raise ValueError('Object provenance contradicts source changes')
        if kind=='inherited':
            if any(row[k]!=previous[k]for k in ('source','sha256','bytes')):raise ValueError('Inherited bytes/path relabeled')
            if row['producerSha256']!=actual_producer(parent,previous):raise ValueError('Inherited producer relabeled')
        elif row['producerSha256']!=m['currentProducer']['sha256']:
            raise ValueError('New object does not name current compiler')
    if seen!=set(original)or counts!=m['counts']:raise ValueError('Object set/count differs')
    return m


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','parent-root','parent-manifest','distribution','producer','parent-producer','output'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--allow-producer-source-change',action='append',default=[])
    p.add_argument('--expect-recompiled',type=int)
    p.add_argument('--plan-only',action='store_true')
    a=p.parse_args();root=a.root.resolve();old=a.parent_root.resolve();output=a.output.resolve()
    if output.exists():raise ValueError('Use a fresh output directory')
    output.mkdir(parents=True)
    parent,proof=plan(a.parent_manifest.resolve(),old,root,a.distribution.resolve())
    if a.expect_recompiled is not None and proof['recompiled']!=a.expect_recompiled:
        raise ValueError('Unexpected target rebuild count: '+str(proof['recompiled']))
    old_compiler=sha(a.parent_producer)
    if parent['schema']==2:
        validate_mixed_manifest(a.parent_manifest,old,a.parent_producer)
        if parent['currentProducer']['sha256']!=old_compiler:
            raise ValueError('Parent current producer does not match supplied executable')
        producers=copy.deepcopy(parent['producers'])
    else:
        expected={actual_producer(parent,r)for r in parent['objects']}
        if expected!={old_compiler}:raise ValueError('Parent actual producer does not match supplied executable')
        producers={old_compiler:dict(path=str(a.parent_producer.resolve()),sha256=old_compiler)}
    producer=sha(a.producer)
    producers[producer]=dict(path=str(a.producer.resolve()),sha256=producer)
    tcc_files=lambda selected:{p.relative_to(selected).as_posix():sha(p)
        for p in(selected/'lib/tcc').rglob('*')if p.is_file()and p.suffix in('.c','.h','.inc')}
    before,after=tcc_files(old),tcc_files(root)
    changes=sorted(n for n in before.keys()|after.keys()if before.get(n)!=after.get(n))
    if changes!=sorted(a.allow_producer_source_change):raise ValueError('Unexpected TCC producer source changes: '+str(changes))
    (output/'plan.json').write_text(json.dumps(dict(inheritanceProof=proof,producerSourceChanges=changes,
        compilerObjectsToBuild=proof['recompiled'],inheritedObjects=proof['inherited']),indent=2)+'\n')
    if a.plan_only:print(output/'plan.json');return
    m=dict(schema=2,target=parent['target'],sdk=parent['sdk'],sources=source_fingerprint(root),
        sourceRoot=str(root),distribution=str(a.distribution.resolve()),flags=parent['flags'],
        currentProducer=dict(path=str(a.producer.resolve()),sha256=producer,
            kind=(parent['currentProducer']['kind']if parent['schema']==2 else
                ('tinycc'if parent['nativeCompiler']==parent['tcc']else'native-compiler'))),
        catalogPreprocessor=dict(sha256=parent.get('tcc',parent.get('catalogPreprocessor',{}).get('sha256')),
            provenance='Inherited unchanged component catalog, not re-preprocessed by the current compiler'),
        producers=producers,
        parentManifest=dict(path=str(a.parent_manifest.resolve()),sha256=sha(a.parent_manifest),
            sourceRoot=str(old)),inheritanceProof=proof,producerSourceChanges=changes,
        counts=dict(recompiled=proof['recompiled'],inherited=proof['inherited']),fullRecompile=False,
        nativeSystemHeaders=parent['nativeSystemHeaders'],
        components=parent['components'],nativeTranslationUnitSources=parent['nativeTranslationUnitSources'],
        objects=[],commands=[])
    for previous,decision in zip(parent['objects'],proof['rows']):
        row=copy.deepcopy(previous);kind='recompiled'if decision['recompile']else'inherited'
        flags=flags_for(parent,previous);relative=decision['sourceRelative']
        if decision['recompile']:
            destination=output/previous['virtual'].removeprefix('__xl_objects/')
            destination.parent.mkdir(parents=True,exist_ok=True)
            command=[str(a.producer.resolve()),*flags,'-c',relative,'-o',str(destination)]
            started=time.monotonic();r=subprocess.run(command,cwd=root,capture_output=True)
            log=output/('compile-'+str(len(m['commands']))+'.log');log.write_bytes(r.stdout+r.stderr)
            m['commands'].append(dict(command=command,exitCode=r.returncode,seconds=round(time.monotonic()-started,3),logSha256=sha(log)))
            if r.returncode:
                (output/'failure.json').write_text(json.dumps(m,indent=2)+'\n')
                raise RuntimeError(str(log))
            row.update(source=str(destination),sha256=sha(destination),bytes=destination.stat().st_size)
        row.update(producerSha256=producer if decision['recompile']else actual_producer(parent,previous),
            compileFlags=flags,provenance=dict(kind=kind,parentVirtual=previous['virtual'],
                parentManifestSha256=sha(a.parent_manifest),sourceRelative=relative,
                sourceClosureSha256=decision['closureSha256']))
        m['objects'].append(row)
    for name in ('static-native.o','shared-native.o'):
        # The existing packer CLI retains this literal compatibility input;
        # bytes and actual object paths in rows keep their parent provenance.
        source=a.parent_manifest.resolve().parent/name
        if os.name=='nt':os.link(source,output/name)
        else:os.symlink(source,output/name)
    m['signature']=hashlib.sha256(json.dumps(m,sort_keys=True).encode()).hexdigest()
    destination=output/'manifest.json';destination.write_text(json.dumps(m,indent=2)+'\n')
    validate_mixed_manifest(destination,root,a.producer)
    print(destination)

if __name__=='__main__':main()
