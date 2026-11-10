"""Inline native classifier admission, retaining the complete original rules.

Only the audited current IR cohort is eligible. Copy output initialization and
early rejection verbatim; admitted nodes still use the unchanged native body.
Reference scopes select that body directly, including in nested build phases.
"""
import hashlib
from check_native_bridge_catalog import function
from native_ir_derived_overlay import current_pair, current_contract
from native_c_interop_profile import pins_for


SPECS = (
    ('xl6IrClassifyLoopAbi',
     '7b823681b882acef030f75a78bd7e48837f8d130278caabd64c7f7d49a5c47dc',
     '    if ( errorAbi != NULL )', '    targets = xl6IrNodeGet', '',
     'module,node,errorAbi,keyAbi,itemAbi,resourceAbi,resourceType,'
     'protocolIterTarget,protocolNextTarget,protocolIterableReceiverType,'
     'protocolIteratorReceiverType,protocolIteratorType,protocolNextResultType,'
     'protocolItemType,generatorItemType'),
    ('xl6IrClassifyImplicitNameAbi',
     '917ec574589afe3852f15dea537091e783e8d476157fd1a8fb616129c7dfa4dd',
     '    if ( receiverTypeOut != NULL )', '    callableId =',
     '    const XL6_IrNode* name = xl6IrNodeGet(module,id);\n',
     'module,id,receiverTypeOut,fieldDeclarationOut'),
    ('xl6IrClassifyMemberAbi',
     'e7e68109061173a54a24ae8ab12219e3caa1645bc054e1221f78400e1e12d2d1',
     '    if ( module == NULL', '    receiver = xl6IrNodeGet', '',
     'module,member'),
)


def derive(source, frontend, source_digest, header_digest):
    if not current_pair(source_digest,header_digest) or not current_contract(pins_for(frontend)):
        return source, dict(enabled=False, scope='Unreviewed cohort; native classifiers retained')
    bodies = [(spec, function(source, spec[0])) for spec in SPECS]
    for (name, digest, *_), body in bodies:
        if hashlib.sha256(body.encode('utf8')).hexdigest() != digest:
            raise ValueError('Native classifier changed; audit admission and output defaults: ' + name)
    if '#define XLS_NATIVE_IR_CLASSIFIER_PREFIXES' in source:
        raise ValueError('Classifier overlay already applied')
    for (name, _, begin, end, locals_, arguments), body in bodies:
        brace = body.index('{')
        signature = body[:brace].strip()
        start = body.index(begin, brace)
        finish = body.index(end, start)
        prefix = body[start:finish]
        original = 'xlsClassifyOriginal_' + name
        renamed = signature.replace(name, original, 1)
        inline = signature.replace('static ', 'static XLS_CLASSIFY_INLINE ', 1)
        # In the pinned native implicit classifier every non-default output
        # and ABI update is inside classLayouts iteration. All earlier getters
        # and ancestor tests are read-only and have no diagnostics. An empty
        # table therefore returns exactly the already initialized defaults.
        empty = ('    if(!module->classLayoutCount)return XL6_IR_IMPLICIT_NAME_NONE;\n'
                 if name == 'xl6IrClassifyImplicitNameAbi' else '')
        wrapper = (inline + '\n{\n'
                   '    if(xlsActiveIrScan && xlsActiveIrScan->originalClassifiers)\n'
                   '        return ' + original + '(' + arguments + ');\n' + locals_ + prefix + empty +
                   '    return ' + original + '(' + arguments + ');\n}\n')
        source = source.replace(body, signature + ';\n' + renamed + '\n' + body[brace:] + '\n' + wrapper, 1)
    source = '''#define XLS_NATIVE_IR_CLASSIFIER_PREFIXES 1
#if defined(__GNUC__) || defined(__clang__)
#define XLS_CLASSIFY_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define XLS_CLASSIFY_INLINE __forceinline
#else
#define XLS_CLASSIFY_INLINE inline
#endif
''' + source
    return source, dict(enabled=True, bodies={s[0]: s[1] for s in SPECS},
                        scope='Native output defaults/admission copied verbatim; unchanged heavy bodies; empty class table shortcut; original reference scope; no result cache or new TLS object')
