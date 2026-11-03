from pathlib import Path
import argparse,ctypes,json,subprocess,sys,time
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;P=O/'acceptance-chain-v2';P.mkdir()
parser=argparse.ArgumentParser();parser.add_argument('--fault-pid',type=int,required=True);args=parser.parse_args()
record=dict(status='running',accepted=False,scope='Wait on the already running corrected native producer omission test by an OS handle;then verify61 prior passed command receipts,remaining schema guards,and one fixed performance comparison. No build restart or publication.',faultPid=args.fault_pid,stages=[])
def save():(P/'result.json').write_text(json.dumps(record,indent=2)+'\n')
save();kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.OpenProcess.argtypes=[ctypes.c_ulong,ctypes.c_int,ctypes.c_ulong];kernel.OpenProcess.restype=ctypes.c_void_p
kernel.WaitForSingleObject.argtypes=[ctypes.c_void_p,ctypes.c_ulong];kernel.WaitForSingleObject.restype=ctypes.c_ulong
kernel.GetExitCodeProcess.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_ulong)];kernel.GetExitCodeProcess.restype=ctypes.c_int
kernel.CloseHandle.argtypes=[ctypes.c_void_p];kernel.CloseHandle.restype=ctypes.c_int
try:
 handle=kernel.OpenProcess(0x00100000|0x1000,False,args.fault_pid)
 if handle:
  try:
   while True:
    state=kernel.WaitForSingleObject(handle,50000)
    if state==0:break
    assert state==258,ctypes.get_last_error()
   code=ctypes.c_ulong();assert kernel.GetExitCodeProcess(handle,ctypes.byref(code));assert not code.value,('full build exit',code.value)
  finally:kernel.CloseHandle(handle)
 full=json.loads((O/'ast-replacements-faults-v2/result.json').read_text());assert full['status']=='passed',full
 print('Corrected actual producer omissions passed;resuming unchanged verified protocol receipts and remaining guards',flush=True)
 for name in ['validation-v2','performance']:
  c=[sys.executable,'-X','utf8','-B',str(O/('run-'+name+'.py'))];row=dict(name=name,command=c,status='running');record['stages'].append(row);save()
  begin=time.monotonic();log=P/(name+'.log')
  with log.open('w',encoding='utf8') as target:
   process=subprocess.Popen(c,cwd=R,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf8',errors='replace')
   row['pid']=process.pid;save()
   for line in process.stdout:target.write(line);target.flush();print(line,end='',flush=True)
   code=process.wait()
  row.update(status='passed' if not code else 'failed',exit=code,seconds=time.monotonic()-begin);save();assert not code,(name,str(log))
 record['status']='completed';print('Acceptance chain completed;results await final review',flush=True)
except BaseException as e:record.update(status='failed',error=repr(e));raise
finally:save()
