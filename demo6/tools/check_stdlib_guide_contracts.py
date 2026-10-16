"""Check current public guide contracts and the executable container example.
This is documentation consistency only, never a substitute for actual product
execution or a claim that every prose statement/53-module requirement is proved.
"""
from pathlib import Path
import argparse,hashlib,json,re,sys


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def validate(body,example):
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
    return True


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();assert args.root.is_dir() and not args.out.exists()
    task=Path(__file__).resolve().parents[1];guide=task/'docs/标准库设计.md';fixture=task/'test/cases/stdlib_containers_guide.xl'
    files=[Path(__file__).resolve(),Path(sys.executable).resolve(),guide,fixture]
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in files];args.out.mkdir()
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Scope=__doc__,Controls=[],Failures=[],Full53Acceptance=False)
    try:
        body=guide.read_text(encoding='utf-8-sig');example=fixture.read_text(encoding='utf-8-sig')
        assert validate(body,example);report['Controls'].append(dict(Name='current-guide',Passed=True))
        mutations=[('example-change',body.replace('assert ordered.set("a\\0b", null)','assert ordered.set("wrong", null)',1)),
            ('retired-output',body.replace('`popBatch(maximum)`','`popBatchInto(output, maximum)`',1)),
            ('wrong-timeout',body.replace('process.exec/success/exitCode(command, timeoutMs = -1','process.exec/success/exitCode(command, timeoutMs = 0',1)),
            ('missing-slotmap-key',body.replace('SlotMapKey','RawSlotAddress')),
            ('old-regex-scope',body.replace('`regex` 已按三组标准库方案接入默认环境','`regex` 将仅由扩展库提供')),
            ('wrong-stack-return',body.replace('pushBatch 返回 bool','pushBatch 返回 Array'))]
        for name,changed in mutations:
            assert changed!=body,name
            try:validate(changed,example)
            except AssertionError:report['Controls'].append(dict(Name=name,Rejected=True))
            else:raise AssertionError(('Bad guide accepted',name))
        report['InputsUnchanged']=all(sha(i['Path'])==i['Sha256'] for i in inputs)
        report['Complete']=report['InputsUnchanged'] and len(report['Controls'])==7
    except Exception as error:report['Failures'].append(repr(error))
    (args.out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:report[k] for k in ('Complete','Failures')},ensure_ascii=False),flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':raise SystemExit(main())
