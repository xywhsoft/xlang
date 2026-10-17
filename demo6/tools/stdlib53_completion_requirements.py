"""Reviewed contract-to-assertion traceability for the approved 12+20+21 scope.

The approved module purposes come from the task's 53-row table, not from green
test names. Markers identify concrete reviewed assertions, NOT automatic branch
instrumentation. Every fixture must separately have same-source actual-product
JIT/module and relocated AOT evidence on both operating systems.
"""

BEHAVIORS = {}


def behavior(key, requirement, gates, *fixtures):
    BEHAVIORS[key] = dict(Requirement=requirement, Gates=gates.split(), Fixtures=[
        dict(Name=name, Assertions=list(assertions)) for name, *assertions in fixtures])


behavior('S01', 'UTC Epoch 微秒、完整宽度、负 Epoch、算术溢出与单调时钟', 'native-system',
    ('stdlib_time_core', 'assert time.toUnix(time(-1)) == -1', 'assert caught == 8', 'assert time.monotonic() >= before'))
behavior('S02', 'UTC 日历字段、闰年、半开范围与 ISO 周', 'native-system',
    ('stdlib_time_calendar', 'assert isoYear == 2020 and isoWeek == 53 and isoDay == 5', 'assert caught == 9'))
behavior('S03', '只读 DateTime、固定 offset、当地字段转换和显式 fold 策略；Windows DST 实测独立待验收', 'native-system',
    ('stdlib_time_local', 'assert local.toTime() == instant and local.microsecond == 123456', 'assert caught == 1'))
behavior('S04', '精确长度时间格式、单次 try 解析、RFC3339/HTTPDate 与拥有字段结果', 'native-system',
    ('stdlib_time_protocol', 'assert ok and parsed == value', 'assert caught == 3'),
    ('stdlib_time_fields_text', 'assert fields.toTime() == time.fromDateTime', 'assert caught == 2'))
behavior('S05', '环境变量缺失/null 与空串区分、独立副本、精确 OS 文本和失败不改旧值', 'native-system',
    ('stdlib_environment', r'assert env.get(name) == null and not env.has(name)', r'assert (saved ?? "missing") == "你-value"', 'assert invalid and (env.get(name) ?? "missing") == ""'))
behavior('S06', 'signal 原生处置、注册线程显式泵、独立事件、一次性、回调错误和路由背压', 'native-system signal-language-oom resource-identity',
    ('stdlib_signal_dispatch', 'assert counter.hits == 0', 'assert counter.hits == 1'),
    ('stdlib_signal_dispatch_lifetime', 'assert throwing.pending == 0', 'assert once.closed and not once.active'),
    ('stdlib_signal_dispatch_routes', 'assert route.pending > 0', 'assert route.dispatch() == 1'))
behavior('S07', 'ProcessCommand 拥有 OS 文本/argv/env 配置、直接 argv 与显式 shell、安全创建线程身份', 'resource-identity process-oom process-language-oom',
    ('stdlib_process_command_config', 'assert rejected == 5', 'assert options.requested'),
    ('stdlib_process_run_bytes', r'assert literalArgs.stdout == b"a b\0\0quote\"tail\0你\0$&;\0"'))
behavior('S08', '同步捕获保留精确 bytes、独立结果/状态、双流排空、限额/期限/取消与 checked 基础设施错误', 'process-oom process-language-oom',
    ('stdlib_process_run_bytes', r'assert result.stdout == b"a\0你b" and result.stderr == b"E\0R"', r'assert saved == b"a\0你b"'),
    ('stdlib_process_run_limits', 'assert complete.stdout.length == 2097152', 'assert cancelled.status.kind == 0', 'assert rejected == 3'),
    ('stdlib_process_os_alias', 'assert immediate.timedOut and not immediate.success'))
behavior('S09', 'live Process、nullable 等待状态、唯一流租约、owned Reader/Writer 与子进程退役', 'process-oom process-language-oom graphs aot-graphs',
    ('stdlib_process_live_streams', 'assert denied == 2', r'assert output.readAll() == b"a\0你b"', 'assert processHandle.closeStdin()'))
