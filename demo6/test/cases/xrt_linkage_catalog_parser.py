"""Provenance/parser counterexamples independent of the generated catalog."""
from pathlib import Path
import importlib.util

script = Path(__file__).resolve().parents[2] / "tools/gen_xrt_linkage_catalog.py"
spec = importlib.util.spec_from_file_location("linkage_catalog", script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
text = r'''
# 1 "__xrt_catalog__/src/first.c"
extern int xrtPrototype(int);
const char* noise = "int xrtStringFake(void) { return 0; }";
int (xrtParenthesized)(int value) { return value; }
int xrtOuter(void (*callback)(int)) {
    if (xrtNestedCall()) { return 1; }
    return 0;
}
# 1 "__xrt_catalog__/src/second.c"
int xrtBareDefinition(void) { return '}'; }
'''
names = {"xrtPrototype", "xrtStringFake", "xrtParenthesized", "xrtOuter",
         "xrtNestedCall", "xrtBareDefinition"}
assert module.definitions(text, names) == {
    "xrtParenthesized": "src/first.c", "xrtOuter": "src/first.c",
    "xrtBareDefinition": "src/second.c"}
try:
    module.definitions(text + '\nint xrtBareDefinition(void) { return 0; }', names)
    raise AssertionError("duplicate actual definition accepted")
except ValueError as error:
    assert "duplicate" in str(error)
print("native linkage parser: prototype/literal/nested call excluded; parenthesized and unannotated definitions retained; duplicates refused")
