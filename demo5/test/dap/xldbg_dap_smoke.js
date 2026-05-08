const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn, spawnSync } = require('child_process');

const repoRoot = path.resolve(__dirname, '..', '..');
const xldbgPath = path.resolve(repoRoot, process.argv[2] || 'release/xldbg.exe');

function assert(cond, message) {
  if (!cond) {
    throw new Error(message);
  }
}

function delay(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

class DapSession {
  constructor() {
    this.child = spawn(xldbgPath, ['dap'], {
      cwd: repoRoot,
      stdio: ['pipe', 'pipe', 'pipe'],
    });
    this.seq = 1;
    this.buf = Buffer.alloc(0);
    this.messages = [];
    this.stderr = '';
    this.stdoutPollution = '';
    this.child.stdout.on('data', data => {
      this.buf = Buffer.concat([this.buf, data]);
      this.pump();
    });
    this.child.stderr.on('data', data => {
      this.stderr += data.toString('utf8');
    });
  }

  send(command, args = {}) {
    const requestSeq = this.seq++;
    const body = JSON.stringify({
      seq: requestSeq,
      type: 'request',
      command,
      arguments: args,
    });
    this.child.stdin.write(`Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`);
    return requestSeq;
  }

  sendRawBody(body) {
    this.child.stdin.write(`Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`);
  }

  pump() {
    while (this.buf.length !== 0) {
      const text = this.buf.toString('utf8');
      if (!text.startsWith('Content-Length:')) {
        if ('Content-Length:'.startsWith(text)) {
          return;
        }
        this.stdoutPollution = text;
        return;
      }
      const headerEnd = text.indexOf('\r\n\r\n');
      if (headerEnd < 0) {
        return;
      }
      const header = text.slice(0, headerEnd);
      const match = /^Content-Length: (\d+)/.exec(header);
      if (!match) {
        this.stdoutPollution = text;
        return;
      }
      const bodyStart = Buffer.byteLength(text.slice(0, headerEnd + 4));
      const bodyLen = Number(match[1]);
      if (this.buf.length < bodyStart + bodyLen) {
        return;
      }
      const raw = this.buf.slice(bodyStart, bodyStart + bodyLen).toString('utf8');
      this.messages.push(JSON.parse(raw));
      this.buf = this.buf.slice(bodyStart + bodyLen);
    }
  }

  async waitFor(predicate, timeoutMs = 7000) {
    const started = Date.now();
    while ((Date.now() - started) < timeoutMs) {
      assert(!this.stdoutPollution, `non-DAP stdout: ${this.stdoutPollution}`);
      const message = this.messages.find(predicate);
      if (message) {
        return message;
      }
      await delay(20);
    }
    throw new Error(`timeout; messages=${JSON.stringify(this.messages)}; stderr=${this.stderr}`);
  }

  async initialize() {
    this.send('initialize');
    const response = await this.waitFor(m => m.type === 'response' && m.command === 'initialize');
    assert(response.success === true, 'initialize should succeed');
    await this.waitFor(m => m.type === 'event' && m.event === 'initialized');
    this.send('configurationDone');
    const config = await this.waitFor(m => m.type === 'response' && m.command === 'configurationDone');
    assert(config.success === true, 'configurationDone should succeed');
  }

  async waitForExit(timeoutMs = 7000) {
    if (this.child.exitCode !== null) {
      return this.child.exitCode;
    }
    return await new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        reject(new Error(`timeout waiting for exit; stderr=${this.stderr}`));
      }, timeoutMs);
      this.child.once('exit', code => {
        clearTimeout(timer);
        resolve(code);
      });
    });
  }

  close() {
    if (!this.child.killed) {
      this.child.kill('SIGKILL');
    }
  }
}

async function withSession(fn) {
  const session = new DapSession();
  try {
    await session.initialize();
    await fn(session);
  } finally {
    session.close();
  }
}

function writeTempProgram(name, text) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'xldbg-dap-'));
  const file = path.join(dir, name);
  fs.writeFileSync(file, text);
  return file;
}

async function waitForStoppedCount(session, count) {
  await session.waitFor(
    () => session.messages.filter(m => m.type === 'event' && m.event === 'stopped').length >= count
  );
  return session.messages.filter(m => m.type === 'event' && m.event === 'stopped')[count - 1];
}

async function testNormalExit() {
  await withSession(async session => {
    session.send('launch', { program: path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl') });
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === true, 'launch should succeed');
    const exited = await session.waitFor(m => m.type === 'event' && m.event === 'exited');
    assert(exited.body.exitCode === 42, 'normal exit should report exitCode=42');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
  });
}

async function testXldbgRunRegression() {
  const result = spawnSync(xldbgPath, ['run', 'release/examples/full/bit_ops_basic.xl'], {
    cwd: repoRoot,
    encoding: 'utf8',
  });
  assert(result.status === 0, 'xldbg run should exit 0');
  assert(result.stdout.includes('RunResult exitCode=42'), 'xldbg run should report exitCode=42');
  assert(!result.stdout.includes('xl_dbg_'), 'xldbg run should not expose debug probes');
  assert(result.stderr === '', 'xldbg run should not write stderr for bit_ops_basic');
}

async function testXldbgDbgRepl() {
  const result = spawnSync(xldbgPath, ['dbg', 'release/examples/full/bit_ops_basic.xl'], {
    cwd: repoRoot,
    input: 'break 3\nrun\nbt\nlocals\ncontinue\nquit\n',
    encoding: 'utf8',
  });
  assert(result.status === 0, `xldbg dbg should exit cleanly, got ${result.status}\n${result.stderr}`);
  assert(result.stdout.includes('breakpoint 1'), 'xldbg dbg should confirm the breakpoint');
  assert(result.stdout.includes('stopped reason=breakpoint'), 'xldbg dbg should stop at the breakpoint');
  assert(result.stdout.includes('#0 main'), 'xldbg dbg should print a stack frame');
  assert(result.stdout.includes('mask = 10 (int)'), 'xldbg dbg should print current locals');
  assert(result.stdout.includes('exited 42'), 'xldbg dbg should continue to normal exit');
  assert(!/Content-Length:/i.test(result.stdout), 'xldbg dbg stdout must not contain DAP frames');
}

async function testCompileFailure() {
  const program = writeTempProgram('bad_launch.xl', 'func main() as int\n\treturn missing_value\nend\n');
  await withSession(async session => {
    session.send('launch', { program });
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === false, 'compile failure launch should fail');
    await delay(250);
    assert(
      !session.messages.some(m => m.type === 'event' && m.event === 'terminated'),
      'compile failure should not start and terminate a debuggee'
    );
    assert(session.stderr.includes('[error][sem]'), 'compile failure should keep diagnostics on stderr');
  });
}

