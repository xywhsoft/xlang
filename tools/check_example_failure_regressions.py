"""Reproduce the 44 reported example failures without Redis or cloud credentials.

Games use static AOT and an explicit frame budget. The cloud-live example is
built, then run with its credentials removed to validate the no-request guard.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,subprocess,time

EXT = "csv_basic epub fix graph graph_basic ical json_patch rss rss_pair sitemap sitemap_pair socks srt statistics xregex xregex_basic tar memcached modbus mqtt mqtt_tcp volc_core volc_phone volc_phone_live volc_sts xbot_agent".split()
STD = "stdlib_buffer stdlib_time_calendar stdlib_log stdlib_log_formats stdlib_logging stdlib_api_contracts stdlib_time_core stdlib_os".split()
GAMES = "2048 asteroids breakout flappy minesweeper pong shooter snake tetris".split()
CASES = ["extlibs/"+name for name in EXT]+STD+["games/"+name for name in GAMES]+["tutorial/09_class_lifecycle"]
CORE = ["xlir_destroy_receiver_borrow", "xlir_nested_array_append_context", "cgen_destroy_field_assignment"]

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument("--root",type=Path,default=Path(__file__).resolve().parents[1])
 p.add_argument("--runtime",type=Path,required=True)
 p.add_argument("--compiler",type=Path,required=True)
 p.add_argument("--out",type=Path,required=True)
 p.add_argument("--jobs",type=int,default=2)
 p.add_argument("--aot",action="store_true")
 p.add_argument("--skip-games",action="store_true")
 p.add_argument("--case",action="append",choices=CASES)
 a=p.parse_args();root=a.root.resolve();runtime=a.runtime.resolve();compiler=a.compiler.resolve();out=a.out.resolve()
 if out.exists():raise SystemExit("Use a new report directory")
 out.mkdir(parents=True);env={k:v for k,v in os.environ.items()if not k.upper().startswith("VOLC_")}
 env["XLANG_PACKAGE_PATH"]=str(runtime/"extlibs")
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 def execute(command,log,expected=0,timeout=120):
  started=time.monotonic()
  try:
   v=subprocess.run(list(map(str,command)),cwd=runtime,env=env,capture_output=True,timeout=timeout);code=v.returncode;raw=v.stdout+v.stderr
  except subprocess.TimeoutExpired as e:code=124;raw=(e.stdout or b"")+(e.stderr or b"")+b"\nTIMEOUT"
  log.write_bytes(raw)
  return dict(command=list(map(str,command)),exitCode=code,expectedExitCode=expected,passed=code==expected,seconds=round(time.monotonic()-started,3),log=str(log),logSha256=sha(log))
 def case(name):
  game=name.startswith("games/");source=runtime/"examples"/(name+".xl");label=name.replace("/","-");steps=[]
  steps.append(execute([compiler,"check",source],out/(label+"-check.log")))
  if steps[-1]["passed"]:
   if game or a.aot:
    target=out/(label+(".exe"if os.name=="nt"else""));steps.append(execute([compiler,"build",source,"-o",target],out/(label+"-build.log")))
    if steps[-1]["passed"]:steps.append(execute([target]+(["3"]if game else[]),out/(label+"-aot-run.log"),2 if name=="extlibs/volc_phone_live"else 0,30 if game else 120))
   if not game:steps.append(execute([compiler,"run",source],out/(label+"-jit-run.log"),2 if name=="extlibs/volc_phone_live"else 0))
  row=dict(name=name,source=str(source),sourceSha256=sha(source),passed=all(v["passed"]for v in steps),steps=steps)
  print(name,"PASS"if row["passed"]else"FAIL",flush=True);return row
 selected=[name for name in (a.case or CASES) if not(a.skip_games and name.startswith("games/"))]
 missing=[name for name in selected if not(runtime/"examples"/(name+".xl")).exists()]
 if missing:raise SystemExit("Missing examples: "+str(missing))
 with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs)as pool:rows=list(pool.map(case,selected))
 core=[]
 for name in ([] if a.case else CORE):
  source=root/"test/cases"/(name+".xl");core.append(dict(name=name,sourceSha256=sha(source),**execute([compiler,"run",source],out/(name+".log"))))
 binaries={str(p):sha(p)for p in [compiler,*[compiler.parent/name for name in ("xlc.dll","xl.dll")if(compiler.parent/name).exists()]]}
 report=dict(schema=1,passed=all(v["passed"]for v in rows+core),examples=len(rows),binaries=binaries,redis="excluded by user",cloud="credentials removed; live case must exit 2 without a request",rows=rows,core=core)
 (out/"report.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
 return 0 if report["passed"]else 1
if __name__=="__main__":raise SystemExit(main())
