"""Audited current semantic include closure and matching standard-library data.

The checker/registry/AST are the already qualified current ABI; this profile
binds its real include closure and whole declarations to the committed engine.
"""
from pathlib import Path
import hashlib
from native_index_overlay import profile as index_profile

PINS = {'lib/xrt/xrt.h': 'd8c4d3cb7da7d91c35b38cdb22be516425d3cbd2ed69ddb90b59ec5dfb5714cc',
    'release/include/xl_clock.h': 'a203f88a96bf8db9a90144d628fee5db3e1bcb3168e28eb3e97b7f95bf69bb18',
    'release/include/xrt_decl.h': '442badfd81330e9b9cb93b12ca1d1dd7000691a018b0a77372e5553bb024906b',
    'release/include/xruntime_decl.h': '1d59ca49004a33027d773b3e5c8f479dbcfae1251a643569171f2f01e621c7dd',
    'src/abi/xl6_channel_receive_ops.inc': 'f0e576c8dbac05d6ad7da99a0da3b121ef2890c2c365641597193ef1a5849fbc',
    'src/abi/xl6_channel_send_ops.inc': '9536bba40ab2043c8ccf83d7c76e618b73e8b14cd6cdeb6e8a05bc091335fa03',
    'src/abi/xl6_native_collection_contracts.inc': 'a6e68f9939ca4cc5877b26e7a86ef8653043e492abc365dfc7ba54572c91d84a',
    'src/abi/xl6_native_generic.h': 'ce857ca925b907d3e6aacf5df1698c480c6c57dd749102931326a96651110359',
    'src/abi/xl6_native_generic_contracts.inc': 'cf359f59c7ad9ce8cc7b1eca645447db2fc8c4503cf9cd18f45ca4753278a4a6',
    'src/abi/xl6_native_value_contracts.inc': '6f70625df8ad8b91ec99d2f28d07b68abb9a603b72e49304092c891d2ff3a24f',
    'src/abi/xl6_net_batch_collection_contracts.inc': '56f5840956259546b70876b799859058d0894f5ba51085dee22f85b6218780a3',
    'src/abi/xl6_net_vector_collection_contracts.inc': '0a18886438ecd7b4b0cff258eaeff5c32c83604c6dd5e8aae58d3c171b440b5e',
    'src/abi/xl6_slot_map_ops.inc': 'a66d7bae7897478c8f22088462c4c515a41d4f02387cb2c9adfd97f9a8cc0e56',
    'src/abi/xl6_tcp_vector_collection_contracts.inc': '5bd959d08b83160c59d95b5835a96b2cee1ab01389fa941c7dc20b1e54e65bd0',
    'src/abi/xl6_tls_identity_collection_contracts.inc': '76f6a1b05e4eba75d3390d2797e4d4f4aae0f2a389496e69a6843cdc2f00e41a',
    'src/abi/xl6_tls_parameters_collection_contracts.inc': '87feba451c8686bb142c12a17a8b77146c02436e0b3799834e59765a4ad77f26',
    'src/abi/xl6_tls_resume_collection_contracts.inc': '765c322bcf89139e4cc70ae326c7603d44a1eac62b6b7cb9f64870d588c84c22',
    'src/abi/xl6_tls_role_context_collection_contracts.inc': '4209be67402ddaa31f780586d906db38f5b370feff27bad13f97521da886f618',
    'src/abi/xl6_tls_vector_collection_contracts.inc': '9019b52042a358ac196ee2672e7e6835dade36dfcbea0e4ba2c89dad9020cd25',
    'src/abi/xl6_tls_verifier_collection_contracts.inc': '60fc41db7872a25eab293fb66875fd181fe58de924e61953a0bcf932ad1b7d40',
    'src/abi/xl6_tree_query_ops.inc': '9f90fb2906f0f52145cab38450a9f8ea805a072f4d338fef4b52b31448c92ba0',
    'src/abi/xl6_udp_collection_contracts.inc': '9f9d27a478307caaafbd61c0acb318fae3efdd03ea01d6de7e31fccd535b6441',
    'src/abi/xl6_wait_queue_receive_ops.inc': '092d535e79b322552e1fbedfcf5f84195d377747026cda01da68287ec05dd6a6',
    'src/abi/xl6_x509_path_collection_contracts.inc': '9f1117585d3c8d569d4211fd2af29c80b26ab1dfc74f7b36f60d1a608fafd6ee',
    'src/common/xl6_common.h': '2d0358bd7731369448be24fddbc47ebe974ec38548e1b0a258faef3cae089d28',
    'src/common/xl6_output.h': '81881129b2dda6c5bc83595aa6223c7d9c5d380f01dfe01809a605b0cd58b24b',
    'src/common/xl6_xrt.h': '587cab71dfcb44cdaee9629f4ff7dfec6f0449649a903f6a1d1d13d97a4665a1',
    'src/compile/xl6_source_report.h': 'a5b27b170bc665467831a52960dce64787eec68549ff9b0c4d7012b5f5702bd9',
    'src/diag/xl6_diag.h': '1f4965ca10c15e62b18750da9330206d5c5ae2f877cb25e66d0b748af5fdc947',
    'src/lexer/xl6_token.h': 'ec15d9e2e9c69c148d2d5e2d3e1bf8042db673e961d516f7cc2a68d5e302e345',
    'src/sem/xl6_completion.h': 'cd110fcf9e8102093d83cb5d139c262ec50a89c66684d9d046089556605d3a5d',
    'src/sem/xl6_protocol.h': '25d9c20ec61f4255fa95b8e199902aef026d9ee04c1819762294b05591a215ae',
    'src/sem/xl6_sem.c': '407ca4e51cbc7889b535cbe591f7f823f59131cd179aa285073c034c6796e0b0',
    'src/sem/xl6_sem.h': 'ba993a3b092f5c48bb368dc06f31eb24029563ba3bee48fffef1d05cbfa9272d',
    'src/sem/xl6_sem_c_owned.c': 'd1dd260e6770a1091fc7ee22938bc92fcdde36e086a76bc886b443ee2e817adf',
    'src/sem/xl6_sem_constructor.inc': '4324e6be688eedf51dada4c8ed09a1965217237b8ddf41971f940752aa5ad8e8',
    'src/sem/xl6_sem_docs.h': 'd5fd57ba4a3175df308a8c7aa4ed6b3bc08219a47c6ac0d588ab602338c2731a',
    'src/sem/xl6_sem_field_attrs.h': 'ac1cd72c734b22c1347518ef133090321a5a48957c0c42a98755b324d8919591',
    'src/sem/xl6_sem_special_collection.c': '5b5433565e190455fee63e7a7c579106c7cf1501cf1f9a0e4cd029156749fc42',
    'src/sem/xl6_type_registry.h': '27f0415459b060075b8e18a677e9c05d7460412ef5d908a86dc10d519f30ccd1',
    'src/stdlib/xl6_stdlib.c': 'e45f88b2e1dba9bad5cf4caab928eccf6d04cf0dcc9c7b694141609cc7e0ef10',
    'src/stdlib/xl6_stdlib.h': 'daff3b156a46f48b19a0e279e54b065ca12ae9abd52f30d3e36c49cadf3c8484',
    'src/stdlib/xl6_stdlib_data.c': 'e64459b1515b703c21b2890b8707e425dedd9fe8f659db40846d17a4605efb81',
    'src/stdlib/xl6_stdlib_data.h': '7d4018e94a594aeeaa117cf41f7576806cbe9337c85ec7a0adf1f82c1f81c6e7',
    'src/stdlib/xl6_stdlib_func_index.inc': 'b3823628cf3e577d28b4b69f518b54f156439885aa8b5196adcd58328f4a06c8',
    'src/type/xl6_c_owned_type.h': 'e6c0bbf48b39718d9c989bf9f2f83b28359704eba1df80f4a10829d33679c92b',
    'src/type/xl6_call_rank.h': '7606776e31446d5f01389e721aff9f811acd9d2aca29e412892c75d29e0658f2',
    'src/type/xl6_constructor_flow.h': 'f25eb6e89f4d9c2d32700c818428ff4a9eb5bcbc3307d54c753de398e53a1da8',
    'src/type/xl6_semantic_plan.h': '655fc01fc7e6a9bd3edb3c949fc3437f2f132adaf5150502c607432a227a0e31',
    'src/type/xl6_type_id.h': '662ae3b34d48ef626c5973db765d7d6007475692aa2448d82245ee74a9ffbde7',
    'src/type/xl6_type_model.h': '4b1e9c0302a3df75fbe32956819c3e8148555bf2393276a0d5f812b3d904e612'}


# Standard declarations and their actual native bridge/resource contracts.
PINS.update({'src/ir/xl6_ir_direct_bridge.c': '351405d1bb667aa889a02725693fa1756af2d18c9244277260369762b5fdd574', 'src/runtime/xl6_native_resource_policies.inc': 'f09e5bb181829f4cf6aeaab2b1ff5750aa5f457fa7a79df72d0004bb9dbbcaea'})


def profile(frontend:Path):
    audited=index_profile(frontend)
    for rel,digest in PINS.items():
        path=frontend/rel
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
            raise ValueError("current semantic include/declaration cohort changed; audit read/write/registry contracts: "+rel)
        audited[str(path)]=digest
    return audited