async function testHostRuntimeFailureOutput() {
  const program = writeTempProgram(
    'missing_loadlib.xl',
    '#loadlib "xldbg_dap_missing_runtime_library.dll"\n' +
      'func main() as int\n' +
      '\treturn 42\n' +
      'end\n'
  );
  await withSession(async session => {
    session.send('launch', { program });
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === true, 'host runtime failure should launch before load/link failure');

    const output = await session.waitFor(m => m.type === 'event' && m.event === 'output');
    assert(output.body.category === 'stderr', 'host runtime failure output should use stderr category');
    assert(output.body.output.includes('xldbg launch failed'), 'host runtime failure should report launch failure text');
    assert(output.body.output.includes('host runtime failure'), 'host runtime failure should identify runtime/link failure');
    assert(output.body.output.includes('loadlib.'), 'host runtime failure should include trace details');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
    assert(
      !session.messages.some(m => m.type === 'event' && m.event === 'exited'),
      'host runtime failure should not emit normal exited event'
    );
  });
}

async function testExceptionStop() {
  const program = path.join(repoRoot, 'release/examples/full/throw_basic.xl');
  await withSession(async session => {
    session.send('launch', { program });
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === true, 'exception launch should succeed before runtime throw');

    const stopped = await session.waitFor(m => m.type === 'event' && m.event === 'stopped');
    assert(stopped.body.reason === 'exception', 'uncaught throw should stop with exception reason');
    assert(
      stopped.body.text && stopped.body.text.includes('boom'),
      'exception stopped event should include readable exception text'
    );

    const stackSeq = session.send('stackTrace', { threadId: 1 });
    const stack = await session.waitFor(m => m.type === 'response' && m.request_seq === stackSeq);
    assert(stack.success === true, 'stackTrace after exception should succeed');
    assert(stack.body.stackFrames.length === 1, 'exception stackTrace should have one fallback frame');
    assert(stack.body.stackFrames[0].line === 2, 'exception stack frame should report throw line');
    assert(
      stack.body.stackFrames[0].source.path.endsWith('throw_basic.xl'),
      'exception stack frame should report throw source path'
    );

    const contSeq = session.send('continue', { threadId: 1 });
    const cont = await session.waitFor(m => m.type === 'response' && m.request_seq === contSeq);
    assert(cont.success === true, 'continue after exception stop should succeed');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
    assert(
      !session.messages.some(m => m.type === 'event' && m.event === 'exited'),
      'uncaught exception should not emit normal exited event'
    );
  });
}

async function testThreadsAndExceptionBreakpoints() {
  await withSession(async session => {
    session.send('setExceptionBreakpoints', { filters: [] });
    const exceptions = await session.waitFor(m => m.type === 'response' && m.command === 'setExceptionBreakpoints');
    assert(exceptions.success === true, 'setExceptionBreakpoints should succeed');
    assert(Array.isArray(exceptions.body.breakpoints), 'setExceptionBreakpoints should return breakpoints array');
    assert(exceptions.body.breakpoints.length === 0, 'setExceptionBreakpoints should return empty breakpoints array');

    session.send('threads');
    const threads = await session.waitFor(m => m.type === 'response' && m.command === 'threads');
    assert(threads.success === true, 'threads should succeed');
    assert(threads.body.threads.length === 1, 'threads should return one thread');
    assert(threads.body.threads[0].id === 1, 'threads should return thread id 1');
    assert(threads.body.threads[0].name === 'main', 'threads should return main thread');
  });
}

async function testUnsupportedRequest() {
  await withSession(async session => {
    session.send('madeUpCommand');
    const response = await session.waitFor(m => m.type === 'response' && m.command === 'madeUpCommand');
    assert(response.success === false, 'unsupported request should fail');
    assert(response.message === 'unsupported request', 'unsupported request should report a clear message');
  });
}

async function testMalformedJsonTerminates() {
  const session = new DapSession();
  try {
    session.sendRawBody('{"seq":1,"type":"request","command":"initialize"');
    const response = await session.waitFor(m => m.type === 'response' && m.request_seq === 1);
    assert(response.success === false, 'malformed JSON should return an error response');
    assert(response.command === 'unknown', 'malformed JSON response should use unknown command');
    assert(response.message === 'invalid JSON', 'malformed JSON should report invalid JSON');
    const exitCode = await session.waitForExit();
    assert(exitCode === 0, 'malformed JSON should terminate the DAP session cleanly');
  } finally {
    session.close();
  }
}

