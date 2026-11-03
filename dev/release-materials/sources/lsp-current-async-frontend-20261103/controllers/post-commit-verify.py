from pathlib import Path
import subprocess,hashlib,json,gzip,sys
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build
sha=lambda raw:hashlib.sha256(raw).hexdigest()
read=lambda p:json.loads(Path(p).read_text(encoding='utf8'))
report=read(R/'dev/release-materials/provenance/lsp-current-async-frontend-20261103.json')
head='' if '--staged' in sys.argv else subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip()
def blob(revision,path):return subprocess.check_output(['git','show',revision+':'+path],cwd=R)
for row in report['sourceSnapshots']:
 raw=(R/row['archivePath']).read_bytes();assert sha(raw)==row['sha256'];assert blob(head,row['archivePath'])==raw
 archive=Path(row['archivePath']).parts
 if 'after' in archive:
  rel=Path(*archive[archive.index('after')+1:]).as_posix();assert blob(head,rel).replace(b'\r\n',b'\n')==raw.replace(b'\r\n',b'\n'),rel
 if 'before' in archive:
  rel=Path(*archive[archive.index('before')+1:]).as_posix();assert blob(report['sourceBase'],rel).replace(b'\r\n',b'\n')==raw.replace(b'\r\n',b'\n'),rel
for row in report['reportArchives']:
 packed=(R/row['archivePath']).read_bytes();assert sha(packed)==row['archiveSha256'];assert blob(head,row['archivePath'])==packed
 raw=gzip.decompress(packed);assert sha(raw)==row['originalSha256'] and raw==Path(row['recordedPath']).read_bytes()
B=Path(report['build']['path']);normal=read(B/'build-result.json');verify_build(normal,B)
base=report['baseline'];A=Path(base['path']);a=read(A/'build-result.json');aliases={r['originalPath']:r['archivedPath'] for r in base['archivedChangedInputs']}
for path,digest in a['inputs'].items():assert sha(Path(aliases.get(path,path)).read_bytes())==digest,path
for name,digest in a['outputs'].items():assert sha((A/name).read_bytes())==digest,name
for path,digest in report['protectedRuntime'].items():assert sha(Path(path).read_bytes())==digest,path
result=dict(status='passed',commit=head,rawGitSnapshotBlobs=len(report['sourceSnapshots']),gzipGitReportBlobs=len(report['reportArchives']),normalInputs=len(normal['inputs']),normalOutputs=len(normal['outputs']),baselineInputs=len(a['inputs']),baselineOutputs=len(a['outputs']),rootRuntimeSettingsAndDeliveryIndexUnchanged=True)
(O/('pre-commit-verify.json' if not head else 'post-commit-verify.json')).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
