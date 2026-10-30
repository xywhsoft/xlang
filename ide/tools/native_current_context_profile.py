"""Coherent current loader,source report,package and real include closure."""
from pathlib import Path
import hashlib
from native_index_overlay import profile as index_profile

PINS = {'lib/xrt/xrt.h': 'd8c4d3cb7da7d91c35b38cdb22be516425d3cbd2ed69ddb90b59ec5dfb5714cc',
 'release/include/xl.h': 'ede52f40d6b5ee668c3a32860f7acf9c1dae92bc6144d9cf6d6149076b447df4',
 'release/include/xl_clock.h': 'a203f88a96bf8db9a90144d628fee5db3e1bcb3168e28eb3e97b7f95bf69bb18',
 'release/include/xl_package.h': 'bdfc71e45338b1a74e0dba463ef46761c7e4922e80b0466499cc08fd9381ab1b',
 'release/include/xl_version.h': '136fb1796ca1e39bbd3924604e6f10718b331a2eec3a4a181779127df893c992',
 'release/include/xrt_decl.h': '442badfd81330e9b9cb93b12ca1d1dd7000691a018b0a77372e5553bb024906b',
 'release/include/xruntime_decl.h': '1d59ca49004a33027d773b3e5c8f479dbcfae1251a643569171f2f01e621c7dd',
 'src/common/xl6_common.h': '2d0358bd7731369448be24fddbc47ebe974ec38548e1b0a258faef3cae089d28',
 'src/common/xl6_output.h': '81881129b2dda6c5bc83595aa6223c7d9c5d380f01dfe01809a605b0cd58b24b',
 'src/common/xl6_path_identity.h': '48fd7456ad348c974da20aefa87f32862daeab664a341cfa144e2817d03c2683',
 'src/common/xl6_xrt.h': '587cab71dfcb44cdaee9629f4ff7dfec6f0449649a903f6a1d1d13d97a4665a1',
 'src/compile/xl6_build_inputs.h': 'b6efbfdc156ef862c52b5a1db12b488afc2869a8bedadb57ebda198f8198e1f6',
 'src/compile/xl6_imports.c': '97bb5423ee677d61ee8c10b0dc924c83d8238343552c0d9b590d5992a0464e18',
 'src/compile/xl6_imports.h': '2ae270ef6c1f4f22555d03c6238a4ad429cc63952ca6a2f093483d493658c2b9',
 'src/compile/xl6_source_overlay.c': '7e2e3c2335dd8e785d4f6205ee677272777c20472f9c36458c5b618cccde52c9',
 'src/compile/xl6_source_overlay.h': '5cd34f6e72cd0a848fc774dfa0e74658b280ff36bd9ad667100f7305ba8285e0',
 'src/compile/xl6_source_report.c': '5fa20cfe1b687f3fb2834bc4417e064e52e6714bce394c69a2a3a3a868d16b66',
 'src/compile/xl6_source_report.h': 'a5b27b170bc665467831a52960dce64787eec68549ff9b0c4d7012b5f5702bd9',
 'src/diag/xl6_diag.h': '1f4965ca10c15e62b18750da9330206d5c5ae2f877cb25e66d0b748af5fdc947',
 'src/lexer/xl6_token.h': 'ec15d9e2e9c69c148d2d5e2d3e1bf8042db673e961d516f7cc2a68d5e302e345',
 'src/package/xl6_compiled_interface.inc': 'f878ffd3b89df689bead5a781336e9a953d7e90eebefa43d62788d7b2fd092d3',
 'src/package/xl6_compiled_package.h': '476c0ff4c5a549bac8f757154363dd7c0ce26f39af2014e4b84d37bca57322b2',
 'src/package/xl6_package.c': 'a83408a5c617e152a01ac7af20e3b500d85d674d04182c073d5b1c885928fbf3',
 'src/package/xl6_package.h': 'f6cf49203fe68c37ff7817cd12a1c8a3fb921b2731c31367f94434ba3f5c1b9b',
 'src/parser/xl6_parser_api.h': 'bb389787227c49b2d4ae8155d6cb0aa65a5f9c20e8155271989e2b8f8e01bb10',
 'src/sem/xl6_completion.c': 'd56f80b8b3ca93e9ec4009a56ee9235a86b66674f2c2ea174a0f5db36fdef2bf',
 'src/sem/xl6_completion.h': 'cd110fcf9e8102093d83cb5d139c262ec50a89c66684d9d046089556605d3a5d',
 'src/sem/xl6_type_registry.h': '27f0415459b060075b8e18a677e9c05d7460412ef5d908a86dc10d519f30ccd1',
 'src/type/xl6_semantic_plan.h': '655fc01fc7e6a9bd3edb3c949fc3437f2f132adaf5150502c607432a227a0e31',
 'src/type/xl6_type_id.h': '662ae3b34d48ef626c5973db765d7d6007475692aa2448d82245ee74a9ffbde7'}


def profile(frontend:Path):
    audited=index_profile(frontend)
    for rel,digest in PINS.items():
        path=frontend/rel
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
            raise ValueError('current resolution/report/package cohort changed; audit lookup,ownership and failure contracts: '+rel)
        audited[str(path)]=digest
    return audited