async function testTcpListenUnsupported() {
  const result = spawnSync(xldbgPath, ['dap', '--listen', '127.0.0.1:0'], {
    cwd: repoRoot,
    encoding: 'utf8',
  });
  assert(result.status !== 0, 'dap --listen should fail in V1');
  assert(result.stdout === '', 'dap --listen should not write DAP output to stdout');
  assert(
    result.stderr.includes('TCP DAP listen mode is reserved but not implemented yet'),
    'dap --listen should report reserved TCP mode on stderr'
  );
}

async function testRequestValidationErrors() {
  await withSession(async session => {
    session.send('launch');
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === false, 'launch without program should fail');
    assert(launch.message === 'launch requires program', 'launch without program should report missing program');

    session.send('setBreakpoints', { breakpoints: [{ line: 2 }] });
    const breakpoints = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(breakpoints.success === false, 'setBreakpoints without source.path should fail');
    assert(
      breakpoints.message === 'setBreakpoints requires source.path',
      'setBreakpoints without source.path should report missing source.path'
    );

    for (const command of ['continue', 'next', 'stepIn', 'stepOut']) {
      session.send(command, { threadId: 1 });
      const response = await session.waitFor(m => m.type === 'response' && m.command === command);
      assert(response.success === false, `${command} before stop should fail`);
      assert(response.message === 'debuggee is not stopped', `${command} before stop should report debuggee is not stopped`);
    }
  });
}

async function testBreakpointContinue() {
  const program = path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl');
  await withSession(async session => {
    session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 2 }] });
    const bp = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(bp.success === true, 'setBreakpoints should succeed');
    session.send('launch', { program });
    const stopped = await session.waitFor(m => m.type === 'event' && m.event === 'stopped');
    assert(stopped.body.reason === 'breakpoint', 'breakpoint should stop with breakpoint reason');
    session.send('stackTrace', { threadId: 1 });
    const stack = await session.waitFor(m => m.type === 'response' && m.command === 'stackTrace');
    assert(stack.body.stackFrames[0].line === 2, 'breakpoint stack frame should report line 2');
    session.send('continue', { threadId: 1 });
    const cont = await session.waitFor(m => m.type === 'response' && m.command === 'continue');
    assert(cont.success === true, 'continue should succeed');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
  });
}

async function testVerifiedBreakpointAfterLaunch() {
  const program = path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl');
  await withSession(async session => {
    session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 2 }] });
    const setResponse = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(setResponse.success === true, 'verified breakpoint setup should succeed');
    assert(setResponse.body.breakpoints[0].verified === false, 'pre-launch breakpoint should start unverified');
    assert(
      setResponse.body.breakpoints[0].message === 'breakpoint is not bound until launch',
      'pre-launch breakpoint should explain delayed binding'
    );

    session.send('launch', { program });
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === true, 'verified breakpoint launch should succeed');
    const changed = await session.waitFor(
      m => m.type === 'event' && m.event === 'breakpoint' && m.body.breakpoint.line === 2
    );
    assert(changed.body.reason === 'changed', 'launch should emit breakpoint changed event');
    assert(changed.body.breakpoint.verified === true, 'executable line breakpoint should become verified');
    assert(changed.body.breakpoint.message === '', 'verified breakpoint should not have a warning message');
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'breakpoint');
  });
}

async function testUnverifiedNonExecutableBreakpoint() {
  const program = path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl');
  await withSession(async session => {
    session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 10 }] });
    const setResponse = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(setResponse.success === true, 'non-executable breakpoint setup should succeed');

    session.send('launch', { program });
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === true, 'non-executable breakpoint launch should succeed');
    const changed = await session.waitFor(
      m => m.type === 'event' && m.event === 'breakpoint' && m.body.breakpoint.line === 10
    );
    assert(changed.body.breakpoint.verified === false, 'non-executable line breakpoint should stay unverified');
    assert(
      changed.body.breakpoint.message === 'no executable code at requested line',
      'non-executable line breakpoint should report exact reason'
    );
    const exited = await session.waitFor(m => m.type === 'event' && m.event === 'exited');
    assert(exited.body.exitCode === 42, 'unverified non-executable breakpoint should not stop execution');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
    assert(
      !session.messages.some(m => m.type === 'event' && m.event === 'stopped'),
      'unverified non-executable breakpoint should not produce stopped event'
    );
  });
}

async function testExecutableLineTableDump() {
  const program = path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl');
  await withSession(async session => {
    session.send('launch', { program, stopOnEntry: true });
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === true, 'line table dump launch should succeed');
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'entry');

    const dumpSeq = session.send('xldbg/executableLines');
    const dump = await session.waitFor(m => m.type === 'response' && m.request_seq === dumpSeq);
    assert(dump.success === true, 'line table dump request should succeed');
    assert(path.normalize(dump.body.source) === path.normalize(program), 'line table source should match launched program');

    const lines = new Set(dump.body.lines.map(line => line.line));
    assert(lines.has(1), 'line table should include function entry line');
    assert(lines.has(2), 'line table should include bind statement line');
    assert(lines.has(9), 'line table should include return line');
    assert(lines.has(11), 'line table should include fallback return line');
    assert(!lines.has(10), 'line table should not include non-executable end line');
    assert(
      dump.body.lines.every(line => line.line > 0 && line.column > 0),
      'line table should not expose synthetic zero-location nodes'
    );
    assert(dump.body.lines.every(line => typeof line.kind === 'string' && line.kind.length > 0), 'line table should include MIR kind metadata');
    const entryLine = dump.body.lines.find(line => line.line === 1);
    const bindLine = dump.body.lines.find(line => line.line === 2);
    assert(entryLine && entryLine.kind === 'Func' && entryLine.function === 'main', 'function entry line should include kind/function metadata');
    assert(bindLine && bindLine.kind === 'Bind' && bindLine.function === 'main', 'bind line should include kind/function metadata');

    session.send('disconnect');
    const disconnect = await session.waitFor(m => m.type === 'response' && m.command === 'disconnect');
    assert(disconnect.success === true, 'disconnect after line table dump should succeed');
  });
}

