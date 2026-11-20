"""Start optional IR indexes after the original native topology admission.

The original initialization,cancellation,topology call and failure diagnostics
remain in their original order. No validation result is cached or bypassed.
"""
import hashlib
from check_native_bridge_catalog import function
from native_ir_derived_overlay import current_pair,current_contract
from native_c_interop_profile import pins_for
from native_final_c_interop_profile import OWNERSHIP_PINS, XGE_PINS

BODY_SHA256='c989f2e46adbdb57d629193e0be8d35e813a1d602c467af97bcb237e069b5d98'
OWNERSHIP_BODY_SHA256='0c7898bb5630f124ff6bc0082d3ff0b22ea20c5acbfcf8c4860c65888e9fdb6d'
XGE_BODY_SHA256='4a6546bc84afa01e030be53a8f3f168226090570a954075323c5beda273d7fcd'
FEATURE='XLS_NATIVE_IR_TOPOLOGY_OBSERVER'


def derive(source,frontend,source_digest,header_digest):
    pins=pins_for(frontend)
    if not current_pair(source_digest,header_digest) or not current_contract(pins):
        return source,dict(enabled=False,scope='Unreviewed cohort;original wrapper preflight retained')
    source=source.replace('\r\n','\n').replace('\r','\n')
    if '#define '+FEATURE in source:raise ValueError('Topology observer overlay already applied')
    body=function(source,'xlsIrVerifyModuleOriginal')
    expected=XGE_BODY_SHA256 if pins is XGE_PINS else OWNERSHIP_BODY_SHA256 if pins is OWNERSHIP_PINS else BODY_SHA256
    if hashlib.sha256(body.encode('utf8')).hexdigest()!=expected:
        raise ValueError('Native IR verifier changed;audit initialization and topology admission order')
    point='if (!xl6IrVerifyNodeTopology(module, diags)) return 0;'
    include='#include "analysis/frontend_ir.inc"'
    if body.count(point)!=1 or source.count(include)!=1:
        raise ValueError('Native IR topology observer seam changed')
    source=source.replace(body,body.replace(point,point+'\n    xlsIrScanVerifyTopologyReady(module);',1),1)
    source=source.replace(include,'#define '+FEATURE+' 1\n'+include,1)
    return source,dict(enabled=True,originalBodySha256=expected,scope='Original guard/topology call retained;observer starts optional scan only after success. Complete current owned-output and integer checks remain original. No result cache,new TLS object or skipped validation. Exit-plan producer keeps its preflight.')
