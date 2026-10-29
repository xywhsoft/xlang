"""Ordinary product-only Channel subscription ABI test; offline, no networking."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root",type=Path,required=True)
    parser.add_argument("--out",type=Path,required=True)
    parser.add_argument("--host",type=Path,default=Path(__file__).resolve().parents[1]/"test/cases/stdlib_channel_subscribe_abi_host.c")
    args=parser.parse_args()
    root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists(),"preserve previous evidence"
    products=[root/"release"/name for name in (("xl.dll","xlc.dll") if os.name=="nt" else ("libxl.so","libxlc.so"))]
    header=root/"release/include/xruntime_decl.h"
    compiler=Path(shutil.which("gcc")).resolve()
    files=[args.host.resolve(),header,*products,compiler,Path(__file__).resolve()]
    inputs={str(path):digest(path) for path in files}
    out.mkdir()
    report={"complete":False,"inputs":inputs,"phases":[],"scope":__doc__}
    def save(): (out/"report.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    def run(label,command):
        started=time.time()
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
        (out/(label+".log")).write_bytes(result.stdout)
        report["phases"].append({"name":label,"command":command,"exit":result.returncode,"seconds":time.time()-started})
        save()
        assert result.returncode==0,(label,result.stdout.decode(errors="replace")[-1200:])
    save()
    for optimization in ("0","2"):
        host=out/("host_o"+optimization+(".exe" if os.name=="nt" else ""))
        command=[str(compiler),"-std=c11","-Wall","-Wextra","-Werror","-O"+optimization,
                 '-DXL6_SUBSCRIBE_DECL_HEADER="'+header.as_posix()+'"',str(args.host.resolve()),"-o",str(host)]
        if os.name!="nt": command += ["-ldl"]
        run("compile_o"+optimization,command)
        for product in products: run(product.stem+"_o"+optimization,[str(host),str(product)])
    report["inputsUnchanged"]=all(digest(path)==inputs[str(path)] for path in files)
    report["complete"]=report["inputsUnchanged"]
    save()
    assert report["complete"]
    print("actual ordinary products: subscription ABI and ownership passed",flush=True)


if __name__=="__main__":
    main()
