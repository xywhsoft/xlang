const net = require("net");
const path = require("path");
const { spawn } = require("child_process");

const repoRoot = path.resolve(__dirname, "../../..");
const demoRoot = path.join(repoRoot, "demo5");
const xlExe = process.platform === "win32"
	? path.join(demoRoot, "release", "xl.exe")
	: path.join(demoRoot, "release", "xl.exe");

const kv = new Map();
const hashes = new Map();
const lists = new Map();
const sets = new Map();
const subscribers = new Map();
let redirectPort = 0;
let finalRedirectPort = 0;
let sentinelRetryCount = 0;
let clusterReadonlyRetryCount = 0;
let childProcess = null;

function bulk(value) {
	if (value === undefined || value === null) {
		return "$-1\r\n";
	}
	const text = String(value);
	return "$" + Buffer.byteLength(text) + "\r\n" + text + "\r\n";
}

function arrayBulk(items) {
	let out = "*" + items.length + "\r\n";
	for (const item of items) {
		out += bulk(item);
	}
	return out;
}

function parseOne(buffer) {
	if (buffer.length < 4 || buffer[0] !== 42) {
		return null;
	}
	const first = buffer.indexOf("\r\n");
	if (first < 0) {
		return null;
	}
	const count = Number(buffer.slice(1, first).toString());
	let pos = first + 2;
	const args = [];
	for (let i = 0; i < count; i++) {
		if (pos >= buffer.length || buffer[pos] !== 36) {
			return null;
		}
		const lenEnd = buffer.indexOf("\r\n", pos);
		if (lenEnd < 0) {
			return null;
		}
		const len = Number(buffer.slice(pos + 1, lenEnd).toString());
		pos = lenEnd + 2;
		if (buffer.length < pos + len + 2) {
			return null;
		}
		args.push(buffer.slice(pos, pos + len).toString());
		pos += len + 2;
	}
	return { args, rest: buffer.slice(pos) };
}

function keyExists(key) {
	return kv.has(key) || hashes.has(key) || lists.has(key) || sets.has(key);
}

function delKeys(keys) {
	let count = 0;
	for (const key of keys) {
		if (kv.delete(key)) count++;
		if (hashes.delete(key)) count++;
		if (lists.delete(key)) count++;
		if (sets.delete(key)) count++;
	}
	return count;
}

function setReply(args) {
	const mode = (args[3] || "").toUpperCase();
	if (mode === "NX" && keyExists(args[1])) {
		return bulk(null);
	}
	if (mode === "XX" && !keyExists(args[1])) {
		return bulk(null);
	}
	kv.set(args[1], args[2]);
	return "+OK\r\n";
}

