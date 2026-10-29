"""Compile-only portability control for an embedded TCC aggregate initializer.
No ACME operation, network traffic, security test or product acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['root','baseline-root','out']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--tcc',type=Path,required=True);a=p.parse_args()
    root=a.root.resolve();old=a.baseline_root.resolve();out=a.out.resolve();tcc=a.tcc.resolve()
    assert not out.exists();out.mkdir(parents=True);relative='src/runtime/xl6_xacme.c'
    control=out/'old-initializer-control'/relative;control.parent.mkdir(parents=True)
    shutil.copy2(old/relative,control)
    original=control.read_text();current=(root/relative).read_text()
    old_line='    const char* values[5]={0};xstrview views[]={root,domain,chain,key,directory};bool ok=false;\n'
    assert original.count(old_line)==1
    old_start=original.index(old_line);start=current.index('    const char* values[5]={0};xstrview views[5];\n')
    old_line_number=original[:old_start].count('\n')+1
    end=current.index('    for(size_t i=0;i<5;++i)',start)
    assert current[:start]==original[:old_start] and current[end:]==original[old_start+len(old_line):]
    # Added explanatory source lines legitimately change downstream __LINE__
    # diagnostics. Retain actual new objects, plus a separately named compile
    # control that aligns ONLY the unchanged suffix's diagnostic locations.
    # No object/disassembly bytes are normalized or silently discarded.
    aligned=out/'diagnostic-aligned-new-control'/relative;aligned.parent.mkdir(parents=True)
    next_line=original[:old_start+len(old_line)].count('\n')+1
    aligned.write_text(current[:end]+'#line '+str(next_line)+'\n'+current[end:])
    gcc=Path(shutil.which('gcc')).resolve()
    inputs={Path(__file__).resolve(),gcc,tcc,old/relative,control,aligned,root/relative}
    for name in ['src','include','release/include','release/extlibs/xacme']:
        inputs.update(q for q in (root/name).rglob('*') if q.is_file() and q.suffix in ['.h','.inc','.c'])
    report=dict(Complete=False,Rows=[],Comparisons=[],Inputs={str(q):sha(q) for q in sorted(inputs)},Scope=__doc__,ShippingAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,expected=0):
        command=list(map(str,command));log=out/(name+'.log')
        with log.open('wb') as stream:code=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,cwd=root,timeout=300).returncode
        report['Rows'].append(dict(Name=name,Command=command,Exit=code,Expected=expected,Log=str(log),Sha256=sha(log)));save()
        print(name,code,flush=True)
        assert (code!=0 if expected=='failure' else code==expected),str(log)
        return log.read_text(errors='replace')
    common=['-std=c11','-Wall','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(root/'src/runtime')]
    if os.name!='nt':common+=['-D_GNU_SOURCE']
    for level in ['O0','O2','Os']:
        outputs=[]
        for variant,source in [('old',control),('new',root/relative),('diagnostic-aligned-new',aligned)]:
            target=out/(level+'.'+variant+'.o');outputs.append(target)
            run('gcc.'+level+'.'+variant,[gcc,*common,'-'+level,
                '-ffile-prefix-map='+str(root)+'=/project',
                '-ffile-prefix-map='+str(out/'old-initializer-control')+'=/project',
                '-ffile-prefix-map='+str(out/'diagnostic-aligned-new-control')+'=/project','-c',source,'-o',target])
        assert sha(outputs[0])==sha(outputs[2]),(level,'Aligned control differs; inspect, do not normalize away changes')
        report['Comparisons'].append(dict(Level=level,DiagnosticAlignedEqual=True,OldSha256=sha(outputs[0]),
            ActualNewSha256=sha(outputs[1]),AlignedNewSha256=sha(outputs[2]),
            Control='Only unchanged suffix diagnostic line numbers aligned by explicit #line; actual source object retained'))
    original=run('tcc.old',[tcc,*common,'-c',control,'-o',out/'tcc.old.o'],expected='failure')
    overflow='initializer overflow' in original and 'init_assert' in original
    index_error=str(control)+':'+str(old_line_number)+': error: index too large'
    assert overflow or original.strip()==index_error
    run('tcc.new',[tcc,*common,'-c',root/relative,'-o',out/'tcc.new.o'])
    report['InputsUnchanged']=all(sha(q)==h for q,h in report['Inputs'].items());assert report['InputsUnchanged']
    report['Complete']=True;report['Products']={str(q):sha(q) for q in out.rglob('*') if q.is_file() and q.name!='results.json'};save()
if __name__=='__main__':main()
