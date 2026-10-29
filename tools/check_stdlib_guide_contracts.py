"""Check current public guide contracts and the executable container example.
This is documentation consistency only, never a substitute for actual product
execution or a claim that every prose statement/53-module requirement is proved.
"""
from pathlib import Path
import argparse,hashlib,json,re,sys


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def public_methods(source):
    return sorted({n for n in re.findall(r'\bfunc (\w+)\(',source) if not n.startswith('__')})


def declaration_surfaces(declarations,signal_declarations):
    # Read the actual embedded class declarations, not a second maintained API list.
    buffer=declarations.split('"class Buffer\\n"',1)[1].split('const char* xl6StdlibXidClassSource',1)[0]
    buffer=''.join(json.loads(s) for s in re.findall(r'"(?:\\.|[^"\\])*"',buffer))
    signal=''.join(json.loads(s) for s in re.findall(r'"(?:\\.|[^"\\])*"',signal_declarations))
    classes={name:public_methods(source) for name,source in re.findall(r'class (Signal\w+)\n(.*?)(?=class Signal|\Z)',signal,re.S)}
    functions=sorted(set(re.findall(r'\{"signal", "([^"]+)", "__xl_std_signal_',declarations)))
    assert len(public_methods(buffer))>=25 and len(functions)==15 and len(classes)==3
    return dict(Buffer=public_methods(buffer),SignalClasses=classes,SignalFunctions=functions)


