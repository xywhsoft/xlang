"""Exercise real frontend extraction, visibility, source fidelity and freshness guards."""
import argparse
import json
from pathlib import Path
import shutil
import unittest

from catalog_extensions import extract
from catalog_sources import sha, verify
from refresh_extensions import check

IDE = Path(__file__).resolve().parents[1]


class ExtensionCatalogTests(unittest.TestCase):
    def setUp(self):
        self.work = OUTPUT / self._testMethodName
        self.package = self.work / 'root/release/extlibs/sample'
        self.package.mkdir(parents=True)
        self.source = self.package / 'api.xl'
        self.source.write_bytes((IDE / 'tests/catalog_extension_declarations.xl').read_bytes())
        self.manifest = self.package / 'xlang.package.json'
        self.manifest.write_text(json.dumps(dict(name='sample', version='1.2.3', kind='demo6-extlib',
            artifact=dict(xlSources=['api.xl']))), encoding='utf-8')
        (self.package / 'readme.md').write_text('# 测试包\n\n公开说明。\n', encoding='utf-8')
        self.number = 0

    def read(self):
        self.number += 1
        self.materials = {}

        def capture(path):
            key = path.relative_to(self.work / 'root').as_posix()
            raw = path.read_bytes()
            self.materials[key] = raw
            return key, raw

        return extract(self.package.parent, EXPORTER, self.work / ('parse-' + str(self.number)), capture)

    def named(self, entries, name):
        return [e for e in entries if e['name'] == 'sample.' + name]

    def test_public_declarations_and_private_boundaries(self):
        entries, documents, coverage = self.read()
        for name in ('named', 'ANSWER', 'current', 'Count', 'State', 'State.idle', 'Packet',
                     'Packet.empty', 'Packet.value', 'Word', 'Word.bits', 'Box', 'Box.value',
                     'Box.total', 'Box.create', 'Box.item', 'Box.size', 'Box.__item__', 'Named.name'):
            self.assertTrue(self.named(entries, name), name)
        for name in ('Box.hidden', 'Box.secret', 'Box.privateMethod', 'Native.handle', 'Native.destroy', 'Hidden.hidden'):
            self.assertFalse(self.named(entries, name), name)
        self.assertEqual(2, len(self.named(entries, 'overload')))
        self.assertEqual(2, len(self.named(entries, 'Box.item')))
        self.assertEqual({'get', 'let'}, {e['signature'].split()[0] for e in self.named(entries, 'Box.size')})
        self.assertEqual('function', self.named(entries, 'Packet.value')[0]['kind'])
        self.assertEqual('constant', self.named(entries, 'Packet.empty')[0]['kind'])
        self.assertEqual('variable', self.named(entries, 'current')[0]['kind'])
        self.assertEqual('static int sample.Box.total = 0', self.named(entries, 'Box.total')[0]['signature'])
        self.assertEqual('static sample.Box.zero() as int', self.named(entries, 'Box.zero')[0]['signature'])
        self.assertEqual(['sample'], coverage['packages'])
        self.assertEqual('package:sample', documents[0]['id'])
        self.assertTrue(documents[0]['source'].endswith('/readme.md'))

    def test_factory_instance_surface_not_global_completion(self):
        entries, _, _ = self.read()
        for name in ('Native', 'Native.read'):
            entry = self.named(entries, name)[0]
            self.assertEqual('', entry['insert'])
            self.assertIn('未直接导出', entry['description'])
        self.assertFalse(self.named(entries, 'Hidden'))

    def test_utf8_bom_crlf_defaults_and_doc(self):
        raw = self.source.read_bytes().decode('utf-8-sig').replace('\r\n', '\n')
        self.source.write_bytes(b'\xef\xbb\xbf' + raw.replace('\n', '\r\n').encode('utf-8'))
        entries, _, _ = self.read()
        entry = self.named(entries, 'named')[0]
        self.assertEqual('sample.named(int 参数 = 1) as int', entry['signature'])
        self.assertEqual('保留中文参数名。', entry['parameters'][0]['description'])
        self.assertTrue(entry['parameters'][0]['optional'])
        self.assertIn('返回首个整数。', entry['description'])
        self.assertEqual('sample.named(1)', entry['example'])
        nested = self.named(entries, 'nested')[0]
        self.assertEqual(['" a,(b)=c "', '[1, 2]', 'func(int n) as int => n'], [p['default'] for p in nested['parameters']])
        self.assertEqual('(int, string)', nested['returns'])
        for e in entries:
            self.assertTrue(1 <= e['sourceLine'] <= len(self.materials[e['source']].decode('utf-8-sig').splitlines()))

    def test_variadics_and_return_whitespace(self):
        self.source.write_bytes(self.source.read_bytes().replace(b'as int\n', b'as\tint\n'))
        entries, _, _ = self.read()
        for name, marker, value_type in (('many', '...', 'array<var>'), ('keyed', '***', 'dict<var>')):
            entry = self.named(entries, name)[0]
            self.assertIn(marker, entry['signature'])
            self.assertEqual('arg', entry['parameters'][-1]['name'])
            self.assertEqual(value_type, entry['parameters'][-1]['type'])
        self.assertEqual('int', self.named(entries, 'named')[0]['returns'])

    def test_api_add_delete_and_package_removal(self):
        before, _, _ = self.read()
        self.source.write_text('export func replacement() as int\nreturn 2\nend\n', encoding='utf-8')
        after, _, _ = self.read()
        self.assertFalse(self.named(after, 'named'))
        self.assertTrue(self.named(after, 'replacement'))
        self.assertNotEqual({e['id'] for e in before}, {e['id'] for e in after})
        self.manifest.unlink()
        entries, documents, coverage = self.read()
        self.assertEqual(([], [], []), (entries, documents, coverage['packages']))

    def test_private_packages_are_not_commands(self):
        data = json.loads(self.manifest.read_text())
        data['kind'] = 'demo6-private-extlib'
        self.manifest.write_text(json.dumps(data), encoding='utf-8')
        entries, documents, coverage = self.read()
        self.assertEqual(([], [], ['sample']), (entries, documents, coverage['excludedPrivate']))

    def test_invalid_syntax_refuses_partial_result(self):
        self.source.write_text('export func broken(\n', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'parse failed'):
            self.read()

    def test_escape_and_missing_sources_refused(self):
        self.manifest.write_text(json.dumps(dict(name='sample', artifact=dict(xlSources=['../escape.xl']))), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'Invalid declared'):
            self.read()
        self.manifest.write_text(json.dumps(dict(name='sample', artifact=dict(xlSources=['missing.xl']))), encoding='utf-8')
        with self.assertRaises(FileNotFoundError):
            self.read()

    def test_large_class_exports_declarations_without_bodies(self):
        methods = '\n'.join('extern "read_%d" func read%d(int key, string text = "x,y") as int' % (i, i) for i in range(1800))
        self.source.write_text('export class Large\n' + methods + '\nend\n', encoding='utf-8')
        entries, _, _ = self.read()
        self.assertEqual(1802, len(entries))  # module + class + all overload-independent members
        self.assertTrue(self.named(entries, 'Large.read1799'))

    def test_installed_pair_coverage_and_preservation(self):
        if PAIR is None:
            self.skipTest('--pair is required for publication checks')
        verify(PAIR / 'catalog-sources', PAIR / 'commands.json')
        catalog = json.loads((PAIR / 'commands.json').read_text(encoding='utf-8'))
        before = json.loads((BASE / 'commands.json').read_text(encoding='utf-8'))
        self.assertEqual([e for e in before['entries'] if e['category'] != 'extension'],
                         [e for e in catalog['entries'] if e['category'] != 'extension'])
        self.assertEqual(before['compilerSha256'], catalog['compilerSha256'])
        expected = {json.loads(p.read_text(encoding='utf-8-sig'))['name'] for p in (IDE.parent / 'release/extlibs').glob('*/xlang.package.json')
                    if json.loads(p.read_text(encoding='utf-8-sig'))['kind'] != 'demo6-private-extlib'}
        self.assertEqual(expected, {e['name'] for e in catalog['entries'] if e['category'] == 'extension' and e['kind'] == 'module'})
        self.assertEqual(len(catalog['entries']), len({e['id'] for e in catalog['entries']}))
        self.assertEqual('current', check(IDE.parent, PAIR)['status'])
        lines = {}
        index = json.loads((PAIR / 'catalog-sources/index.json').read_text(encoding='utf-8'))
        for row in index['entries']:
            lines[row['path']] = len((PAIR / 'catalog-sources' / (row['sha256'] + '.source')).read_text(encoding='utf-8-sig').splitlines())
        for e in catalog['entries'] + catalog['documents']:
            self.assertTrue(1 <= e['sourceLine'] <= lines[e['source']], e['id'])

    def test_freshness_rejects_mutated_source_manifest_readme_frontend(self):
        if PAIR is None:
            self.skipTest('--pair is required for freshness checks')
        catalog = json.loads((PAIR / 'commands.json').read_text(encoding='utf-8'))
        metadata = catalog['extensionCatalog']
        root = self.work / 'controlled-root'
        for relative in metadata['inputs'].keys() | metadata['frontendInputs'].keys():
            destination = root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(IDE.parent / relative, destination)
        self.assertEqual('current', check(root, PAIR)['status'])
        for suffix, collection in (('.xl', 'inputs'), ('xlang.package.json', 'inputs'), ('readme.md', 'inputs'), ('.c', 'frontendInputs')):
            name = next(n for n in metadata[collection] if n.lower().endswith(suffix))
            path = root / name
            original = path.read_bytes()
            path.write_bytes(original + b'\n')
            with self.assertRaisesRegex(ValueError, 'stale'):
                check(root, PAIR)
            path.write_bytes(original)
        self.assertEqual('current', check(root, PAIR)['status'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exporter', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--pair', type=Path)
    parser.add_argument('--base', type=Path, default=IDE / 'data')
    args = parser.parse_args()
    global EXPORTER, OUTPUT, PAIR, BASE
    EXPORTER, OUTPUT = args.exporter.resolve(strict=True), args.output.resolve()
    PAIR, BASE = args.pair.resolve() if args.pair else None, args.base.resolve()
    OUTPUT.mkdir(parents=True, exist_ok=False)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(ExtensionCatalogTests))
    report = dict(status='passed' if result.wasSuccessful() else 'failed', tests=result.testsRun,
                  skipped=len(result.skipped), failures=result.failures, errors=result.errors,
                  exporterSha256=sha(EXPORTER))
    report['failures'] = [(str(test), message) for test, message in result.failures]
    report['errors'] = [(str(test), message) for test, message in result.errors]
    (OUTPUT / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()
