"""Compile every package-selected native bridge unit and index native imports.

This distinguishes a public declaration from a compiler/policy consumer. It
does not infer equivalent behavior, complete ownership, runtime acceptance or
API completeness from a symbol reference. All unreviewed rows remain pending.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from full_compiler_qualification import verify_full_compiler
from stage_xge3_replay_contracts import verify_replay_package
from check_xge3_full_surface import source_bindings
from xge_replay_contracts import CLOSE_REPLACEMENTS as XGE_CLOSE_REPLACEMENTS, REPLACED as XGE_REPLACEMENTS
from xui_document_replacements import CLOSE_REPLACEMENTS as DOCUMENT_CLOSE_REPLACEMENTS,LIFECYCLE_REPLACEMENTS,REPLACEMENTS as DOCUMENT_REPLACEMENTS
from xui_managed_contracts import CLOSE_REPLACEMENTS as XUI_CLOSE_REPLACEMENTS,REPLACED as XUI_REPLACEMENTS

# Logical source-group selectors may name an object-like header alias. Prove
# this one against an actual C relocation, rather than assuming a name suffix.
PROVIDER_ALIASES={'xl6Xge3InitPacked':('xge3_init.h','xl6Xge3InitWithCode')}
REPLACED = {**XGE_REPLACEMENTS, **XUI_REPLACEMENTS, **DOCUMENT_REPLACEMENTS, **LIFECYCLE_REPLACEMENTS}
CLOSE_REPLACEMENTS = {**XGE_CLOSE_REPLACEMENTS, **DOCUMENT_CLOSE_REPLACEMENTS, **XUI_CLOSE_REPLACEMENTS}
for selection_release in ('xuiCodeSelectionDestroy', 'xuiCodeSelectionDestroyChecked'):
    CLOSE_REPLACEMENTS[selection_release] = ('xui.CodeSelectionModel.close',
        'xl6Xui3CodeSelectionPolicy', 'destroy_code_selection', 'xuiCodeSelectionDestroyChecked')

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
RENDER_RESOURCE_CLOSES = {
    'xgeShaderFree': ('xge.Shader.close', 'xl6Xge3ShaderPolicyDestroy', 'xgeShaderFree'),
    'xgeBufferFree': ('xge.GpuBuffer.close', 'xl6Xge3GpuBufferPolicyDestroy', 'xgeBufferFree'),
    'xgeMeshFree': ('xge.Mesh.close', 'xl6Xge3MeshPolicyDestroy', 'xgeMeshFree'),
    'xgeShapeBatchFree': ('xge.ShapeBatch.close', 'xl6Xge3ShapeBatchPolicyDestroy', 'xgeShapeBatchFree'),
    'xgeSpriteBatchFree': ('xge.SpriteBatch.close', 'xl6Xge3SpriteBatchPolicyDestroy', 'xgeSpriteBatchFree'),
    'xgeShaderVariantSetFree': ('xge.ShaderVariants.close', 'xl6Xge3ShaderVariantsPolicyDestroy', 'xgeShaderVariantSetFree'),
    'xgeRenderTargetFree': ('xge.RenderTarget.close', 'xl6Xge3TargetDestroy', 'xgeRenderTargetReleaseOwnerChecked'),
    'xgeRenderTargetReleaseOwnerChecked': ('xge.RenderTarget.close', 'xl6Xge3TargetDestroy', 'xgeRenderTargetReleaseOwnerChecked'),
    'xgePassFree': ('xge.Pass.close', 'xl6Xge3PassDestroy', 'xgePassReleaseOwnerChecked'),
    'xgePassReleaseOwnerChecked': ('xge.Pass.close', 'xl6Xge3PassDestroy', 'xgePassReleaseOwnerChecked'),
}
AUDIO_RESOURCE_CLOSES = {
    'xgeAudioGroupFree': ('xge.AudioGroup.close', 'xl6Xge3AudioGroupDestroy', 'xgeAudioGroupFreeChecked'),
    'xgeAudioGroupFreeChecked': ('xge.AudioGroup.close', 'xl6Xge3AudioGroupDestroy', 'xgeAudioGroupFreeChecked'),
    'xgeSoundFree': ('xge.Sound.close', 'xl6Xge3SoundDestroy', 'xgeSoundFreeChecked'),
    'xgeSoundFreeChecked': ('xge.Sound.close', 'xl6Xge3SoundDestroy', 'xgeSoundFreeChecked'),
    'xgeMusicFree': ('xge.Music.close', 'xl6Xge3SoundDestroy', 'xgeSoundFreeChecked'),
    'xgeStreamClose': ('xge.AudioStream.close', 'xl6Xge3SoundDestroy', 'xgeSoundFreeChecked'),
}
FONT_RESOURCE_CLOSES = {
    'xgeFontFaceFree': ('xge.FontFace.close', 'xl6Xge3FontFacePolicyDestroy', 'xgeFontFaceFree'),
    'xgeFontFamilyFree': ('xge.FontFamily.close', 'xl6Xge3FontFamilyPolicyDestroy', 'xgeFontFamilyFree'),
    'xgeEmojiPackFree': ('xge.EmojiPack.close', 'xl6Xge3EmojiPackPolicyDestroy', 'xgeEmojiPackFree'),
    'xgeGlyphBitmapFree': ('xge.GlyphBitmap.close', 'xl6Xge3GlyphBitmapPolicyDestroy', 'xgeGlyphBitmapResourceFree'),
}
CODE_LEAF_CLOSES = {
    'xuiCodeAnnotationSnapshotDestroy': ('xui.CodeAnnotationSnapshot.close',
        'xl6Xui3CodeAnnotationSnapshotDestroy', 'xuiCodeAnnotationSnapshotDestroy'),
}
NATIVE_RELEASE_DELEGATES = {
    'xgeGlyphBitmapFree': ('single/xge.c', 'xgeGlyphBitmapResourceFree', 'xgeGlyphBitmapFree'),
    'xuiCodeAnnotationStoreDestroy': ('src/xui/code/xui_code_annotation.c',
        'xuiCodeAnnotationStoreDestroy', 'xuiCodeAnnotationStoreDestroyChecked'),
    'xuiCodeTokenBufferDestroy': ('src/xui/code/xui_code_token_buffer.c',
        'xuiCodeTokenBufferDestroy', 'xuiCodeTokenBufferDestroyChecked'),
    'xuiCodeSelectionDestroy': ('src/xui/code/xui_code_selection.c',
        'xuiCodeSelectionDestroy', 'xuiCodeSelectionDestroyChecked'),
    'xuiDockPanelStateFree': ('src/xui/controls/xui_dock_panel.c',
        'xuiDockPanelStateFree', 'xrtValueRelease'),
}
for leaf in ('CodeAnnotationStore', 'CodeTokenBuffer'):
    for suffix in ('Destroy', 'DestroyChecked'):
        CODE_LEAF_CLOSES['xui'+leaf+suffix] = (f'xui.{leaf}.close',
            'xl6Xui3'+leaf+'Destroy', 'xui'+leaf+'DestroyChecked')
INPUT_ACTION_CLOSES = {
    symbol: ('xge.Actions.close', 'xl6Xge3ActionsDestroy', 'xgeActionsFreeChecked')
    for symbol in ('xgeActionsFree', 'xgeActionsFreeChecked')
}
RESOURCE_CLOSE_GROUPS = {
    'renderResourceHeaderPolicies': ('xge-render-resource-header-policies', RENDER_RESOURCE_CLOSES),
    'audioResourceHeaderPolicies': ('xge-audio-resource-header-policies', AUDIO_RESOURCE_CLOSES),
    'fontResourceHeaderPolicies': ('xge-font-resource-header-policies', FONT_RESOURCE_CLOSES),
    'codeLeafHeaderPolicies': ('xui-code-leaf-header-policies', CODE_LEAF_CLOSES),
    'inputActionHeaderPolicies': ('xge-input-action-header-policies', INPUT_ACTION_CLOSES),
}
RESOURCE_CLOSES = {
    symbol: (group, logical, destroy, release)
    for group, (_, entries) in RESOURCE_CLOSE_GROUPS.items()
    for symbol, (logical, destroy, release) in entries.items()
}
HEADER_CLOSE_REPLACEMENTS.update({symbol: (logical, release)
    for symbol, (_, logical, _, release) in RESOURCE_CLOSES.items()})
CORE_POLICY_CLASSES = {
    'xge.Scene', 'xge.PlatformBackend', 'xge.GraphicsBackend', 'xge.GraphicsContext', 'xge.HostSession',
    'xge.ImageLoad', 'xge.TextureLoad', 'xge.FontLoad', 'xge.SoundLoad',
    'xge.ParticleWorld', 'xge.ParticleDefinition', 'xge.ParticleRenderer', 'xge.Resource',
}
CORE_HEADER_CLOSES = {
    'xgeSceneReleaseChecked': ('Scene', 'Scene', 'xgeSceneReleaseOwnerChecked'),
    'xgePlatformBackendReleaseChecked': ('PlatformBackend', 'PlatformBackend', 'xgePlatformBackendReleaseOwnerChecked'),
    'xgeGraphicsBackendReleaseChecked': ('GraphicsBackend', 'GraphicsBackend', 'xgeGraphicsBackendReleaseOwnerChecked'),
    'xgeGraphicsContextReleaseChecked': ('GraphicsContext', 'GraphicsContext', 'xgeGraphicsContextReleaseOwnerChecked'),
    'xgeHostSessionReleaseChecked': ('HostSession', 'HostSession', 'xgeHostSessionReleaseOwnerChecked'),
    'xgeAsyncLoadReleaseChecked': ('ImageLoad', 'AsyncLoad', 'xgeAsyncLoadReleaseOwnerChecked'),
    'xgeAsyncRequestFree': ('ImageLoad', 'AsyncLoad', 'xgeAsyncLoadReleaseOwnerChecked'),
    'xgeParticleWorldFree': ('ParticleWorld', 'ParticleWorld', 'xgeParticleWorldReleaseOwnerChecked'),
    'xgeParticleWorldFreeChecked': ('ParticleWorld', 'ParticleWorld', 'xgeParticleWorldReleaseOwnerChecked'),
    'xgeParticleRendererFree': ('ParticleRenderer', 'ParticleRenderer', 'xgeParticleRendererReleaseOwnerChecked'),
    'xgeParticleRendererFreeChecked': ('ParticleRenderer', 'ParticleRenderer', 'xgeParticleRendererReleaseOwnerChecked'),
    'xgeResourceFree': ('Resource', 'ResourcePolicy', 'xgeResourceFree'),
}
PARTICLE_WORLD_FACTORIES = {'xgeParticleWorldCreate', 'xgeParticleWorldCreateOwned'}
PARTICLE_WORLD_BRIDGE = 'xl6Xge3ParticleWorldCreateOwned'
PARTICLE_WORLD_PRODUCER = 'xgeParticleWorldCreateOwnedWithCode'
TRANSFER_PUBLISHERS = {
    'xl6Xge3RunOwned': ('xge.run', 'xgeRunOwnedWithTransfer'),
    'xl6Xge3ClipboardSetItems': ('xge.clipboardSetItems', 'xgeClipboardSetItemsWithTransfer'),
    'xl6Xui3ClipboardSetItems': ('xui.Context.setClipboardItems', 'xuiClipboardSetItemsWithTransfer'),
    'xl6Xge3DragBegin': ('xge.DataObject.startDrag', 'xgeDragBeginWithTransfer'),
}
ENGINE_OWNERS = {
    'src/init.c': ('xgeInit', 'xge.initNative', 'xl6Xge3InitWithCode', 'xgeInitWithCode'),
    'src/scene.c': ('xgeSceneCreateOwned', 'xge.createScene', 'xl6Xge3SceneCreateOwned', 'xgeSceneCreateOwnedWithCode'),
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
HEADER_CLOSE_REPLACEMENTS['xuiCodeFoldStateDestroy'] = (
    'xui.CodeFoldState.close', 'xuiCodeFoldStateDestroyChecked')
HEADER_CLOSE_REPLACEMENTS['xuiCodeCommandMapDestroy'] = (
    'xui.CodeCommandMap.close', 'xuiCodeCommandMapDestroyChecked')
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
FLOW_LIFETIME_CALLS = {
    'src/xui/workflow/xui_flow_graph_widget.c': {
        '__xuiFlowGraphWidgetTraceOwnership': (
            'xuiFlowGraphIsOwned', 'xuiFlowGraphOwnership', 'xuiFlowGraphTraceOwnedValues'),
    },
    'src/xui/workflow/xui_workflow_widget.c': {
        '__xuiWorkflowWidgetTraceSlot': (
            'xuiWorkflowIsOwned', 'xuiWorkflowOwnership', 'xuiWorkflowTraceOwnedValues'),
    },
}
SEQUENCE_WITNESSES = (
    ('xge3d.MeshGeometry3D', 'vertices', 'xge3dMeshGeometryVertices', 'xge3dMeshGeometryAddRef', 'xge3dMeshGeometryReleaseChecked'),
    ('xge3d.SkinPalette3D', 'matrices', 'xge3dSkinPaletteMatrices', 'xge3dSkinPaletteAddRef', 'xge3dSkinPaletteReleaseChecked'),
    ('xge.AudioClip', 'samples', 'xgeAudioClipGetSamples', 'xgeAudioClipAddRef', 'xgeAudioClipFree'),
    ('xge.GlyphBitmap', 'pixels', 'xgeGlyphBitmapResourceGetPixels', 'xgeGlyphBitmapResourceAddRef', 'xgeGlyphBitmapResourceFree'),
    ('xge.GlyphRun', 'glyphs', 'xgeGlyphRunGetGlyphs', 'xgeGlyphRunAddRef', 'xgeGlyphRunFree'),
    ('xge.ShapeDashSnapshot', 'pattern', 'xgeShapeDashSnapshotPattern', 'xgeShapeDashSnapshotAddRef', 'xgeShapeDashSnapshotReleaseChecked'),
    ('xge.ShapeGradientSnapshot', 'stops', 'xgeShapeGradientSnapshotStops', 'xgeShapeGradientSnapshotAddRef', 'xgeShapeGradientSnapshotReleaseChecked'),
    ('xge.ShapePathSnapshot', 'commands', 'xgeShapePathSnapshotCommands', 'xgeShapePathSnapshotAddRef', 'xgeShapePathSnapshotReleaseChecked'),
    ('xui.CodeLayoutSnapshot', 'lines', 'xuiCodeLayoutSnapshotGetLines', 'xuiCodeLayoutSnapshotAddRef', 'xuiCodeLayoutSnapshotRelease'),
    ('xui.CodeMarginLayout', 'entries', 'xuiCodeMarginLayoutGetEntries', 'xuiCodeMarginLayoutAddRef', 'xuiCodeMarginLayoutRelease'),
    ('xui.CodeTokenSnapshot', 'tokens', 'xuiCodeTokenSnapshotGetTokens', 'xuiCodeTokenSnapshotAddRef', 'xuiCodeTokenSnapshotDestroy'),
    ('xui.DocumentChangeSet', 'operations', 'xuiDocumentChangeSetGetOperations', 'xuiDocumentChangeSetAddRef', 'xuiDocumentChangeSetReleaseOwnerChecked'),
    ('xui.DocumentMarkdownLosses', 'entries', 'xuiDocumentMarkdownLossesGetEntries', 'xuiDocumentMarkdownLossesAddRef', 'xuiDocumentMarkdownLossesReleaseOwnerChecked'),
    ('xui.DocumentMatches', 'ranges', 'xuiDocumentMatchesGetRanges', 'xuiDocumentMatchesAddRef', 'xuiDocumentMatchesReleaseOwnerChecked'),
    ('xui.DocumentRangeRects', 'entries', 'xuiDocumentRangeRectsGetEntries', 'xuiDocumentRangeRectsAddRef', 'xuiDocumentRangeRectsReleaseOwnerChecked'),
    ('xui.FindMatches', 'results', 'xuiFindMatchesGet', 'xuiFindMatchesAddRef', 'xuiFindMatchesRelease'),
    ('xui.InventoryIndices', 'indices', 'xuiInventoryIndicesGet', 'xuiInventoryIndicesAddRef', 'xuiInventoryIndicesRelease'),
    ('xui.TextShape', 'clusters', 'xuiTextShapeGetClusters', 'xuiTextShapeAddRef', 'xuiTextShapeReleaseOwnerChecked'),
)
SEQUENCE_DESTROY_CALLBACKS = {
    'xl6Xge3AudioClipPolicy': 'destroy_audio_clip',
    'xl6Xui3CodeLayoutSnapshotPolicy': 'xl6Xui3CodeLayoutSnapshotDestroy',
    'xl6Xui3CodeMarginLayoutPolicy': 'xl6Xui3CodeMarginLayoutRelease',
    'xl6Xui3CodeTokenSnapshotPolicy': 'xl6Xui3CodeTokenSnapshotDestroy',
    'xl6Xui3DocumentChangeSetPolicy': 'destroy_DocumentChangeSet',
    'xl6Xui3DocumentMarkdownLossesPolicy': 'destroy_DocumentMarkdownLosses',
    'xl6Xui3DocumentMatchesPolicy': 'destroy_DocumentMatches',
    'xl6Xui3DocumentRangeRectsPolicy': 'destroy_DocumentRangeRects',
    'xl6Xui3FindMatchesPolicy': 'xl6Xui3FindMatchesDestroy',
    'xl6Xui3InventoryIndicesPolicy': 'destroy_inventory_indices',
    'xl6Xui3TextShapePolicy': 'destroy_text_shape',
}
SEQUENCE_RELEASE_ADAPTERS = {
    'xge3dMeshGeometryReleaseChecked': 'xl6Xge3dReleaseGeometry',
    'xge3dSkinPaletteReleaseChecked': 'xl6Xge3dReleaseSkinPalette',
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


def close_relocation_path(relocations,policy,destroy,release,adapter=None):
    # A policy can also reach native functions through tracing and quiescence.
    # Only its descriptor's registered consuming function proves close.
    paths=[]
    for prefix in ('.text.','.text$'):
        getter=prefix+policy+'Get';consumer=prefix+destroy
        call_tail=[prefix+adapter,release] if adapter else [release]
        if call_tail[0] not in relocations.get(consumer,()):continue
        if adapter and release not in relocations.get(call_tail[0],()):continue
        for descriptor in relocations.get(getter,()):
            if consumer in relocations.get(descriptor,()):
                paths.append((getter,descriptor,consumer,*call_tail))
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


def sequence_release_path(body_edges, generated, resident, policy, release):
    """Resolve the actual descriptor's release slot, including resident policies."""
    getter = policy+'Get'
    local_getters = body_edges & {'.text.'+getter,'.text$'+getter}
    assert len(local_getters)+int(getter in body_edges)==1, (policy, body_edges)
    relocations, unit = (generated,'generated') if local_getters else (resident,'src/runtime.c')
    destroy = SEQUENCE_DESTROY_CALLBACKS.get(policy,policy+'Destroy')
    path = close_relocation_path(relocations,policy,destroy,release,SEQUENCE_RELEASE_ADAPTERS.get(release))
    assert not local_getters or path[0] in local_getters, (policy, path)
    return unit, path


