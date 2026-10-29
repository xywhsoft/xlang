"""Offline key-format foundation: current XRT DER/PEM, fixed public vectors,
independent normal-format interoperability, and native allocation rollback.
Does not publish language APIs or claim P4/source/shipping completion.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import time

import cryptography
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec, ed25519, rsa, x25519, x448


SOURCES = [
    'xl6_key_format_algorithm', 'xl6_key_format_spki', 'xl6_key_format_pkcs8',
    'xl6_key_format_rsa', 'xl6_key_format_sec1', 'xl6_key_format_curve',
    'xl6_key_format_pem',
]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def constants(path, names):
    text = path.read_text(encoding='utf-8-sig')
    result = {}
    for name in names:
        pattern = r'^static const uint8\s+' + re.escape(name) + r'\[\]\s*=\s*\{([^}]+)\};'
        matches = re.findall(pattern, text, re.MULTILINE)
        assert len(matches) == 1, (path, name)
        fields = [field.strip() for field in matches[0].split(',') if field.strip()]
        assert all(re.fullmatch(r'0[xX][0-9a-fA-F]{1,2}', field) for field in fields)
        result[name] = bytes(int(field, 16) for field in fields)
    return result


def vectors(root):
    native = root / 'test/support'
    names = ['dataModulus', 'dataExponent', 'dataPrivateExponent', 'dataPrime1',
             'dataPrime2', 'dataExponent1', 'dataExponent2', 'dataCoefficient']
    parts = constants(native / 'crypto_rsa_runtime.c', names)
    n, e, d, p, q, dp, dq, qi = [int.from_bytes(parts[name], 'big') for name in names]
    private = rsa.RSAPrivateNumbers(p, q, d, dp, dq, qi, rsa.RSAPublicNumbers(e, n)).private_key()
    result = [dict(name='rsa', algorithm=1, key=private, raw=b'',
                   public=private.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.PKCS1),
                   rsa=[parts[name] for name in names])]
    nist = constants(native / 'crypto_nist_runtime.c', ['p256Private', 'p256Public', 'p384Private', 'p384Public'])
    for name, algorithm, curve in [('p256', 2, ec.SECP256R1()), ('p384', 3, ec.SECP384R1())]:
        raw = nist[name + 'Private']
        key = ec.derive_private_key(int.from_bytes(raw, 'big'), curve)
        public = key.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
        assert public == nist[name + 'Public']
        result.append(dict(name=name, algorithm=algorithm, key=key, raw=raw, public=public, rsa=[]))
    ed = constants(native / 'crypto_ed25519_runtime.c', ['seedData', 'publicData'])
    key = ed25519.Ed25519PrivateKey.from_private_bytes(ed['seedData'])
    assert key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw) == ed['publicData']
    result.append(dict(name='ed25519', algorithm=4, key=key, raw=ed['seedData'], public=ed['publicData'], rsa=[]))
    xdh = constants(native / 'crypto_xdh_runtime.c', ['x25519_private', 'x25519_public', 'x448_private', 'x448_public'])
    for name, algorithm, cls in [('x25519', 5, x25519.X25519PrivateKey), ('x448', 6, x448.X448PrivateKey)]:
        raw = xdh[name + '_private'];key = cls.from_private_bytes(raw)
        public = key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
        assert public == xdh[name + '_public']
        result.append(dict(name=name, algorithm=algorithm, key=key, raw=raw, public=public, rsa=[]))
    for vector in result:
        key = vector['key']
        vector['spki'] = key.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
        vector['pkcs8'] = key.private_bytes(serialization.Encoding.DER, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
        vector['pem'] = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
        if vector['algorithm'] <= 3:
            vector['inner'] = key.private_bytes(serialization.Encoding.DER, serialization.PrivateFormat.TraditionalOpenSSL, serialization.NoEncryption())
        else:
            # RFC8410's CurvePrivateKey is one OCTET STRING, not bare bytes.
            vector['inner'] = bytes([4, len(vector['raw'])]) + vector['raw']
    return result


def header(path, cases):
    lines = ['/* Fixed public test data; no original native tests are executed. */',
             'typedef struct KeyVector { const char* name; XL6_KeyAlgorithmV1 algorithm;',
             'xbytesview privateKey,publicKey,spki,pkcs8,inner; xstrview pem; xbytesview rsa[8]; } KeyVector;']
    records = []
    for vector in cases:
        spans = {}
        for field in ['raw', 'public', 'spki', 'pkcs8', 'inner', 'pem']:
            name = 'vector_' + vector['name'] + '_' + field
            data = vector[field]
            if not data:
                spans[field] = '{0}'
                continue
            lines.append('static const uint8 ' + name + '[]={' + ','.join(hex(byte) for byte in data) + '};')
            spans[field] = '{' + ('(const char*)' if field == 'pem' else '') + name + ',sizeof(' + name + ')}'
        parts = []
        for index, data in enumerate(vector['rsa']):
            name = 'vector_rsa_part_' + str(index)
            lines.append('static const uint8 ' + name + '[]={' + ','.join(hex(byte) for byte in data) + '};')
            parts.append('{' + name + ',sizeof(' + name + ')}')
        records.append('{"' + vector['name'] + '",' + str(vector['algorithm']) + ',' +
                       ','.join(spans[field] for field in ['raw', 'public', 'spki', 'pkcs8', 'inner', 'pem']) +
                       ',{' + (','.join(parts) if parts else '{0}') + '}}')
    lines.append('static const KeyVector keyVectors[]={\n' + ',\n'.join(records) + '\n};')
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def interop(directory, cases):
    products = []
    for vector in cases:
        prefix = directory / vector['name']
        spki = Path(str(prefix) + '.spki.der').read_bytes()
        assert spki == vector['spki']
        public = serialization.load_der_public_key(spki)
        assert public.public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo) == vector['spki']
        der = Path(str(prefix) + '.pkcs8.der').read_bytes()
        pem = Path(str(prefix) + '.pkcs8.pem').read_bytes()
        for key in [serialization.load_der_private_key(der, None), serialization.load_pem_private_key(pem, None)]:
            assert key.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo) == vector['spki']
        if vector['algorithm'] <= 3:
            inner_key = serialization.load_der_private_key(Path(str(prefix) + '.inner.der').read_bytes(), None)
            assert inner_key.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo) == vector['spki']
        for extension in ['spki.der', 'pkcs8.der', 'pkcs8-public.der', 'inner.der', 'pkcs8.pem']:
            path = Path(str(prefix) + '.' + extension)
            products.append(dict(Path=str(path), Sha256=sha(path)))
    return products


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args();root = args.root.resolve();out = args.out.resolve()
    assert not out.exists()
    gcc = Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    tcc = Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    sources = [root / ('src/runtime/' + name + '.c') for name in SOURCES]
    native = root / 'test/support/key_format_wire.c'
    fixtures = [root / ('test/support/crypto_' + name + '_runtime.c') for name in ['rsa', 'nist', 'ed25519', 'xdh']]
    includes = [root / 'lib/xrt/xrt.h', root / 'release/include/xrt_decl.h', root / 'release/include/xruntime_decl.h',
                root / 'src/runtime/xl6_key_format_internal.h']
    inputs = {str(path): sha(path) for path in [Path(__file__).resolve(), gcc, tcc, native, *sources, *fixtures, *includes]}
    out.mkdir(parents=True)
    cases = vectors(root);vector_header = out / 'key_vectors.h';header(vector_header, cases)
    implementation = out / 'native.c'
    implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_ASN1_DER\n'
                              '#define XRT_MODULE_PEM\n#define XRT_MODULE_CRYPTO_CORE\n'
                              '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n', encoding='utf-8')
    rows = [];products = [dict(Path=str(path), Sha256=sha(path)) for path in [vector_header, implementation]]
    failures = []
    report = dict(Complete=False, Inputs=inputs, Rows=rows, Products=products, Failures=failures,
                  Scope=__doc__, NetworkTests=False, NetworkSecurityTests=False, LanguageApiPublished=False,
                  ShippingAcceptance=False, CryptoAlgorithmBodiesAdded=False,
                  Oracle=dict(Cryptography=cryptography.__version__, OpenSSL=default_backend().openssl_version_text()))

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(name, command):
        log = out / (name + '.log');started = time.monotonic()
        with log.open('wb') as stream:
            result = subprocess.run(list(map(str, command)), cwd=root, stdout=stream, stderr=subprocess.STDOUT, timeout=240)
        rows.append(dict(Name=name, Exit=result.returncode, Command=list(map(str, command)),
                         Log=str(log), Sha256=sha(log), Seconds=time.monotonic() - started));save()
        print(name, result.returncode, flush=True)
        assert result.returncode == 0, str(log)

    flags = ['-std=c11', '-DXL_STATIC_EMBED', '-I' + str(root), '-I' + str(root / 'release'),
             '-I' + str(root / 'release/include'), '-I' + str(out)]
    if os.name != 'nt':
        flags.append('-D_GNU_SOURCE')
    modes = [('gcc.O0', gcc, ['-O0']), ('gcc.O2', gcc, ['-O2']), ('tcc', tcc, [])]
    if os.name != 'nt':
        modes.append(('asan', gcc, ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']))
    libraries = ['-ladvapi32'] if os.name == 'nt' else ['-pthread', '-ldl', '-lm']
    for name, compiler, options in modes:
        try:
            directory = out / name;directory.mkdir()
            binary = directory / ('wire.exe' if os.name == 'nt' else 'wire')
            strict = [] if compiler == tcc else ['-Wall', '-Wextra', '-Werror', '-Wno-missing-field-initializers', '-Wno-unused-function']
            run(name + '.build', [compiler, *flags, *strict, *options, native, implementation, *sources, *libraries, '-o', binary])
            products.append(dict(Path=str(binary), Sha256=sha(binary)))
            run(name + '.run', [binary, directory])
            products.extend(interop(directory, cases))
            rows.append(dict(Name=name + '.interop', Exit=0, Vectors=len(cases),
                             Scope='normal DER SPKI/PKCS8/PKCS1/SEC1 and PEM; RFC5958 v1 covered by independent RFC8410 fixture, not an OpenSSL acceptance claim'))
            save()
        except Exception as error:
            failures.append(dict(Name=name, Error=repr(error)));save();print(name, repr(error), flush=True)
    report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items())
    report['ProductsUnchanged'] = all(sha(item['Path']) == item['Sha256'] for item in products)
    report['LogsUnchanged'] = all(sha(item['Log']) == item['Sha256'] for item in rows if 'Log' in item)
    report['Complete'] = not failures and all(report[key] for key in ['InputsUnchanged', 'ProductsUnchanged', 'LogsUnchanged'])
    save();assert report['Complete'], failures


if __name__ == '__main__':
    main()
