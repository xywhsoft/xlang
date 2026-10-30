"""Reviewed ed78373dd provider/retirement cohort; retain earlier sets unchanged.

These include producer/backend contracts beyond actual C includes. No language
rules are copied; complete selections are checked by their consuming adapters.
"""
from pathlib import Path
import hashlib

COHORT = 'current-ed78373d'
PINS = {'lib/xrt/xrt.h': '05e8c00fb0eef0215f4d0c9291d240438cb37782df5de1c0e9ba8699b0dcd5eb', 'release/include/xrt_decl.h': 'c519d9af13dad826810c01026cec3f6353f1f0dafad9793f477bcac792d5af9d', 'src/ir/xl6_ir.c': 'a5cb13b8ea19560a592717c5799c9a2e89ebb74fe331a6f5918d1ca46e8f7b53', 'src/ir/xl6_ir.h': '2673e4889b4cd4cd68a49f0d87b9991a01959addc005136df4cd18e10997f2b4', 'src/ir/xl6_ir_lower.c': 'a607fac450f873efb18605d13c1ddcf9d4f80b4434777c6ed67de74537445187', 'src/ir/xl6_ir_xlang_bridge.c': '8fc647f77cbff891e92fd12ef8f7da531babbf439177190bb5c7af8ef679e263', 'src/runtime/xl6_native_resource_policies.inc': '6c4dd001936876357c2e19cd7ea0857b2ea7f402a72b6f48c8f19950d82e7dd1', 'src/stdlib/xl6_stdlib_log.inc': 'de646bcecc3923b9645eb3d437995028b543835ad5f60d5a219a59c4ff655d8c'}
RUNTIME_SHA256 = PINS['lib/xrt/xrt.h']
IR_DIGESTS = (PINS['src/ir/xl6_ir.c'], '10ebdce6803d96609ea7aac51e9b395b0d04a15fb8190d39b8d285295c2975d4')


def role_pins(frontend: Path, previous):
    runtime=frontend/'lib/xrt/xrt.h'
    return {**previous,**PINS} if runtime.is_file() and hashlib.sha256(runtime.read_bytes()).hexdigest()==RUNTIME_SHA256 else previous