function reply(args, socket) {
	const cmd = (args[0] || "").toUpperCase();
	switch (cmd) {
		case "PING":
			return "+PONG\r\n";
		case "AUTH":
		case "SELECT":
			return "+OK\r\n";
		case "SENTINEL":
			if ((args[1] || "").toUpperCase() === "GET-MASTER-ADDR-BY-NAME") {
				return arrayBulk(["127.0.0.1", String(redirectPort)]);
			}
			return "-ERR unsupported fake sentinel command\r\n";
		case "CLUSTER": {
			const sub = (args[1] || "").toUpperCase();
			if (sub === "SLOTS") {
				return "*1\r\n*3\r\n:0\r\n:16383\r\n*2\r\n" + bulk("127.0.0.1") + ":" + finalRedirectPort + "\r\n";
			}
			if (sub === "KEYSLOT") {
				return ":12182\r\n";
			}
			return "-ERR unsupported fake cluster command\r\n";
		}
		case "DBSIZE":
			return ":" + kv.size + "\r\n";
		case "XLANG_UNKNOWN_COMMAND":
			return "-ERR unknown command\r\n";
		case "SET":
			return setReply(args);
		case "SETEX":
			kv.set(args[1], args[3]);
			return "+OK\r\n";
		case "MSET":
			for (let i = 1; i + 1 < args.length; i += 2) {
				kv.set(args[i], args[i + 1]);
			}
			return "+OK\r\n";
		case "MGET":
			return arrayBulk(args.slice(1).map((key) => kv.get(key)));
		case "APPEND": {
			const next = String(kv.get(args[1]) || "") + String(args[2] || "");
			kv.set(args[1], next);
			return ":" + Buffer.byteLength(next) + "\r\n";
		}
		case "STRLEN":
			return ":" + Buffer.byteLength(String(kv.get(args[1]) || "")) + "\r\n";
		case "GET":
			if (args[1] === "xlang:redirect:moved") {
				return "-MOVED 12182 127.0.0.1:" + redirectPort + "\r\n";
			}
			if (args[1] === "xlang:redirect:ask") {
				return "-ASK 3999 127.0.0.1:" + redirectPort + "\r\n";
			}
			if (args[1] === "xlang:redirect:chain") {
				return "-MOVED 12000 127.0.0.1:" + redirectPort + "\r\n";
			}
			return bulk(kv.get(args[1]));
		case "EXISTS":
			return ":" + (keyExists(args[1]) ? 1 : 0) + "\r\n";
		case "DEL":
			return ":" + delKeys(args.slice(1)) + "\r\n";
		case "EXPIRE":
			return ":" + (keyExists(args[1]) ? 1 : 0) + "\r\n";
		case "PERSIST":
			return ":" + (keyExists(args[1]) ? 1 : 0) + "\r\n";
		case "TTL":
			return ":" + (keyExists(args[1]) ? 60 : -2) + "\r\n";
		case "PTTL":
			return ":" + (keyExists(args[1]) ? 60000 : -2) + "\r\n";
		case "INCR": {
			const next = Number(kv.get(args[1]) || "0") + 1;
			kv.set(args[1], String(next));
			return ":" + next + "\r\n";
		}
		case "INCRBY": {
			const next = Number(kv.get(args[1]) || "0") + Number(args[2] || "0");
			kv.set(args[1], String(next));
			return ":" + next + "\r\n";
		}
		case "DECR": {
			const next = Number(kv.get(args[1]) || "0") - 1;
			kv.set(args[1], String(next));
			return ":" + next + "\r\n";
		}
		case "DECRBY": {
			const next = Number(kv.get(args[1]) || "0") - Number(args[2] || "0");
			kv.set(args[1], String(next));
			return ":" + next + "\r\n";
		}
		case "HSET": {
			const hash = hashes.get(args[1]) || new Map();
			const isNew = !hash.has(args[2]);
			hash.set(args[2], args[3]);
			hashes.set(args[1], hash);
			return ":" + (isNew ? 1 : 0) + "\r\n";
		}
		case "HGET": {
			const hash = hashes.get(args[1]);
			return bulk(hash ? hash.get(args[2]) : null);
		}
		case "HEXISTS": {
			const hash = hashes.get(args[1]);
			return ":" + (hash && hash.has(args[2]) ? 1 : 0) + "\r\n";
		}
		case "HLEN": {
			const hash = hashes.get(args[1]);
			return ":" + (hash ? hash.size : 0) + "\r\n";
		}
		case "HSTRLEN": {
			const hash = hashes.get(args[1]);
			const value = hash ? hash.get(args[2]) : null;
			return ":" + (value == null ? 0 : Buffer.byteLength(value)) + "\r\n";
		}
		case "HGETALL": {
			const hash = hashes.get(args[1]);
			const items = [];
			if (hash) {
				for (const [key, value] of hash) {
					items.push(key, value);
				}
			}
			return arrayBulk(items);
		}
		case "HDEL": {
			const hash = hashes.get(args[1]);
			return ":" + (hash && hash.delete(args[2]) ? 1 : 0) + "\r\n";
		}
		case "LPUSH": {
			const list = lists.get(args[1]) || [];
			list.unshift(args[2]);
			lists.set(args[1], list);
			return ":" + list.length + "\r\n";
		}
		case "RPUSH": {
			const list = lists.get(args[1]) || [];
			list.push(args[2]);
			lists.set(args[1], list);
			return ":" + list.length + "\r\n";
		}
		case "LLEN": {
			const list = lists.get(args[1]) || [];
			return ":" + list.length + "\r\n";
		}
		case "LRANGE":
			return arrayBulk(lists.get(args[1]) || []);
		case "LPOP": {
			const list = lists.get(args[1]) || [];
			return bulk(list.shift());
		}
		case "RPOP": {
			const list = lists.get(args[1]) || [];
			return bulk(list.pop());
		}
		case "SADD": {
			const set = sets.get(args[1]) || new Set();
			const before = set.size;
			set.add(args[2]);
			sets.set(args[1], set);
			return ":" + (set.size === before ? 0 : 1) + "\r\n";
		}
		case "SISMEMBER": {
			const set = sets.get(args[1]);
			return ":" + (set && set.has(args[2]) ? 1 : 0) + "\r\n";
		}
		case "SMEMBERS": {
			const set = sets.get(args[1]) || new Set();
			return arrayBulk(Array.from(set));
		}
		case "SREM": {
			const set = sets.get(args[1]);
			return ":" + (set && set.delete(args[2]) ? 1 : 0) + "\r\n";
		}
		case "SUBSCRIBE": {
			const channel = args[1];
			const set = subscribers.get(channel) || new Set();
			set.add(socket);
			subscribers.set(channel, set);
			return arrayBulk(["subscribe", channel, "1"]);
		}
		case "UNSUBSCRIBE": {
			const channel = args[1];
			const set = subscribers.get(channel);
			if (set) {
				set.delete(socket);
			}
			return arrayBulk(["unsubscribe", channel, "0"]);
		}
		case "PUBLISH": {
			const channel = args[1];
			const message = args[2];
			const set = subscribers.get(channel);
			let count = 0;
			if (set) {
				for (const sub of set) {
					if (!sub.destroyed) {
						sub.write(arrayBulk(["message", channel, message]));
						count++;
					}
				}
			}
			return ":" + count + "\r\n";
		}
		default:
			return "-ERR unsupported fake command\r\n";
	}
}

