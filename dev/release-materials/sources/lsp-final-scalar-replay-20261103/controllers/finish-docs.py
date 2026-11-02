from pathlib import Path
import json
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent
r=json.loads((R/'dev/release-materials/provenance/lsp-final-scalar-replay-20261103.json').read_text())
aa,ab=r['performance']['aa'],r['performance']['ab'];aaf,abf=len(aa['failedGates']),len(ab['failedGates'])
assert r['build']['status']=='passed'
passing=not aaf and not abf
p=R/'ide/docs/lsp-final-scalar-replay.md';s=p.read_text();s=s.replace('完整验收进行中','正确性通过、本轮比较门槛通过' if passing else '正确性通过、性能未准入')
s=s.replace('协议、实际源码遗漏／回退和固定性能对照仍待补齐。','56 套协议、61 条总验收命令通过；集成请求覆盖 131 个进程、168 项检查、336 项导航和 26 项补全。')
s=s.replace('当前定向验证通过：','当前验证通过：')
needle='## 后续与发布';assert s.count(needle)==1
names={'32-single-literal':'32 单文件字面量','32-single-declaration':'32 单文件声明','32-imported-literal':'32 导入字面量','32-imported-declaration':'32 导入声明','512-single-literal':'512 单文件字面量','512-single-declaration':'512 单文件声明','512-imported-literal':'512 导入字面量','512-imported-declaration':'512 导入声明'}
lines=['## 实际故障与性能对照','',
 '新增五组、14 条命令验证：正常控制、遗漏只读值比较、错误写入只读符号、遗漏标量掩码边界，以及实际匹配器拒绝后的完整原生回退。三个实际 C 错误变体均被边界夹具拒绝；回退在密集单文件／导入编辑中继续核对完整 AST、类型化事实、IR 和源／诊断报告。既有规范字符串 11 组／35 命令、几何八组／23 命令及九组原生借用遗漏也通过。','',
 f'固定 M115 基线先运行四对 A/A，再运行八对 A/B，均为 14 代、32／512 函数、单文件／导入和两类编辑。分别完成 {aa["queries"]}／{ab["queries"]} 个查询、{aa["processes"]}／{ab["processes"]} 个进程；原 64 项门槛分别失败 {aaf}／{abf} 项，不重跑或豁免。进程退出、清理及原 5／3 GiB 资源条件通过；最低可用提交额度约 {min(aa["minimumCommitGiB"],ab["minimumCommitGiB"]):.2f} GiB。','',
 '| 热请求 P50（ms） | M115 | M116 | 变化 |','| --- | ---: | ---: | ---: |']
for q in r['hotP50']:lines.append(f'| {names[q["case"]]} | {q["baselineMs"]:.4f} | {q["candidateMs"]:.4f} | {q["changePercent"]:+.2f}% |')
lines+=['','以上为本轮观察值。'+('本轮比较门槛通过，仍不能替代当前前端、真实项目、长期资源和 GUI 体验验收。' if passing else '全部失败门槛、冷／首次／尾部／CPU 样本保持；部分热请求改善不构成整体性能准入。'),'','实际协议结果中的 `lastBuildIndexCanonical` 保存每个热场景的复用、格式化调用、回退和保留量，最小／最大值见阶段报告。没有把调用次数下降直接解释为用户按键响应等比例改善。','']
s=s.replace(needle,'\n'.join(lines)+'\n'+needle,1)
old='根 `release` 程序、用户设置及交付索引保持；本阶段不据定向结果发布候选。完整测量与交付记录将在阶段报告中补全。'
new=f'根 `release` 程序、用户设置及交付索引保持；本阶段未重编或签收 GUI，候选未安装。完整证据见[阶段报告](../../dev/release-materials/provenance/lsp-final-scalar-replay-20261103.json)。{len(r["sourceSnapshots"])} 份源码／控制器／故障快照和 {len(r["reportArchives"])} 份无损原始报告保存在 `dev/release-materials/sources/lsp-final-scalar-replay-20261103/`。长期目标尚未完成。'
assert s.count(old)==1;s=s.replace(old,new);p.write_text(s,encoding='utf8',newline='\n')
case=next(q for q in r['hotP50'] if q['case']=='512-single-declaration')
status='本轮比较门槛通过，尚无当前前端／GUI／发行签收' if passing else '整体性能未准入'
paragraph=f'2026-11-03 M7a2b116：原最终语义回执增加九项只读标量的当前逐值验证，沿用原字段编码器和九项写集合；未知载荷、区域／两遍提交及原生回退保持，没有新增缓存保留量。真实局部声明编辑恢复 425 个回放，511 个未改密集函数从最终注解到索引字符串复用；格式化调用 56,255→131。179 项边界、775,407 项完整 AST／类型化事实／IR／源报告对照、新增五组实际 C 故障／回退通过；完整232单元／134原生／140私有消费者、56协议、61条验收命令及816输入／136产物一致。首轮密集矩阵触发30秒总时限，专属fixture120秒后的完整重建通过，正式门槛保持。固定A/A960查询64进程、A/B1920查询128进程完成，原64门槛分别失败{aaf}／{abf}项；512单文件声明热P50 {case["baselineMs"]:.4f}→{case["candidateMs"]:.4f}ms（{case["changePercent"]:+.2f}%），全部样本／退出／资源保留。{status}，根release／设置／交付索引保持。已消费前端输入中当前有四项源码变化，下一阶段优先对齐当前编译器，再推进最终IR局部生产和完整IDE／LSP目标。见[最终回执只读标量验证](lsp-final-scalar-replay.md)。\n\n'
p=R/'ide/docs/lsp-development.md';s=p.read_text();assert 'M7a2b116' not in s;point='# xlang LSP 长期实施计划\n\n';assert s.count(point)==1;p.write_text(s.replace(point,point+paragraph,1),encoding='utf8',newline='\n')
print('docs completed;AA failures',aaf,'AB failures',abf)
