from pathlib import Path
import json,hashlib,zipfile,shutil,subprocess
ROOT=Path('C:/Users/Administrator/.codex/worktrees/c-owned-string-bytes/x-lang');BASE=Path('D:/GIT/x-lang/build/c-owned-async-fix-20261103')
OUT=ROOT/'dev/release-materials/provenance/c-owned-async-fix-20261103';OUT.mkdir(parents=True,exist_ok=False)
SOURCE=ROOT/'dev/release-materials/sources/c-owned-async-fix-20261103'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
freeze=json.loads((BASE/'final-source-revision.json').read_text());assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip()==freeze['revision']
assert all(sha(ROOT/p)==v for p,v in freeze['files'].items())
results={}
paths={'windows-acceptance':BASE/'windows-final-acceptance/result.json','windows-independent-C':BASE/'windows-final-acceptance/independent-C/result.json','windows-matrix':BASE/'windows-final-matrix/result.json','linux-acceptance':BASE/'linux-final-export/acceptance.json','linux-independent-C':BASE/'linux-final-export/independent-C.json','linux-matrix':BASE/'linux-final-matrix/result.json'}
for name,path in paths.items():
 d=json.loads(path.read_text(encoding='utf-8'));rows=d.get('checks',d.get('rows',[]));assert d['complete']
 assert all(r['passed'] if 'passed' in r else r.get('exitCode',r.get('exit'))==0 for r in rows)
 expected=314 if name.endswith('acceptance') else 63 if name.endswith('independent-C') else 164
 assert len(rows)==expected
 shutil.copy2(path,OUT/(name+'.json'));results[name]=dict(passed=True,checks=len(rows),sha256=sha(path))
# Every immutable Windows source/runtime input and object is verified again.
win=json.loads((BASE/'windows-final-provider/build.json').read_text(encoding='utf-8-sig'))
assert win['Complete'] and win['InputsUnchanged'] and win['SourceXrt'] and len(win['Objects'])==1410
for row in win['Inputs']+win['Objects']+win['Headers']:assert sha(row['Path']).lower()==row['Sha256'].lower(),row['Path']
assert sha(win['Driver'])==win['DriverSha256'].lower()
# Copy raw receipts exactly. Their paths and hashes describe actual execution.
receipts={
 'windows-build.json':BASE/'windows-final-provider/build.json','windows-parent-build.json':BASE/'windows-final-driver/build.json',
 'windows-provider.json':BASE/'windows-final-provider/provider.json','windows-precompiled.json':BASE/'precompiled-final/manifest.json',
 'linux-build-raw.json':BASE/'linux-final-export/build.json','linux-parent-build.json':BASE/'linux-final-export/parent-build.json',
 'linux-precompiled.json':BASE/'linux-final-export/precompiled.json','linux-tcc-config.h':BASE/'linux-final-export/tcc-config.h'}