def compile_owned_sequence_consumers(view, bound, classes, native_names, cc, packages, flags,
    tools, out, run, symbols, sha, resident):
    witnesses = []
    for owner, method, producer, retain, release in SEQUENCE_WITNESSES:
        if producer not in bound:
            continue
        name = f'{owner}.{method}'
        matches = [row for row in bound[producer] if row['language']==name]
        assert len(matches)==1 and {producer,retain,release}<=native_names, name
        line = class_lines(view, matches[0])[0]
        assert '@c_owned_sequence(' in line and '@c_retain(' in line, name
        declaration = class_lines(view, classes[owner])[0]
        policy = re.search(r'@policy\("(\w+)"\)', declaration)
        assert policy and '@lifetime("owned")' in declaration, owner
        witnesses.append((owner, method, producer, retain, release, policy[1]))
    if not witnesses:
        return None

    stem = 'owned-sequence-consumers'
    source, generated, header, obj = (out/f'{stem}.{suffix}' for suffix in ('xl','c','h','o'))
    code = [f'import {module}' for module in sorted({owner.split('.')[0] for owner,*_ in witnesses})]
    for index, (owner, method, *_rest) in enumerate(witnesses):
        code.extend((f'export "audit_owned_sequence_{index}" func witness{index}({owner} owner) as int',
            f'    auto entries = owner.{method}()', '    int count = 0', '    for entry in entries',
            '        count += 1', '    end', '    return count', 'end'))
    source.write_text('\n'.join(code)+'\n', encoding='utf-8')
    env = {key:value for key,value in os.environ.items() if not key.startswith('XL6_')}
    env['XLANG_PACKAGE_PATH'] = str(packages)
    run(stem+'-emit', [cc,'cgen',source,'--emit',generated,'--emit-header',header], cwd=out, env=env)
    run(stem+'-compile', [tools['gcc'],*flags,'-O0','-fno-inline','-c',generated,'-o',obj])
    undefined = symbols(run(stem+'-undefined', [tools['nm'],'-u',obj]))
    relocations = object_relocations(run(stem+'-relocations', [tools['objdump'],'-r',obj]))
    rows = []
    for index, (owner, method, producer, retain, release, policy) in enumerate(witnesses):
        entry = f'audit_owned_sequence_{index}'
        required = {producer,retain,'xl6NativeSequenceResultReserveV1','xl6NativeSequenceResultSpanV1',
            'xl6NativeSequenceResultAdoptV1','xl6NativeSequenceResultTakeV1','xl6NativeSequenceResultEndV1'}
        bodies = [(section,body) for section in ('.text.'+entry,'.text$'+entry)
            for body in relocations.get(section, ()) if body.startswith(('.text.','.text$'))
            and required<=relocations.get(body,set())]
        assert len(bodies)==1 and {producer,retain}<=undefined, (entry, producer, retain, bodies)
        export, body = bodies[0]
        release_unit, release_path = sequence_release_path(relocations[body],relocations,resident,policy,release)
        if release_unit=='generated':
            assert release in undefined, release
        rows.append(dict(language=f'{owner}.{method}',producer=producer,retain=retain,release=release,
            entryPath=[export,body],policy=policy,releaseUnit=release_unit,releasePath=release_path))
    return dict(source=str(source), sourceSha256=sha(source), generated=str(generated), generatedSha256=sha(generated),
        object=str(obj), objectSha256=sha(obj), packages=str(packages), entries=rows)