async function testFunctionEntryBreakpoint() {
  const program = path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl');
  await withSession(async session => {
    session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 1 }] });
    const setResponse = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(setResponse.success === true, 'function entry breakpoint setup should succeed');

    session.send('launch', { program });
    const changed = await session.waitFor(
      m => m.type === 'event' && m.event === 'breakpoint' && m.body.breakpoint.line === 1
    );
    assert(changed.body.breakpoint.verified === true, 'function declaration line should bind as executable');

    const stopped = await session.waitFor(m => m.type === 'event' && m.event === 'stopped');
    assert(stopped.body.reason === 'breakpoint', 'function entry breakpoint should stop with breakpoint reason');
    session.send('stackTrace', { threadId: 1 });
    const stack = await session.waitFor(m => m.type === 'response' && m.command === 'stackTrace');
    assert(stack.body.stackFrames[0].line === 1, 'function entry stack frame should report line 1');

    session.send('continue', { threadId: 1 });
    const cont = await session.waitFor(m => m.type === 'response' && m.command === 'continue');
    assert(cont.success === true, 'continue from function entry should succeed');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
  });
}

async function testReturnBreakpoint() {
  const program = path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl');
  await withSession(async session => {
    session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 9 }] });
    const setResponse = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(setResponse.success === true, 'return breakpoint setup should succeed');

    session.send('launch', { program });
    const changed = await session.waitFor(
      m => m.type === 'event' && m.event === 'breakpoint' && m.body.breakpoint.line === 9
    );
    assert(changed.body.breakpoint.verified === true, 'return line should bind as executable');

    const stopped = await session.waitFor(m => m.type === 'event' && m.event === 'stopped');
    assert(stopped.body.reason === 'breakpoint', 'return breakpoint should stop with breakpoint reason');
    session.send('stackTrace', { threadId: 1 });
    const stack = await session.waitFor(m => m.type === 'response' && m.command === 'stackTrace');
    assert(stack.body.stackFrames[0].line === 9, 'return breakpoint stack frame should report line 9');

    session.send('continue', { threadId: 1 });
    const cont = await session.waitFor(m => m.type === 'response' && m.command === 'continue');
    assert(cont.success === true, 'continue from return breakpoint should succeed');
    const exited = await session.waitFor(m => m.type === 'event' && m.event === 'exited');
    assert(exited.body.exitCode === 42, 'return breakpoint program should still return 42');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
  });
}

async function testUnverifiedCommentBreakpoint() {
  const program = writeTempProgram(
    'comment_breakpoint.xl',
    [
      'func main() as int',
      '\t// comment only',
      '\treturn 42',
      'end',
      '',
    ].join('\n')
  );
  await withSession(async session => {
    session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 2 }] });
    const setResponse = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(setResponse.success === true, 'comment breakpoint setup should succeed');

    session.send('launch', { program });
    const changed = await session.waitFor(
      m => m.type === 'event' && m.event === 'breakpoint' && m.body.breakpoint.line === 2
    );
    assert(changed.body.breakpoint.verified === false, 'comment line breakpoint should stay unverified');
    assert(
      changed.body.breakpoint.message === 'no executable code at requested line',
      'comment line breakpoint should report exact reason'
    );

    const exited = await session.waitFor(m => m.type === 'event' && m.event === 'exited');
    assert(exited.body.exitCode === 42, 'comment breakpoint should not stop execution');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
    assert(
      !session.messages.some(m => m.type === 'event' && m.event === 'stopped'),
      'comment breakpoint should not produce stopped event'
    );
  });
}

async function testLoopBodyBreakpointRepeats() {
  const program = writeTempProgram(
    'loop_body_breakpoint.xl',
    [
      'func main() as int',
      '\tint i = 0',
      '\twhile i < 3 do',
      '\t\ti = i + 1',
      '\tend',
      '\treturn 42',
      'end',
      '',
    ].join('\n')
  );
  await withSession(async session => {
    session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 4 }] });
    const setResponse = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(setResponse.success === true, 'loop body breakpoint setup should succeed');

    session.send('launch', { program });
    const changed = await session.waitFor(
      m => m.type === 'event' && m.event === 'breakpoint' && m.body.breakpoint.line === 4
    );
    assert(changed.body.breakpoint.verified === true, 'loop body assignment line should bind as executable');

    for (let i = 1; i <= 3; ++i) {
      const stopped = await waitForStoppedCount(session, i);
      assert(stopped.body.reason === 'breakpoint', `loop body stop ${i} should be a breakpoint`);
      const stackSeq = session.send('stackTrace', { threadId: 1 });
      const stack = await session.waitFor(
        m => m.type === 'response' && m.request_seq === stackSeq && m.body.stackFrames[0].line === 4
      );
      assert(stack.body.stackFrames[0].line === 4, `loop body stop ${i} should report line 4`);
      const contSeq = session.send('continue', { threadId: 1 });
      const cont = await session.waitFor(
        m => m.type === 'response' && m.request_seq === contSeq && m.success === true
      );
      assert(cont.success === true, `continue after loop body stop ${i} should succeed`);
    }

    const exited = await session.waitFor(m => m.type === 'event' && m.event === 'exited');
    assert(exited.body.exitCode === 42, 'loop body breakpoint program should return 42');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
  });
}

