"""Private diagnostic CreateProcessW adapter with an actual STARTUPINFOW.lpDesktop."""
import ctypes as c
from ctypes import wintypes as w
import msvcrt
import subprocess
import time
import os

class StartupInfo(c.Structure):
    _fields_=[('cb',w.DWORD),('reserved',w.LPWSTR),('desktop',w.LPWSTR),('title',w.LPWSTR),
              ('x',w.DWORD),('y',w.DWORD),('xSize',w.DWORD),('ySize',w.DWORD),
              ('xCount',w.DWORD),('yCount',w.DWORD),('fill',w.DWORD),('flags',w.DWORD),
              ('show',w.WORD),('reservedSize',w.WORD),('reservedBytes',c.c_void_p),
              ('stdin',w.HANDLE),('stdout',w.HANDLE),('stderr',w.HANDLE)]
class ProcessInfo(c.Structure):
    _fields_=[('process',w.HANDLE),('thread',w.HANDLE),('pid',w.DWORD),('tid',w.DWORD)]

class NativeDesktopProcess:
    def __init__(self,arguments,stdout,stderr,startupinfo, creation_flags=0x08000000, cwd=None, env=None, stdin=None):
        assert c.sizeof(StartupInfo)==104 and c.sizeof(ProcessInfo)==24
        self.k=c.WinDLL('kernel32',use_last_error=True)
        def api(name,result,args):
            f=getattr(self.k,name);f.restype=result;f.argtypes=args;return f
        self.closeHandle=api('CloseHandle',w.BOOL,[w.HANDLE])
        self.waitHandle=api('WaitForSingleObject',w.DWORD,[w.HANDLE,w.DWORD])
        self.exitCode=api('GetExitCodeProcess',w.BOOL,[w.HANDLE,c.POINTER(w.DWORD)])
        self.terminate=api('TerminateProcess',w.BOOL,[w.HANDLE,w.UINT])
        create=api('CreateProcessW',w.BOOL,[w.LPCWSTR,w.LPWSTR,c.c_void_p,c.c_void_p,w.BOOL,w.DWORD,c.c_void_p,w.LPCWSTR,c.POINTER(StartupInfo),c.POINTER(ProcessInfo)])
        setInfo=api('SetHandleInformation',w.BOOL,[w.HANDLE,w.DWORD,w.DWORD])
        getInfo=api('GetHandleInformation',w.BOOL,[w.HANDLE,c.POINTER(w.DWORD)])
        createFile=api('CreateFileW',w.HANDLE,[w.LPCWSTR,w.DWORD,w.DWORD,c.c_void_p,w.DWORD,w.DWORD,w.HANDLE])
        inputHandle=createFile('NUL',0x80000000,3,None,3,0,None) if stdin is None else msvcrt.get_osfhandle(stdin.fileno())
        assert inputHandle!=c.c_void_p(-1).value,c.get_last_error()
        handles=[inputHandle,msvcrt.get_osfhandle(stdout.fileno()),msvcrt.get_osfhandle(stderr.fileno())]
        old=[];info=ProcessInfo();self.returncode=None;self.handle=None;self.job=None
        desktop=startupinfo.lpDesktop if hasattr(startupinfo,'lpDesktop') else startupinfo
        startup=StartupInfo(cb=c.sizeof(StartupInfo),desktop=desktop,flags=0x100|1,show=0,stdin=handles[0],stdout=handles[1],stderr=handles[2])
        command=c.create_unicode_buffer(subprocess.list2cmdline(arguments))
        environment=None
        if env is not None:
            # Windows environment names are case-insensitive. A Python copy may
            # contain both PSMODULEPATH and a later PSModulePath override.
            values={k.upper():v for k,v in env.items()}
            environment=c.create_unicode_buffer(chr(0).join(k+'='+v for k,v in sorted(values.items()))+chr(0)*2)
            creation_flags|=0x400
        createJob=api('CreateJobObjectW',w.HANDLE,[c.c_void_p,w.LPCWSTR])
        setJob=api('SetInformationJobObject',w.BOOL,[w.HANDLE,c.c_int,c.c_void_p,w.DWORD])
        assignJob=api('AssignProcessToJobObject',w.BOOL,[w.HANDLE,w.HANDLE])
        resumeThread=api('ResumeThread',w.DWORD,[w.HANDLE])
        self.terminateJob=api('TerminateJobObject',w.BOOL,[w.HANDLE,w.UINT])
        self.job=createJob(None,None);assert self.job,c.get_last_error()
        limits=c.create_string_buffer(144);c.c_uint32.from_buffer(limits,16).value=0x2000
        assert setJob(self.job,9,limits,len(limits)),c.get_last_error()
        try:
            for handle in handles:
                flags=w.DWORD();assert getInfo(handle,c.byref(flags)),c.get_last_error();old.append(flags.value)
                assert setInfo(handle,1,1),c.get_last_error()
            if not create(arguments[0],command,None,None,True,creation_flags|4,environment,cwd,c.byref(startup),c.byref(info)):
                raise c.WinError(c.get_last_error())
            self.handle=info.process;self.pid=info.pid;self.initialThread=info.tid
            if not assignJob(self.job,self.handle):
                error=c.get_last_error();self.terminate(self.handle,1);self.closeHandle(info.thread);raise c.WinError(error)
            if resumeThread(info.thread)==0xffffffff:
                error=c.get_last_error();self.terminateJob(self.job,1);self.closeHandle(info.thread);raise c.WinError(error)
            self.closeHandle(info.thread)
        finally:
            for handle,flags in zip(handles,old):setInfo(handle,1,flags&1)
            if stdin is None:self.closeHandle(inputHandle)
    def poll(self):
        if self.returncode is not None:return self.returncode
        result=self.waitHandle(self.handle,0)
        if result==258:return None
        if result!=0:raise c.WinError(c.get_last_error())
        code=w.DWORD()
        if not self.exitCode(self.handle,c.byref(code)):raise c.WinError(c.get_last_error())
        self.returncode=code.value;return self.returncode
    def wait(self,timeout=None):
        ms=0xffffffff if timeout is None else max(0,min(int(timeout*1000),0xfffffffe))
        result=self.waitHandle(self.handle,ms)
        if result==258:raise subprocess.TimeoutExpired('owned native desktop process',timeout)
        if result!=0:raise c.WinError(c.get_last_error())
        return self.poll()
    def kill(self):
        if self.poll() is None and not self.terminateJob(self.job,1):raise c.WinError(c.get_last_error())
    def close(self):
        if self.job:
            self.closeHandle(self.job);self.job=None
        if self.handle:
            self.closeHandle(self.handle);self.handle=None
    def __del__(self):
        self.close()
