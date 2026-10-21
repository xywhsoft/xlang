// 可选独立 oracle：传入 node-semver 的检出目录与已编译的 C 查询程序。
const { spawnSync } = require('node:child_process');
if (process.argv.length < 4) throw new Error('usage: node compare_node_semver.js NODE_SEMVER_DIR C_CLI');
const semver = require(process.argv[2]);
const cli = process.argv[3];

const versions = [];
const ranges = new Set(['', '*', '>=1.0.0 <3.0.0', '^0.2.0 || ^2.0.0',
    '>=*', '<=*', '=*', '>x', '<x', '^x', '~x', '1.x.3', '1..2',
    '1.0.0 -', '1.0.0 - 2.0.0', '1.2.3  -  2.3.4',
    '1.2.3\t-\t2.3.4', '1.2.3 - 2.3.4 >=2.0.0',
    '1.2.3+meta', '>1.2.3-alpha',
    '>= 1.2.3 < 2.0.0', '~ 1.2', '^ 1.0.0 || ^ 2.0.0']);
for (let major = 0; major <= 3; major++) {
    for (let minor = 0; minor <= 3; minor++) {
        for (let patch = 0; patch <= 3; patch++) {
            const base = `${major}.${minor}.${patch}`;
            versions.push(base, `${base}-alpha`, `${base}-alpha.2`, `${base}+build.3`);
            for (const prefix of ['', '=', '<', '<=', '>', '>=', '^', '~']) {
                ranges.add(prefix + base);
            }
        }
        ranges.add(`${major}.${minor}`);
        ranges.add(`${major}.${minor}.x`);
        ranges.add(`${major}.${minor}.0 - ${major}.${minor}.3`);
    }
    ranges.add(`${major}.x`);
    ranges.add(`${major}`);
}
for (const base of ['0.0.1', '0.2.0', '1.2.3', '2.0.0']) {
    for (const suffix of ['alpha.1', 'alpha.2', 'alpha.10', 'beta', 'rc.1']) {
        const prerelease = `${base}-${suffix}`;
        versions.push(prerelease);
        for (const prefix of ['', '>', '>=', '<', '<=', '^', '~']) {
            ranges.add(prefix + prerelease);
        }
    }
}
for (const range of [
    '>=1.2.3-alpha.1 <1.2.3',
    '>=1.2.3-beta <2.0.0',
    '^0.2.0-alpha.1 || ^1.2.3-beta',
    '0.2.0-alpha.1 - 0.2.0-rc.1',
    '1.2.3-alpha.1 - 1.2.3-rc.1'
]) ranges.add(range);
const cases = [];
for (const version of versions) {
    for (const range of ranges) {
        cases.push({ version, range });
    }
}
const input = cases.map(({ version, range }) => `${version}\t${range}\n`).join('');
const result = spawnSync(cli, [], { input, encoding: 'utf8', maxBuffer: 32 * 1024 * 1024 });
if (result.status !== 0) throw new Error(`C oracle CLI failed: ${result.status} ${result.stderr}`);
const actual = result.stdout.trimEnd().split(/\r?\n/);
if (actual.length !== cases.length) throw new Error(`result count ${actual.length} != ${cases.length}`);
let mismatches = 0;
for (let i = 0; i < cases.length; i++) {
    const { version, range } = cases[i];
    const expected = semver.satisfies(version, range) ? '1' : '0';
    if (actual[i] !== expected) {
        if (mismatches < 20) console.error(`${version} ${JSON.stringify(range)}: C=${actual[i]} node=${expected}`);
        mismatches++;
    }
}
console.log(`semver oracle: ${cases.length} cases, ${mismatches} mismatches`);
process.exitCode = mismatches === 0 ? 0 : 1;
