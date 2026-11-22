"""Compile every package-selected native bridge unit and index native imports.

This distinguishes a public declaration from a compiler/policy consumer. It
does not infer equivalent behavior, complete ownership, runtime acceptance or
API completeness from a symbol reference. All unreviewed rows remain pending.
"""
from pathlib import Path
import argparse,hashlib,json,re,shutil,subprocess,time
from full_compiler_qualification import verify_full_compiler
from stage_xge3_replay_contracts import verify_replay_package
from check_xge3_full_surface import source_bindings
from xge_replay_contracts import REPLACED as XGE_REPLACEMENTS
from xui_document_replacements import CLOSE_REPLACEMENTS as DOCUMENT_CLOSE_REPLACEMENTS,LIFECYCLE_REPLACEMENTS,REPLACEMENTS as DOCUMENT_REPLACEMENTS
from xui_managed_contracts import CLOSE_REPLACEMENTS as XUI_CLOSE_REPLACEMENTS,REPLACED as XUI_REPLACEMENTS

# Logical source-group selectors may name an object-like header alias. Prove
# this one against an actual C relocation, rather than assuming a name suffix.
PROVIDER_ALIASES={'xl6Xge3InitPacked':('xge3_init.h','xl6Xge3InitWithCode')}
REPLACED = {**XGE_REPLACEMENTS, **XUI_REPLACEMENTS, **DOCUMENT_REPLACEMENTS, **LIFECYCLE_REPLACEMENTS}
CLOSE_REPLACEMENTS = {**DOCUMENT_CLOSE_REPLACEMENTS, **XUI_CLOSE_REPLACEMENTS}

# The old releases consume the same owner as the checked release in close().
# These policies live in the package headers, so runtime.c cannot prove them.
CHECKED_3D_CLOSES = (
    ('Animator', 'Animator', ('Free', 'ReleaseChecked')),
    ('AnimationClip', 'Clip', ('Free', 'ReleaseChecked')),
    ('Environment3D', 'Environment', ('Free', 'ReleaseChecked')),
    ('Material3D', 'Material', ('Free', 'ReleaseChecked')),
    ('MaterialSnapshot3D', 'MaterialSnapshot', ('ReleaseChecked',)),
    ('Mesh3D', 'Mesh', ('Free', 'ReleaseChecked')),
    ('Model3D', 'Model', ('Free', 'ReleaseChecked')),
    ('ModelCache3D', 'Cache', ('Free',)),
    ('ModelInstanceBuilder', 'ModelInstance', ('Free', 'ReleaseChecked')),
    ('ModelLoader3D', 'Loader', ('Free',)),
    ('ModelRetirement', 'ModelRetire', ('Free',)),
    ('Renderer', 'Renderer', ('Free', 'ReleaseChecked')),
    ('RenderTarget3D', 'Target', ('Free', 'ReleaseChecked')),
    ('Scene', 'Scene', ('Free', 'ReleaseChecked')),
    ('Shader3D', 'Shader', ('Free', 'ReleaseChecked')),
    ('Terrain', 'Terrain', ('Free', 'ReleaseChecked')),
    ('Texture3D', 'Texture', ('Free', 'ReleaseChecked')),
)
HEADER_CLOSE_REPLACEMENTS = {
    f'xge3d{family}{old}': (f'xge3d.{owner}.close', f'xge3d{family}ReleaseOwnerChecked')
    for owner, family, variants in CHECKED_3D_CLOSES for old in variants
}
HEADER_CLOSE_REPLACEMENTS['xge3dQueryResultsReleaseChecked'] = (
    'xge3d.QueryResults.close', 'xge3dQueryResultsRelease')