def compile_list_selection_buffer_consumer(view, binding, cc, packages, flags, tools, out, run, symbols, sha):
    logical = 'xui.ListView.selectionBuffer'
    assert binding['language'] == logical
    declaration = class_lines(view, binding)[0]
    assert '@c_result("retained_view")' in declaration and '@c_retain(' in declaration
    stem = 'list-selection-buffer-consumer'
    source, generated, header, obj = (out/f'{stem}.{suffix}' for suffix in ('xl', 'c', 'h', 'o'))
    source.write_text('''import xui
from xui import ListSelectionBuffer

export "audit_list_selection_buffer" func witness(xui.ListView list) as c_int
    auto buffer = list.selectionBuffer()
    if buffer == null then return -1 end
    return (buffer as ListSelectionBuffer).get(0)
end
''', encoding='utf-8')
    env = {key: value for key, value in os.environ.items() if not key.startswith('XL6_')}
    env['XLANG_PACKAGE_PATH'] = str(packages)
    run(stem+'-emit', [cc, 'cgen', source, '--emit', generated, '--emit-header', header], cwd=out, env=env)
    run(stem+'-compile', [tools['gcc'], *flags, '-O0', '-fno-inline', '-c', generated, '-o', obj])
    undefined = symbols(run(stem+'-undefined', [tools['nm'], '-u', obj]))
    relocations = object_relocations(run(stem+'-relocations', [tools['objdump'], '-r', obj]))
    retain = 'xuiListSelectionBufferAddRef'
    required = {'xuiListViewGetSelectionBufferOwned', retain}
    paths = [(section, body) for section in ('.text.audit_list_selection_buffer', '.text$audit_list_selection_buffer')
        for body in relocations.get(section, ()) if body.startswith(('.text.', '.text$'))
        and required <= relocations.get(body, set())]
    assert len(paths) == 1 and required <= undefined, (logical, paths)
    return dict(language=logical, retain=retain, relocationPath=[*paths[0], retain],
        source=str(source), sourceSha256=sha(source), generated=str(generated), generatedSha256=sha(generated),
        object=str(obj), objectSha256=sha(obj))