behavior('S10', '真实并发 pipeline、独立配置和 stage 快照、共享期限、逐段 stderr/状态与预取消', 'process-oom process-language-oom',
    ('stdlib_process_pipeline_bytes', 'assert result.count == 3 and result.completedCount == 3', r'assert saved == b"a\0你b"'),
    ('stdlib_process_pipeline_limits', 'assert full.stage(0).stderr.length == 262144', 'assert skipped.completedCount == 0', 'assert rejected == 6'))
behavior('S11', 'typed Future 等待、独立重复结果、取消一个等待不杀进程/不取消其他等待及 provider 寿命', 'graphs aot-graphs native-future-policy process-language-oom',
    ('stdlib_process_future', 'assert status.kind == 1 and status.code == 7', 'assert status.code == 7 and repeated.code == 7'),
    ('stdlib_process_future_cancel', 'assert caught and cancelled.isCancelled() and not cancelled.isFailed() and p.running'))
behavior('S12', '原生文件 capability 独立复制与绑定回滚；VFS 无原生句柄时拒绝、不落回磁盘', 'process-oom process-language-oom',
    ('stdlib_process_file_bytes', r'assert output.readAtExact(0, 6) == b"a\0你b"', 'assert input.close()'),
    ('stdlib_process_file_config', 'assert denied == 2', r'assert input.read(3) == b"a\0b"'))
behavior('S13', '真实 PTY/ConPTY、原始字节、stderr 合流、尺寸/就绪协议及受控资源退役', 'terminal terminal-ledger',
    ('stdlib_process_terminal_session', 'assert handle.resize(96, 32)', r'requireTerminalBytes(received, b"tty:1:1:1"', 'if status != null then assert status.code == 0 end'))
behavior('S14', '系统关联打开请求与应用完成分离、精确目标、不插入 shell；真实平台 launcher', 'process-open',
    ('stdlib_process_open_only', 'assert rejected'))

behavior('T01', '精确 Data/Size、NUL、char/uint8/bytes 结果及 scalar/byte 索引与 UTF-8 边界', 'native-text retired-text',
    ('stdlib_text_queries', 'assert string.len(text) == 7', 'assert string.byteLen(text) == 12', "assert scalar == '你'", 'assert utfCaught'))
behavior('T02', '拥有字符串编辑、ASCII 大小写、scalar 切片、选定长度、独立值修改与溢出拒绝', 'native-text retired-text',
    ('stdlib_text_values', r'assert string.repeat("a\0你", 0) == ""', r'assert second == "A\0你"'),
    ('stdlib_text_sizes', 'assert caught == 6'))
behavior('T03', 'eager 拆分空项/行/ASCII fields、typed join 与源/结果独立寿命', 'native-text',
    ('stdlib_text_split', 'assert(pieces.length == 5)', 'assert(noLines.length == 0)', 'assert(rejected)'),
    ('stdlib_text_join', 'assert(string.byteLen(joined) == 16)'))
behavior('T04', 'lazy 拆分自有输入、独立项、共享游标、EOF/close、失败可重试和创建线程', 'native-text resource-identity',
    ('stdlib_text_iterator', 'assert(it.next() == null and it.closed())', 'assert(alias.closed() and alias.next() == null)', 'assert(unwound and finalized.closed())'))
behavior('T05', '完整/窄整数和 binary64 严格解析、flags/size、完整消费、try 错误分类与有界 writer', 'native-text',
    ('stdlib_number_values', 'assert int.parse("123", 10, 0, 200) == 123', 'assert not narrowBad and narrowValue == int8(0)', 'assert caught == 6 and output.len() == before'))
behavior('T06', 'typed 数字格式、完整 u64、char/NUL、正确舍入、精度/宽度预算与事务 Buffer 追加', 'native-text',
    ('stdlib_number_format', r'assert string.format(0, "c") == "\0"', 'assert string.len(float.format(1.0, ".1000f")) == 1002', 'assert caught == 5 and output.len() == original'))
behavior('T07', '普通/raw/模板/三引号插值使用同一 typed 格式和精确长度、表达式只求值一次', 'retired-text nested',
    ('stdlib_text_interpolation', 'assert visits == 1', r'assert snapshot == "head:a\0你:tail"'),
    ('stdlib_text_literals_nested', 'assert f"{string.format(42, "04")}" == "0042"'))
