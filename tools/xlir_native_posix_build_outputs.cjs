const path = require('path').posix;

/* Native build.py logs concurrently with multi-argument print. On POSIX a
 * program has no .exe delimiter: its -o path may be immediately followed by
 * another compiler's absolute path. Remove only an exact, caller-observed
 * compiler suffix; do not infer executable locations by scanning out/.
 * Callers must still check the exact unique manifest and binary digests. */
function nativePosixBuildOutputs(log, vendor, compilerPaths) {
  if (!path.isAbsolute(vendor) || path.normalize(vendor) === '/') {
    throw Error('Expected a private absolute POSIX build root');
  }
  if (!compilerPaths.length || compilerPaths.some(p => !path.isAbsolute(p))) {
    throw Error('Expected observed absolute compiler paths');
  }
  const root = path.normalize(vendor), out = path.join(root, 'out');
  const tokens = [...log.matchAll(/ @([^\r\n]+?\.rsp) arguments=\d+/g)]
    .map(m => ({ Output: m[1].slice(0, -4), ViaResponse: true }));
  for (const m of log.matchAll(/ -o (?:"([^"\r\n]+)"|(\S+))/g)) {
    let output = m[1] || m[2];
    if (!m[1]) {
      // A multi-argument print can also append the next command's build tag.
      // Strip only the exact tag followed by an observed compiler, never a
      // free-form suffix or an inferred executable found by scanning out/.
      const rest = log.slice(m.index + m[0].length).replace(/^\s+/, '');
      if (output.endsWith('[build]') && compilerPaths.some(p =>
        rest.startsWith(p) && /^[ \t]/.test(rest.slice(p.length)))) {
        output = output.slice(0, -7);
      }
      const suffixes = compilerPaths.filter(p => output.endsWith(p));
      if (suffixes.length > 1) throw Error('Ambiguous compiler suffix');
      if (suffixes.length === 1) output = output.slice(0, -suffixes[0].length);
    }
    tokens.push({ Output: output, ViaResponse: false });
  }
  const outputs = [];
  for (const token of tokens) {
    if (token.Output.endsWith('.o')) continue;
    const candidate = path.resolve(root, token.Output);
    const relative = path.relative(out, candidate);
    if (!relative || relative === '..' || relative.startsWith('../') || path.isAbsolute(relative)) {
      throw Error('Build output outside candidate ' + candidate);
    }
    outputs.push({ Path: candidate, ViaResponse: token.ViaResponse });
  }
  return outputs;
}

module.exports = { nativePosixBuildOutputs };

if (require.main === module) {
  const assert = require('assert'), root = '/tmp/native-output-test';
  const gcc = '/usr/bin/gcc', tcc = '/usr/bin/tcc';
  const a = root + '/out/gcc/suite/test_a', b = root + '/out/gcc/suite/test_b';
  const parse = log => nativePosixBuildOutputs(log, root, [gcc, tcc]);
  assert.deepStrictEqual(parse(`[build] ${gcc} -o ${a}\n`), [{ Path: a, ViaResponse: false }]);
  assert.deepStrictEqual(parse(`[build][build] ${gcc} -o ${a}${gcc} -o ${b}\n`),
    [{ Path: a, ViaResponse: false }, { Path: b, ViaResponse: false }]);
  assert.deepStrictEqual(parse(`[build][build] ${gcc} -o ${a} ${gcc} -o ${b}\n`),
    [{ Path: a, ViaResponse: false }, { Path: b, ViaResponse: false }]);
  assert.deepStrictEqual(parse(`[build] ${gcc} -o ${a}[build] ${gcc} -o ${b}\n`),
    [{ Path: a, ViaResponse: false }, { Path: b, ViaResponse: false }]);
  assert.deepStrictEqual(parse(`[build] ${gcc} -o ${a}[build]\n ${gcc} -o ${b}\n`),
    [{ Path: a, ViaResponse: false }, { Path: b, ViaResponse: false }]);
  assert.deepStrictEqual(parse(`${gcc} -o ${a}[build] unrelated\n`),
    [{ Path: a + '[build]', ViaResponse: false }]);
  assert.deepStrictEqual(parse(`${gcc} -o ${a}[build] ${gcc}-unobserved -v\n`),
    [{ Path: a + '[build]', ViaResponse: false }]);
  assert.deepStrictEqual(parse(`${gcc} -o "${a}"\n${gcc} -c x.c -o ${a}.o\n`),
    [{ Path: a, ViaResponse: false }]);
  assert.deepStrictEqual(parse(`${tcc} @out/gcc/suite/test_a.rsp arguments=350${gcc} -o ${b}\n`),
    [{ Path: a, ViaResponse: true }, { Path: b, ViaResponse: false }]);
  assert.deepStrictEqual(parse(`${gcc} @out/gcc/suite/test_a.o.rsp arguments=350\n`), []);
  assert.deepStrictEqual(parse(`${gcc} -o ${a}\n${gcc} -o ${a}\n`).length, 2);
  assert.throws(() => parse(`${gcc} -o ../outside\n`));
  assert.throws(() => parse(`${gcc} @../outside.rsp arguments=350\n`));
  assert.throws(() => nativePosixBuildOutputs('', '/', [gcc]));
  assert.throws(() => nativePosixBuildOutputs('', root, ['gcc']));
  console.log('POSIX build mapping: direct, response, joined/spaced concurrent commands, quoted output, object exclusion, duplicates preserved and containment passed');
}
