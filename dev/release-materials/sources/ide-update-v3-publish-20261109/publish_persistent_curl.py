"""Resume the existing transaction with a persistent, fully checked HTTPS batch."""
from pathlib import Path
import io
import json
import re
import shlex
import sys
import tarfile
import tempfile
import urllib.parse

ROOT = Path('D:/GIT/x-lang')
sys.path.insert(0, str(ROOT / 'ide/tools/update'))
import update_manager as manager


def verify_public_batch(remote, stage, names, specs):
    parsed = urllib.parse.urlsplit(remote.url)
    assert parsed.scheme == 'https' and parsed.hostname == 'xcompiler.com'
    assert all(manager.HEX.fullmatch(name) for name in names)
    config = ''.join('url = ' + json.dumps(remote.url + '/update/' + name) + '\noutput = ' + json.dumps(name) + '\n' for name in names).encode('ascii')
    hashes = ''.join(specs[name]['sha256'] + '  ' + name + '\n' for name in names).encode('ascii')
    sizes = ''.join(str(specs[name]['size']) + ' ' + name + '\n' for name in names).encode('ascii')
    with tempfile.TemporaryFile() as stream:
        with tarfile.open(fileobj=stream, mode='w') as archive:
            for name, raw in (('curl.conf', config), ('.checksums', hashes), ('.sizes', sizes)):
                info = tarfile.TarInfo(name)
                info.size = len(raw)
                info.mode = 0o600
                archive.addfile(info, io.BytesIO(raw))
        stream.seek(0)
        script = '''set -eu
umask 077
verify=$(mktemp -d STAGE/public-keepalive.XXXXXX)
trap 'rm -f -- "$verify"/*.lzma "$verify/curl.conf" "$verify/.checksums" "$verify/.sizes"; rmdir -- "$verify"' EXIT
tar --no-same-owner --no-same-permissions -xf - -C "$verify"
cd "$verify"
curl --noproxy '*' --http2 -fsS --retry 5 --retry-all-errors --retry-delay 1 --connect-timeout 10 --max-time 120 --config curl.conf
sha256sum -c -- .checksums > /dev/null
count=0
while read -r size name; do
  test -f "$name" && test ! -L "$name"
  test "$(wc -c < "$name")" = "$size"
  count=$((count+1))
done < .sizes
printf 'VERIFIED %s\\n' "$count"
'''.replace('STAGE', shlex.quote(stage))
        result = remote.run(script, file=stream)
    assert result.strip() == 'VERIFIED ' + str(len(names)), result
    print('VERIFY PUBLIC SERVER PERSISTENT HTTPS ' + str(len(names)) + '/' + str(len(names)), flush=True)


manager.verify_public_on_server = verify_public_batch
raise SystemExit(manager.main(['push', '--platform', 'win_x64', '--verify-public-on-server']))
