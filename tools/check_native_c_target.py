"""Compare actual embedded TCC target queries with compiled, executed C facts."""
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, required=True)
    args = parser.parse_args()
    compiler = args.compiler.resolve(strict=True).parent
    output = args.output_root.resolve() / 'native-c-target'
    output.mkdir(parents=True, exist_ok=False)
    library = c.CDLL(str(compiler / 'xlc.dll'))

    class Scalar(c.Structure):
        _fields_ = [(name, c.c_int) for name in ('version', 'pointer', 'long', 'longDouble', 'longDoubleAlign')]

    class Layout(c.Structure):
        _fields_ = [('microsoft', c.c_int)]

    signatures = {
        'tcc_new': ([], c.c_void_p), 'tcc_delete': ([c.c_void_p], None),
        'tcc_set_lib_path': ([c.c_void_p, c.c_char_p], None),
        'tcc_add_include_path': ([c.c_void_p, c.c_char_p], c.c_int),
        'tcc_set_output_type': ([c.c_void_p, c.c_int], c.c_int),
        'tcc_set_options': ([c.c_void_p, c.c_char_p], c.c_int),
        'tcc_compile_string': ([c.c_void_p, c.c_char_p], c.c_int),
        'tcc_relocate': ([c.c_void_p], c.c_int),
        'tcc_get_symbol': ([c.c_void_p, c.c_char_p], c.c_void_p),
        'tcc_xlang_scalar_target': ([c.POINTER(Scalar)], c.c_int),
        'tcc_xlang_record_layout': ([c.c_void_p, c.POINTER(Layout)], c.c_int),
    }
    for name, (arguments, result) in signatures.items():
        function = getattr(library, name)
        function.argtypes, function.restype = arguments, result
    assert library.tcc_xlang_scalar_target(None) == 0
    sentinel = Layout(17)
    assert library.tcc_xlang_record_layout(None, c.byref(sentinel)) == 0 and sentinel.microsoft == 17
    scalar = Scalar()
    assert library.tcc_xlang_scalar_target(c.byref(scalar)) == 1
    expected = {name: getattr(scalar, name) for name, _ in Scalar._fields_}
    source = b'''struct Bits { unsigned int a:3; unsigned int b:7; unsigned int c:6; char tail; };
int version_probe(void) { return __TINYC__; }
int pointer_probe(void) { return sizeof(void*); }
int long_probe(void) { return sizeof(long); }
int longDouble_probe(void) { return sizeof(long double); }
int longDoubleAlign_probe(void) { return __alignof__(long double); }
int size_probe(void) { return sizeof(struct Bits); }
int tail_probe(void) { return (int)(long long)&((struct Bits *)0)->tail; }
'''
    records = []
    for flags, wanted, size, tail in ((b'-std=c11', 0, 4, 2),
            (b'-std=c11 -mms-bitfields', 1, 8, 4),
            (b'-std=c11 -mms-bitfields -mno-ms-bitfields', 0, 4, 2)):
        state = library.tcc_new()
        assert state
        try:
            library.tcc_set_lib_path(state, str(compiler.parent / 'res/tcc/win64/x86_64').encode('utf8'))
            assert library.tcc_add_include_path(state, str(compiler.parent / 'lib/tcc/include').encode('utf8')) == 0
            assert library.tcc_set_output_type(state, 1) == 0 and library.tcc_set_options(state, flags) == 0
            layout = Layout(17)
            assert library.tcc_xlang_record_layout(state, c.byref(layout)) == 1 and layout.microsoft == wanted
            assert library.tcc_xlang_record_layout(state, None) == 0
            assert library.tcc_compile_string(state, source) == 0 and library.tcc_relocate(state) == 0
            measurements = {}
            for name in (*expected, 'size', 'tail'):
                pointer = library.tcc_get_symbol(state, (name + '_probe').encode())
                assert pointer
                measurements[name] = c.CFUNCTYPE(c.c_int)(pointer)()
            assert measurements == dict(expected, size=size, tail=tail), measurements
            records.append(dict(flags=flags.decode(), queriedMsBitfields=layout.microsoft,
                executedMeasurements=measurements))
        finally:
            library.tcc_delete(state)
    evidence = dict(status='actual-tcc-target-queries-and-execution-passed', scalar=expected,
        records=records, nullInputsRejectedWithoutChangingOutput=True,
        xlcSha256=hashlib.sha256((compiler / 'xlc.dll').read_bytes()).hexdigest(),
        abiEquivalenceGuaranteed=False)
    path = output / 'evidence.json'
    path.write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf8')
    print(json.dumps(dict(status=evidence['status'], actualConfiguredModes=len(records), evidence=str(path))))


if __name__ == '__main__':
    main()
