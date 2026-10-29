from pathlib import Path
import json
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from compiler_response import command_with_response


class CompilerResponse(unittest.TestCase):
    def test_real_long_command_and_quoting(self):
        compiler = shutil.which('gcc')
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory(prefix='xlang response ') as temporary:
            root = Path(temporary)
            source = root / 'input space.c'
            binary = root / ('output.exe' if sys.platform == 'win32' else 'output')
            source.write_text('#include <stdio.h>\nint main(void){puts(PROBE);return 0;}\n', encoding='ascii')
            define = '-DPROBE="back\\\\slash \\"quoted\\""'
            command = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', define]
            command += ['-I' + str(root / ('unused_include_' + str(i).zfill(4))) for i in range(800)]
            command += [source, '-o', binary]
            self.assertGreater(sum(len(str(value)) + 1 for value in command), 32767)
            response = root / 'compile space.rsp'
            result = subprocess.run(command_with_response(command, compiler, response), capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
            output = subprocess.check_output([binary]).decode().strip()
            self.assertEqual(output, 'back\\slash "quoted"')
            receipt = json.loads(Path(str(response) + '.json').read_text())
            self.assertEqual(receipt['Arguments'], list(map(str, command[1:])))
            with self.assertRaises(FileExistsError):
                command_with_response(command, compiler, response)

    def test_invalid_and_non_compiler_commands(self):
        with tempfile.TemporaryDirectory(prefix='xlang_response_') as temporary:
            output = Path(temporary) / 'invalid.rsp'
            for invalid in ('@nested.rsp', 'bad\nargument', 'bad\0argument'):
                with self.assertRaises(ValueError):
                    command_with_response([sys.executable, invalid], sys.executable, output)
                self.assertFalse(output.exists())
            command = [str(Path(temporary) / 'program.exe'), 'normal']
            self.assertEqual(command_with_response(command, sys.executable, output), command)
            self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