behavior('T08', 'Unicode scalar glob、ASCII 忽略大小写、精确 NUL 和非法模式/flags 拒绝', 'native-text',
    ('stdlib_text_glob', r'assert string.glob("a\0b", "a?b")', 'catch ValueError'))
behavior('T09', 'scalar 编辑距离、限额与 OOM 区分、归一化相似度及有界容差', 'native-text',
    ('stdlib_text_distance', 'assert string.distance("abc", "xyz", 2) == -1', 'assert string.sim("abc", "xyz") == 0.0', 'catch RangeError'))
behavior('T10', 'Unicode scalar 合法性、char 编码、scalar/byte 偏移和严格验证偏移', 'native-text',
    ('stdlib_unicode', 'assert not unicode.isScalar(55296)', 'assert not invalid and error == 1', 'assert caught == 5'))
behavior('T11', 'UTF-8 增量验证/重置/结束、绝对错误偏移、精确流式输入与替换解码', 'native-text resource-identity',
    ('stdlib_unicode_validator', 'assert v.total == 9 and v.errorOffset == -1 and v.finished', 'assert v.errorOffset == 2 and v.finished', 'assert caught == 2'),
    ('stdlib_unicode_repair', r'assert unicode.repair("A你", 2) == "A\uFFFD"'))
behavior('T12', '五种 UTF 编码、精确 bytes/string、BOM 策略、strict/replace、宽度/配置错误和独立值', 'native-text',
    ('stdlib_encoding', r'assert utf8 == b"A\0\xE4\xBD\xA0\xF0\x9F\x98\x80"', 'assert caught == 12', 'assert utf8[0] == uint8(65)'))
behavior('T13', '编码猜测返回 encoding/BOM/confidence，unknown 不是 UTF-8，BOM 不证明正文有效', 'native-text',
    ('stdlib_encoding_detect', 'assert empty == encoding.unknown and zero == 0 and uncertain == 0', 'assert not valid and offset == 3'))
behavior('T14', '区分 HTML text 与 quoted attribute 转义，保持 NUL/UTF-8，不冒充 sanitizer', 'native-text',
    ('stdlib_html', r'assert html.escapeText("&lt;") == "&amp;lt;"', 'assert caught == 2'))
behavior('T15', '不可变编译 Regex、flags/预算、精确错误 offset/patternIndex 和闭合 ABI', 'native-text',
    ('stdlib_regex_errors', 'assert caught == 12'),
    ('stdlib_regex_error_metadata', 'assert regex.errorPatternIndex(error) == 1', 'assert caught == 3'))
behavior('T16', 'find/at/full/all、独立输入/捕获快照、空组/null 区别、字节 span 和迭代寿命', 'native-text resource-identity',
    ('stdlib_regex_match', 'assert second.startByte(2) == -1', r'assert first.input == "x你a\0b!"'),
    ('stdlib_regex_iterators', 'assert it.next() == null and it.closed', 'assert empty.next() == null'),
    ('stdlib_regex_byte_boundaries', 'assert rejected == 3'))
behavior('T17', '模板/函数替换、limit、独立拥有结果与 Buffer 事务；回调错误原样传播', 'native-text graphs aot-graphs',
    ('stdlib_regex_replace_only', r'assert Regex("").replace("你A", "_") == "_你_A_"'),
    ('stdlib_regex_replace_func_errors', 'assert target.len() == 4', 'assert caught == 4'),
    ('stdlib_regex_replace_buffer_errors', 'assert rejected and target.len() == 7'))
behavior('T18', 'lazy Regex split 的 capture、未参与/null 与空项、limit/skipEmpty、快照寿命', 'native-text',
    ('stdlib_regex_split_only', 'assert unmatched.startByte == -1', 'assert count == 2', 'assert requireText(first) == "a"'))
behavior('T19', '不可变 RegexSet、重复模式、递增命中索引、最小 first、精确 itemSize 与空集合', 'native-text',
    ('stdlib_regex_set_only', 'assert hits.length == 3', 'assert zero.count == 0', 'assert capped.pattern(0) == "A"'))
