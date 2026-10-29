"""Approved module roles and independent positive/negative pruning contracts.
Typed equivalents intentionally do not require the old pointer/raw modules.
The executable entry's common error/value dependencies are not optional APIs.
"""

PROFILES = {}


def profile(name, fixture, resident, module, family):
    PROFILES[name] = dict(Fixture=fixture, RequiredSources=resident.split(),
        RequiredFeatures=[module.replace('_MODULE_', '_FEATURE_')] if module else [],
        SymbolModule=module, Family=family)


profile('baseline','xlir_artifact_linkage_literal','','','baseline')
profile('time','stdlib_time_core_only','xl6_time_core.c','XRT_MODULE_TIME','system')
profile('local','stdlib_time_local','xl6_time_local.c xl6_time_datetime.c','XRT_MODULE_TIME_LOCAL','system')
profile('time-text','stdlib_time_text_only','xl6_time_text.c','XRT_MODULE_TIME_TEXT','system')
profile('environment','stdlib_environment_only','xl6_environment.c','XRT_MODULE_ENVIRONMENT','system')
profile('signal','stdlib_signal_subscription_only','xl6_signal_core.c xl6_signal_queue.c','XRT_MODULE_SIGNAL','system')
profile('process','stdlib_process_live_streams','xl6_process_live.c xl6_process_stream.c','XRT_MODULE_PROCESS','process')
profile('open','stdlib_process_open_only','xl6_process_open.c','XRT_MODULE_PROCESS_OPEN','process')
profile('run','stdlib_process_run_bytes','xl6_process_run.c','XRT_MODULE_PROCESS_RUN','process')
profile('pipeline','stdlib_process_pipeline_bytes','xl6_process_pipeline.c','XRT_MODULE_PROCESS_PIPELINE','process')
profile('process-future','stdlib_process_future','xl6_process_future.c','XRT_MODULE_PROCESS_FUTURE','process')
profile('process-file','stdlib_process_file_config','xl6_process_file.c','XRT_MODULE_PROCESS_FILE','process')
profile('terminal','stdlib_process_terminal_session','xl6_process_terminal_live.c','XRT_MODULE_PROCESS_TERMINAL','process')
profile('integer','stdlib_number_integer_only','xl6_number_integer.c','XRT_MODULE_NUMBER_INTEGER','text')
profile('float','stdlib_number_float_only','xl6_number_float.c','XRT_MODULE_NUMBER_FLOAT','text')
profile('number-format','stdlib_number_format_only','xl6_number_format.c','XRT_MODULE_NUMBER_FORMAT','text')
profile('string','stdlib_text_queries','xl6_text_query.c','XRT_MODULE_STRING','text')
profile('split','stdlib_text_split_only','xl6_text_split.c','XRT_MODULE_STRING_SPLIT','text')
profile('format','stdlib_text_interpolation','xl6_number_format.c','XRT_MODULE_NUMBER_FORMAT','text')
profile('glob','stdlib_text_glob','xl6_text_glob.c','XRT_MODULE_STRING_GLOB','text')
profile('distance','stdlib_text_distance','xl6_text_distance.c','XRT_MODULE_UNICODE_DISTANCE','text')
profile('html','stdlib_html_only','xl6_html.c','XRT_MODULE_HTML_ESCAPE','text')
profile('unicode','stdlib_unicode_only','xl6_unicode.c','XRT_MODULE_UNICODE','text')
profile('unicode-text','stdlib_text_chars_only','xl6_text_chars.c','XRT_MODULE_UNICODE_TEXT','text')
profile('unicode-validator','stdlib_unicode_validator','xl6_unicode_validator.c','XRT_MODULE_UNICODE','text')
profile('charset','stdlib_encoding_transcode_only','xl6_encoding.c','XRT_MODULE_CHARSET','text')
profile('charset-detect','stdlib_encoding_detect_only','xl6_encoding_detect.c','XRT_MODULE_CHARSET_DETECT','text')
profile('pattern','stdlib_pattern_builder','xl6_pattern_builder.c xl6_pattern_core.c','XRT_MODULE_PATTERN','pattern')
profile('regex-core','stdlib_regex_core_only','xl6_regex_core.c','XRT_MODULE_REGEX_CORE','regex')
profile('regex-match','stdlib_regex_match','xl6_regex_match.c','XRT_MODULE_REGEX_MATCH','regex')
profile('regex-replace','stdlib_regex_replace_only','xl6_regex_replace.c','XRT_MODULE_REGEX_REPLACE','regex')
profile('regex-split','stdlib_regex_split_only','xl6_regex_split.c','XRT_MODULE_REGEX_SPLIT','regex')
profile('regex-set','stdlib_regex_set_only','xl6_regex_set.c','XRT_MODULE_REGEX_SET','regex')
profile('spsc','stdlib_concurrent_queue_spsc_only','xl6_queue_spsc.c','XRUNTIME_MODULE_TYPED_QUEUE_SPSC','container')
profile('mpsc','stdlib_concurrent_queue_mpsc_only','xl6_queue_mpsc.c','XRUNTIME_MODULE_TYPED_QUEUE_MPSC','container')
profile('mpmc','stdlib_concurrent_queue_mpmc_only','xl6_queue_mpmc.c','XRUNTIME_MODULE_TYPED_QUEUE_MPMC','container')
profile('wait','stdlib_wait_queue_owned','xl6_wait_queue.c','XRUNTIME_MODULE_TYPED_QUEUE_MPMC','container')
profile('array','stdlib_array_create_language','xl6_array.c','XRUNTIME_MODULE_TYPED_ARRAY','container')
profile('list','stdlib_list_methods','xl6_list.c','XRUNTIME_MODULE_TYPED_LIST','container')
profile('buffer','stdlib_buffer_core_only','xl6_buffer_core.c','XRT_MODULE_BUFFER','container')
profile('hex','stdlib_buffer_hex','xl6_buffer_hex.c','XRT_MODULE_BUFFER_HEX','container')
profile('base64','stdlib_buffer_base64','xl6_buffer_base64.c','XRT_MODULE_BUFFER_BASE64','container')
profile('slot-map','stdlib_slot_map_default','xl6_slot_map.c','XRT_MODULE_SLOT_MAP','container')
profile('tree','stdlib_tree_char_keys','xl6_tree.c','XRUNTIME_MODULE_TYPED_TREE','container')
profile('dict','stdlib_dict_direct_slot_leaf','xl6_dict.c','XRUNTIME_MODULE_TYPED_DICT','container')
profile('set','stdlib_set_source_leaf','xl6_set.c','XRUNTIME_MODULE_TYPED_SET','container')
profile('fixed-stack','stdlib_fixed_stack_only','xl6_stack.c','XRUNTIME_MODULE_TYPED_STACK','container')
profile('stack','stdlib_stack_batch_public','xl6_stack.c','XRUNTIME_MODULE_TYPED_STACK','container')