async function testSetBreakpointWhileRunning() {
  const program = writeTempProgram(
    'running_breakpoint.xl',
    [
      'func main() as int',
      '\tint i = 0',
      '\twhile i < 2000000 do',
      '\t\ti = i + 1',
      '\tend',
      '\treturn 42',
      'end',
      '',
    ].join('\n')
  );
  await withSession(async session => {
    session.send('launch', { program });
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === true, 'running breakpoint launch should succeed');

    const setSeq = session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 3 }] });
    const setResponse = await session.waitFor(m => m.type === 'response' && m.request_seq === setSeq);
    assert(setResponse.success === true, 'setBreakpoints while running should succeed');
    assert(setResponse.body.breakpoints.length === 1, 'setBreakpoints while running should return one breakpoint');
    assert(setResponse.body.breakpoints[0].verified === true, 'running breakpoint should bind immediately');

    const stopped = await session.waitFor(m => m.type === 'event' && m.event === 'stopped');
    assert(stopped.body.reason === 'breakpoint', 'running breakpoint should stop with breakpoint reason');
    session.send('stackTrace', { threadId: 1 });
    const stack = await session.waitFor(m => m.type === 'response' && m.command === 'stackTrace');
    assert(stack.body.stackFrames[0].line === 3, 'running breakpoint should stop at line 3');
    session.send('disconnect');
    const disconnect = await session.waitFor(m => m.type === 'response' && m.command === 'disconnect');
    assert(disconnect.success === true, 'disconnect after running breakpoint should succeed');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
  });
}

async function testConditionalBreakpointResponse() {
  const program = path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl');
  await withSession(async session => {
    session.send('setBreakpoints', {
      source: { path: program },
      breakpoints: [{ line: 2, condition: 'mask == 10' }],
    });
    const bp = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(bp.success === true, 'conditional setBreakpoints should succeed at protocol level');
    assert(bp.body.breakpoints.length === 1, 'conditional setBreakpoints should return one breakpoint');
    assert(bp.body.breakpoints[0].verified === false, 'conditional breakpoint should be unverified');
    assert(
      bp.body.breakpoints[0].message === 'conditional breakpoints are not supported',
      'conditional breakpoint should report unsupported message'
    );
  });
}

async function testMultiFileBreakpointIsolation() {
  const programA = path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl');
  const programB = path.join(repoRoot, 'release/examples/full/anonymous_function_basic.xl');
  await withSession(async session => {
    const setASeq = session.send('setBreakpoints', { source: { path: programA }, breakpoints: [{ line: 2 }] });
    const setA = await session.waitFor(m => m.type === 'response' && m.request_seq === setASeq);
    assert(setA.success === true, 'setBreakpoints for file A should succeed');
    assert(setA.body.breakpoints.length === 1, 'file A should return one breakpoint');

    const setBSeq = session.send('setBreakpoints', { source: { path: programB }, breakpoints: [{ line: 5 }] });
    const setB = await session.waitFor(m => m.type === 'response' && m.request_seq === setBSeq);
    assert(setB.success === true, 'setBreakpoints for file B should succeed');
    assert(setB.body.breakpoints.length === 1, 'file B should return one breakpoint');

    const clearASeq = session.send('setBreakpoints', { source: { path: programA }, breakpoints: [] });
    const clearA = await session.waitFor(m => m.type === 'response' && m.request_seq === clearASeq);
    assert(clearA.success === true, 'clearing file A breakpoints should succeed');
    assert(clearA.body.breakpoints.length === 0, 'file A should return no breakpoints after clear');

    session.send('launch', { program: programB });
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === true, 'launch for file B should succeed');
    const stopped = await session.waitFor(m => m.type === 'event' && m.event === 'stopped');
    assert(stopped.body.reason === 'breakpoint', 'file B breakpoint should remain after clearing file A');
  });
}

async function testClearedBreakpointsDoNotStop() {
  const program = path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl');
  await withSession(async session => {
    const setSeq = session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 2 }] });
    const setResponse = await session.waitFor(m => m.type === 'response' && m.request_seq === setSeq);
    assert(setResponse.success === true, 'initial breakpoint set should succeed');
    assert(setResponse.body.breakpoints.length === 1, 'initial breakpoint set should return one breakpoint');

    const clearSeq = session.send('setBreakpoints', { source: { path: program }, breakpoints: [] });
    const clearResponse = await session.waitFor(m => m.type === 'response' && m.request_seq === clearSeq);
    assert(clearResponse.success === true, 'breakpoint clear should succeed');
    assert(clearResponse.body.breakpoints.length === 0, 'breakpoint clear should return no breakpoints');

    session.send('launch', { program });
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === true, 'launch after clearing breakpoint should succeed');
    const exited = await session.waitFor(m => m.type === 'event' && m.event === 'exited');
    assert(exited.body.exitCode === 42, 'cleared breakpoint run should finish normally');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
    assert(
      !session.messages.some(m => m.type === 'event' && m.event === 'stopped'),
      'cleared breakpoints should not produce stopped event'
    );
  });
}