CORE_POLICY_CLASSES = {
    'xge.Scene', 'xge.PlatformBackend', 'xge.GraphicsBackend', 'xge.HostSession',
    'xge.ImageLoad', 'xge.TextureLoad', 'xge.FontLoad', 'xge.SoundLoad',
    'xge.ParticleWorld', 'xge.ParticleDefinition', 'xge.ParticleRenderer',
}
CORE_HEADER_CLOSES = {
    'xgeSceneReleaseChecked': ('Scene', 'Scene', 'xgeSceneReleaseOwnerChecked'),
    'xgePlatformBackendReleaseChecked': ('PlatformBackend', 'PlatformBackend', 'xgePlatformBackendReleaseOwnerChecked'),
    'xgeGraphicsBackendReleaseChecked': ('GraphicsBackend', 'GraphicsBackend', 'xgeGraphicsBackendReleaseOwnerChecked'),
    'xgeHostSessionReleaseChecked': ('HostSession', 'HostSession', 'xgeHostSessionReleaseOwnerChecked'),
    'xgeAsyncLoadReleaseChecked': ('ImageLoad', 'AsyncLoad', 'xgeAsyncLoadReleaseOwnerChecked'),
    'xgeAsyncRequestFree': ('ImageLoad', 'AsyncLoad', 'xgeAsyncLoadReleaseOwnerChecked'),
    'xgeParticleWorldFree': ('ParticleWorld', 'ParticleWorld', 'xgeParticleWorldReleaseOwnerChecked'),
    'xgeParticleWorldFreeChecked': ('ParticleWorld', 'ParticleWorld', 'xgeParticleWorldReleaseOwnerChecked'),
    'xgeParticleRendererFree': ('ParticleRenderer', 'ParticleRenderer', 'xgeParticleRendererReleaseOwnerChecked'),
    'xgeParticleRendererFreeChecked': ('ParticleRenderer', 'ParticleRenderer', 'xgeParticleRendererReleaseOwnerChecked'),
}
HEADER_CLOSE_REPLACEMENTS.update({
    old: (f'xge.{owner}.close', release)
    for old, (owner, _, release) in CORE_HEADER_CLOSES.items()
})
HEADER_CLOSE_REPLACEMENTS['xuiTerminalSessionDestroy'] = (
    'xui.TerminalSession.close', 'xuiTerminalSessionReleaseOwnerChecked')
ICON_REFERENCE_RELEASES = {'xuiIconRelease', 'xuiIconCategoryRelease', 'xuiIconReferenceRelease'}
HEADER_CLOSE_REPLACEMENTS.update({
    name: ('xui.IconReference.close', 'xuiIconReferenceReleaseOwnerChecked')
    for name in ICON_REFERENCE_RELEASES
})
DOCUMENT_RENDERER_SOURCE = 'src/xui/document/xui_document_renderer.c'
DOCUMENT_MODEL_SOURCE = 'src/xui/document/xui_document.c'
DOCUMENT_FONT_TAIL = {'xuiDocumentFontOutputInit', 'xuiDocumentFontOutputDisposeChecked'}
DOCUMENT_EXPORTS = (
    'xuiDocumentSerialize', 'xuiDocumentExportHtml', 'xuiDocumentSnapshotCopyPlainText',
    'xuiDocumentSnapshotCopyRange', 'xuiDocumentSnapshotCopyTableMatrix',
    'xuiDocumentFragmentSerialize', 'xuiDocumentFragmentExportHtml',
)
TABLE_VIEW_SOURCE = 'src/xui/controls/xui_table_view.c'
TABLE_GRID_SOURCE = 'src/xui/controls/xui_table_grid.c'
TABLE_LIFETIME_CALLS = {
    TABLE_VIEW_SOURCE: {
        '__xuiTableViewGetCell': ('xuiTableViewCellOutputInit',),
        '__xuiTableViewLeaseEnd': ('xuiTableViewCellOutputDisposeChecked',),
        'xuiTableViewGetCellOwned': ('xuiTableViewCellOutputCallScopeBegin', 'xuiTableViewCellOutputCallScopeEnd'),
    },
    TABLE_GRID_SOURCE: {
        '__xuiTableGridInit': ('xuiTableGridEditorConfigOutputInit',),
    },
}


def object_relocations(text):
    """Read GNU objdump section relocations in COFF and ELF objects."""
    relocations={}
    section=None
    for line in text.splitlines():
        match=re.fullmatch(r'RELOCATION RECORDS FOR \[(.+)\]:',line)
        if match:
            section=relocations.setdefault(match[1],set())
            continue
        target=re.fullmatch(r'[0-9a-fA-F]+\s+\S+\s+(\S+)',line.strip())
        if section is not None and target:
            name=re.sub(r'[+-]0x[0-9a-fA-F]+$','',target[1]).removeprefix('__imp_')
            section.add(name)
    return relocations


