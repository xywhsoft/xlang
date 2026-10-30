import { spawnSync } from 'node:child_process';
import { createRequire } from 'node:module';

// 可选开发门禁：使用指定版本的官方 JS 实现，不加入产品运行依赖。
const [executable, referencePath] = process.argv.slice(2);
if (!executable || !referencePath) {
    throw new Error('usage: node reference_probe.mjs <C probe exe> <json5 reference directory>');
}
const require = createRequire(import.meta.url);
if (require(`${referencePath}/package.json`).version !== '2.2.3') {
    throw new Error('reference must be the official JSON5 2.2.3 package');
}
const reference = require(referencePath);
const cases = [
    '{}', '{a:1}', '{a:1,}', '{a:"x"}', '{a:\'x\'}',
    '{你好:1}', '{\\u4F60\\u597D:1}', '{cafe\\u0301:1}', '{𐐀:1}',
    '{a\\u200Cb:1}', '{a\\u200Db:1}', '{a\\u0024:1}', '{\\u0024a:1}',
    '{☃:1}', '{a☃:1}', '{1a:1}', '{\\u002Da:1}', '{a\\u002D:1}',
    '{\\uD800:1}', '{a\\uD800:1}', '{a:1 // comment\n}',
    '{a:+0x2a, b:.5, c:1., d:Infinity, e:NaN}',
    '{a:"line\\\nnext"}', '{a:"\\x41\\u4F60"}',
    '{a:1} trailing', '/* unclosed', '[1,2,]', '{a:true,b:false,c:null}'
];
for (const point of [0x00A0, 0x1680, 0x2000, 0x2001, 0x2002, 0x2003,
    0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200A,
    0x202F, 0x205F, 0x3000, 0x2028, 0x2029, 0xFEFF]) {
    cases.push(`{a${String.fromCodePoint(point)}:1}`);
}
for (const point of [0x03B1, 0x05D0, 0x0939, 0x4F60, 0x10400]) {
    cases.push(`{${String.fromCodePoint(point)}:1}`);
}
for (const point of [0x0301, 0x0661, 0x200C, 0x200D]) {
    cases.push(`{a${String.fromCodePoint(point)}:1}`);
}

// 固定组合矩阵覆盖词法交界，不依赖随机数或外部测试框架。
const keys = [
    'a', '_a', '$a', 'a0', 'a-b', 'a b', '0a', '你好', 'α',
    '\\u0061', 'a\\u200Cb', 'a\\u200Db', '\\uD800', 'a\\u002D'
];
const numbers = [
    '0', '-0', '+0', '12', '-12', '+12', '.5', '-.5', '12.',
    '1e3', '1E-3', '0x2a', '+0X2A', '-0x2a', '01', '1e', '0x',
    '1_0', 'Infinity', '-Infinity', 'NaN'
];
for (const key of keys) {
    for (const number of numbers) cases.push(`{${key}:${number},}`);
}
const stringValues = [
    String.raw`plain`, String.raw`\\`, String.raw`\/`, String.raw`\b`,
    String.raw`\f`, String.raw`\n`, String.raw`\r`, String.raw`\t`,
    String.raw`\v`, String.raw`\0`, String.raw`\x41`, String.raw`\u0041`,
    String.raw`\uD83C\uDFBC`, String.raw`\x4`, String.raw`\uGGGG`,
    'raw\nnewline'
];
// 孤立代理项有单独负例：JS 可保留 UTF-16 码元，XRT string 必须是有效 UTF-8。
for (const value of stringValues) {
    for (const quote of ['"', "'"]) cases.push(`[${quote}${value}${quote}]`);
}
for (const whitespace of [' ', '\t', '\r', '\n', '\u00A0', '\u1680',
    '\u2000', '\u2028', '\u2029', '\u3000', '\uFEFF']) {
    cases.push(`${whitespace}{a:${whitespace}1}${whitespace}`);
    cases.push(`{a:1,${whitespace}b:2}`);
}
for (const comment of ['// one\n', '/* two */', '/* unterminated',
    '// one\r', '/* two * /']) {
    cases.push(`{a:1,${comment}b:2}`);
    cases.push(`[1,${comment}2]`);
}
const nestedValues = [
    '[]', '{}', '[true,false,null,0,"x"]',
    '{outer:{inner:[1,2,3]}}', '[{a:1},{b:2}]',
    '{arr:[{x:"你好"},{x:"世界"}]}',
    '{zero:-0,half:.5}', '{escaped:{"a.b":"value"}}',
    '{a:[1,/* inside */2]}', '{a:[],b:{}}'
];
for (const value of nestedValues) {
    cases.push(value, `[${value}]`, `{root:${value}}`,
        `/* lead */${value}// end\n`, `\u00A0${value}\u00A0`,
        `[{left:0},${value}]`);
}

