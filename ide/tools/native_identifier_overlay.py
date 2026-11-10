"""Expose allocation-free IDENT validation on the actual native lexer DFA.

Append one query to a new build-owned lexer TU. Preserve every original byte,
derive state/rule constants from its match loop and action, and reject unknown
shapes. The keyword table and scanner tables stay in their original TU.
"""
from pathlib import Path
import hashlib
import re


def derive(source, output):
    source, output = Path(source), Path(output)
    raw = source.read_bytes()
    text = raw.decode("utf8").replace("\r\n", "\n")
    marker = "XL6_TokenKind kind = xl6LookupKeyword(yytext, yyleng);"
    if text.count(marker) != 1 or not re.search(r"^#define INITIAL\s+0\s*$", text, re.M):
        raise ValueError("Unrecognized native IDENT action or INITIAL state")
    at = text.index(marker)
    rule = re.search(r"\ncase\s+(\d+):\s*\nYY_RULE_SETUP[^:]*$", text[:at])
    if not rule or "YY_BREAK" in rule[0]:
        raise ValueError("Cannot bind IDENT action to its native rule")
    match = text.split("\nyy_match:", 1)
    if len(match) != 2:
        raise ValueError("Missing native match loop")
    loop = match[1].split("yy_cp = yyg->yy_last_accepting_cpos", 1)[0]
    meta = re.findall(r"if\s*\(\s*yy_current_state\s*>=\s*(\d+)\s*\)", loop)
    jam = re.findall(r"while\s*\(\s*yy_current_state\s*!=\s*(\d+)\s*\)", loop)
    required = ["yy_ec[YY_SC_TO_UI(*yy_cp)]", "yy_chk[yy_base[yy_current_state] + yy_c]",
                "yy_def[yy_current_state]", "yy_meta[yy_c]", "yy_nxt[yy_base[yy_current_state] + yy_c]"]
    if len(meta) != 1 or len(jam) != 1 or not all(x in loop for x in required):
        raise ValueError("Unrecognized native DFA match transitions")
    constants = dict(rule=int(rule[1]), meta=int(meta[0]), jam=int(jam[0]))
    suffix = ("\n/* Build-owned IDENT query; original TU bytes above are unchanged. */\n" +
              "".join(f"#define XLS_IDENTIFIER_{key.upper()} {value}\n" for key, value in constants.items()) +
              '#include "analysis/frontend_identifier.inc"\n')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(raw + suffix.encode())
    assert output.read_bytes()[:len(raw)] == raw
    return dict(source=str(source), sourceSha256=hashlib.sha256(raw).hexdigest(),
                compiledSource=str(output), compiledSha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                sourceBytesPreserved=len(raw), constants=constants)
