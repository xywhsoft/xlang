"""Feed actual native-service full/delta answers into the IDE's C decoder.

Both executables must be owned aliases in the real release runtime. The absent
compiler proves native analysis. Python independently computes UTF16-to-byte
expectations; C checks every numeric word and immutable render range.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_semantic_tokens import CAP, apply, decode

ROLES = ["type", "class", "interface", "enum", "struct", "typeParameter",
         "parameter", "variable", "property", "enumMember", "function", "method"]
MODIFIERS = {"declaration": 1, "readonly": 2, "async": 4}


def expected_ranges(text, words, legend):
    lines = text.splitlines(keepends=True)
    offsets = [0]
    for line in lines:
        offsets.append(offsets[-1] + len(line.encode("utf-8")))
    result = []
    for line, column, length, role, modifiers in decode(words, legend):
        content = lines[line].rstrip("\r\n")
        utf16 = content.encode("utf-16-le")
        start = utf16[:column * 2].decode("utf-16-le")
        end = utf16[:(column + length) * 2].decode("utf-16-le")
        assert len(start.encode("utf-16-le")) == column * 2
        assert len(end.encode("utf-16-le")) == (column + length) * 2
        result.append([offsets[line] + len(start.encode("utf-8")),
                       offsets[line] + len(end.encode("utf-8")),
                       ROLES.index(role), sum(MODIFIERS[m] for m in modifiers)])
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--server", type=Path, required=True)
    p.add_argument("--decoder", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    server, decoder = args.server.resolve(strict=True), args.decoder.resolve(strict=True)
    assert server.parent == decoder.parent
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    protected = {str(path): sha(path) for path in (server, decoder, Path(__file__),
                 Path(__file__).with_name("check_lsp_semantic_tokens.py"),
                 Path(__file__).with_name("check_language_snapshots.py"))}
    c = AutomaticClient(server, out / "absent/xl.exe", server.parent)
    cases, summary = [], []
    report = dict(status="failed", protected=protected, cases=summary, scope=__doc__)
    source = ("\ufeffclass Box\r\n int value\r\n func read() as int\r\n return self.value\r\n end\r\nend\r\n"
              "func pick(int 输入) as int\r\n return 输入\r\nend\r\n"
              "func main(...) as int\r\n string 文本=\"😀\"; int 局部=pick(4)\r\n"
              " auto box=Box()\r\n return 局部+box.read()\r\nend\r\n")
    path = out / "input.xl"
    path.write_bytes(b"// disk must not override native open text\n")
    protected[str(path)] = sha(path)
    try:
        c.send("initialize", dict(capabilities=dict(textDocument=dict(semanticTokens=CAP)),
               initializationOptions=dict(automaticDiagnostics=False, checkOnSave=False,
                                          backgroundWorkspaceSymbols=False)), 1)
        reply, _ = c.reply(1)
        provider = reply["result"]["capabilities"]["semanticTokensProvider"]
        assert provider["full"] == dict(delta=True)
        c.send("initialized", {})
        c.open(path, source, 1)
        words = []

        def request(name, text, previous=None):
            nonlocal words
            params = dict(textDocument=dict(uri=path.as_uri()))
            method = "textDocument/semanticTokens/full"
            if previous is not None:
                method += "/delta"
                params["previousResultId"] = previous
            result = c.call(method, params)
            words = apply(words, result) if result is not None else []
            expected = expected_ranges(text, words, provider["legend"])
            cases.append(dict(text=text, result=result, expected=expected, words=words.copy()))
            summary.append(dict(name=name, tokens=len(expected), delta=isinstance(result, dict) and "edits" in result))
            return result

        first = request("full-bom-crlf-unicode-members", source)
        assert first["resultId"] and len(words) > 50
        assert {"class", "function", "property", "method", "parameter"}.issubset(
            {provider["legend"]["tokenTypes"][words[i + 3]] for i in range(0, len(words), 5)})
        same = request("identical-delta", source, first["resultId"])
        assert same["edits"] == []
        shifted = source.replace("\ufeff", "\ufeff// 😀 shifted line\r\n", 1)
        c.change(path, shifted, 2)
        changed = request("newline-incremental-delta", shifted, same["resultId"])
        assert "edits" in changed
        renamed = shifted.replace("局部", "新的局部")
        c.change(path, renamed, 3)
        changed = request("longer-unicode-name-delta", renamed, changed["resultId"])
        assert words
        broken = "func main(...) as int\n return missingName\nend\n"
        c.change(path, broken, 4)
        assert request("rejected-source-clears-color", broken) is None
        c.change(path, renamed, 5)
        recovered = request("source-recovers-full", renamed)
        assert recovered["data"] and recovered["resultId"]
        replacement = request("unknown-history-falls-back-full", renamed, "not-a-current-result")
        assert replacement["data"] == recovered["data"]
        null = request("unchanged-after-full-replacement", renamed, replacement["resultId"])
        assert null["edits"] == []
        batch = out / "native-decode-batch.json"
        batch.write_text(json.dumps(dict(provider=provider, cases=cases), ensure_ascii=False, indent=2) + "\n", encoding="utf8")
        cmd = [str(decoder), "--protocol", str(batch)]
        z = subprocess.run(cmd, cwd=server.parent, capture_output=True, timeout=30)
        (out / "decoder.log").write_bytes(z.stdout + z.stderr)
        assert not z.returncode, (z.stdout, z.stderr)
        report.update(status="passed", command=cmd, batchSha256=sha(batch),
                      nativeDecoderOutput=z.stdout.decode(errors="replace"))
    finally:
        c.stop()
        (out / "transcript.json").write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2) + "\n", encoding="utf8")
        for name, h in protected.items():
            assert sha(name) == h, name
        (out / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf8")
    print("PASS native-service to IDE color decoder", len(cases), "cases", flush=True)


if __name__ == "__main__":
    main()