def close_relocation_path(relocations,policy,destroy,release):
    # A policy can also reach native functions through tracing and quiescence.
    # Only its descriptor's registered consuming function proves close.
    paths=[]
    for prefix in ('.text.','.text$'):
        getter=prefix+policy+'Get';consumer=prefix+destroy
        if release not in relocations.get(consumer,()):continue
        for descriptor in relocations.get(getter,()):
            if consumer in relocations.get(descriptor,()):
                paths.append((getter,descriptor,consumer,release))
    assert len(paths)==1,(policy,destroy,release,paths)
    return paths[0]


def class_lines(view, binding):
    return (view/binding['source']).read_text(encoding='utf-8-sig').splitlines()[binding['line']-1:]


def require_close(lines, policy, logical):
    assert '@lifetime("owned")' in lines[0] and f'@policy("{policy}")' in lines[0], logical
    class_end = next(i for i,line in enumerate(lines) if line.strip()=='end')
    close = 'extern "xl6ForeignViewCloseV1" @no_raise @c_control(0) @c_result("bool_status") func close()'
    assert any(line.strip()==close for line in lines[1:class_end]), logical


def reachable_relocations(relocations, name):
    reachable = set()
    pending = [name]
    while pending:
        target = pending.pop()
        if target not in reachable:
            reachable.add(target)
            pending.extend(relocations.get(target, ()))
    return reachable


def compile_header_policies(name, view, classes, native_names, flags, tools, out, run, symbols, sha):
    headers = set()
    selectors = {}
    for logical, binding in sorted(classes.items()):
        lines = class_lines(view, binding)
        declaration = lines[0]
        policy = re.search(r'@policy\("(\w+)"\)', declaration)
        if not policy:
            continue
        header = re.search(r'@header\("(\w+\.h)"\)', declaration)
        assert header, declaration
        headers.add(header[1])
        require_close(lines, policy[1], logical)
        selectors[policy[1]] = 'XL6_ForeignViewDescriptorV1'
        domain = re.search(r'@domain\("(\w+)"\)', declaration)
        if domain:
            selectors[domain[1]] = 'XL6_OwnershipDomainV1'
    assert selectors

    # Each witness references the descriptor selected by an actual declaration.
    # Keeping the getter and callbacks out of line preserves their object edges.
    source = out/(name+'.c')
    obj = out/(name+'.o')
    includes = [f'#include <{header}>' for header in sorted(headers)]
    accessors = [f'const {kind} *audit_{selector}(void) {{ return &{selector}; }}'
        for selector,kind in sorted(selectors.items())]
    source.write_text('\n'.join(includes+accessors)+'\n', encoding='utf-8')
    run(name+'-compile', [tools['gcc'],*flags,'-O0','-fno-inline','-c',source,'-o',obj])
    undefined = symbols(run(name+'-undefined', [tools['nm'],'-u',obj]))
    relocations = object_relocations(run(name+'-relocations', [tools['objdump'],'-r',obj]))
    imports = {}
    for selector in sorted(selectors):
        getters = [prefix+selector+'Get' for prefix in ('.text.','.text$')
            if prefix+selector+'Get' in relocations.get(prefix+'audit_'+selector, ())]
        assert len(getters)==1, (selector, getters)
        imports[selector] = sorted(reachable_relocations(relocations,getters[0]) & native_names)
        assert imports[selector] and set(imports[selector])<=undefined, (selector, imports[selector])
    evidence = dict(source=str(source), sha256=sha(source), object=str(obj), objectSha256=sha(obj),
        headerSelectors=imports)
    return evidence, relocations