// 用精确 BigInt 构造半 ULP 邻域，避免只比较格式化后看起来相同的近似数字。
for (const exponent of [63n, 64n, 65n, 95n, 128n, 256n, 511n, 512n, 900n, 1023n]) {
    const base = 1n << exponent;
    const half = 1n << (exponent - 53n);
    for (const offset of [half - 1n, half, half + 1n, 3n * half - 1n, 3n * half, 3n * half + 1n]) {
        const digits = (base + offset).toString(16);
        const hex = `0x${digits}`;
        cases.push(hex, `-${hex}`, `+0X000${digits.toUpperCase()}`, `{n:${hex}}`);
    }
}

// identity escape 消费完整码点；LS/PS 的转义续行与原始字符串内容不同。
const unicodePoints = [0x80, 0xa0, 0xe9, 0x3b1, 0x301, 0x4f60, 0x1f3b6, 0x10ffff, 0x2028, 0x2029];
for (const point of unicodePoints) {
    const character = String.fromCodePoint(point);
    for (const quote of ["'", '"']) {
        for (const slash of ['', '\\']) {
            cases.push(`{v:${quote}p${slash}${character}q${quote}}`);
            cases.push(`{${quote}p${slash}${character}q${quote}:'ok'}`);
        }
    }
}
for (const line of ['\n', '\r', '\r\n', '\u2028', '\u2029']) {
    cases.push(`//你好${line}{v:'ok'}`, `{//你好${line}v:'ok'}`,
        `{v://你好${line}'ok'}`, `{v:'ok'//你好${line}}`,
        `/*你好${line}*/{v:'ok'}`, `{/*你好${line}*/v:'ok'}`,
        `{v:/*你好${line}*/'ok'}`, `{v:'ok'/*你好${line}*/}`);
}

const input = cases.map((value) => Buffer.from(value).toString('hex')).join('\n') + '\n';
const result = spawnSync(executable, [], { input, encoding: 'utf8' });
if (result.status !== 0) {
    throw new Error(`C probe failed: ${result.status}\n${result.stderr}`);
}
const actual = result.stdout.trimEnd().split(/\r?\n/);
if (actual.length !== cases.length) throw new Error('C probe output count mismatch');
let failures = 0;
let comparedValues = 0;
for (let i = 0; i < cases.length; i++) {
    let expected;
    let parsed;
    try {
        parsed = reference.parse(cases[i]);
        expected = '1';
    } catch {
        expected = '0';
    }
    const [status, hex] = actual[i].split('\t');
    if (status !== expected) {
        console.error(`case ${i}: expected ${expected}, actual ${status}: ${JSON.stringify(cases[i])}`);
        failures++;
    } else if (status === '1' && !/\b(?:Infinity|NaN)\b/.test(cases[i])) {
        try {
            if (!hex) throw new Error('C result was not serializable');
            const decoded = JSON.parse(Buffer.from(hex, 'hex').toString('utf8'));
            if (JSON.stringify(decoded) !== JSON.stringify(parsed)) {
                throw new Error(`value mismatch: C=${JSON.stringify(decoded)} JS=${JSON.stringify(parsed)}`);
            }
            comparedValues++;
        } catch (error) {
            console.error(`case ${i}: ${error.message}: ${JSON.stringify(cases[i])}`);
            failures++;
        }
    }
}
if (failures) process.exitCode = 1;
else console.log(`JSON5 reference agreement: ${cases.length} parse cases, ${comparedValues} value cases`);
