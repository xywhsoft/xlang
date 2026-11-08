"""Admit the complete current compiler closure and retain exact historical inputs."""
from pathlib import Path
import argparse,json,hashlib,shutil
import native_final_c_interop_profile as native


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frontend',type=Path,required=True)
    parser.add_argument('--checkout',type=Path,required=True)
    parser.add_argument('--previous',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();source=args.frontend.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    record=dict(status='running',cases=[])
    def save():(out/'result.json').write_bytes((json.dumps(record,indent=2)+'\n').encode())
    def passed(name):record['cases'].append(name);save()
    protected=native.profile(source);previous=native.profile(args.previous.resolve())
    try:
        assert len(protected)==457 and len(previous)==455
        assert native.source_revision(source)=='current-core-20261108'
        assert native.completion_facts(source)
        assert set(native.CURRENT_PINS)-set(native.PINS)=={'src/package/xl6_package_native_link.inc','src/package/xl6_package_native_variants.inc'}
        passed('complete-new-closure-and-old-cohort')
        projected,meta=native.prepare(args.checkout.resolve(),out/'checkout-prepared')
        assert meta['sourceRevision']=='current-core-20261108'
        assert native.profile(projected)=={str(projected/p):h for p,h in native.CURRENT_PINS.items()}
        assert all(sha(p)==h for p,h in meta['originalInputs'].items())
        passed('actual-checkout-eol-and-sdk-projection')
        fixture=out/'fixture';shutil.copytree(source,fixture)
        for relative in ['src/package/xl6_package_native_link.inc','src/package/xl6_package_native_variants.inc','src/package/xl6_package.h','src/ir/xl6_ir.h','src/sem/xl6_sem.c','lib/xrt/xrt.h']:
            path=fixture/relative;before=path.read_bytes()
            try:
                path.write_bytes(before+b'\n/* unknown dependency */\n')
                try:native.profile(fixture)
                except ValueError:passed('reject-unknown-'+relative)
                else:raise AssertionError(relative+' admitted')
            finally:path.write_bytes(before)
        path=fixture/'src/package/xl6_package.h';before=path.read_bytes()
        try:
            path.write_bytes((args.previous/'src/package/xl6_package.h').read_bytes())
            try:native.profile(fixture)
            except ValueError:passed('reject-old-package-layout-in-current-cohort')
            else:raise AssertionError('mixed package ABI admitted')
        finally:path.write_bytes(before)
        for relative in native.CURRENT_PINS:
            path=fixture/relative;path.write_bytes(path.read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))
        projected,meta=native.prepare(fixture,out/'crlf-prepared')
        assert len(meta['originalInputs'])==458
        assert all(sha(projected/p)==h for p,h in native.CURRENT_PINS.items())
        passed('whole-crlf-checkout-restores-exact-known-inputs')
        original_write=Path.write_bytes;mutated=[];victim=fixture/'src/package/xl6_package_native_variants.inc';before=victim.read_bytes()
        def drift(path,data):
            result=original_write(path,data)
            if not mutated and path.is_relative_to(out/'drifted'):
                original_write(victim,before+b'\n/* source drift */\n');mutated.append(True)
            return result
        try:
            Path.write_bytes=drift
            try:native.prepare(fixture,out/'drifted')
            except ValueError:
                assert mutated;passed('reject-drift-during-projection')
            else:raise AssertionError('source drift admitted')
        finally:Path.write_bytes=original_write;victim.write_bytes(before)
        assert all(sha(p)==h for p,h in {**protected,**previous}.items())
        record['status']='passed'
    finally:save()
    print(json.dumps(dict(status=record['status'],cases=len(record['cases']))))


if __name__=='__main__':main()