function redirectReply(args) {
	const cmd = (args[0] || "").toUpperCase();
	if (cmd === "ASKING") {
		return "+OK\r\n";
	}
	if (cmd === "PING") {
		return args.length > 1 ? bulk(args[1]) : "+PONG\r\n";
	}
	if (cmd === "SELECT") {
		return "+OK\r\n";
	}
	if (cmd === "DBSIZE") {
		return ":" + kv.size + "\r\n";
	}
	if (cmd === "AUTH") {
		return "+OK\r\n";
	}
	if (cmd === "GET" && args[1] === "xlang:redirect:moved") {
		return bulk("RedirectMovedOK");
	}
	if (cmd === "GET" && args[1] === "xlang:redirect:ask") {
		return bulk("RedirectAskOK");
	}
	if (cmd === "GET" && args[1] === "xlang:redirect:chain") {
		return "-MOVED 12001 127.0.0.1:" + finalRedirectPort + "\r\n";
	}
	if (cmd === "GET" && args[1] === "xlang:slot:stale") {
		return "-MOVED 12002 127.0.0.1:" + finalRedirectPort + "\r\n";
	}
	if (cmd === "GET" && args[1] === "xlang:sentinel:master") {
		return bulk("SentinelMasterOK");
	}
	if (cmd === "GET" && args[1] === "xlang:sentinel:retry") {
		sentinelRetryCount++;
		if (sentinelRetryCount === 1) {
			return "-READONLY simulated failover\r\n";
		}
		return bulk("SentinelRetryOK");
	}
	if (cmd === "SET" && args[1] === "xlang:sentinel:wrapper") {
		return setReply(args);
	}
	if (cmd === "SETEX" && args[1] === "xlang:sentinel:ttl") {
		kv.set(args[1], args[3]);
		return "+OK\r\n";
	}
	if (cmd === "MSET") {
		for (let i = 1; i + 1 < args.length; i += 2) {
			kv.set(args[i], args[i + 1]);
		}
		return "+OK\r\n";
	}
	if (cmd === "MGET") {
		return arrayBulk(args.slice(1).map((key) => kv.get(key)));
	}
	if (cmd === "APPEND") {
		const next = String(kv.get(args[1]) || "") + String(args[2] || "");
		kv.set(args[1], next);
		return ":" + Buffer.byteLength(next) + "\r\n";
	}
	if (cmd === "STRLEN") {
		return ":" + Buffer.byteLength(String(kv.get(args[1]) || "")) + "\r\n";
	}
	if (cmd === "GET" && args[1] === "xlang:sentinel:wrapper") {
		return bulk(kv.get(args[1]));
	}
	if (cmd === "GET" && args[1] === "xlang:sentinel:missing") {
		return bulk(null);
	}
	if (cmd === "GET" && args[1] && args[1].startsWith("xlang:sentinel:")) {
		return bulk(kv.get(args[1]));
	}
	if (cmd === "EXISTS" && args[1] === "xlang:sentinel:wrapper") {
		return ":" + (keyExists(args[1]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "DEL" && args[1] === "xlang:sentinel:wrapper") {
		return ":" + delKeys(args.slice(1)) + "\r\n";
	}
	if (cmd === "DEL" && (args[1] === "xlang:sentinel:ttl" || args[1] === "xlang:sentinel:counter")) {
		return ":" + delKeys(args.slice(1)) + "\r\n";
	}
	if (cmd === "EXPIRE" && args[1] === "xlang:sentinel:ttl") {
		return ":" + (keyExists(args[1]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "PERSIST" && args[1] === "xlang:sentinel:ttl") {
		return ":" + (keyExists(args[1]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "TTL" && args[1] === "xlang:sentinel:ttl") {
		return ":" + (keyExists(args[1]) ? 60 : -2) + "\r\n";
	}
	if (cmd === "PTTL" && args[1] === "xlang:sentinel:ttl") {
		return ":" + (keyExists(args[1]) ? 60000 : -2) + "\r\n";
	}
	if (cmd === "INCR" && args[1] === "xlang:sentinel:counter") {
		const next = Number(kv.get(args[1]) || "0") + 1;
		kv.set(args[1], String(next));
		return ":" + next + "\r\n";
	}
	if (cmd === "INCRBY" && args[1] === "xlang:sentinel:counter") {
		const next = Number(kv.get(args[1]) || "0") + Number(args[2] || "0");
		kv.set(args[1], String(next));
		return ":" + next + "\r\n";
	}
	if (cmd === "DECR" && args[1] === "xlang:sentinel:counter") {
		const next = Number(kv.get(args[1]) || "0") - 1;
		kv.set(args[1], String(next));
		return ":" + next + "\r\n";
	}
	if (cmd === "DECRBY" && args[1] === "xlang:sentinel:counter") {
		const next = Number(kv.get(args[1]) || "0") - Number(args[2] || "0");
		kv.set(args[1], String(next));
		return ":" + next + "\r\n";
	}
	if (cmd === "HSET" && args[1] === "xlang:sentinel:hash") {
		const hash = hashes.get(args[1]) || new Map();
		const isNew = !hash.has(args[2]);
		hash.set(args[2], args[3]);
		hashes.set(args[1], hash);
		return ":" + (isNew ? 1 : 0) + "\r\n";
	}
	if (cmd === "HGET" && args[1] === "xlang:sentinel:hash") {
		const hash = hashes.get(args[1]);
		return bulk(hash ? hash.get(args[2]) : null);
	}
	if (cmd === "HEXISTS" && args[1] === "xlang:sentinel:hash") {
		const hash = hashes.get(args[1]);
		return ":" + (hash && hash.has(args[2]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "HLEN" && args[1] === "xlang:sentinel:hash") {
		const hash = hashes.get(args[1]);
		return ":" + (hash ? hash.size : 0) + "\r\n";
	}
	if (cmd === "HSTRLEN" && args[1] === "xlang:sentinel:hash") {
		const hash = hashes.get(args[1]);
		const value = hash ? hash.get(args[2]) : null;
		return ":" + (value == null ? 0 : Buffer.byteLength(value)) + "\r\n";
	}
	if (cmd === "HGETALL" && args[1] === "xlang:sentinel:hash") {
		const hash = hashes.get(args[1]);
		const items = [];
		if (hash) {
			for (const [key, value] of hash) {
				items.push(key, value);
			}
		}
		return arrayBulk(items);
	}
	if (cmd === "HDEL" && args[1] === "xlang:sentinel:hash") {
		const hash = hashes.get(args[1]);
		return ":" + (hash && hash.delete(args[2]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "LPUSH" && args[1] === "xlang:sentinel:list") {
		const list = lists.get(args[1]) || [];
		list.unshift(args[2]);
		lists.set(args[1], list);
		return ":" + list.length + "\r\n";
	}
	if (cmd === "RPUSH" && args[1] === "xlang:sentinel:list") {
		const list = lists.get(args[1]) || [];
		list.push(args[2]);
		lists.set(args[1], list);
		return ":" + list.length + "\r\n";
	}
	if (cmd === "LLEN" && args[1] === "xlang:sentinel:list") {
		const list = lists.get(args[1]) || [];
		return ":" + list.length + "\r\n";
	}
	if (cmd === "LRANGE" && args[1] === "xlang:sentinel:list") {
		return arrayBulk(lists.get(args[1]) || []);
	}
	if (cmd === "LPOP" && args[1] === "xlang:sentinel:list") {
		const list = lists.get(args[1]) || [];
		return bulk(list.shift());
	}
	if (cmd === "RPOP" && args[1] === "xlang:sentinel:list") {
		const list = lists.get(args[1]) || [];
		return bulk(list.pop());
	}
	if (cmd === "SADD" && args[1] === "xlang:sentinel:set") {
		const set = sets.get(args[1]) || new Set();
		const before = set.size;
		set.add(args[2]);
		sets.set(args[1], set);
		return ":" + (set.size === before ? 0 : 1) + "\r\n";
	}
	if (cmd === "SISMEMBER" && args[1] === "xlang:sentinel:set") {
		const set = sets.get(args[1]);
		return ":" + (set && set.has(args[2]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "SMEMBERS" && args[1] === "xlang:sentinel:set") {
		const set = sets.get(args[1]) || new Set();
		return arrayBulk(Array.from(set));
	}
	if (cmd === "SREM" && args[1] === "xlang:sentinel:set") {
		const set = sets.get(args[1]);
		return ":" + (set && set.delete(args[2]) ? 1 : 0) + "\r\n";
	}
	return "-ERR unsupported fake redirect command\r\n";
}

const redirectServer = net.createServer((socket) => {
	let buffer = Buffer.alloc(0);
	socket.on("error", () => {});
	socket.on("data", (chunk) => {
		buffer = Buffer.concat([buffer, chunk]);
		while (true) {
			const parsed = parseOne(buffer);
			if (!parsed) {
				break;
			}
			buffer = parsed.rest;
			socket.write(redirectReply(parsed.args));
		}
	});
});

function finalRedirectReply(args) {
	const cmd = (args[0] || "").toUpperCase();
	if (cmd === "AUTH") {
		return "+OK\r\n";
	}
	if (cmd === "SET") {
		return setReply(args);
	}
	if (cmd === "SETEX") {
		kv.set(args[1], args[3]);
		return "+OK\r\n";
	}
	if (cmd === "MSET") {
		for (let i = 1; i + 1 < args.length; i += 2) {
			kv.set(args[i], args[i + 1]);
		}
		return "+OK\r\n";
	}
	if (cmd === "MGET") {
		return arrayBulk(args.slice(1).map((key) => kv.get(key)));
	}
	if (cmd === "GET" && args[1] === "xlang:redirect:chain") {
		return bulk("RedirectChainOK");
	}
	if (cmd === "GET" && args[1] === "xlang:slot:route") {
		return bulk("SlotRouteOK");
	}
	if (cmd === "GET" && args[1] === "xlang:slot:stale") {
		return bulk("SlotStaleOK");
	}
	if (cmd === "GET" && args[1] === "xlang:slot:readonly") {
		clusterReadonlyRetryCount++;
		if (clusterReadonlyRetryCount === 1) {
			return "-READONLY simulated cluster failover\r\n";
		}
		return bulk("ReadOnlyRecoveredOK");
	}
	if (cmd === "GET") {
		return bulk(kv.get(args[1]));
	}
	if (cmd === "APPEND") {
		const next = String(kv.get(args[1]) || "") + String(args[2] || "");
		kv.set(args[1], next);
		return ":" + Buffer.byteLength(next) + "\r\n";
	}
	if (cmd === "STRLEN") {
		return ":" + Buffer.byteLength(String(kv.get(args[1]) || "")) + "\r\n";
	}
	if (cmd === "EXISTS") {
		return ":" + (keyExists(args[1]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "EXPIRE") {
		return ":" + (keyExists(args[1]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "PERSIST") {
		return ":" + (keyExists(args[1]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "TTL") {
		return ":" + (keyExists(args[1]) ? 60 : -2) + "\r\n";
	}
	if (cmd === "PTTL") {
		return ":" + (keyExists(args[1]) ? 60000 : -2) + "\r\n";
	}
	if (cmd === "INCR") {
		const next = Number(kv.get(args[1]) || "0") + 1;
		kv.set(args[1], String(next));
		return ":" + next + "\r\n";
	}
	if (cmd === "INCRBY") {
		const next = Number(kv.get(args[1]) || "0") + Number(args[2] || "0");
		kv.set(args[1], String(next));
		return ":" + next + "\r\n";
	}
	if (cmd === "DECR") {
		const next = Number(kv.get(args[1]) || "0") - 1;
		kv.set(args[1], String(next));
		return ":" + next + "\r\n";
	}
	if (cmd === "DECRBY") {
		const next = Number(kv.get(args[1]) || "0") - Number(args[2] || "0");
		kv.set(args[1], String(next));
		return ":" + next + "\r\n";
	}
	if (cmd === "HSET") {
		const hash = hashes.get(args[1]) || new Map();
		const isNew = !hash.has(args[2]);
		hash.set(args[2], args[3]);
		hashes.set(args[1], hash);
		return ":" + (isNew ? 1 : 0) + "\r\n";
	}
	if (cmd === "HGET") {
		const hash = hashes.get(args[1]);
		return bulk(hash ? hash.get(args[2]) : null);
	}
	if (cmd === "HEXISTS") {
		const hash = hashes.get(args[1]);
		return ":" + (hash && hash.has(args[2]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "HLEN") {
		const hash = hashes.get(args[1]);
		return ":" + (hash ? hash.size : 0) + "\r\n";
	}
	if (cmd === "HSTRLEN") {
		const hash = hashes.get(args[1]);
		const value = hash ? hash.get(args[2]) : null;
		return ":" + (value == null ? 0 : Buffer.byteLength(value)) + "\r\n";
	}
	if (cmd === "HGETALL") {
		const hash = hashes.get(args[1]);
		const items = [];
		if (hash) {
			for (const [key, value] of hash) {
				items.push(key, value);
			}
		}
		return arrayBulk(items);
	}
	if (cmd === "HDEL") {
		const hash = hashes.get(args[1]);
		return ":" + (hash && hash.delete(args[2]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "LPUSH") {
		const list = lists.get(args[1]) || [];
		list.unshift(args[2]);
		lists.set(args[1], list);
		return ":" + list.length + "\r\n";
	}
	if (cmd === "RPUSH") {
		const list = lists.get(args[1]) || [];
		list.push(args[2]);
		lists.set(args[1], list);
		return ":" + list.length + "\r\n";
	}
	if (cmd === "LLEN") {
		const list = lists.get(args[1]) || [];
		return ":" + list.length + "\r\n";
	}
	if (cmd === "LRANGE") {
		return arrayBulk(lists.get(args[1]) || []);
	}
	if (cmd === "LPOP") {
		const list = lists.get(args[1]) || [];
		return bulk(list.shift());
	}
	if (cmd === "RPOP") {
		const list = lists.get(args[1]) || [];
		return bulk(list.pop());
	}
	if (cmd === "SADD") {
		const set = sets.get(args[1]) || new Set();
		const before = set.size;
		set.add(args[2]);
		sets.set(args[1], set);
		return ":" + (set.size === before ? 0 : 1) + "\r\n";
	}
	if (cmd === "SISMEMBER") {
		const set = sets.get(args[1]);
		return ":" + (set && set.has(args[2]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "SMEMBERS") {
		const set = sets.get(args[1]) || new Set();
		return arrayBulk(Array.from(set));
	}
	if (cmd === "SREM") {
		const set = sets.get(args[1]);
		return ":" + (set && set.delete(args[2]) ? 1 : 0) + "\r\n";
	}
	if (cmd === "DEL") {
		return ":" + delKeys(args.slice(1)) + "\r\n";
	}
	return "-ERR unsupported fake final redirect command\r\n";
}

const finalRedirectServer = net.createServer((socket) => {
	let buffer = Buffer.alloc(0);
	socket.on("error", () => {});
	socket.on("data", (chunk) => {
		buffer = Buffer.concat([buffer, chunk]);
		while (true) {
			const parsed = parseOne(buffer);
			if (!parsed) {
				break;
			}
			buffer = parsed.rest;
			socket.write(finalRedirectReply(parsed.args));
		}
	});
});

const server = net.createServer((socket) => {
	let buffer = Buffer.alloc(0);
	socket.on("error", () => {});
	socket.on("data", (chunk) => {
		buffer = Buffer.concat([buffer, chunk]);
		while (true) {
			const parsed = parseOne(buffer);
			if (!parsed) {
				break;
			}
			buffer = parsed.rest;
			socket.write(reply(parsed.args, socket));
		}
	});
	socket.on("close", () => {
		for (const set of subscribers.values()) {
			set.delete(socket);
		}
	});
});

function startMainServer() {
server.listen(0, "127.0.0.1", () => {
	const port = server.address().port;
	const env = {
		...process.env,
		XL_REDIS_REAL: "1",
		XL_REDIS_PORT: String(port),
		XL_REDIS_PASSWORD: "",
		XL_REDIS_DB: "0",
		XL_REDIS_FAKE_CLUSTER_REDIRECT: "1",
		XL_REDIS_FAKE_SENTINEL_MASTER: "1",
	};
	delete env.XL_REDIS_HOST;
	const child = spawn(xlExe, ["run", "release/examples/redis/redis_basic.xl"], {
		cwd: demoRoot,
		env,
		stdio: ["ignore", "pipe", "pipe"],
	});
	childProcess = child;
	let stdout = "";
	let stderr = "";
	child.stdout.on("data", (chunk) => {
		const text = chunk.toString();
		stdout += text;
		process.stdout.write(text);
	});
	child.stderr.on("data", (chunk) => {
		const text = chunk.toString();
		stderr += text;
		process.stderr.write(text);
	});
	child.on("close", (code) => {
		childProcess = null;
		const match = stdout.match(/RunResult exitCode=(\d+)/);
		const runCode = match ? Number(match[1]) : code;
		if (runCode !== 0 && stderr.length === 0) {
			console.error("x.redis fake smoke failed with run exitCode=" + runCode);
		}
		server.close(() => redirectServer.close(() => finalRedirectServer.close(() => process.exit(runCode))));
	});
});
}

finalRedirectServer.listen(0, "127.0.0.1", () => {
	finalRedirectPort = finalRedirectServer.address().port;
	redirectServer.listen(0, "127.0.0.1", () => {
		redirectPort = redirectServer.address().port;
		startMainServer();
	});
});

setTimeout(() => {
	console.error("x.redis fake smoke timed out");
	if (childProcess) {
		childProcess.kill("SIGKILL");
	}
	server.close(() => redirectServer.close(() => finalRedirectServer.close(() => process.exit(124))));
}, 120000);