async function testNext() {
  const program = path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl');
  await withSession(async session => {
    session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 2 }] });
    const bp = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(bp.success === true, 'setBreakpoints for next should succeed');
    session.send('launch', { program });
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'breakpoint');
    session.send('next', { threadId: 1 });
    const next = await session.waitFor(m => m.type === 'response' && m.command === 'next');
    assert(next.success === true, 'next should succeed');
    const stopped = await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'step');
    assert(stopped.body.reason === 'step', 'next should stop with step reason');
    session.send('stackTrace', { threadId: 1 });
    const stack = await session.waitFor(m => m.type === 'response' && m.command === 'stackTrace');
    assert(stack.body.stackFrames[0].line === 3, 'next should stop at line 3');
  });
}

async function testStepIn() {
  const program = path.join(repoRoot, 'release/examples/full/anonymous_function_basic.xl');
  await withSession(async session => {
    session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 5 }] });
    const bp = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(bp.success === true, 'setBreakpoints for stepIn should succeed');
    session.send('launch', { program });
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'breakpoint');
    session.send('stepIn', { threadId: 1 });
    const stepIn = await session.waitFor(m => m.type === 'response' && m.command === 'stepIn');
    assert(stepIn.success === true, 'stepIn should succeed');
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'step');
    session.send('stackTrace', { threadId: 1 });
    const stack = await session.waitFor(m => m.type === 'response' && m.command === 'stackTrace');
    assert(stack.body.stackFrames[0].name.startsWith('__lambda_'), 'stepIn should enter generated lambda frame');
    assert(stack.body.stackFrames[0].line === 2, 'stepIn should stop at lambda entry line 2');
  });
}

async function testStepOut() {
  const program = path.join(repoRoot, 'release/examples/full/class_stage1_base_destroy_runtime.xl');
  await withSession(async session => {
    session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 29 }] });
    const bp = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(bp.success === true, 'setBreakpoints for stepOut should succeed');
    session.send('launch', { program });
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'breakpoint');
    session.send('stepOut', { threadId: 1 });
    const stepOut = await session.waitFor(m => m.type === 'response' && m.command === 'stepOut');
    assert(stepOut.success === true, 'stepOut should succeed');
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'step');
    session.send('stackTrace', { threadId: 1 });
    const stack = await session.waitFor(m => m.type === 'response' && m.command === 'stackTrace');
    assert(stack.body.stackFrames[0].name === 'main', 'stepOut should return to main');
    assert(stack.body.stackFrames[0].line === 35, 'stepOut should stop at main line 35');
  });
}

async function testScopesVariablesAndStaleRefs() {
  const program = path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl');
  await withSession(async session => {
    session.send('setBreakpoints', {
      source: { path: program },
      breakpoints: [{ line: 3 }, { line: 4 }],
    });
    const bp = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(bp.success === true, 'setBreakpoints for variables should succeed');
    session.send('launch', { program });
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'breakpoint');

    const scopesSeq = session.send('scopes', { frameId: 1 });
    const scopes = await session.waitFor(m => m.type === 'response' && m.request_seq === scopesSeq);
    assert(scopes.success === true, 'scopes should succeed');
    const localsScope = scopes.body.scopes.find(scope => scope.name === 'Locals');
    assert(localsScope && localsScope.variablesReference > 0, 'scopes should expose Locals reference');

    const variablesSeq = session.send('variables', { variablesReference: localsScope.variablesReference });
    const variables = await session.waitFor(m => m.type === 'response' && m.request_seq === variablesSeq);
    assert(variables.success === true, 'variables should succeed for current locals reference');
    const mask = variables.body.variables.find(variable => variable.name === 'mask');
    assert(mask && mask.value === '10', 'locals should include mask = 10');

    session.send('continue', { threadId: 1 });
    const cont = await session.waitFor(m => m.type === 'response' && m.command === 'continue');
    assert(cont.success === true, 'continue to second breakpoint should succeed');
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'breakpoint');

    const staleSeq = session.send('variables', { variablesReference: localsScope.variablesReference });
    const stale = await session.waitFor(m => m.type === 'response' && m.request_seq === staleSeq);
    assert(stale.success === false, 'old locals reference should be stale after next stop');
    assert(stale.message === 'stale variablesReference', 'old locals reference should report stale variablesReference');
  });
}

async function testMultiFrameScopesVariables() {
  const program = path.join(repoRoot, 'release/examples/full/anonymous_function_basic.xl');
  await withSession(async session => {
    session.send('setBreakpoints', {
      source: { path: program },
      breakpoints: [{ line: 3 }],
    });
    const bp = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(bp.success === true, 'setBreakpoints for multi-frame variables should succeed');
    session.send('launch', { program });
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'breakpoint');

    const stackSeq = session.send('stackTrace', { threadId: 1 });
    const stack = await session.waitFor(m => m.type === 'response' && m.request_seq === stackSeq);
    assert(stack.success === true, 'multi-frame stackTrace should succeed');
    assert(stack.body.stackFrames.length >= 2, 'lambda breakpoint should expose current and caller frames');
    assert(stack.body.stackFrames[0].name.startsWith('__lambda_'), 'top frame should be the lambda');
    assert(stack.body.stackFrames[1].name === 'main', 'caller frame should be main');

    const currentScopesSeq = session.send('scopes', { frameId: stack.body.stackFrames[0].id });
    const currentScopes = await session.waitFor(m => m.type === 'response' && m.request_seq === currentScopesSeq);
    const currentArgsRef = currentScopes.body.scopes.find(scope => scope.name === 'Arguments').variablesReference;
    const currentArgsSeq = session.send('variables', { variablesReference: currentArgsRef });
    const currentArgs = await session.waitFor(m => m.type === 'response' && m.request_seq === currentArgsSeq);
    const argA = currentArgs.body.variables.find(variable => variable.name === 'a');
    const argB = currentArgs.body.variables.find(variable => variable.name === 'b');
    assert(argA && argA.value === '20', 'lambda arguments should include a = 20');
    assert(argB && argB.value === '22', 'lambda arguments should include b = 22');

    const callerScopesSeq = session.send('scopes', { frameId: stack.body.stackFrames[1].id });
    const callerScopes = await session.waitFor(m => m.type === 'response' && m.request_seq === callerScopesSeq);
    assert(callerScopes.success === true, 'caller frame scopes should succeed');
    const callerLocalsRef = callerScopes.body.scopes.find(scope => scope.name === 'Locals').variablesReference;
    assert(callerLocalsRef > 0 && callerLocalsRef !== currentArgsRef, 'caller locals should use a distinct variablesReference');
    const callerLocalsSeq = session.send('variables', { variablesReference: callerLocalsRef });
    const callerLocals = await session.waitFor(m => m.type === 'response' && m.request_seq === callerLocalsSeq);
    assert(callerLocals.success === true, 'caller frame locals variables should succeed');
  });
}

