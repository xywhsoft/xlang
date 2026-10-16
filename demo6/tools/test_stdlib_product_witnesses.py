"""Negative controls for the public-signature/product evidence association."""
from pathlib import Path
import copy
import hashlib
import json
import os
import tempfile
import unittest
import check_stdlib_product_witnesses as audit


def sample():
    source = '/evidence/stdlib_array_witness.xl'
    signature = dict(Name='operation', Fixtures=['stdlib_array_witness'],
                     ReachableFunctionFixtures=['stdlib_array_witness'])
    census = dict(DiscoveryComplete=True, UsesActualCompilerSO=True, CompilerSourceObjects=False,
                  ReachableFunctionBindingAcceptance=True, CallReachabilityControl=True,
                  RuntimeExecutionAcceptance=False, Full53Acceptance=False,
                  NativeBindings=[copy.deepcopy(signature)], StaticBindings=[copy.deepcopy(signature)],
                  ClassMembers=[dict(signature, Category='callable')],
                  NativeExpectedCount=1, NativeBoundCount=1, StaticExpectedCount=1, StaticBoundCount=1,
                  ClassCallableCount=1, ClassReachableCount=1,
                  Cases=[dict(Name='stdlib_array_witness', Row=dict(Command=['observer', source]))],
                  Inputs=[dict(Path=source, Sha256='source')])
    for key in ('MissingNativeBindings', 'UnexpectedNativeBindings', 'MissingStaticBindings', 'MissingClassBindings',
                'UnreachableNativeBindings', 'UnreachableStaticBindings', 'UnreachableClassBindings'):
        census[key] = []
    reports = {}
    for lane in audit.LANES:
        names = ('xl.exe', 'xl.dll', 'xlc.dll') if lane.startswith('windows') else ('xl', 'libxl.so', 'libxlc.so')
        inputs = [dict(Path='/release/'+name, Sha256=name) for name in names]
        if lane.startswith('linux') and not any(r['Path'] == '/release/libxlc.so' for r in census['Inputs']):
            census['Inputs'].extend(copy.deepcopy(inputs))
        inputs.append(dict(Path=source, Sha256='source'))
        aot = lane.endswith('-aot')
        case = dict(Name='stdlib_array_witness', Status='passed', Executed=True, Passed=True,
                    Rows=[dict(Name='stdlib_array_witness'+suffix, Exit=0) for suffix in
                          ('-build', '-run') if aot] or
                         [dict(Name='stdlib_array_witness'+suffix, Exit=0) for suffix in ('-run', '-module')])
        if aot:
            case.update(Image=dict(Path='/relocated/program', Sha256='program'), Dependencies=[])
        reports[lane] = dict(AllExecutablePassed=True, CandidateCount=1, SelectedCount=1,
                             Full53Acceptance=False, FunctionCoverageAcceptance=False,
                             RelocatedAotInventoryAcceptance=True, SourceProductAcceptance=True,
                             EmbeddedVfsWithoutFallback=True, ProviderOverrides=False, Cases=[case], Inputs=inputs)
    return census, reports


@unittest.skipIf(os.name == 'nt', 'Run the WSL evidence-audit controls under Linux')
class ProductWitnessTests(unittest.TestCase):
    def setUp(self):
        self.census, self.reports = sample()

    def missing(self):
        witnesses, missing, _, _ = audit.associate(self.census, self.reports)
        self.assertFalse(witnesses)
        self.assertEqual(len(missing), 3)

    def test_identical_executed_fixture(self):
        witnesses, missing, counts, population = audit.associate(self.census, self.reports)
        self.assertEqual((len(witnesses), missing, counts, population), (3, [], dict(native=1, static=1, **{'class':1}), 1))

    def test_uncalled_binding_is_not_a_witness(self):
        for binding in self.census['NativeBindings']+self.census['StaticBindings']+self.census['ClassMembers']:
            binding['ReachableFunctionFixtures'] = ['merely_declared']
        self.missing()

    def test_host_requirement_is_not_execution(self):
        self.reports['windows-products']['Cases'][0]['Status'] = 'requires-host'
        self.missing()

    def test_platform_requirement_is_not_execution(self):
        self.reports['linux-aot']['Cases'][0]['Status'] = 'requires-platform'
        self.missing()

    def test_false_executed_flag(self):
        self.reports['windows-products']['Cases'][0]['Executed'] = False
        with self.assertRaises(AssertionError):
            audit.associate(self.census, self.reports)

    def test_module_must_really_run(self):
        self.reports['linux-products']['Cases'][0]['Rows'].pop()
        with self.assertRaises(AssertionError):
            audit.associate(self.census, self.reports)

    def test_failed_command_is_not_success(self):
        self.reports['windows-aot']['Cases'][0]['Rows'][0]['Exit'] = 1
        with self.assertRaises(AssertionError):
            audit.associate(self.census, self.reports)

    def test_different_source_is_not_same_fixture(self):
        self.reports['windows-aot']['Inputs'][-1]['Sha256'] = 'different source'
        self.missing()

    def test_different_products(self):
        self.reports['linux-products']['Inputs'][0]['Sha256'] = 'different product'
        with self.assertRaises(AssertionError):
            audit.associate(self.census, self.reports)

    def test_different_populations(self):
        self.reports['windows-aot']['Cases'].clear()
        with self.assertRaises(AssertionError):
            audit.associate(self.census, self.reports)

    def test_aot_runtime_dependency(self):
        self.reports['windows-aot']['Cases'][0]['Dependencies'] = ['xl.dll']
        with self.assertRaises(AssertionError):
            audit.associate(self.census, self.reports)

    def test_no_full53_or_branch_claim(self):
        self.reports['linux-products']['FunctionCoverageAcceptance'] = True
        with self.assertRaises(AssertionError):
            audit.associate(self.census, self.reports)

    def test_missing_reachable_signature(self):
        self.census['UnreachableStaticBindings'] = [{}]
        with self.assertRaises(AssertionError):
            audit.associate(self.census, self.reports)

    def test_paths_are_resolved_not_rewritten(self):
        self.assertEqual(audit.physical('D:\\reports\\results.json'), Path('/mnt/d/reports/results.json'))
        with self.assertRaises(AssertionError):
            audit.physical('relative/results.json')

    def test_tampered_log_and_subset_reports(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            log = root/'run.log'
            log.write_bytes(b'actual')
            report = root/'results.json'
            data = dict(Complete=True, InputsUnchanged=True, Failures=[], Inputs=[],
                        Rows=[dict(Log=str(log), LogSha256=hashlib.sha256(b'not actual').hexdigest())])
            report.write_text(json.dumps(data), encoding='utf-8')
            with self.assertRaises(AssertionError):
                audit.Evidence().report(report)
            data.update(Rows=[], ExplicitSubset=True)
            report.write_text(json.dumps(data), encoding='utf-8')
            with self.assertRaises(AssertionError):
                audit.Evidence().report(report)

    def test_scoped_child_is_not_a_complete_population(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory).resolve()/'results.json'
            report.write_text(json.dumps(dict(Complete=True, InputsUnchanged=True, ExplicitSubset=True,
                                              Inputs=[], Rows=[], Failures=[])), encoding='utf-8')
            evidence = audit.Evidence()
            self.assertTrue(evidence.report(report, population=False)['Complete'])
            # A cached scoped report still cannot later stand for the full population.
            with self.assertRaises(AssertionError):
                evidence.report(report)


if __name__ == '__main__':
    unittest.main()
