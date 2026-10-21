"""Drive the C and XLang serial examples through a real POSIX PTY."""

import os
import pty
import select
import subprocess
import sys


def check(command, cwd):
    master, slave = pty.openpty()
    path = os.ttyname(slave)
    process = subprocess.Popen(
        command + [path], cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    received = b""
    try:
        while len(received) < 4:
            readable, _, _ = select.select([master], [], [], 5)
            if not readable:
                raise RuntimeError(f"device write timed out: {command}")
            received += os.read(master, 4 - len(received))
        if received != b"PING":
            raise RuntimeError(f"unexpected device request: {received!r}")
        os.write(master, b"PONG")
        stdout, stderr = process.communicate(timeout=10)
        if process.returncode != 0 or stdout != b"serial device: PASS\n":
            raise RuntimeError(
                f"{command}: exit={process.returncode}, out={stdout!r}, err={stderr!r}"
            )
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate()
        os.close(master)
        os.close(slave)


if __name__ == "__main__":
    release, c_example = sys.argv[1:3]
    check([c_example], release)
    if len(sys.argv) > 3:
        check([sys.argv[3]], release)
        check([os.path.join(release, "xl"), "run", "extlibs/serial/examples/device_c.c", "--"], release)
    check([os.path.join(release, "xl"), "run", "extlibs/serial/examples/device.xl", "--"], release)
    print("serial C/XLang PTY examples: PASS")
