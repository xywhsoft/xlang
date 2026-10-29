"""Preserve logical compiler argv in a GCC response file for test drivers."""
from pathlib import Path
import hashlib
import json


def command_with_response(command, compiler, output):
    command = list(map(str, command))
    if Path(command[0]).resolve() != Path(compiler).resolve():
        return command
    arguments = command[1:]
    if any('\0' in value or '\n' in value or '\r' in value or value.startswith('@') for value in arguments):
        raise ValueError('Invalid or nested compiler response argument')
    output = Path(output)
    text = '\n'.join('"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"' for value in arguments) + '\n'
    with output.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(text)
    receipt = dict(Compiler=command[0], Arguments=arguments, ResponseFile=str(output),
                   ResponseSha256=hashlib.sha256(output.read_bytes()).hexdigest())
    with Path(str(output) + '.json').open('x', encoding='utf-8') as stream:
        json.dump(receipt, stream, indent=2)
        stream.write('\n')
    return [command[0], '@' + str(output)]
