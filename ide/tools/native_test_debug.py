"""Observe one Windows x64 native test without debugging its child programs.

The JSONL event stream is flushed before each ContinueDebugEvent. First-chance
exceptions are returned to the program; an unhandled exception also produces a
small minidump. This runner is diagnostic, not an alternative acceptance gate.
Run: python native_test_debug.py --output <fresh-dir> -- <test.exe> <arguments>
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time


class ExceptionRecord(c.Structure):
    _fields_ = [('code', w.DWORD), ('flags', w.DWORD), ('record', c.c_void_p),
                ('address', c.c_void_p), ('count', w.DWORD),
                ('information', c.c_size_t * 15)]


class ExceptionInfo(c.Structure):
    _fields_ = [('record', ExceptionRecord), ('firstChance', w.DWORD)]


class CreateProcessInfo(c.Structure):
    _fields_ = [('file', w.HANDLE), ('process', w.HANDLE), ('thread', w.HANDLE),
                ('base', c.c_void_p), ('debugOffset', w.DWORD),
                ('debugSize', w.DWORD), ('start', c.c_void_p),
                ('imageName', c.c_void_p), ('unicode', w.WORD)]


class CreateThreadInfo(c.Structure):
    _fields_ = [('thread', w.HANDLE), ('tls', c.c_void_p), ('start', c.c_void_p)]


class LoadDllInfo(c.Structure):
    _fields_ = [('file', w.HANDLE), ('base', c.c_void_p),
                ('debugOffset', w.DWORD), ('debugSize', w.DWORD),
                ('imageName', c.c_void_p), ('unicode', w.WORD)]


class ExitInfo(c.Structure):
    _fields_ = [('code', w.DWORD)]


class EventData(c.Union):
    _fields_ = [('exception', ExceptionInfo), ('process', CreateProcessInfo),
                ('thread', CreateThreadInfo), ('dll', LoadDllInfo),
                ('exit', ExitInfo), ('base', c.c_void_p)]


class DebugEvent(c.Structure):
    _fields_ = [('kind', w.DWORD), ('pid', w.DWORD), ('tid', w.DWORD),
                ('data', EventData)]


class MemoryCounters(c.Structure):
    _fields_ = [('size', w.DWORD), ('pageFaults', w.DWORD),
                ('peakWorkingSet', c.c_size_t), ('workingSet', c.c_size_t),
                ('peakPagedPool', c.c_size_t), ('pagedPool', c.c_size_t),
                ('peakNonPagedPool', c.c_size_t), ('nonPagedPool', c.c_size_t),
                ('pagefile', c.c_size_t), ('peakPagefile', c.c_size_t),
                ('privateBytes', c.c_size_t)]


class DumpExceptionInfo(c.Structure):
    _pack_ = 4  # DbgHelp minidump declarations use pack(4), including pointers.
    _fields_ = [('threadId', w.DWORD), ('pointers', c.c_void_p),
                ('clientPointers', w.BOOL)]


class ExceptionPointers(c.Structure):
    _fields_ = [('record', c.c_void_p), ('context', c.c_void_p)]


def observe(arguments, output, timeout=95):
    if sys.platform != 'win32' or c.sizeof(c.c_void_p) != 8:
        raise RuntimeError('This diagnostic runner requires Windows x64 Python')
    assert c.sizeof(ExceptionRecord) == 152
    assert c.sizeof(DebugEvent) == 176 and DebugEvent.data.offset == 16
    assert c.sizeof(DumpExceptionInfo) == 16 and DumpExceptionInfo.pointers.offset == 4
    if timeout <= 0:
        raise ValueError('timeout must be positive')
    output = Path(output).absolute()
    output.mkdir(parents=True, exist_ok=False)
    arguments = [str(x) for x in arguments]
    exe = Path(arguments[0]).resolve(strict=True)
    k = c.WinDLL('kernel32', use_last_error=True)
    ps = c.WinDLL('psapi', use_last_error=True)
    u = c.WinDLL('user32', use_last_error=True)
    def api(lib, name, result, args):
        f = getattr(lib, name)
        f.restype, f.argtypes = result, args
        return f
    wait = api(k, 'WaitForDebugEvent', w.BOOL, [c.POINTER(DebugEvent), w.DWORD])
    resume = api(k, 'ContinueDebugEvent', w.BOOL, [w.DWORD, w.DWORD, w.DWORD])
    close = api(k, 'CloseHandle', w.BOOL, [w.HANDLE])
    final_path = api(k, 'GetFinalPathNameByHandleW', w.DWORD,
                     [w.HANDLE, w.LPWSTR, w.DWORD, w.DWORD])
    get_context = api(k, 'GetThreadContext', w.BOOL, [w.HANDLE, c.c_void_p])
    get_memory = api(ps, 'GetProcessMemoryInfo', w.BOOL,
                     [w.HANDLE, c.POINTER(MemoryCounters), w.DWORD])
    get_handles = api(k, 'GetProcessHandleCount', w.BOOL, [w.HANDLE, c.POINTER(w.DWORD)])
    get_gui = api(u, 'GetGuiResources', w.DWORD, [w.HANDLE, w.DWORD])
    started = time.monotonic()
    threads, modules = {}, {}
    process_handle = None
    initial_breakpoint = False
    last_chance = []
    dump_results = []
    exit_code = None
    timed_out = False
    errors = []
    event_count = 0
    def path_from_handle(h):
        buf = c.create_unicode_buffer(32768)
        n = final_path(h, buf, len(buf), 0) if h else 0
        if not n or n >= len(buf):
            return None
        return buf.value
    def counters():
        if not process_handle:
            return {}
        m = MemoryCounters(size=c.sizeof(MemoryCounters))
        count = w.DWORD()
        result = {}
        if get_memory(process_handle, c.byref(m), m.size):
            result.update(peakWorkingSet=m.peakWorkingSet, workingSet=m.workingSet,
                          privateBytes=m.privateBytes, peakPagefile=m.peakPagefile)
        if get_handles(process_handle, c.byref(count)):
            result['handles'] = count.value
        result.update(gdi=get_gui(process_handle, 0), user=get_gui(process_handle, 1))
        return result
    def module_address(address):
        # Modules are sorted by base; retain the nearest base and raw address.
        # An address beyond a module's image is not asserted to belong to it.
        values = [(base, name) for base, name in modules.items() if base <= address]
        if not values:
            return None
        base, name = max(values)
        return {'nearestBase': hex(base), 'offset': hex(address-base), 'path': name}
    def minidump(event):
        record = ExceptionRecord.from_buffer_copy(event.data.exception.record)
        # AMD64 CONTEXT is 1232 bytes with 16-byte alignment. CONTEXT_ALL at +48.
        buffer = c.create_string_buffer(1232 + 15)
        address = (c.addressof(buffer) + 15) & ~15
        c.c_uint32.from_address(address + 48).value = 0x10001f
        if not get_context(threads.get(event.tid), address):
            return {'status': 'failed', 'operation': 'GetThreadContext',
                    'win32Error': c.get_last_error()}
        pointers = ExceptionPointers(c.addressof(record), address)
        info = DumpExceptionInfo(event.tid, c.addressof(pointers), False)
        dbg = c.WinDLL('dbghelp', use_last_error=True)
        write_dump = api(dbg, 'MiniDumpWriteDump', w.BOOL,
                         [w.HANDLE, w.DWORD, w.HANDLE, w.DWORD,
                          c.POINTER(DumpExceptionInfo), c.c_void_p, c.c_void_p])
        import msvcrt
        destination = output / ('unhandled-%08x-%d.dmp' % (record.code, event_count))
        with destination.open('xb') as f:
            # Normal memory plus thread info and unloaded modules; no full heap.
            ok = write_dump(process_handle, event.pid, msvcrt.get_osfhandle(f.fileno()),
                            0x1000 | 0x20, c.byref(info), None, None)
            error = c.get_last_error() if not ok else 0
        return {'status': 'saved' if ok else 'failed', 'path': str(destination),
                'sha256': hashlib.sha256(destination.read_bytes()).hexdigest(),
                'bytes': destination.stat().st_size, 'win32Error': error,
                'rip': hex(c.c_uint64.from_address(address + 248).value),
                'rsp': hex(c.c_uint64.from_address(address + 152).value)}
    with (output/'stdout.log').open('xb') as stdout, \
            (output/'stderr.log').open('xb') as stderr, \
            (output/'events.jsonl').open('x', encoding='utf8') as events:
        process = subprocess.Popen(arguments, stdout=stdout, stderr=stderr,
                                   creationflags=0x2 | 0x08000000)
        try:
            while exit_code is None:
                if not timed_out and time.monotonic()-started > timeout:
                    timed_out = True
                    # Terminate only the process created by this runner.
                    process.kill()
                if timed_out and time.monotonic()-started > timeout+10:
                    raise RuntimeError('Timed out waiting for the owned process exit debug event')
                event = DebugEvent()
                if not wait(c.byref(event), 100):
                    error = c.get_last_error()
                    if error != 121:  # ERROR_SEM_TIMEOUT
                        raise c.WinError(error)
                    continue
                event_count += 1
                row = {'number': event_count, 'seconds': round(time.monotonic()-started, 6),
                       'kind': event.kind, 'pid': event.pid, 'tid': event.tid}
                disposition = 0x10002  # DBG_CONTINUE for nonexception events
                try:
                    if event.kind == 3:  # CREATE_PROCESS_DEBUG_EVENT
                        d = event.data.process
                        process_handle = d.process
                        threads[event.tid] = d.thread
                        modules[int(d.base or 0)] = path_from_handle(d.file)
                        row.update(image=modules[int(d.base or 0)], base=hex(d.base or 0))
                        if d.file:
                            close(d.file)
                    elif event.kind == 2:
                        threads[event.tid] = event.data.thread.thread
                        row['start'] = hex(event.data.thread.start or 0)
                    elif event.kind == 4:
                        row['exitCode'] = event.data.exit.code
                        h = threads.pop(event.tid, None)
                        if h:
                            close(h)
                    elif event.kind == 5:
                        exit_code = event.data.exit.code
                        row.update(exitCode=exit_code, exitHex='0x%08x' % exit_code)
                    elif event.kind == 6:
                        d = event.data.dll
                        modules[int(d.base or 0)] = path_from_handle(d.file)
                        row.update(image=modules[int(d.base or 0)], base=hex(d.base or 0))
                        if d.file:
                            close(d.file)
                    elif event.kind == 7:
                        row['base'] = hex(event.data.base or 0)
                        modules.pop(int(event.data.base or 0), None)
                    elif event.kind == 1:
                        e = event.data.exception
                        code = e.record.code
                        row.update(exception='0x%08x' % code, firstChance=bool(e.firstChance),
                                   address=hex(e.record.address or 0),
                                   information=[hex(e.record.information[i])
                                                for i in range(min(e.record.count, 15))],
                                   module=module_address(e.record.address or 0))
                        loader_break = code == 0x80000003 and e.firstChance and not initial_breakpoint
                        if loader_break:
                            initial_breakpoint = True
                        else:
                            disposition = 0x80010001  # DBG_EXCEPTION_NOT_HANDLED
                            if not e.firstChance:
                                last_chance.append(row.copy())
                                try:
                                    dump_results.append(minidump(event))
                                except Exception as exc:
                                    dump_results.append({'status': 'failed', 'error': str(exc)})
                        row['loaderBreakpointConsumed'] = bool(loader_break)
                    if event.kind in (1, 2, 3, 4, 5):
                        row['resources'] = counters()
                finally:
                    row['continueStatus'] = '0x%08x' % disposition
                    events.write(json.dumps(row, ensure_ascii=False)+'\n')
                    events.flush()
                    if not resume(event.pid, event.tid, disposition):
                        raise c.WinError(c.get_last_error())
            process.wait(timeout=5)
        except Exception as exc:
            errors.append(str(exc))
            if process.poll() is None:
                process.kill()
            # An unrecoverable debugger API failure is a failed observation.
            raise
        finally:
            for h in threads.values():
                close(h)
            if process_handle:
                close(process_handle)
    result = {'format': 'xlang-ide-native-debug-1',
              'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'arguments': arguments, 'pid': process.pid, 'debugChildren': False,
              'exeSha256': hashlib.sha256(exe.read_bytes()).hexdigest(),
              'seconds': round(time.monotonic()-started, 6), 'events': event_count,
              'exitCode': exit_code, 'exitHex': '0x%08x' % exit_code,
              'subprocessExit': process.returncode, 'timedOut': timed_out,
              'unhandledExceptions': last_chance, 'minidumps': dump_results,
              'errors': errors, 'files': {}, 'formalRelease': False}
    for name in ('stdout.log', 'stderr.log', 'events.jsonl'):
        result['files'][name] = hashlib.sha256((output/name).read_bytes()).hexdigest()
    (output/'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--timeout', type=float, default=95)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command:
        parser.error('a native executable is required after --')
    result = observe(command, args.output, args.timeout)
    print('Native debug exit %s; %d events; %d unhandled exceptions; %s' %
          (result['exitHex'], result['events'], len(result['unhandledExceptions']), args.output))
    return int(result['exitCode'] != 0 or result['timedOut'] or bool(result['unhandledExceptions']))


if __name__ == '__main__':
    sys.exit(main())