def witness_build_inputs(build, audit, packages, gcc, sha, units, included_units=()):
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
    assert set(units).union(included_units) <= freeze['sourceInputs'].keys()
    for name, digest in freeze['sourceInputs'].items():
        if name in units or name in included_units or Path(name).suffix in ('.h','.inc'):
            path = (source/name).resolve()
            assert path.is_relative_to(source) and sha(path)==digest, name
            pins.append(path)
    return source, metadata['flags'], pins


def prove_native_lifetime(source, flags, calls, tools, out, run, sha):
    evidence = {}
    imports = next((Path(flag) for flag in flags if Path(flag).name=='xge_host_xrt_imports.h'),None)
    import_macros = set(imports.read_text(encoding='utf-8').splitlines()) if imports else set()
    slots = set((imports.parent/'xge_host_xrt_slots.inc').read_text(encoding='utf-8').splitlines()) if imports else set()
    for path, expected in calls.items():
        label = Path(path).stem+'-lifetime'
        obj = out/(label+'.o')
        run(label+'-compile', [tools['gcc'],*flags,'-O0','-fno-inline','-ffunction-sections',
            '-fdata-sections','-c',source/path,'-o',obj], cwd=source)
        relocations = object_relocations(run(label+'-relocations',[tools['objdump'],'-r',obj]))
        paths = {}
        for caller, names in expected.items():
            refs = relocations.get('.text.'+caller, set()) | relocations.get('.text$'+caller, set())
            paths[caller] = {}
            for name in names:
                direct = {'.text.'+name, '.text$'+name, name} & refs
                if direct:
                    paths[caller][name] = [min(direct)]
                    continue
                slot = 'XgeHostXrtSlot_'+name
                assert '-DXGE_XRT_HOST=1' in flags and f'XGE_HOST_XRT_SLOT({name})' in slots
                assert f'#define {name} (*{slot})' in import_macros
                if slot in refs:
                    paths[caller][name] = [slot]
                    continue
                # COFF may route the address of the import slot through a refptr section.
                refptr = '.refptr.'+slot
                assert refptr in refs and slot in relocations.get('.rdata$'+refptr,()), (caller,name,refs)
                paths[caller][name] = [refptr,slot]
        evidence[path] = dict(object=str(obj), objectSha256=sha(obj), nativeCalls=expected,
            relocationPaths=paths)
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
    p.add_argument('--flow-engine-build',type=Path,
        help='Matching hosted engine build-report.json for FlowGraph and Workflow lifetime witnesses')
    p.add_argument('--resource-engine-build',type=Path,
        help='Matching hosted engine build-report.json for native resource release witnesses')
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
    lifetime_builds = {}
    if a.document_engine_build:
        document_source, document_flags, evidence_inputs = witness_build_inputs(
            a.document_engine_build.resolve(),audit,packages,tools['gcc'],sha,{DOCUMENT_RENDERER_SOURCE, DOCUMENT_MODEL_SOURCE})
        files.extend(evidence_inputs)
    for name, build_report, calls in (
        ('tableLifetime', a.table_engine_build, TABLE_LIFETIME_CALLS),
        ('flowLifetime', a.flow_engine_build, FLOW_LIFETIME_CALLS),
    ):
        if not build_report:
            continue
        source, build_flags, evidence_inputs = witness_build_inputs(
            build_report.resolve(),audit,packages,tools['gcc'],sha,calls.keys())
        files.extend(evidence_inputs)
        lifetime_builds[name] = (source, build_flags, calls)
    if a.resource_engine_build:
        calls = {}
        for unit, caller, callee in NATIVE_RELEASE_DELEGATES.values():
            calls.setdefault(unit, {})[caller] = (callee,)
        if 'xuiLanguageGetTextAt' in functions:
            calls.setdefault('src/xui/core/xui_core.c', {})['xuiLanguageGetTextAt'] = ('xrtArrayConstGet',)
        source, build_flags, evidence_inputs = witness_build_inputs(
            a.resource_engine_build.resolve(),audit,packages,tools['gcc'],sha,calls.keys(),
            ('src/xge/text/xge_text_resources.c',))
        files.extend(evidence_inputs)
        lifetime_builds['resourceLifetime'] = (source, build_flags, calls)
    inputs={str(f):sha(f) for f in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],objects=[],providerAliases=[],
        runtimeAcceptance=False,apiCompletenessClaim=False,shippingDelivery=False,networkTests=False,graphicsTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,cwd=None,env=None):
        command=list(map(str,command));start=time.monotonic();z=subprocess.run(command,cwd=cwd,env=env,capture_output=True,timeout=180)
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
        zeroed_factory = 'xl6Xge3BufferCreateZeroedOwned'
        zeroed_evidence = None
        if zeroed_factory in bound:
            binding, = bound[zeroed_factory]
            declaration = (view/binding['source']).read_text(encoding='utf-8-sig').splitlines()[binding['line']-1]
            assert binding['language']=='xge.createGpuBuffer' and '@c_null(3,"const void*")' in declaration
            probe = out/'zeroed-gpu-buffer-alias.c'
            obj = out/'zeroed-gpu-buffer-alias.o'
            probe.write_text('#include <xge3_gpu_buffer.h>\n'
                'int create_zeroed(xge_buffer* out,int kind,int usage,int size) {\n'
                '    return xl6Xge3BufferCreateZeroedOwned(out,kind,usage,(const void*)0,size);\n}\n',
                encoding='utf8')
            run('zeroed-gpu-buffer-alias-compile',[tools['gcc'],*flags,'-c',probe,'-o',obj])
            refs = symbols(run('zeroed-gpu-buffer-alias-undefined',[tools['nm'],'-u',obj]))
            assert 'xgeBufferCreateOwned' in refs and zeroed_factory not in refs,refs
            zeroed_evidence = dict(alias=zeroed_factory, native='xgeBufferCreateOwned',
                source=str(probe), sourceSha256=sha(probe), objectSha256=sha(obj))
            report['zeroedGpuBufferAlias'] = zeroed_evidence
        policy_relocations={}
        particle_factory=None
        transfer_publishers={}
        engine_owners={}
        for number,source in enumerate(sources):
            name='unit-'+str(number);obj=out/(name+'.o')
            run(name+'-compile',[tools['gcc'],*flags,'-c',core/source,'-o',obj])
            undefined=symbols(run(name+'-undefined',[tools['nm'],'-u',obj]))
            defined=symbols(run(name+'-defined',[tools['nm'],'--defined-only','-g',obj]))
            if source=='src/runtime.c' and (CLOSE_REPLACEMENTS.keys() & functions.keys()
                or any(producer in bound for _,_,producer,_,_ in SEQUENCE_WITNESSES)):
                policy_relocations=object_relocations(run(name+'-relocations',[tools['objdump'],'-r',obj]))
            if source=='src/particle_world.c' and PARTICLE_WORLD_FACTORIES & functions.keys():
                assert PARTICLE_WORLD_BRIDGE in defined and PARTICLE_WORLD_PRODUCER in functions
                relocations=object_relocations(run(name+'-relocations',[tools['objdump'],'-r',obj]))
                targets={PARTICLE_WORLD_PRODUCER,'xl6CodeLeaseAcquireScoped','xl6CodeLeaseRelease'}
                sections=[section for section in ('.text.', '.text$')
                          if targets <= relocations.get(section+PARTICLE_WORLD_BRIDGE,set())]
                assert len(sections)==1,(source,targets,relocations)
                call_section=sections[0]+PARTICLE_WORLD_BRIDGE
                bindings=bound[PARTICLE_WORLD_BRIDGE]
                factories={row['language'] for row in bindings if '@c_code_owner(2)' in
                    (view/row['source']).read_text(encoding='utf-8-sig').splitlines()[row['line']-1]}
                assert 'xge.createParticleWorld' in factories,bindings
                particle_factory=dict(language='xge.createParticleWorld',
                    native=PARTICLE_WORLD_PRODUCER, adapter=PARTICLE_WORLD_BRIDGE, source=source,
                    relocationPath=[call_section,PARTICLE_WORLD_PRODUCER])
                if 'xge.createDefaultParticleWorld' in factories:
                    particle_factory['defaultFactory']='xge.createDefaultParticleWorld'
            if source=='src/transfer.c' and TRANSFER_PUBLISHERS.keys() & bound.keys():
                relocations=object_relocations(run(name+'-relocations',[tools['objdump'],'-r',obj]))
                for adapter in sorted(TRANSFER_PUBLISHERS.keys() & bound.keys()):
                    language,native=TRANSFER_PUBLISHERS[adapter]
                    assert adapter in defined and native in functions,(adapter,native)
                    assert any(row['language']==language for row in bound[adapter]),(adapter,language)
                    sections=[prefix+adapter for prefix in ('.text.', '.text$')
                              if native in relocations.get(prefix+adapter,())]
                    assert len(sections)==1,(adapter,native)
                    calls=reachable_relocations(relocations,sections[0])
                    assert {'xl6CodeLeaseAcquireScoped','xl6CodeLeaseRelease'} <= calls,(adapter,calls)
                    transfer_publishers[adapter]=dict(language=language,native=native,adapter=adapter,
                        source=source,relocationPath=[sections[0],native])
            if source in ENGINE_OWNERS:
                old,language,adapter,native=ENGINE_OWNERS[source]
                if adapter in bound and old in functions:
                    assert adapter in defined and native in functions,(adapter,native)
                    binding, = (row for row in bound[adapter] if row['language']==language)
                    assert '@c_code_owner(1)' in class_lines(view,binding)[0],binding
                    relocations=object_relocations(run(name+'-relocations',[tools['objdump'],'-r',obj]))
                    sections=[prefix+adapter for prefix in ('.text.', '.text$')
                              if native in relocations.get(prefix+adapter,())]
                    assert len(sections)==1,(adapter,native)
                    obligations={'xl6CodeLeaseAcquireScoped','xl6CodeLeaseRelease','xl6CodeLeaseOwnership',
                                 'xl6CodeLeaseHoldNativeV1','xl6CodeLeaseDropNativeV1'}
                    if old=='xgeInit':
                        obligations.add('xl6CodeLeaseAcquireImageV1')
                    assert obligations <= reachable_relocations(relocations,sections[0]),(adapter,obligations)
                    engine_owners[old]=dict(language=language,native=native,adapter=adapter,
                        source=source,relocationPath=[sections[0],native])
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
        for key, (label, entries) in RESOURCE_CLOSE_GROUPS.items():
            owners = {logical.rsplit('.', 1)[0] for logical, _, _ in entries.values()}
            policy_groups[key] = (label, {name: class_bindings[name] for name in sorted(owners)})
        if 'xui.CodeLayoutSnapshot' in class_bindings:
            policy_groups['codeLayoutHeaderPolicies'] = ('xui-code-layout-header-policies',
                {'xui.CodeLayoutSnapshot': class_bindings['xui.CodeLayoutSnapshot']})
        if 'xui.CodeMarginLayout' in class_bindings:
            policy_groups['codeMarginHeaderPolicies'] = ('xui-code-margin-header-policies',
                {'xui.CodeMarginLayout': class_bindings['xui.CodeMarginLayout']})
        if 'xui.CodeFoldState' in class_bindings:
            policy_groups['codeFoldStateHeaderPolicies'] = ('xui-code-fold-state-header-policies',
                {'xui.CodeFoldState': class_bindings['xui.CodeFoldState']})
        if 'xui.CodeCommandMap' in class_bindings:
            policy_groups['codeCommandMapHeaderPolicies'] = ('xui-code-command-map-header-policies',
                {'xui.CodeCommandMap': class_bindings['xui.CodeCommandMap']})
        header_relocations = {}
        for key, (name, classes) in policy_groups.items():
            evidence, relocations = compile_header_policies(name,view,classes,set(functions),
                flags,tools,out,run,symbols,sha)
            report[key] = evidence
            header_relocations[key] = relocations
            imports = {symbol for names in evidence['headerSelectors'].values() for symbol in names}
            for symbol in sorted(imports):
                consumers.setdefault(symbol,[]).append(name)
        sequence_evidence = compile_owned_sequence_consumers(view,bound,class_bindings,set(functions),cc,packages,
            flags,tools,out,run,symbols,sha,policy_relocations)
        if sequence_evidence:
            report['ownedSequenceConsumers'] = sequence_evidence
            for entry in sequence_evidence['entries']:
                label = 'owned-sequence:'+entry['language']
                for symbol in (entry['retain'],entry['release']):
                    consumers.setdefault(symbol,[]).append(label)
        if 'xuiListViewGetSelectionBufferOwned' in bound:
            binding, = bound['xuiListViewGetSelectionBufferOwned']
            selection = compile_list_selection_buffer_consumer(view, binding,
                cc, packages, flags, tools, out, run, symbols, sha)
            report['listSelectionBufferConsumer'] = selection
            consumers.setdefault(selection['retain'], []).append('retained-view:'+selection['language'])
        if a.document_engine_build:
            document = prove_document_lifetime(document_source,document_flags,view,bound,packages,tools,out,run,sha)
            report['documentLifetime'] = document
            for symbol in sorted(DOCUMENT_FONT_TAIL):
                consumers.setdefault(symbol,[]).append(DOCUMENT_RENDERER_SOURCE)
        if 'tableLifetime' in lifetime_builds:
            assert any(item['language']=='xui.TableView.getCell' for item in bound['xuiTableViewGetCellOwned'])
            assert any(item['language']=='xui.Context.tableGridNative' for item in bound['xuiTableGridCreate'])
        if 'flowLifetime' in lifetime_builds:
            assert any(item['language']=='xui.FlowGraphWidget.getGraph' for item in bound['xuiFlowGraphWidgetGetGraphOwned'])
            assert any(item['language']=='xui.WorkflowWidget.getWorkflow' for item in bound['xuiWorkflowWidgetGetWorkflowOwned'])
        for label, (source, build_flags, calls) in lifetime_builds.items():
            report[label] = prove_native_lifetime(source,build_flags,calls,tools,out,run,sha)
            for source_path, expected in calls.items():
                for names in expected.values():
                    for symbol in names:
                        entries = consumers.setdefault(symbol,[])
                        if source_path not in entries:
                            entries.append(source_path)
        for symbol,item in sorted(functions.items()):
            if item['declarationPresent']:continue
            replacement=REPLACED.get(symbol)
            if symbol=='xgeBufferCreate' and zeroed_evidence is None:
                replacement=None
            evidence=None
            if replacement:
                assert replacement[1] in functions and functions[replacement[1]]['declarationPresent'],symbol
                assert any(r['language']==replacement[0] for r in bound[replacement[1]]),replacement
                evidence=dict(language=replacement[0],native=replacement[1])
                if symbol=='xgeBufferCreate':
                    evidence['zeroedOverload']=zeroed_evidence['alias']
            elif symbol in PARTICLE_WORLD_FACTORIES:
                assert particle_factory is not None,symbol
                evidence=particle_factory
            elif symbol=='xgeClipboardSetItems' and 'xl6Xge3ClipboardSetItems' in transfer_publishers:
                evidence=transfer_publishers['xl6Xge3ClipboardSetItems']
            elif symbol=='xgeDragBegin' and 'xl6Xge3DragBegin' in transfer_publishers:
                evidence=transfer_publishers['xl6Xge3DragBegin']
            elif symbol in ('xgeRun','xgeRunOwned') and 'xl6Xge3RunOwned' in transfer_publishers:
                evidence=transfer_publishers['xl6Xge3RunOwned']
            elif symbol in engine_owners:
                evidence=engine_owners[symbol]
            elif symbol in CLOSE_REPLACEMENTS and (symbol not in NATIVE_RELEASE_DELEGATES or a.resource_engine_build):
                logical,policy,destroy,release=CLOSE_REPLACEMENTS[symbol]
                owner=class_bindings[logical.rsplit('.',1)[0]]
                require_close(class_lines(view,owner),policy,logical)
                assert release in functions and 'src/runtime.c' in consumers.get(release,()),release
                evidence=dict(language=logical,native=release,policy=policy,
                    relocationPath=close_relocation_path(policy_relocations,policy,destroy,release))
            elif symbol in HEADER_CLOSE_REPLACEMENTS and (symbol not in NATIVE_RELEASE_DELEGATES or a.resource_engine_build):
                logical,release=HEADER_CLOSE_REPLACEMENTS[symbol]
                owner=class_bindings[logical.rsplit('.',1)[0]]
                policy=re.search(r'@policy\("(\w+)"\)',class_lines(view,owner)[0])[1]
                if symbol in RESOURCE_CLOSES:
                    key, _, destroy, _ = RESOURCE_CLOSES[symbol]
                    require_close(class_lines(view,owner),policy,logical)
                elif symbol in CORE_HEADER_CLOSES:
                    key='coreHeaderPolicies'
                    destroy='xl6Xge3'+CORE_HEADER_CLOSES[symbol][1]+'Destroy'
                elif symbol=='xuiTerminalSessionDestroy':
                    key='terminalHeaderPolicies'
                    destroy='xl6Xui3TerminalSessionDestroy'
                elif symbol in ICON_REFERENCE_RELEASES:
                    key='iconHeaderPolicies'
                    destroy='xl6Xui3IconReferenceDestroy'
                elif symbol=='xuiCodeFoldStateDestroy':
                    key='codeFoldStateHeaderPolicies'
                    destroy='xl6Xui3CodeFoldStateDestroy'
                elif symbol=='xuiCodeCommandMapDestroy':
                    key='codeCommandMapHeaderPolicies'
                    destroy='xl6Xui3CodeCommandMapDestroy'
                else:
                    key='headerPolicies'
                    destroy='xl6Xge3dSceneDestroy' if policy=='xl6Xge3dScenePolicy' else policy+'Destroy'
                assert release in report[key]['headerSelectors'][policy],(logical,release)
                evidence=dict(language=logical,native=release,policy=policy,
                    relocationPath=close_relocation_path(header_relocations[key],policy,destroy,release))
            elif document and symbol=='xuiDocumentFreeBuffer':
                evidence=document['bufferReplacement']
            elif symbol=='xuiGetLanguageTextArray' and a.resource_engine_build and 'xuiLanguageGetTextAt' in bound:
                source = 'src/xui/core/xui_core.c'
                witness = report['resourceLifetime'][source]
                bindings = [row for row in bound['xuiLanguageGetTextAt'] if row['language']=='xui.Language.textAt']
                assert len(bindings)==1
                line = (view/bindings[0]['source']).read_text(encoding='utf-8-sig').splitlines()[bindings[0]['line']-1]
                assert '@c_output_record(2,"xui_language_text_view_t","borrowed")' in line
                assert '@c_output_field(1,"sText","cstring")' in line
                assert any(row['language']=='xui.Language.textCount' for row in bound['xuiLanguageGetTextCount'])
                assert {row['language'] for row in bound['xuiLanguageSetText']} == {
                    'xui.Context.setLanguageText','xui.Context.clearLanguageText'}
                evidence=dict(language='xui.Language.textAt',native='xuiLanguageGetTextAt',
                    ownership='Indexed snapshot of the pack slot, with checked mutation through Context',
                    nativeArrayWitness=dict(source=source,objectSha256=witness['objectSha256'],
                        relocationPath=witness['relocationPaths']['xuiLanguageGetTextAt']['xrtArrayConstGet']))
            elif symbol=='xuiDockPanelStateFree' and a.resource_engine_build:
                bindings = [row for row in bound['xuiDockPanelSaveState']
                            if row['language']=='xui.DockPanel.saveState']
                assert len(bindings)==1
                line = (view/bindings[0]['source']).read_text(encoding='utf-8-sig').splitlines()[bindings[0]['line']-1]
                assert '@c_output(1,0) @c_output_view(1,"owned_value")' in line
                evidence=dict(language='xui.DockPanel.saveState',native='xrtValueRelease',
                    ownership='Resident XRT value released by the language owner')
            if evidence and symbol in NATIVE_RELEASE_DELEGATES:
                unit, caller, callee = NATIVE_RELEASE_DELEGATES[symbol]
                witness = report['resourceLifetime'][unit]
                assert callee in witness['nativeCalls'][caller]
                evidence['nativeReleaseWitness'] = dict(source=unit,caller=caller,callee=callee,
                    relocationPath=witness['relocationPaths'][caller][callee],objectSha256=witness['objectSha256'])
            rows.append(dict(id=item['id'],symbol=symbol,family=item['family'],conditions=item['conditions'],
                publicDeclarationPresent=False,compiledNativeConsumers=consumers.get(symbol,[]),
                explicitReplacement=evidence,
                contractReview='pending',runtimeAcceptance='not-certified-by-this-audit'))
        verify_replay_package(packages,replay)
        report.update(items=rows,transferPublishers=transfer_publishers,
            nativeRevision=audit['nativeRevision'],functionPopulation=len(functions),
            directDeclarations=sum(r['declarationPresent'] for r in functions.values()),nonDirectFunctions=len(rows),
            compiledConsumerFunctions=sum(bool(r['compiledNativeConsumers']) for r in rows),
            explicitReplacements=sum(bool(r['explicitReplacement']) for r in rows),complete=True,
            inputsUnchanged=all(sha(f)==h for f,h in inputs.items()) and
                all(sha(view/f)==h for f,h in view_pins.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except BaseException as error:report.update(complete=True,passed=False,error=repr(error));raise
    finally:save()


if __name__=='__main__':main()