MODULES = {
    'time':('public','time'), 'time_local':('public','local'), 'time_text':('public','time-text'),
    'environment':('public','environment'), 'signal':('public','signal'), 'process':('public','process'),
    'process_open':('public','open'), 'process_run':('public','run'), 'process_pipeline':('public','pipeline'),
    'process_future':('public','process-future'), 'process_file':('public','process-file'), 'process_terminal':('public','terminal'),
    'number_integer':('public','integer'), 'number_float':('public','float'), 'number_format':('public','number-format'),
    'string':('public','string'), 'string_split':('public','split'), 'string_format':('typed-format','format'),
    'string_glob':('public','glob'), 'unicode_distance':('public','distance'), 'html_escape':('public','html'),
    'unicode':('public','unicode'), 'unicode_text':('public','unicode-text unicode-validator charset'), 'charset':('public','charset'),
    'charset_detect':('public','charset-detect'), 'pattern':('public','pattern'),
    'regex_core':('public','regex-core'), 'regex_match':('public','regex-match'), 'regex_replace':('public','regex-replace'),
    'regex_split':('public','regex-split'), 'regex_set':('public','regex-set'),
    'regex':('aggregate','regex-core regex-match regex-replace regex-split regex-set'),
    'queue':('typed-equivalent','mpmc'), 'queue_spsc':('typed-equivalent','spsc'),
    'queue_mpsc':('typed-equivalent','mpsc'), 'queue_mpmc':('typed-equivalent','mpmc'),
    'array':('typed-equivalent','array'), 'list':('typed-equivalent','list'), 'buffer':('public','buffer'),
    'buffer_hex':('public','hex'), 'buffer_base64':('public','base64'),
    'ptr_array':('internal','baseline'), 'slot_map':('public','slot-map'), 'avl':('internal','baseline'),
    'avl_tree':('typed-equivalent','tree'), 'int_map':('internal','list'), 'map':('typed-equivalent','dict'),
    'set':('typed-equivalent','set'), 'fixed_stack':('typed-equivalent','fixed-stack'),
    'ptr_fixed_stack':('excluded','fixed-stack'), 'stack':('typed-equivalent','stack'),
    'block_stack':('excluded','stack'), 'ptr_stack':('excluded','stack'),
}

INTERNAL = {'PTR_ARRAY':'xrtPtrArrayInit', 'AVL':'xrtAVLInit', 'AVL_TREE':'xrtAVLTreeInit', 'INT_MAP':'xrtIntMapInit'}
RAW_STACKS = {'STACK':'xrtStack', 'FIXED_STACK':'xrtFixedStack', 'PTR_STACK':'xrtPtrStack',
              'PTR_FIXED_STACK':'xrtPtrFixedStack', 'BLOCK_STACK':'xrtBlockStack'}


def check_features(name, features):
    contract=PROFILES[name]
    assert set(contract['RequiredFeatures'])<=features,(name,'missing required feature')
    if name in ('baseline','array','list','tree','stack','fixed-stack'):
        assert not any('XRT_FEATURE_'+n in features for n in RAW_STACKS),(name,'legacy raw stack selected for typed storage')
    family=contract['Family']
    if family not in ('regex',):
        assert not any(f.startswith('XRT_FEATURE_REGEX') for f in features),(name,'unrelated regex selected')
    if family!='pattern':assert 'XRT_FEATURE_PATTERN' not in features,(name,'unrelated pattern selected')
    if family!='process':assert not any(f.startswith('XRT_FEATURE_PROCESS') for f in features),(name,'unrelated process selected')
    if name=='regex-core':
        assert not any('XRT_FEATURE_REGEX_'+n in features for n in ('MATCH','REPLACE','SPLIT','SET')),'core retained other regex modules'
    return True


assert len(MODULES)==53
assert all(p in PROFILES for _,ids in MODULES.values() for p in ids.split())