def validate(body,example,string_functions,surfaces):
    samples=re.findall(r'<!-- stdlib-guide-example: stdlib_containers_guide -->\s*```xlang\n(.*?)\n```',body,re.S)
    assert len(samples)==1 and samples[0]+'\n'==example.split('\n',1)[1], 'Guide and executed source differ'
    containers=body.split('### 原生容器\n',1)[1].split('### 模板\n',1)[0]
    assert not re.search(r'\b\w+Into\s*\(',containers), 'Retired output-variable call in current guide'
    for stale in ('typed 迁移待完成','batch/FixedStack 与整体验收仍待完成','第一个参数必须是可写具名变量'):
        assert stale not in containers,stale
    for required in ('SlotMapKey','tryLowerBound','queueStatus.ok/full/closed','popBatch(maximum)',
                     '显式初始化','recvTimeout','固定逻辑容量','pushBatch 返回 bool'):
        assert required in containers,required
    process=body.split('`process` 命名空间映射 XRT subprocess 能力。',1)[1].split('`log` 命名空间',1)[0]
    assert 'timeoutMs = 0' not in process and 'timeoutMs = -1' in process
    assert '并发启动真实管道' in process and '启动失败或超时返回' not in process
    assert '`regex` 已按三组标准库方案接入默认环境' in body
    assert '## 容器实现历史' in body
    ownership=body.split('## 字符串所有权\n',1)[1].split('## 命名空间规则\n',1)[0]
    strings=body.split('`string` 命名空间以声明',1)[1].split('`time` 是绝对',1)[0]
    assert '双字段 ABI 已用于现行 XLIR' in ownership and '仍在迁移中' not in ownership
    assert '小于 `-1` 的长度参数抛 RangeError' in ownership
    assert '返回临时字符串' not in strings and '等待标准库重设计' not in strings
    for name in string_functions:assert 'string.'+name+'(' in strings,('Missing public text method',name)
    def row(name):
        matches=re.findall(r'^\| `string\.'+re.escape(name)+r'\([^`]*` \|(.*)$',strings,re.M)
        assert len(matches)==1,name
        return matches[0]
    assert 'array<char>' in row('chars') and 'xl6TextCharsV2' in row('chars')
    assert '返回连续 `bytes`' in row('bytes') and 'array<int>' not in row('bytes')
    assert 'uint8' in row('byteAt') and 'RangeError' in row('byteAt') and '越界返回' not in row('byteAt')
    assert 'char' in row('charAt') and 'RangeError' in row('charAt')
    assert 'array<string>' in row('join') and '只接收' in row('join')
    assert 'ASCII' in row('lower') and '拥有字符串' in row('lower')
    numbers=body.split('数字 `parse/tryParse` 已提供',1)[1].split('新增 CRT 实数数学',1)[0]
    assert '以上解析 API 尚待实现' not in numbers
    assert 'number_integer/number_float' in numbers and '完整消费' in numbers
    for signature in ('T.parse(text, base = 10, flags = 0, size = -1)',
                      'T.tryParse(text, base = 10, flags = 0, size = -1)',
                      'float.parse(text, flags = 8, size = -1)',
                      'float.tryParse(text, flags = 8, size = -1)'):
        assert signature in numbers,signature
    for required in ('allowPrefix = 2','allowSeparator = 4','allowSpecial = 8',
                     '整数不接受 allowSpecial','浮点不接受 allowPrefix','分配或诊断构建失败仍传播 MemoryError'):
        assert required in numbers,required
    signals=body.split('### signal：',1)[1].split('CRT 进程生命周期',1)[0]
    assert '此批不包含脚本监听' not in signals and '整体状态仍为开发中' not in signals
    for name in surfaces['SignalFunctions']:assert 'signal.'+name+'(' in signals,('Missing signal function',name)
    for cls,methods in surfaces['SignalClasses'].items():
        for name in methods:assert cls+'.'+name+'(' in signals,('Missing signal method',cls,name)
    for required in ('注册线程显式事件泵','跨线程 checked 拒绝','失败保留待交事件',
                     '1..65536','快照分配失败也不','handler 抛错会传播','0 不等待，-1 无限等待'):
        assert required in signals,required
    buffer=body.split('`Buffer` 是标准库提供的 native-backed class',1)[1].split('`encoding` 使用精确长度',1)[0]
    for name in surfaces['Buffer']:
        assert 'buf.'+name+'(' in buffer or 'Buffer.'+name+'(' in buffer,('Missing Buffer method',name)
    for stale in ('Buffer(step)','appendBytes(','迭代值为 `int`','BufferError','xrtBufferUnit','xbuffer.Buffer','xbuffer.Length','xbuffer.AllocLength'):
        assert stale not in buffer,stale
    def buffer_row(name):
        matches=re.findall(r'^\| `buf\.'+re.escape(name)+r'\([^\n]*$',buffer,re.M)
        assert len(matches)==1,name
        return matches[0]
    assert '保留容量' in buffer_row('clear') and '不释放 Buffer 对象' in buffer_row('clear')
    assert 'uint8' in buffer_row('byteAt') and 'RangeError' in buffer_row('byteAt')
    assert '保留内嵌 NUL' in buffer_row('text') and 'ValueError' in buffer_row('text')
    for required in ('非负预留容量','负数抛 RangeError','迭代值为 `uint8`','创建 O(n)',
                     '小于 -1 抛 RangeError','显式可信 C 互操作','不属于安全值 API','checked may-raise'):
        assert required in buffer,required
    aliases=body.split('`os` 命名空间提供当前进程环境',1)[1].split('`file` 命名空间映射',1)[0]
    for name in ('exec','run'):
        assert 'os.'+name+'(command, timeoutMs = -1, size = -1)' in aliases
    assert 'timeoutMs = 0' not in aliases and '`process.exitCode(command)`' in aliases
    return True


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();assert args.root.is_dir() and not args.out.exists()
    task=Path(__file__).resolve().parents[1];guide=task/'docs/标准库设计.md';fixture=task/'test/cases/stdlib_containers_guide.xl'
    declarations=task/'src/stdlib/xl6_stdlib_data.c'
    signal_declarations=task/'src/stdlib/xl6_stdlib_signal.inc'
    source=declarations.read_text(encoding='utf-8-sig')
    string_functions=sorted(set(re.findall(r'\{"string", "([^"]+)", "__xl_std_string_[^"]+",',source)))
    surfaces=declaration_surfaces(source,signal_declarations.read_text(encoding='utf-8-sig'))
    assert len(string_functions)>40
    files=[Path(__file__).resolve(),Path(sys.executable).resolve(),guide,fixture,declarations,signal_declarations]
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in files];args.out.mkdir()
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Scope=__doc__,Controls=[],Failures=[],Full53Acceptance=False)
    try:
        body=guide.read_text(encoding='utf-8-sig');example=fixture.read_text(encoding='utf-8-sig')
        assert validate(body,example,string_functions,surfaces);report['Controls'].append(dict(Name='current-guide',Passed=True))
        report['DocumentedTextMethods']=string_functions
        report['DocumentedSurfaces']=surfaces
        mutations=[('example-change',body.replace('assert ordered.set("a\\0b", null)','assert ordered.set("wrong", null)',1)),
            ('retired-output',body.replace('`popBatch(maximum)`','`popBatchInto(output, maximum)`',1)),
            ('wrong-timeout',body.replace('process.exec/success/exitCode(command, timeoutMs = -1','process.exec/success/exitCode(command, timeoutMs = 0',1)),
            ('missing-slotmap-key',body.replace('SlotMapKey','RawSlotAddress')),
            ('old-regex-scope',body.replace('`regex` 已按三组标准库方案接入默认环境','`regex` 将仅由扩展库提供')),
            ('wrong-stack-return',body.replace('pushBatch 返回 bool','pushBatch 返回 Array')),
            ('string-migration',body.replace('双字段 ABI 已用于现行 XLIR','双字段 ABI 仍在迁移中')),
            ('string-temporary',body.replace('ASCII 转小写，返回拥有字符串','ASCII 转小写，返回临时字符串')),
            ('string-chars',body.replace('返回真正的 typed `array<char>`','返回真正的 typed `array<string>`')),
            ('string-bytes',body.replace('返回连续 `bytes`，不是整数数组','返回 `array<int>`')),
            ('string-byte-bounds',body.replace('返回 `uint8`；负下标从尾部计数，越界抛 RangeError','返回 `uint8`；负下标从尾部计数，越界返回 -1')),
            ('string-join',body.replace('只接收 `array<string>`','自动转换任意 `array`')),
            ('string-missing-method',body.replace('string.fieldsIter(', 'string.unknownFieldsIter(')),
            ('string-size',body.replace('小于 `-1` 的长度参数抛 RangeError','小于 `-1` 的长度参数夹紧为 0')),
            ('number-stale-status',body.replace('以上解析 API 已通过','以上解析 API 尚待实现；已通过',1)),
            ('number-integer-signature',body.replace('T.parse(text, base = 10, flags = 0, size = -1)','T.parse(text, base = 10)',1)),
            ('number-float-default',body.replace('float.parse(text, flags = 8, size = -1)','float.parse(text, flags = 0, size = -1)',1)),
            ('number-swallowed-memory',body.replace('分配或诊断构建失败仍传播 MemoryError','分配失败返回 false',1)),
            ('signal-stale-status',body.replace('脚本监听、channel 路由','此批不包含脚本监听、channel 路由',1)),
            ('signal-missing-function',body.replace('signal.once(', 'signal.unknownOnce(')),
            ('signal-missing-method',body.replace('SignalHandle.dispatchFor(', 'SignalHandle.unknownDispatchFor(')),
            ('signal-foreign-thread',body.replace('跨线程 checked 拒绝','跨线程自动调度')),
            ('signal-route-loss',body.replace('失败保留待交事件','失败丢弃待交事件')),
            ('buffer-stale-constructor',body.replace('非负预留容量','Buffer(step) 增长步长',1)),
            ('buffer-clear',body.replace('清空并保留容量，不释放 Buffer 对象','清空并释放容量，保留 Buffer 对象',1)),
            ('buffer-text-nul',body.replace('保留内嵌 NUL；非法 UTF-8 抛 ValueError','内嵌 NUL 或非法 UTF-8 抛 BufferError',1)),
            ('buffer-iterator-type',body.replace('迭代值为 `uint8`','迭代值为 `int`',1)),
            ('buffer-missing-method',body.replace('buf.toBytes(', 'buf.unknownToBytes(')),
            ('buffer-raw-pointer',body.replace('不属于安全值 API','属于安全值 API',1)),
            ('os-exec-timeout',body.replace('os.exec(command, timeoutMs = -1','os.exec(command, timeoutMs = 0',1)),
            ('os-run-timeout',body.replace('os.run(command, timeoutMs = -1','os.run(command, timeoutMs = 0',1)),
            ('os-chain-backend',body.replace('`process.exitCode(command)`','`xrtChain`',1))]
        for name,changed in mutations:
            assert changed!=body,name
            try:validate(changed,example,string_functions,surfaces)
            except AssertionError:report['Controls'].append(dict(Name=name,Rejected=True))
            else:raise AssertionError(('Bad guide accepted',name))
        report['InputsUnchanged']=all(sha(i['Path'])==i['Sha256'] for i in inputs)
        report['Complete']=report['InputsUnchanged'] and len(report['Controls'])==len(mutations)+1
    except Exception as error:report['Failures'].append(repr(error))
    (args.out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:report[k] for k in ('Complete','Failures')},ensure_ascii=False),flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':raise SystemExit(main())
