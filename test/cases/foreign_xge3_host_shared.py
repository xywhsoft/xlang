"""Use one embedded runtime, retire it, unload its code, then exit normally."""
import ctypes
import os
import sys
import _ctypes

library, native = sys.argv[1:3]
directory = os.add_dll_directory(native) if os.name == 'nt' else None
if os.name == 'nt':
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
    kernel.GetModuleHandleW.restype = ctypes.c_void_p

    def loaded(path):
        return bool(kernel.GetModuleHandleW(path))
else:
    dl = ctypes.CDLL('libdl.so.2')
    dl.dlopen.argtypes = [ctypes.c_char_p, ctypes.c_int]
    dl.dlopen.restype = ctypes.c_void_p
    dl.dlclose.argtypes = [ctypes.c_void_p]
    dl.dlclose.restype = ctypes.c_int

    def loaded(path):
        handle = dl.dlopen(os.fsencode(path), os.RTLD_NOW | os.RTLD_NOLOAD)
        if handle:
            assert dl.dlclose(handle) == 0
        return bool(handle)

assert not loaded(library)
image = ctypes.CDLL(library)
assert loaded(library)
image.xl_script_module_init.argtypes = [ctypes.c_void_p]
assert image.xl_script_module_init(None) == 1
image.host_check.restype = ctypes.c_int64
assert image.host_check() == 0 and image.host_check() == 0
assert image.xl_script_module_unit_checked() == 1
# Module cleanup permits reuse. Terminal Windows runtime retirement additionally
# removes XRT's OS FLS destructors. After it succeeds, call no XRT entry again.
if os.name == 'nt':
    assert image.xl_script_runtime_retire_checked() == 1
    assert image.xl_script_runtime_retire_checked() == 1
handle = image._handle
del image
if os.name == 'nt':
    _ctypes.FreeLibrary(handle)
else:
    _ctypes.dlclose(handle)
assert not loaded(library), 'language shared library remained resident'
engine = os.path.join(native, 'xge3_host.dll' if os.name == 'nt' else 'libxge3_host.so')
assert not loaded(engine), 'host engine remained resident'
print('HOST_SHARED_PHYSICAL_UNLOAD_OK')
