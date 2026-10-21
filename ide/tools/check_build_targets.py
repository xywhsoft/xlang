"""Real GUI-entry build/run acceptance for source, project and workspace inputs."""
import argparse
from pathlib import Path
import json
import subprocess

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--ide',type=Path,required=True)
    parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    c=root/'单文件 C'/'main.c';xl=root/'单文件 xlang'/'main.xl'
    project=root/'独立工程'/'app.xlp';workspace=root/'工作空间'/'scene.xlw'
    for p in (c,xl,project,workspace):p.parent.mkdir(parents=True)
    c.write_text('#include <stdio.h>\nint main(void) { puts("DIRECT_C_OK"); return 0; }\n',encoding='utf8')
    body='func main(...) as int\n print("DIRECT_XLANG_OK")\n return 0\nend\n'
    xl.write_text(body,encoding='utf8');(project.parent/'main.xl').write_text(body,encoding='utf8')
    project.write_text(json.dumps(dict(format='xlang-project-1',kind='exe',entry='main.xl',ide=dict(
        format='xlang-ide-project-1',name='独立工程',configurations=[dict(id='dev',name='开发',output='bin/app.exe')]))),encoding='utf8')
    workspace.write_text(json.dumps(dict(format='xlang-workspace-1',name='工作空间',projects=[dict(id='app',path=str(project))],startupProject='app')),encoding='utf8')
    cases=[('c',[str(c)],'run','main.c',False),('xlang',[str(xl)],'run','main.xl',False),('project',[str(project)],'run','独立工程',False),
           ('workspace',[str(workspace)],'run','独立工程',False),('explicit-file',[str(workspace),'--open-file',str(c)],'run-file','main.c',False),
           ('seed-workspace-session',[str(workspace)],'run','独立工程',True),('file-overrides-last-workspace',[str(c)],'run','main.c',True)]
    rows=[]
    for name,target,task,expected,session in cases:
        image=root/(name+'.png');log=root/(name+'.log')
        command=[str(args.ide.resolve()),*target,'--hidden','--frames','3','--settings',str(root/(name+'-settings.json')),
                 '--diagnostic-root',str(root/(name+'-diagnostics')),'--compiler',str(args.compiler.resolve()),'--task',task,'--wait-task','--screenshot',str(image)]
        command += ['--session-root',str(root/'startup-session')] if session else ['--no-session']
        with log.open('wb') as f:result=subprocess.run(command,stdout=f,stderr=f,timeout=60)
        text=log.read_text(encoding='utf8',errors='replace')
        assert result.returncode==0 and 'state=3 built=1 ran=1 code=0' in text,(name,result.returncode,text[-2000:])
        assert ('Task target: '+expected+'\n') in text,(name,text[-2000:])
        assert image.is_file() and image.stat().st_size>1000
        rows.append(dict(name=name,exitCode=result.returncode,arguments=command,log=str(log),image=str(image)))
    for p in (c.parent,xl.parent):
        assert not list(p.rglob('*.xlp')) and not list(p.rglob('*.xlw'))
    value=dict(status='passed',cases=rows,singleFileModesNeedNoManifest=True)
    (root/'result.json').write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(status='passed',cases=len(rows),proof=str(root)),ensure_ascii=False))

if __name__=='__main__':main()