behavior('T20', 'Pattern 路由捕获、tag/key 身份、Builder 预算/版本、事务 addMany 与独立编译快照', 'native-text graphs',
    ('stdlib_pattern_builder', 'assert not other.contains(first)', 'assert rejected == 1 and b.version == before', 'assert many.count == 2'),
    ('stdlib_pattern_match', 'assert item.captureCount == 3'), ('stdlib_pattern_errors', 'assert caught == 9'))

behavior('C01', 'typed Array 构造/导入、精确 schema、值复制/对象身份、批量代数与消费/默认/索引方法', 'array-entry native-defaults worker-fault',
    ('stdlib_array_methods', 'assert caught == 3', 'assert methodCleanup == 1'),
    ('stdlib_array_algebra_language', 'assert left.length == 4 and joined.length == 4'),
    ('stdlib_array_import_language', 'assert(raw[0] == 1 and values[0] == 9)', r'assert(binary[0] == b"a\0b"'))
behavior('C02', '整数稀疏有序 List、完整键、owned 取值/遍历、方法/代数及受支持恢复', 'graphs worker-fault',
    ('stdlib_list_methods', 'assert(methods(values) == 7)', 'assert(caught == 1)'),
    ('stdlib_list_iterator', 'assert(key == -9223372036854775808'),
    ('stdlib_list_persistence_native', 'assert first.narrow[0] == -128 and first.narrow[1] == 127'))
behavior('C03', '精确 string 键 Dict、容量/代数、独立存储与共享对象、动态/typed 索引和恢复', 'graphs worker-fault',
    ('stdlib_dict_source', r'assert numbers["a\0你"] == 7 and merged["a\0你"] == 9', 'assert identityKeys.contains(numbers)'),
    ('stdlib_dict_import_leaf', 'assert(roundtrip.value["a\\0你"] == 7'))
behavior('C04', 'Set 基础值按内容、可变对象/容器按身份，集合代数、复制与受支持恢复', 'graphs worker-fault',
    ('stdlib_set_source_native', 'assert(first in objects)', 'assert(a in arrays and b in arrays)', 'assert(fromJson.small.length == 2'))
behavior('C05', 'Stack owned pop/peek/try 与 nullable 区分、精确元素域和资源/循环拥有图', 'graphs clear aot-graphs native-future-policy',
    ('stdlib_stack_try_owned', 'assert(not hasChar and scalar == null)'),
    ('stdlib_stack_nullable_native_value', 'assert(found and item != null and item == key)'),
    ('stdlib_stack_native_children', 'assert(original[1] == 23 and alias[1] == 23)'))
behavior('C06', 'Stack 批量拥有 Array 返回、顺序/范围、独立快照及失败原子发布', 'graphs worker-fault',
    ('stdlib_stack_batch_public', 'assert(refused == 4 and stack.count() == 3)', 'assert(snapshot == array<int>[3, 2]'),
    ('stdlib_stack_batch_oom', 'assert(source.count() == 0 and empty.length == 0)'))
behavior('C07', 'FixedStack 固定逻辑容量、满/空、nullable 与拥有值；复用 typed Stack 不保留 raw 存储', 'graphs',
    ('stdlib_fixed_stack', 'assert(numbers.full() and not numbers.push(9)', 'assert(hasNull and nullValue == null)', 'assert(errors == 2)'))
behavior('C08', 'OrderedMap 有序键全域、get/take 与 try/presence、边界查询、owned 快照/merge/clone', 'tree-keys graphs worker-fault',
    ('stdlib_tree_owned_queries', 'assert(presentNull and nullItem == null', 'assert(not noTake and emptyTake == null'),
    ('stdlib_tree_bytes_keys', r'assert(values.contains(b"a\0b") and not values.contains(key))', 'assert(copy.equals(values))'),
    ('stdlib_tree_c_scalar_keys', 'assert(first == low and firstValue == 3 and last == high and lastValue == 9)'))
