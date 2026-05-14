const fs = require("fs");
const path = require("path");

const root = path.resolve(__dirname, "..", "..", "..");
const sourcePath = path.join(root, "demo5", "release", "libs", "x.redis", "src", "redis.xl");
const packagePath = path.join(root, "demo5", "release", "libs", "x.redis", "package.json");

const source = fs.readFileSync(sourcePath, "utf8");
const pkg = JSON.parse(fs.readFileSync(packagePath, "utf8"));

const sourceExports = [...source.matchAll(/^export\s+([A-Za-z_][A-Za-z0-9_]*)\s*$/gm)].map((match) => match[1]);
const metadataExports = pkg.modules[0].exports.map((item) => item.name);

function duplicates(values) {
	const seen = new Set();
	const out = [];
	for (const value of values) {
		if (seen.has(value) && !out.includes(value)) {
			out.push(value);
		}
		seen.add(value);
	}
	return out;
}

function difference(left, right) {
	const rightSet = new Set(right);
	return left.filter((value) => !rightSet.has(value));
}

const missingMetadata = difference(sourceExports, metadataExports);
const missingSource = difference(metadataExports, sourceExports);
const duplicateSource = duplicates(sourceExports);
const duplicateMetadata = duplicates(metadataExports);

if (
	missingMetadata.length ||
	missingSource.length ||
	duplicateSource.length ||
	duplicateMetadata.length
) {
	console.error("x.redis export metadata mismatch");
	console.error(JSON.stringify({
		sourceCount: sourceExports.length,
		metadataCount: metadataExports.length,
		missingMetadata,
		missingSource,
		duplicateSource,
		duplicateMetadata,
	}, null, 2));
	process.exit(1);
}

console.log(`x.redis export metadata OK: ${sourceExports.length} exports`);
