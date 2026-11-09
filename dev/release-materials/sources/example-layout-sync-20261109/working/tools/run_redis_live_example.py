"""Run redis_live against a disposable authenticated local Redis, never an existing DB."""
from pathlib import Path
import argparse, json, os, secrets, shutil, socket, subprocess, time
from runtime_layout import runtime_directory, runtime_example

def command(stream, *parts):
    payload = ('*%d\r\n' % len(parts)).encode()
    for value in parts:
        value = str(value).encode()
        payload += ('$%d\r\n' % len(value)).encode() + value + b'\r\n'
    stream.sendall(payload)
    return stream.recv(1024)

def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, default=runtime_directory(root))
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--redis-server', help='Native redis-server executable')
    args = parser.parse_args()
    source = runtime_example(args.runtime.resolve(), 'extlibs/redis_live.xl')
    if args.list:
        print(json.dumps([str(source)])); return 0
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    runtime = args.runtime.resolve(); xl = runtime / ('xl.exe' if os.name == 'nt' else 'xl')
    report = {'passed': False, 'skipped': False, 'runtime': str(xl)}
    launcher = []; server = args.redis_server or shutil.which('redis-server')
    if not server and os.name == 'nt' and shutil.which('wsl.exe'):
        try:
            found = subprocess.run(['wsl.exe', '-d', 'Ubuntu', '-e', 'sh', '-c', 'command -v redis-server'], capture_output=True, timeout=8)
            if found.returncode == 0 and found.stdout.strip():
                server = found.stdout.decode().strip(); launcher = ['wsl.exe', '-d', 'Ubuntu', '-e']
        except (OSError, subprocess.TimeoutExpired): pass
    if not server:
        report.update(skipped=True, reason='redis-server unavailable; no external instance contacted')
        (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print('SKIP: redis-server unavailable'); return 0
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0)); port = reservation.getsockname()[1]
    password = secrets.token_hex(24)
    process = None; authenticated = False
    try:
        with (out / 'redis.log').open('wb') as log:
            process = subprocess.Popen([*launcher, server, '--bind', '127.0.0.1', '--port', str(port), '--save', '', '--appendonly', 'no', '--protected-mode', 'yes', '--requirepass', password], stdout=log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if process.poll() is not None: raise RuntimeError('private Redis exited during startup')
                try:
                    with socket.create_connection(('127.0.0.1', port), timeout=.3) as stream:
                        authenticated = command(stream, 'AUTH', password).startswith(b'+OK')
                        if authenticated and command(stream, 'PING').startswith(b'+PONG'): break
                except OSError: pass
                time.sleep(.05)
            else: raise RuntimeError('private Redis readiness timeout')
            if not authenticated: raise RuntimeError('private fixture authentication failed')
            env = os.environ.copy(); env.update(REDIS_LIVE_PORT=str(port), REDIS_LIVE_PASSWORD=password, REDIS_LIVE_PREFIX='xlang:fixture:' + secrets.token_hex(12))
            result = subprocess.run([str(xl), 'run', str(source)], env=env, capture_output=True, timeout=30, cwd=runtime)
            (out / 'example.log').write_bytes(result.stdout + result.stderr)
            binary = out / ('redis-live.exe' if os.name == 'nt' else 'redis-live')
            built = subprocess.run([str(xl), 'build', str(source), '-o', str(binary)], env=env, capture_output=True, timeout=120, cwd=runtime)
            (out / 'build.log').write_bytes(built.stdout + built.stderr)
            aot_code = None
            if built.returncode == 0:
                aot = subprocess.run([str(binary)], env=env, capture_output=True, timeout=30, cwd=runtime)
                (out / 'aot.log').write_bytes(aot.stdout + aot.stderr)
                aot_code = aot.returncode
            report.update(passed=result.returncode == 0 and built.returncode == 0 and aot_code == 0,
                          exitCode=result.returncode, buildExitCode=built.returncode, aotExitCode=aot_code,
                          fixture='private authenticated Redis', port=port)
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        report['error'] = str(error)
    finally:
        if process is not None:
            try:
                with socket.create_connection(('127.0.0.1', port), timeout=1) as stream:
                    if command(stream, 'AUTH', password).startswith(b'+OK'):
                        command(stream, 'SHUTDOWN', 'NOSAVE')
            except OSError: pass
            if launcher and process.poll() is None:
                # Authenticate the exact private WSL instance if localhost forwarding failed.
                try:
                    subprocess.run([*launcher, 'redis-cli', '--no-auth-warning', '-a', password, '-p', str(port), 'SHUTDOWN', 'NOSAVE'], capture_output=True, timeout=3)
                except (OSError, subprocess.TimeoutExpired) as error:
                    report['cleanupError'] = str(error)
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()
                try: process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait(timeout=3)
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS: private Redis example' if report['passed'] else 'FAIL: see ' + str(out / 'report.json'))
    return 0 if report['passed'] else 1
if __name__ == '__main__': raise SystemExit(main())