async function testFinallyReturnFrameCleanup() {
  const program = writeTempProgram(
    'finally_return_frame_cleanup.xl',
    'func callee() as int\n' +
      '\ttry\n' +
      '\t\treturn 41\n' +
      '\tfinally\n' +
      '\t\tint cleanup = 1\n' +
      '\tend\n' +
      'end\n' +
      '\n' +
      'func main() as int\n' +
      '\tint value = callee()\n' +
      '\tint marker = value + 1\n' +
      '\treturn marker\n' +
      'end\n'
  );
  await withSession(async session => {
    session.send('setBreakpoints', {
      source: { path: program },
      breakpoints: [{ line: 11 }],
    });
    const bp = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(bp.success === true, 'setBreakpoints for finally return cleanup should succeed');
    session.send('launch', { program });
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'breakpoint');

    const stackSeq = session.send('stackTrace', { threadId: 1 });
    const stack = await session.waitFor(m => m.type === 'response' && m.request_seq === stackSeq);
    assert(stack.success === true, 'finally return cleanup stackTrace should succeed');
    assert(stack.body.stackFrames[0].name === 'main', 'finally return should leave callee before caller breakpoint');
    assert(
      !stack.body.stackFrames.some(frame => frame.name === 'callee'),
      'finally return should not leave a stale callee frame'
    );
  });
}

async function testVarVariableDisplay() {
  const program = writeTempProgram(
    'var_variable_display.xl',
    'func main() as int\n\tvar dyn = 41\n\tvar bag = [40, 2]\n\tint marker = 0\n\treturn marker\nend\n'
  );
  await withSession(async session => {
    session.send('setBreakpoints', {
      source: { path: program },
      breakpoints: [{ line: 4 }],
    });
    const bp = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(bp.success === true, 'setBreakpoints for var display should succeed');
    session.send('launch', { program });
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'breakpoint');

    const stackSeq = session.send('stackTrace', { threadId: 1 });
    const stack = await session.waitFor(m => m.type === 'response' && m.request_seq === stackSeq);
    const frameId = stack.body.stackFrames[0].id;
    const scopesSeq = session.send('scopes', { frameId });
    const scopes = await session.waitFor(m => m.type === 'response' && m.request_seq === scopesSeq);
    const localsRef = scopes.body.scopes.find(scope => scope.name === 'Locals').variablesReference;
    const varsSeq = session.send('variables', { variablesReference: localsRef });
    const vars = await session.waitFor(m => m.type === 'response' && m.request_seq === varsSeq);
    const dyn = vars.body.variables.find(variable => variable.name === 'dyn');
    assert(dyn, 'locals should include var binding dyn');
    assert(dyn.type === 'var', 'var binding should report type=var');
    assert(dyn.value === '41', 'var binding should use xl_value_to_string text');
    const bag = vars.body.variables.find(variable => variable.name === 'bag');
    assert(bag && bag.variablesReference > 0, 'var container should expose children reference');
    const bagChildrenSeq = session.send('variables', { variablesReference: bag.variablesReference });
    const bagChildren = await session.waitFor(m => m.type === 'response' && m.request_seq === bagChildrenSeq);
    assert(bagChildren.success === true, 'var children reference should resolve');
    const first = bagChildren.body.variables.find(variable => variable.name === '[0]');
    const second = bagChildren.body.variables.find(variable => variable.name === '[1]');
    assert(first && first.value === '40', 'var array child [0] should be visible');
    assert(second && second.value === '2', 'var array child [1] should be visible');

    session.send('disconnect');
    const disconnect = await session.waitFor(m => m.type === 'response' && m.command === 'disconnect');
    assert(disconnect.success === true, 'disconnect after var display should succeed');
  });
}

async function testUnknownVariablesReference() {
  await withSession(async session => {
    const variablesSeq = session.send('variables', { variablesReference: 999999 });
    const variables = await session.waitFor(m => m.type === 'response' && m.request_seq === variablesSeq);
    assert(variables.success === false, 'unknown variablesReference should fail');
    assert(variables.message === 'unknown variablesReference', 'unknown variablesReference should report a clear message');
  });
}

async function testDisconnectWhileStopped() {
  const program = path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl');
  await withSession(async session => {
    session.send('setBreakpoints', { source: { path: program }, breakpoints: [{ line: 2 }] });
    const bp = await session.waitFor(m => m.type === 'response' && m.command === 'setBreakpoints');
    assert(bp.success === true, 'setBreakpoints for disconnect should succeed');
    session.send('launch', { program });
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'breakpoint');
    session.send('disconnect');
    const disconnect = await session.waitFor(m => m.type === 'response' && m.command === 'disconnect');
    assert(disconnect.success === true, 'disconnect should succeed while stopped');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
  });
}