def witness_build_inputs(build, audit, packages, gcc, sha, units):
    metadata = json.loads(build.read_text(encoding='utf-8'))
    assert metadata['linked'] and metadata['xrt_mode']=='host'
    assert Path(metadata['compiler'][0]).resolve()==gcc
    source = Path(metadata['source_root']).resolve()
    snapshot = audit['nativeSourceSnapshot']
    record = Path(snapshot['sourceRecord'])
    assert sha(record)==snapshot['sourceRecordSha256']
    freeze = json.loads(record.read_text(encoding='utf-8'))
    assert freeze['engineRevision']==audit['nativeRevision'] and Path(freeze['source']).resolve()==source
    image = build.parent/metadata['runtime_image']
    staged_image = packages/'_xge_native/native'/image.name
    assert sha(image)==sha(staged_image), 'Native witness must match the staged runtime'
    pins = [build, record, image, build.parent/'xge_build_config.h']
    pins.extend((build.parent/'host-xrt').glob('*.*'))
    for name in units:
        assert name in metadata['sources'], name
    for name, digest in freeze['sourceInputs'].items():
        if name in units or Path(name).suffix in ('.h','.inc'):
            path = (source/name).resolve()
            assert path.is_relative_to(source) and sha(path)==digest, name
            pins.append(path)
    return source, metadata['flags'], pins


def prove_table_lifetime(source, flags, bound, tools, out, run, sha):
    assert any(item['language']=='xui.TableView.getCell' for item in bound['xuiTableViewGetCellOwned'])
    assert any(item['language']=='xui.Context.tableGridNative' for item in bound['xuiTableGridCreate'])
    evidence = {}
    for path, expected in TABLE_LIFETIME_CALLS.items():
        label = Path(path).stem+'-lifetime'
        obj = out/(label+'.o')
        run(label+'-compile', [tools['gcc'],*flags,'-O0','-fno-inline','-ffunction-sections',
            '-fdata-sections','-c',source/path,'-o',obj], cwd=source)
        relocations = object_relocations(run(label+'-relocations',[tools['objdump'],'-r',obj]))
        for caller, names in expected.items():
            refs = relocations.get('.text.'+caller, set()) | relocations.get('.text$'+caller, set())
            for name in names:
                assert {'.text.'+name, '.text$'+name, name} & refs, (caller, name)
        evidence[path] = dict(object=str(obj), objectSha256=sha(obj), nativeCalls=expected)
    return evidence