with zipfile.ZipFile(OUT/'receipts.zip','x',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for name,path in receipts.items():z.writestr(name,path.read_bytes())
linux=json.loads(receipts['linux-build-raw.json'].read_text());assert linux['Complete'] and linux['InputsUnchanged'] and len(linux['Objects'])==1409
# Correct a descriptive controller field without rewriting any executed receipt.
work='/var/tmp/xlang-async-fix-20261103-final';old='/var/tmp/xlang-result-fix-20261102-v2/native/build/linux/tcc-source'
providerCommand=['/usr/bin/gcc','-std=c11','-D_GNU_SOURCE','-O2','-fPIC','-DCONFIG_TCC_BUILTIN_VFS=1','-DLIBTCC_AS_DLL','-DCONFIG_TCCDIR="'+work+'/native/build/linux/tcc-dist"','-I'+old,'-I'+work+'/source/lib/lzma','-shared',old+'/libtcc.c',old+'/tcctools.c',old+'/tcc_builtin_vfs.c',work+'/source/lib/lzma/LzmaDec.c',work+'/tcc_builtin_resources.c','-ldl','-lm','-pthread','-o',work+'/native/release/libxlc.so']
(OUT/'linux-provider-command.json').write_text(json.dumps(dict(command=providerCommand,note='The raw receipt ProviderBuildCommand field repeats IncrementalCompileCommand due controller variable reuse. This attestation records the actual provider command from the frozen executed controller. Raw receipt bytes and all verified binaries/inputs are unchanged.'),indent=2)+'\n',encoding='utf-8')
# Preserve raw phase logs, including the independent C consumer and mutations.
logs={}
for directory in ['windows-final-acceptance','windows-final-driver','windows-final-provider','windows-final-matrix','linux-final-matrix']:
 for path in (BASE/directory).rglob('*.log'):logs[path.relative_to(BASE).as_posix()]=path
for path in BASE.glob('*final*.log'):logs[path.name]=path
with zipfile.ZipFile(OUT/'logs.zip','x',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for name,path in sorted(logs.items()):z.writestr(name,path.read_bytes())
shutil.copy2(BASE/'linux-final-export/logs.zip',OUT/'linux-logs.zip')
shutil.copy2(BASE/'linux-final-export/logs-manifest.json',OUT/'linux-logs-manifest.json')
(OUT/'logs-manifest.json').write_text(json.dumps({n:sha(p) for n,p in sorted(logs.items())},indent=2)+'\n',encoding='utf-8')
report=dict(complete=True,passed=True,date='2026-11-03',sourceRevision=freeze['revision'],baseRevision=freeze['base'],results=results,
 commits=['05633dcbf','71f427c2e','80cf36430','6238ce9f0','445eb15ed'],
 fixes=['Complete async override ancestry with cycle budget; Level33/40 dispatch','Declaration-scoped private free type parameters and constraints; nested callable site cloning','Named global/static async logical result adaptation with exact overload ranking','Explicit generic global async values with closed and forwarded free type arguments'],
 runtimeLayoutChanged=False,forwardingFutureAdded=False,shippingBinariesUpdated=False,
 build=dict(windowsUnits=1410,linuxUnits=1409,linuxReusedObjects=1408,linuxRecompiled=['src/sem/xl6_sem.c'],linuxReuseProof=linux['SourceReuseProof'],windowsSource=win['DriverSha256'],linuxSource=linux['DriverSha256']),
 costs='Compile-time private specialization and generated code/virtual-family metadata; existing captures and ordinary-result copies remain. No general zero-overhead claim.',
 sourceManifestSha256=sha(SOURCE/'manifest.json'),rawReceipts={n:sha(p) for n,p in receipts.items()},
 metadataErratum='See linux-provider-command.json; a descriptive command field is corrected separately while preserving historical receipt bytes.',
 artifacts={p.name:sha(p) for p in OUT.iterdir() if p.is_file()})
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
(OUT/'review.txt').write_text('本轮修复已通过最终源码验收。\n\n05633dcbf：完整追溯异步 override 家族，33/40 层分派正确，异常循环拒绝编译。\n71f427c2e：保留调用方自由泛型参数及约束，覆盖普通函数、实例方法、泛型类和嵌套回调。\n80cf36430：全局及静态异步工厂接入私有 worker，保留默认参数、继承和精确 owner 重载优先级。\n445eb15ed：补齐显式泛型全局异步工厂的具体类型和开放泛型转交。\n6238ce9f0 及后续提交扩展并修正了验证程序。\n\nWindows/Linux 各通过 314 项回归检查、63 项独立 C 检查、41 个程序的 164 个复现和对照阶段。\n独立契约检查即时返回异步函数体和状态机 worker；没有新增转发 Future。\nWindows 完整源码队列 1410 单元；Linux 在 1409 单元完整源码构建基础上验证复用 1408 个未变对象，重新编译 sem.c，匹配当前独立构建的 TinyCC/VFS。\n源码修订 '+freeze['revision']+'。原始结果、日志、哈希和命令见 report.json、receipts.zip、logs.zip。\nLinux 构建收据有一处描述性命令字段的记录错误，已在 linux-provider-command.json 单独更正，保留原始收据字节。\n未修改 XRT 布局；现有闭包捕获、普通结果复制和私有分派表成本仍存在。发布二进制未替换。\n',encoding='utf-8')
(OUT/'.gitattributes').write_text('*.json -text\n*.txt -text\n*.zip -text\n',encoding='utf-8')
shutil.copy2(Path(__file__),SOURCE/'collect-final.py')
m=json.loads((SOURCE/'manifest.json').read_text());m['externalFiles']={'collect-final.py':sha(SOURCE/'collect-final.py')};(SOURCE/'manifest.json').write_text(json.dumps(m,indent=2)+'\n',encoding='utf-8')
# Manifest update changes only the source-artifact index, never tested inputs.
report['sourceManifestSha256']=sha(SOURCE/'manifest.json');(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('final evidence complete',len(logs),'logs',results,flush=True)