async function testDisconnectWhileRunning() {
  const program = writeTempProgram(
    'disconnect_loop.xl',
    'func main() as int\n' +
      '\tint i = 0\n' +
      '\twhile i < 50000000 do\n' +
      '\t\ti = i + 1\n' +
      '\tend\n' +
      '\treturn i\n' +
      'end\n'
  );
  await withSession(async session => {
    session.send('launch', { program });
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === true, 'disconnect running launch should succeed');
    session.send('disconnect');
    const disconnect = await session.waitFor(m => m.type === 'response' && m.command === 'disconnect');
    assert(disconnect.success === true, 'disconnect should succeed while running');
    await session.waitFor(m => m.type === 'event' && m.event === 'terminated');
  });
}

async function testStopOnEntry() {
  await withSession(async session => {
    session.send('launch', {
      program: path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl'),
      stopOnEntry: true,
    });
    const stopped = await session.waitFor(m => m.type === 'event' && m.event === 'stopped');
    assert(stopped.body.reason === 'entry', 'stopOnEntry should stop with entry reason');
  });
}

async function testPause() {
  const program = writeTempProgram(
    'pause_loop.xl',
    'func main() as int\n' +
      '\tint i = 0\n' +
      '\tint total = 0\n' +
      '\twhile i < 50000000 do\n' +
      '\t\ttotal = total + 1\n' +
      '\t\ti = i + 1\n' +
      '\tend\n' +
      '\treturn total\n' +
      'end\n'
  );
  await withSession(async session => {
    session.send('launch', { program });
    const launch = await session.waitFor(m => m.type === 'response' && m.command === 'launch');
    assert(launch.success === true, 'pause test launch should succeed');
    session.send('pause', { threadId: 1 });
    const pause = await session.waitFor(m => m.type === 'response' && m.command === 'pause');
    assert(pause.success === true, 'pause should succeed while running');
    const stopped = await session.waitFor(m => m.type === 'event' && m.event === 'stopped');
    assert(stopped.body.reason === 'pause', 'pause should stop with pause reason');
  });
}

async function testPauseErrors() {
  await withSession(async session => {
    session.send('pause', { threadId: 1 });
    const notRunning = await session.waitFor(m => m.type === 'response' && m.command === 'pause');
    assert(notRunning.success === false, 'pause before launch should fail');
    assert(notRunning.message === 'debuggee is not running', 'pause before launch should report debuggee is not running');
  });

  await withSession(async session => {
    session.send('launch', {
      program: path.join(repoRoot, 'release/examples/full/bit_ops_basic.xl'),
      stopOnEntry: true,
    });
    await session.waitFor(m => m.type === 'event' && m.event === 'stopped' && m.body.reason === 'entry');
    session.send('pause', { threadId: 1 });
    const alreadyStopped = await session.waitFor(m => m.type === 'response' && m.command === 'pause');
    assert(alreadyStopped.success === false, 'pause while stopped should fail');
    assert(alreadyStopped.message === 'debuggee is already stopped', 'pause while stopped should report already stopped');
  });
}

const tests = [
  ['normal exit', testNormalExit],
  ['xldbg run regression', testXldbgRunRegression],
  ['xldbg dbg REPL', testXldbgDbgRepl],
  ['compile failure', testCompileFailure],
  ['host runtime failure output', testHostRuntimeFailureOutput],
  ['exception stop', testExceptionStop],
  ['threads and exception breakpoints', testThreadsAndExceptionBreakpoints],
  ['unsupported request', testUnsupportedRequest],
  ['malformed JSON terminates', testMalformedJsonTerminates],
  ['TCP listen unsupported', testTcpListenUnsupported],
  ['request validation errors', testRequestValidationErrors],
  ['breakpoint continue', testBreakpointContinue],
  ['verified breakpoint after launch', testVerifiedBreakpointAfterLaunch],
  ['unverified non-executable breakpoint', testUnverifiedNonExecutableBreakpoint],
  ['executable line table dump', testExecutableLineTableDump],
  ['function entry breakpoint', testFunctionEntryBreakpoint],
  ['return breakpoint', testReturnBreakpoint],
  ['unverified comment breakpoint', testUnverifiedCommentBreakpoint],
  ['loop body breakpoint repeats', testLoopBodyBreakpointRepeats],
  ['set breakpoint while running', testSetBreakpointWhileRunning],
  ['conditional breakpoint response', testConditionalBreakpointResponse],
  ['multi-file breakpoint isolation', testMultiFileBreakpointIsolation],
  ['cleared breakpoints do not stop', testClearedBreakpointsDoNotStop],
  ['next', testNext],
  ['stepIn', testStepIn],
  ['stepOut', testStepOut],
  ['scopes variables stale refs', testScopesVariablesAndStaleRefs],
  ['multi-frame scopes variables', testMultiFrameScopesVariables],
  ['finally return frame cleanup', testFinallyReturnFrameCleanup],
  ['var variable display', testVarVariableDisplay],
  ['unknown variablesReference', testUnknownVariablesReference],
  ['disconnect while stopped', testDisconnectWhileStopped],
  ['disconnect while running', testDisconnectWhileRunning],
  ['stopOnEntry', testStopOnEntry],
  ['pause', testPause],
  ['pause errors', testPauseErrors],
];

(async () => {
  for (const [name, fn] of tests) {
    await fn();
    console.log(`[PASS] ${name}`);
  }
})().catch(err => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