def prove_document_lifetime(source, flags, view, bound, packages, tools, out, run, sha):
    evidence = {}
    for name, path in (('renderer', DOCUMENT_RENDERER_SOURCE), ('model', DOCUMENT_MODEL_SOURCE)):
        label = 'document-'+name+'-lifetime'
        obj = out/(label+'.o')
        # Retain local calls as section relocations while using the same
        # production feature flags and hosted-XRT import definitions.
        run(label+'-compile', [tools['gcc'],*flags,'-O0','-fno-inline','-ffunction-sections',
            '-fdata-sections','-c',source/path,'-o',obj], cwd=source)
        relocations = object_relocations(run(label+'-relocations',[tools['objdump'],'-r',obj]))
        evidence[name] = dict(source=path, object=str(obj), objectSha256=sha(obj))
        if name=='renderer':
            target = 'doc_render_resolve_font'
            references = relocations.get('.text.'+target, set()) | relocations.get('.text$'+target, set())
            for function in sorted(DOCUMENT_FONT_TAIL):
                assert {'.text.'+function,'.text$'+function,function} & references, (target,function)
            evidence[name].update(consumer=target, nativeCalls=sorted(DOCUMENT_FONT_TAIL))
        else:
            target = 'xuiDocumentFreeBuffer'
            references = reachable_relocations(relocations,'.text.'+target) | reachable_relocations(relocations,'.text$'+target)
            slot = 'XgeHostXrtSlot_xrtFree'
            pointer = '.refptr.'+slot
            if slot in references:
                path = [slot]
            else:
                assert pointer in references and slot in relocations.get('.rdata$'+pointer,set()), (target,references)
                path = [pointer,slot]
            evidence[name].update(consumer=target, allocatorPath=path)
    marker = (packages/'_xge_native/include/xui_document.h').read_text(encoding='utf-8')
    assert re.search(r'^#define XUI_DOCUMENT_EXPORT_XRT_ALLOCATOR 1u$', marker, re.M)
    owners = []
    for producer in DOCUMENT_EXPORTS:
        for binding in bound[producer]:
            path = view/binding['source']
            line = path.read_text(encoding='utf-8-sig').splitlines()[binding['line']-1]
            assert '@header("xge3_document_xrt.h")' in line, binding
            assert re.search(r'@c_output_(?:bytes|text)\([^)]*,"xrt",0\)',line), binding
            owners.append(binding['language'])
    evidence['bufferReplacement'] = dict(language='Owned Document export values', native='xrtFree',
        allocatorMarker='XUI_DOCUMENT_EXPORT_XRT_ALLOCATOR', owningExports=sorted(set(owners)))
    return evidence


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('surface','compiler','compiler-report','packages','replay-report','out'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--document-engine-build',type=Path,
        help='Matching hosted engine build-report.json for Document lifetime witnesses')
    p.add_argument('--table-engine-build',type=Path,
        help='Matching hosted engine build-report.json for TableView and TableGrid lifetime witnesses')
    a=p.parse_args();surface,cc,build,packages,replay,out=[v.resolve() for v in
        (a.surface,a.compiler,a.compiler_report,a.packages,a.replay_report,a.out)]
    if out.is_relative_to(packages) or out.is_relative_to(cc.parent):
        p.error('Keep audit outputs outside package/compiler inputs')
    verify_full_compiler(build,cc);sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    proof=verify_replay_package(packages,replay)
    audit=json.loads(surface.read_text(encoding='utf8'));assert audit['inputsUnchanged']
    functions={r['symbol']:r for r in audit['items'] if r['kind']=='functions'}
    assert len(functions)==audit['population']['functions'],'Duplicate function identities require review'
    core=packages/'_xge_native';manifest=core/'xlang.package.json';metadata=json.loads(manifest.read_text(encoding='utf8'))
    actual_headers={p.name:sha(p) for p in (core/'include').glob('*.h')}
    assert all(actual_headers.get(name)==h for name,h in audit['publicHeaderSha256'].items()),'Surface/SDK headers differ'
    artifact=metadata['artifact'];groups=artifact.get('cSourceGroups',[])
    sources=[*artifact['cSources'],*[g['source'] for g in groups]]
    assert len(sources)==len(set(sources))
    tools={name:Path(shutil.which(name)).resolve() for name in ('gcc','nm','objdump')}
    sdk=cc.parent/'include';files=[Path(__file__),Path(__file__).with_name('full_compiler_qualification.py'),
        Path(__file__).with_name('xge_replay_contracts.py'),Path(__file__).with_name('check_xge3_full_surface.py'),
        Path(__file__).with_name('xui_managed_contracts.py'),Path(__file__).with_name('xui_document_replacements.py'),
        Path(__file__).with_name('check_xge3_bindings.py'),surface,cc,build,*proof,*tools.values()]
    for folder in (sdk,packages):files.extend(f for f in folder.rglob('*') if f.is_file())
    document = None
    table_build = None
    if a.document_engine_build:
        document_source, document_flags, evidence_inputs = witness_build_inputs(
            a.document_engine_build.resolve(),audit,packages,tools['gcc'],sha,{DOCUMENT_RENDERER_SOURCE, DOCUMENT_MODEL_SOURCE})
        files.extend(evidence_inputs)
    if a.table_engine_build:
        table_source, table_flags, evidence_inputs = witness_build_inputs(
            a.table_engine_build.resolve(),audit,packages,tools['gcc'],sha,TABLE_LIFETIME_CALLS.keys())
        files.extend(evidence_inputs)
        table_build = (table_source, table_flags)
    inputs={str(f):sha(f) for f in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],objects=[],providerAliases=[],
        runtimeAcceptance=False,apiCompletenessClaim=False,shippingDelivery=False,networkTests=False,graphicsTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,cwd=None):
        command=list(map(str,command));start=time.monotonic();z=subprocess.run(command,cwd=cwd,capture_output=True,timeout=180)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,
            logSha256=sha(log),cwd=str(cwd) if cwd else None));save();print(name,z.returncode,flush=True)
        assert not z.returncode,log
        return data.decode('utf8')
    def symbols(text):
        # x86_64 COFF import indirection names the same authoritative C symbol.
        return {line.split()[-1].removeprefix('__imp_') for line in text.splitlines() if line.split()}
    consumers={}
    try:
        # Reuse the authoritative inventory parser on copies of the actual
        # package-selected declarations. A same-header, stale language package
        # must not inherit the source inventory's declaration counts.
        view=out/'source-view'
        for package,relative in (('xge','xge'),('xui','xui'),('xge3d','xge/three_d'),('xgeDebug','xge/debug')):
            original=packages/package/'xlang.package.json'
            declaration=json.loads(original.read_text(encoding='utf8'))
            target=view/'lib/bindings'/relative;target.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(original,target/'xlang.package.json')
            for name in declaration['artifact']['xlSources']:
                dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(original.parent/name,dest)
        bound,_,bound_types,_,view_pins=source_bindings(view)
        expected={n for n,r in functions.items() if r['declarationPresent']}
        actual=set(bound)&set(functions)
        assert actual==expected,dict(missing=sorted(expected-actual),unexpected=sorted(actual-expected))
        report['bindingView']=dict(source=str(view),files=view_pins,nativeDeclarationsMatch=True)
        flags=['-std=c11','-O2','-ffunction-sections','-fdata-sections',
            '-Werror=incompatible-pointer-types','-Werror=implicit-function-declaration','-Werror=int-conversion',
            '-Werror=return-type','-DXL_STATIC_EMBED=1','-DXRT_MODULE_ALL','-I'+str(sdk),
            *['-I'+str(f) for f in sorted(packages.glob('*/include'))]]
        if any(artifact.get('nativeLink', {}).values()):
            flags.append('-DXL6_NATIVE_LINK_SCHEMA=1')
        policy_relocations={}
        for number,source in enumerate(sources):
            name='unit-'+str(number);obj=out/(name+'.o')
            run(name+'-compile',[tools['gcc'],*flags,'-c',core/source,'-o',obj])
            undefined=symbols(run(name+'-undefined',[tools['nm'],'-u',obj]))
            defined=symbols(run(name+'-defined',[tools['nm'],'--defined-only','-g',obj]))
            if source=='src/runtime.c' and CLOSE_REPLACEMENTS.keys() & functions.keys():
                policy_relocations=object_relocations(run(name+'-relocations',[tools['objdump'],'-r',obj]))
            for group in groups:
                if group['source']!=source:continue
                for alias in set(group['providedSymbols'])-defined:
                    assert alias in PROVIDER_ALIASES,(source,alias)
                    header,target=PROVIDER_ALIASES[alias];assert target in defined,(source,target)
                    probe=out/(alias+'.c');probe.write_text('#include <'+header+'>\nvoid (*proof)(void)=(void(*)(void))&'+alias+';\n',encoding='utf8')
                    output=out/(alias+'.o')
                    macros=run(alias+'-macros',[tools['gcc'],*flags,'-E','-dM',probe])
                    assert '#define '+alias+' '+target in macros.splitlines(),(alias,'not the reviewed object-like alias')
                    run(alias+'-compile',[tools['gcc'],*flags,'-c',probe,'-o',output])
                    refs=symbols(run(alias+'-undefined',[tools['nm'],'-u',output]))
                    assert target in refs and alias not in refs,(alias,refs)
                    report['providerAliases'].append(dict(alias=alias,target=target,header=header,source=source,
                        probeSha256=sha(probe),objectSha256=sha(output)))
            native=sorted(set(functions)&undefined)
            for symbol in native:consumers.setdefault(symbol,[]).append(source)
            report['objects'].append(dict(source=source,path=str(obj),sha256=sha(obj),nativeImports=native,
                otherUndefinedSymbols=sorted(undefined-set(functions))));save()
        rows=[]
        class_bindings={r['language']:r for group in bound_types.values() for r in group}
        assert CORE_POLICY_CLASSES <= class_bindings.keys()
        policy_groups = {
            'headerPolicies': ('xge3d-header-policies',
                {name: binding for name, binding in class_bindings.items() if binding['package']=='xge3d'}),
            'coreHeaderPolicies': ('xge-header-policies',
                {name: class_bindings[name] for name in CORE_POLICY_CLASSES}),
            'terminalHeaderPolicies': ('xui-terminal-header-policies',
                {'xui.TerminalSession': class_bindings['xui.TerminalSession']}),
            'iconHeaderPolicies': ('xui-icon-header-policies',
                {'xui.IconReference': class_bindings['xui.IconReference']}),
        }
        header_relocations = {}
        for key, (name, classes) in policy_groups.items():
            evidence, relocations = compile_header_policies(name,view,classes,set(functions),
                flags,tools,out,run,symbols,sha)
            report[key] = evidence
            header_relocations[key] = relocations
            imports = {symbol for names in evidence['headerSelectors'].values() for symbol in names}
            for symbol in sorted(imports):
                consumers.setdefault(symbol,[]).append(name)
        if a.document_engine_build:
            document = prove_document_lifetime(document_source,document_flags,view,bound,packages,tools,out,run,sha)
            report['documentLifetime'] = document
            for symbol in sorted(DOCUMENT_FONT_TAIL):
                consumers.setdefault(symbol,[]).append(DOCUMENT_RENDERER_SOURCE)
        if table_build:
            report['tableLifetime'] = prove_table_lifetime(*table_build,bound,tools,out,run,sha)
            for source_path, expected in TABLE_LIFETIME_CALLS.items():
                for names in expected.values():
                    for symbol in names:
                        consumers.setdefault(symbol,[]).append(source_path)
        for symbol,item in sorted(functions.items()):
            if item['declarationPresent']:continue
            replacement=REPLACED.get(symbol)
            evidence=None
            if replacement:
                assert replacement[1] in functions and functions[replacement[1]]['declarationPresent'],symbol
                assert any(r['language']==replacement[0] for r in bound[replacement[1]]),replacement
                evidence=dict(language=replacement[0],native=replacement[1])
            elif symbol in CLOSE_REPLACEMENTS:
                logical,policy,destroy,release=CLOSE_REPLACEMENTS[symbol]
                owner=class_bindings[logical.rsplit('.',1)[0]]
                require_close(class_lines(view,owner),policy,logical)
                assert release in functions and 'src/runtime.c' in consumers.get(release,()),release
                evidence=dict(language=logical,native=release,policy=policy,
                    relocationPath=close_relocation_path(policy_relocations,policy,destroy,release))
            elif symbol in HEADER_CLOSE_REPLACEMENTS:
                logical,release=HEADER_CLOSE_REPLACEMENTS[symbol]
                owner=class_bindings[logical.rsplit('.',1)[0]]
                policy=re.search(r'@policy\("(\w+)"\)',class_lines(view,owner)[0])[1]
                if symbol in CORE_HEADER_CLOSES:
                    key='coreHeaderPolicies'
                    destroy='xl6Xge3'+CORE_HEADER_CLOSES[symbol][1]+'Destroy'
                elif symbol=='xuiTerminalSessionDestroy':
                    key='terminalHeaderPolicies'
                    destroy='xl6Xui3TerminalSessionDestroy'
                elif symbol in ICON_REFERENCE_RELEASES:
                    key='iconHeaderPolicies'
                    destroy='xl6Xui3IconReferenceDestroy'
                else:
                    key='headerPolicies'
                    destroy='xl6Xge3dSceneDestroy' if policy=='xl6Xge3dScenePolicy' else policy+'Destroy'
                assert release in report[key]['headerSelectors'][policy],(logical,release)
                evidence=dict(language=logical,native=release,policy=policy,
                    relocationPath=close_relocation_path(header_relocations[key],policy,destroy,release))
            elif document and symbol=='xuiDocumentFreeBuffer':
                evidence=document['bufferReplacement']
            rows.append(dict(id=item['id'],symbol=symbol,family=item['family'],conditions=item['conditions'],
                publicDeclarationPresent=False,compiledNativeConsumers=consumers.get(symbol,[]),
                explicitReplacement=evidence,
                contractReview='pending',runtimeAcceptance='not-certified-by-this-audit'))
        verify_replay_package(packages,replay)
        report.update(items=rows,nativeRevision=audit['nativeRevision'],functionPopulation=len(functions),
            directDeclarations=sum(r['declarationPresent'] for r in functions.values()),nonDirectFunctions=len(rows),
            compiledConsumerFunctions=sum(bool(r['compiledNativeConsumers']) for r in rows),
            explicitReplacements=sum(bool(r['explicitReplacement']) for r in rows),complete=True,
            inputsUnchanged=all(sha(f)==h for f,h in inputs.items()) and
                all(sha(view/f)==h for f,h in view_pins.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except BaseException as error:report.update(complete=True,passed=False,error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
