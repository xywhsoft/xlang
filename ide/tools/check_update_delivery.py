#!/usr/bin/env python3
"""Prepare the actual release, bootstrap from one EXE, and start without user config/packages."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'ide/tools/update'))
import update_manager as manager
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--version',type=int,default=1);args=parser.parse_args()
    source=ROOT/'release/win_x64';store=ROOT/'ide/build/updates/win_x64'
    previous=manager.current(store) if (store/'.state/current.json').exists() else 0
    if args.version<=previous:raise ValueError('keep the existing store and use a new, increasing version')
    notes='新增独立 TUI 更新器\n支持签名校验、安装修复和中断恢复\nIDE 增加检查更新与修复入口\n发布工具支持 SSH 自动推送'
    result=manager.main(['prepare' if previous else 'init','--source',str(source),'--store',str(store),'--platform','win_x64','--version',str(args.version),'--notes',notes])
    if result:raise RuntimeError('release preparation failed')
    manifest=manager.payload(store/'update.json')
    for f in manifest['files']:
        if f['path'].startswith(('extlibs/','projects/','examples/','mdo-home/')) or f['path']=='option/settings.json':raise ValueError('protected path in release')
    evidence=ROOT/'ide/build/Release/update'/('fresh-bootstrap-'+time.strftime('%Y%m%d-%H%M%S'));evidence.mkdir()
    install=evidence/'仅更新器 安装';install.mkdir();shutil.copy2(source/'update.exe',install/'update.exe')
    report={'store':str(store.relative_to(ROOT)).replace('\\','/'),'version':args.version,'files':len(manifest['files']),'objects':len(manager.object_specs(manifest)),'checks':[]}
    def run(name,command,**kwargs):
        r=subprocess.run(list(map(str,command)),capture_output=True,timeout=180,**kwargs);(evidence/(name+'.log')).write_bytes(r.stdout+r.stderr)
        if r.returncode:raise RuntimeError(name+' failed: '+(r.stdout+r.stderr).decode(errors='replace')[-2500:])
        report['checks'].append(name);return r
    run('bootstrap',[install/'update.exe','--offline',store,'--no-launch','--plain'])
    assert not (install/'extlibs').exists() and not (install/'option/settings.json').exists()
    run('same-version-check',[install/'update.exe','--check','--offline',store,'--no-launch','--plain'])
    settings=install/'option/settings.json';picture=evidence/'fresh-workbench.png'
    gui=[install/'xlang_ide.exe','--hidden','--frames','4','--no-session','--no-auto-diagnostics','--settings',settings,'--screenshot',picture]
    run('gui-no-config-no-extlibs',gui)
    settings.write_bytes(b'{broken-user-config');run('gui-corrupt-config',gui)
    assert settings.read_bytes()==b'{broken-user-config'
    c=evidence/'hello.c';c.write_text('#include <stdio.h>\nint main(void){puts("C bootstrap OK");return 0;}\n',encoding='utf-8')
    xl=evidence/'hello.xl';xl.write_text('func main(...) as int\n    println("xlang bootstrap OK")\n    return 0\nend\n',encoding='utf-8')
    env=dict(os.environ,PATH=os.path.join(os.environ['SystemRoot'],'System32'),TEMP=str(evidence),TMP=str(evidence))
    run('c-run-no-external-toolchain',[install/'xl.exe','run',c],cwd=evidence,env=env)
    run('xlang-run-no-extlibs',[install/'xl.exe','run',xl],cwd=evidence,env=env)
    report['mainBytes']=sum(p.stat().st_size for p in (store/'update').iterdir() if p.is_file())
    report['hardlinkedObjects']=sum(p.stat().st_nlink>=2 for p in (store/'update').iterdir() if p.is_file())
    report['status']='passed';(evidence/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    assert install.resolve().is_relative_to(evidence.resolve()) and install!=evidence
    shutil.rmtree(install)
    print(json.dumps({'status':'passed','evidence':str(evidence),'files':report['files'],'objects':report['objects'],'checks':report['checks']},ensure_ascii=False))
if __name__=='__main__':main()
