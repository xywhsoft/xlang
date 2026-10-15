"""Compare real IO object references with declared resident/native closures."""
from pathlib import Path
import re,shutil,subprocess

def audit(root,build,run):
    header=(root/'src/compile/xl6_artifact_runtime.h').read_text()
    body=header.split('typedef enum XL6_ArtifactRuntimeId {',1)[1].split('} XL6_ArtifactRuntimeId;',1)[0]
    names=re.findall(r'\bXL6_ARTIFACT_(?!RUNTIME_COUNT\b)([A-Z_][A-Z_0-9]*)\b',body)
    source=(root/'src/compile/xl6_compile_unit.c').read_text().split('xl6ArtifactComponents[] = {',1)[1].split('\n};',1)[0]
    rows=re.findall(r'\{"((?:src|tools)/[^"]+)",([^{}]*?)\{([^{}]*)\}[^{}]*\}',source,re.S)
    assert len(rows)==len(names) and len(set(names))==len(names)
    components={name:dict(Source=path,Dependencies=re.findall(r'R\((\w+)\)',deps),Modules=re.findall(r'"([^"]+)"',modules)) for name,(path,deps,modules) in zip(names,rows)}
    native={}
    for symbol,module in re.findall(r'XL6_ARTIFACT_NATIVE_SYMBOL\("(\w+)", (NULL|"[^"]+")\)',(root/'src/compile/xl6_artifact_native_symbols.inc').read_text()):
        native.setdefault(symbol,set()).add(module.strip('"'))
    resident={symbol:owner for symbol,owner in re.findall(r'XL6_TEXT_EXPORT\((\w+),\s*(\w+)\)',(root/'src/runtime/xl6_text_symbols.inc').read_text())}
    resident.update(re.findall(r'XL6_ARTIFACT_SYMBOL\("(\w+)", (\w+)\)',(root/'src/compile/xl6_artifact_runtime_symbols.inc').read_text()))
    objects={Path(row['Source']).name:row['Path'] for row in build['Objects']}
    nm=shutil.which('nm');cc=shutil.which('gcc');assert nm and cc
    proofs=[];negative=0
    def expand(modules,label):
        text=run(label,[cc,'-E','-dM','-x','c','-I'+str(root),'-include',root/'lib/xruntime/xruntime.h',*['-D'+module for module in sorted(modules)],'-'])
        result=set(re.findall(rb'^#define ((?:XRT|XRUNTIME)_MODULE_\w+)\b',text,re.M))
        assert b'XRT_MODULE_ALL' not in result and b'XRUNTIME_MODULE_ALL' not in result
        return {name.decode() for name in result}
    for name,component in components.items():
        if not Path(component['Source']).name.startswith('xl6_io_') and name not in ('FS_MAP_OWNER','FS_LOCK_OWNER','FS_ROOT_OWNER'):continue
        closed=set();pending=[name]
        while pending:
            item=pending.pop()
            if item in closed:continue
            closed.add(item);pending+=components[item]['Dependencies']
        modules={module for item in closed for module in components[item]['Modules']}
        available=expand(modules,name+'.modules')
        text=run(name+'.undefined',[nm,'-u',objects[Path(component['Source']).name]])
        symbols=set(re.findall(rb'\bU\s+(\w+)\s*$',text,re.M));missing=[]
        for encoded in symbols:
            symbol=encoded.decode()
            if symbol.startswith('xrt'):
                assert symbol in native,(name,'unknown native reference',symbol)
                if not(native[symbol]&({'NULL'}|available)):missing.append(symbol)
            elif symbol.startswith('xl6'):
                # Handle getters are declared by the resource policy table.
                if symbol not in resident:
                    for owner,getter in re.findall(r'XL6_NATIVE_RESOURCE_POLICY\((?:"\w+"|NULL),(\w+),(\w+)\)',(root/'src/runtime/xl6_native_resource_policies.inc').read_text()):resident[getter]=owner
                assert symbol in resident and resident[symbol] in closed,(name,'missing resident dependency',symbol)
        assert not missing,(name,'missing native modules',missing)
        if name=='IO_READER_DATA':
            reduced=expand((modules-{'XRT_MODULE_IO_BUFFER'})|{'XRT_MODULE_IO'},name+'.missing-buffer')
            assert 'xrtReaderReadAll' in {s.decode() for s in symbols}
            assert not(native['xrtReaderReadAll']&({'NULL'}|reduced));negative+=1
        proofs.append(dict(Component=name,NativeModules=sorted(modules),UndefinedSymbols=len(symbols)))
    assert len(proofs)==20 and negative==1
    return proofs
