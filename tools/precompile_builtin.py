#!/usr/bin/env python3
"""Build ordinary target ELF objects for fixed language/TCC units; no embedded catalogs.

Objects are target/compiler/profile bound. Ordinary generated C and selected
XRT modules remain source compiled, preserving fine-grained deployment.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, subprocess, struct
from pathlib import Path


def run(command, root):
    p = subprocess.run([str(x) for x in command], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if p.returncode:
        raise RuntimeError(" ".join(map(str, command))+"\n"+p.stdout.decode("utf-8", "replace"))


def source_fingerprint(root):
    inputs=sorted({p for folder in ("src","lib/tcc","lib/lzma","lib/xrt","lib/xruntime","release/include","release/extlibs/xacme") for p in (root/folder).rglob("*") if p.is_file() and p.suffix in (".c",".h",".inc",".def")})
    digest=hashlib.sha256()
    for p in inputs:digest.update(p.relative_to(root).as_posix().encode());digest.update(hashlib.sha256(p.read_bytes()).digest())
    return digest.hexdigest()


def source_dependencies(root, source):
    visited=set();todo=[root/source]
    while todo:
        path=todo.pop().resolve()
        if path in visited:continue
        visited.add(path)
        text=path.read_text(encoding="utf-8")
        for name in re.findall(r'^\s*#\s*include\s*["<]([^">\n]+\.c)[">]',text,re.M):
            candidates=[path.parent/name,root/name,root/"src"/name,root/"release"/name]
            found=next((p for p in candidates if p.is_file()),None)
            if found is not None:todo.append(found)
    h=hashlib.sha256()
    for path in sorted(visited):
        h.update(path.relative_to(root).as_posix().encode()+b"\0"+hashlib.sha256(path.read_bytes()).digest())
    return h.hexdigest()


def defined_symbols(path):
    data=path.read_bytes()
    if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", data, 18)[0] != 62:
        raise RuntimeError("expected target ELF64 object: "+str(path))
    offset=struct.unpack_from("<Q",data,40)[0]
    stride,count=struct.unpack_from("<HH",data,58)
    sections=[struct.unpack_from("<IIQQQQIIQQ",data,offset+i*stride) for i in range(count)]
    names=set()
    for section in sections:
        if section[1] != 2:continue
        strings=sections[section[6]]
        table=data[strings[4]:strings[4]+strings[5]]
        for at in range(section[4],section[4]+section[5],section[9]):
            name,info,other,index,value,size=struct.unpack_from("<IBBHQQ",data,at)
            if index and info>>4==1:
                end=table.index(0,name);names.add(table[name:end].decode("utf-8","replace"))
    return names


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, required=True)
    ap.add_argument("--tcc", type=Path, required=True)
    ap.add_argument("--tcc-dist", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--triplet", default="")
    ap.add_argument("--native-cc", type=Path, help="Linux ELF producer with -O2; Windows keeps TCC ELF")
    args=ap.parse_args();root=args.root.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    cc=args.tcc.resolve();dist=args.tcc_dist.resolve()
    if args.native_cc and os.name == "nt":
        ap.error("Windows TCC does not consume GCC COFF objects; use the TCC producer")
    native_cc=args.native_cc.resolve() if args.native_cc else cc
    target="win64" if os.name=="nt" else "linux-x86_64"
    common=["-B"+str(dist),"-std=c11","-D_CRT_SECURE_NO_WARNINGS","-DXL_STATIC_TCC","-DXL6_COMPILER_SOURCE_VFS",
        "-I.","-Isrc","-Irelease","-Irelease/include","-Ilib/tcc","-Ilib/lzma"]
    if os.name=="nt":common += ["-DCONFIG_TCC_STATIC","-I"+str(dist/"include")]
    else:common += ["-D_GNU_SOURCE","-fPIC"]
    if args.triplet:
        common += ['-DCONFIG_TRIPLET="'+args.triplet+'"','-DXL6_TCC_CONFIGURED_TRIPLET="'+args.triplet+'"']
    api=(root/"src/api/xl_api.c").read_text(encoding="utf-8")
    arrays={name:re.findall(r'"([^"\n]+\.c)"',re.search(r'static const char\* '+name+r'\[\] = \{(.*?)\};',api,re.S)[1]) for name in ("runtimeSources","compilerSources")}
    catalog=out/"components.i"
    run([cc,*common,"-DXL_STATIC_EMBED","-E","src/compile/xl6_compile_unit.c","-o",catalog],root)
    text=catalog.read_text(encoding="utf-8",errors="replace");text=text[text.index("static const XL6_ArtifactRuntimeComponent xl6ArtifactComponents[]"):];text=text[:text.index("_Static_assert")]
    while re.search(r'"\s+"',text):text=re.sub(r'"\s+"',"",text)
    components=list(dict.fromkeys(re.findall(r'\{\s*"(src/[^"\n]+\.c)"',text)))
    assert len(components)>1000,"expanded artifact catalog is incomplete"
    tcc_sources=["lib/tcc/libtcc.c","lib/tcc/tcctools.c","lib/tcc/tcc_builtin_vfs.c","lib/lzma/LzmaDec.c","src/api/xl_tcc_standalone_env.c"]
    if os.name=="nt":tcc_sources.append("lib/tcc/tcc_utf8_io.c")
    # GCC -B also changes its system include search. TCC's SDK headers are
    # compiler-specific, so the native producer uses its own host headers.
    native_common=([flag for flag in common if not flag.startswith("-B")]+["-O2"]
                   if args.native_cc else common)
    system_hash=hashlib.sha256()
    if args.native_cc:
        native_env=dict(os.environ, LC_ALL="C")
        system_hash.update(subprocess.check_output([str(native_cc), "--version"], env=native_env))
        system_hash.update(subprocess.check_output([str(native_cc), "-dumpmachine"], env=native_env))
        search=subprocess.run([str(native_cc), "-E", "-v", "-x", "c", "-"], input=b"",
                              capture_output=True, check=True, env=native_env).stderr.decode("utf-8")
        search=search.split("#include <...> search starts here:")[1].split("End of search list.")[0]
        headers={path.resolve() for line in search.splitlines() if line.strip()
                 for path in Path(line.strip()).rglob("*.h") if path.is_file()}
        for path in sorted(headers):
            system_hash.update(str(path).encode()+b"\0"+hashlib.sha256(path.read_bytes()).digest())
    source_hash=source_fingerprint(root)
    header_hash=hashlib.sha256()
    for folder in ("src","lib/tcc","lib/lzma","lib/xrt","lib/xruntime","release/include","release/extlibs/xacme"):
        for path in sorted((root/folder).rglob("*")):
            if path.is_file() and path.suffix in (".h",".inc",".def"):
                header_hash.update(path.relative_to(root).as_posix().encode()+b"\0"+hashlib.sha256(path.read_bytes()).digest())
    sdk=hashlib.sha256()
    for path in sorted((dist/"include").rglob("*")):
        if path.is_file():sdk.update(path.relative_to(dist).as_posix().encode()+b"\0"+hashlib.sha256(path.read_bytes()).digest())
    fingerprint=dict(schema=1,sdk=sdk.hexdigest(),target=target,tcc=hashlib.sha256(cc.read_bytes()).hexdigest(),sources=source_hash,
        producer=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        nativeCompiler=hashlib.sha256(native_cc.read_bytes()).hexdigest(),
        nativeSystemHeaders=system_hash.hexdigest(),flags=native_common)
    signature=hashlib.sha256(json.dumps(fingerprint,sort_keys=True).encode()).hexdigest()
    prior=out/"manifest.json"
    old=json.loads(prior.read_text(encoding="utf-8")) if prior.exists() else {}
    rows=[];reused=0
    for shared in (False,True):
        for compiler in (True,False):
            profile=("shared" if shared else "static")+("-compiler" if compiler else "-runtime")
            # Resident components have one internal/static ABI in all hosts.
            # Only public host entry units vary by DLL/runtime/compiler profile.
            units=list(dict.fromkeys([*arrays["runtimeSources"],*arrays["compilerSources"]] if compiler else arrays["runtimeSources"]))
            if not shared and compiler:units+=components
            if compiler:units+=tcc_sources
            units=list(dict.fromkeys(units))
            flags=native_common+(["-DXL_BUILD_DLL","-DLIBTCC_AS_DLL"] if shared else ["-DXL_STATIC_EMBED"])
            if not compiler:flags += ["-DXL_RUNTIME_ONLY_DLL","-DXL_RUNTIME_ONLY_API"]
            cache_base=json.dumps(dict(version=3,nativeCompiler=fingerprint["nativeCompiler"],nativeSystemHeaders=fingerprint["nativeSystemHeaders"],headers=header_hash.hexdigest(),sdk=sdk.hexdigest(),tcc=fingerprint["tcc"],flags=flags),sort_keys=True)
            for source in units:
                profile_signature=hashlib.sha256((cache_base+source_dependencies(root,source)).encode()).hexdigest()
                dest=out/profile/(source+".o");dest.parent.mkdir(parents=True,exist_ok=True)
                key=Path(str(dest)+".key")
                if dest.exists() and key.exists() and key.read_text()==profile_signature+":"+hashlib.sha256(dest.read_bytes()).hexdigest():reused+=1
                else:
                    run([native_cc,*flags,"-c",source,"-o",dest],root)
                    key.write_text(profile_signature+":"+hashlib.sha256(dest.read_bytes()).hexdigest())
                rows.append(dict(virtual="__xl_objects/"+profile+"/"+source+".o",source=str(dest),sha256=hashlib.sha256(dest.read_bytes()).hexdigest(),bytes=dest.stat().st_size))
            print(profile+": "+str(len(units))+" objects",flush=True)
    # Shared artifacts retain the checked-error release export contract; the
    # remaining implementation bytes are identical and shared by the catalog.
    native_source=out/"xrt_impl.c"
    native_source.write_text('__attribute__((weak)) unsigned int __mzerosf=0x80000000u;\n__attribute__((weak)) unsigned long long __mzerodf=0x8000000000000000ull;\n#if defined(_WIN32) && defined(XL6_BUILD_SHARED)\nstruct xerror;\n__declspec(dllexport) void xrtErrorFree(struct xerror*);\n#endif\n#define XRT_MODULE_ALL\n#define XRUNTIME_MODULE_ALL\n#include "lib/xruntime/xruntime_features.h"\n#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding="utf-8")
    native_objects=[]
    for shared in (False,True):
        profile="shared-native" if shared else "static-native"
        native_object=out/(profile+".o")
        native_flags=native_common+["-DXL_STATIC_EMBED"]+(["-DXL6_BUILD_SHARED"] if shared else [])
        run([native_cc,*native_flags,"-c",native_source,"-o",native_object],root)
        native_objects.append(native_object)
        rows.append(dict(product="xrt",virtual="__xl_objects/"+profile+"/xrt.o",source=str(native_object),sha256=hashlib.sha256(native_object.read_bytes()).hexdigest(),bytes=native_object.stat().st_size))
    from gen_xrt_symbol_table import _api_names
    expected=set(_api_names(root/"lib/xrt/xrt.h"))|set(_api_names(root/"lib/xruntime/xruntime.h"))
    for native_object in native_objects:
        native=defined_symbols(native_object)
        if not expected <= native or any(name.startswith("xl6") for name in native):
            raise RuntimeError("precompiled native product ownership is incomplete or contains a language facade")
    checked=set()
    for row in rows:
        if row.get("product")=="xrt" or row["sha256"] in checked:continue
        checked.add(row["sha256"])
        overlap=expected & defined_symbols(Path(row["source"]))
        if overlap:raise RuntimeError("duplicate XRT implementation in "+row["virtual"]+": "+str(sorted(overlap)[:3]))
    if source_fingerprint(root)!=source_hash:
        raise RuntimeError("producer sources changed during precompilation; retry on a stable tree")
    prior.write_text(json.dumps(dict(**fingerprint,signature=signature,components=len(components),objects=rows),indent=2)+"\n",encoding="utf-8")
    print("precompiled "+str(len(rows))+" objects ("+str(reused)+" unchanged)")

if __name__=="__main__":main()