behavior('C09', 'SlotMap 实例+代际键、失效拒绝、独立 keys/values、成对同次快照及 async/cell/frame 传递', 'native-defaults graphs',
    ('stdlib_slot_map_errors', 'assert(caught == 3'),
    ('stdlib_slot_map_snapshots_language', 'assert(keys[0] == first and keys[1] == last)', 'assert(sum == 24)'),
    ('stdlib_slot_map_loop', 'assert(slots.contains(key))'),
    ('stdlib_slot_map_task_keys', 'assert(await asyncResult == original)'))
behavior('C10', 'Buffer 容量/范围/字节操作、值复制构造、NUL 文本/bytes 出口与独立 uint8 迭代', '',
    ('stdlib_buffer_core', 'assert b.clear() and b.len() == 0 and b.capacity() == capacity', 'assert refused == 7'),
    ('stdlib_buffer_value', r'assert source.text() == "a\0你b"', 'assert rejected == 6'),
    ('stdlib_buffer_snapshot', 'assert count == 3'))
behavior('C11', 'Buffer Hex 的任意字节、flags、精确 size、严格格式和错误码', '',
    ('stdlib_buffer_hex', r'assert raw.hex() == "00ff415a"', 'assert failure.code == 902', 'assert refused == 6'))
behavior('C12', 'Buffer Base64 的 padding/URL/自定义表、精确 size、严格配置/尾位与错误码', '',
    ('stdlib_buffer_base64', r'assert raw.base64() == "AP9BWg=="', 'assert failure.code == 1002', 'assert refused == 6'))
behavior('C13', 'typed SPSC/MPSC/MPMC/Queue 别名、显式正容量/初始化、状态、batch、owned 值与真实拓扑', 'queue-stress native-defaults graphs worker-completion',
    ('stdlib_concurrent_queue_owned', 'assert(s.reset() and not s.isClosed())', 'assert(nullStatus == queueStatus.ok and none == null)', 'assert(bytesStatus == queueStatus.ok'),
    ('stdlib_concurrent_queue_initialization', 'assert(spsc.count() == 0 and mpsc.count() == 0 and mpmc.count() == 0)'),
    ('stdlib_containers_guide', 'MpmcQueue<string?> sameQueue = queue', 'assert received == queueStatus.ok and message == null'))
behavior('C14', 'WaitQueue MPMC、显式 status/拥有值、empty/timeout/closed 区分及无输出写回参数', 'queue-stress native-defaults graphs',
    ('stdlib_wait_queue_owned', 'assert timeout == queueStatus.timeout and nothing == null', 'assert present == queueStatus.ok and nullText == null'))
behavior('I01', '内部指针数组/AVL/IntMap 复用；旧 raw 栈不作为语言容器存储、不重复公开 API', '',)

MODULE_BEHAVIORS = {
    'time':'S01 S02','time_local':'S03','time_text':'S04','environment':'S05','signal':'S06',
    'process':'S07 S09','process_open':'S14','process_run':'S08','process_pipeline':'S10',
    'process_future':'S11','process_file':'S12','process_terminal':'S13',
    'number_integer':'T05','number_float':'T05','number_format':'T06','string':'T01 T02',
    'string_split':'T03 T04','string_format':'T06 T07','string_glob':'T08','unicode_distance':'T09',
    'html_escape':'T14','unicode':'T10','unicode_text':'T01 T10 T11','charset':'T12','charset_detect':'T13',
    'pattern':'T20','regex_core':'T15','regex_match':'T16','regex_replace':'T17','regex_split':'T18',
    'regex_set':'T19','regex':'T15 T16 T17 T18 T19','queue':'C13 C14','queue_spsc':'C13',
    'queue_mpsc':'C13','queue_mpmc':'C13','array':'C01','list':'C02','buffer':'C10','buffer_hex':'C11',
    'buffer_base64':'C12','ptr_array':'I01','slot_map':'C09','avl':'I01','avl_tree':'C08','int_map':'I01',
    'map':'C03','set':'C04','fixed_stack':'C07','ptr_fixed_stack':'I01','stack':'C05 C06',
    'block_stack':'I01','ptr_stack':'I01',
}

assert len(MODULE_BEHAVIORS)==53
assert all(key in BEHAVIORS for refs in MODULE_BEHAVIORS.values() for key in refs.split())
assert all(refs or key=='I01' for key, refs in ((k,v['Fixtures']) for k,v in BEHAVIORS.items()))
