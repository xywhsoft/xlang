"""Check canonical native factory ABI; never infer it from generated C names."""
import json
import sys

case, path = sys.argv[1:]
with open(path, encoding="utf-8-sig") as stream:
    module = json.load(stream)
targets = module["callTargets"]
types = {t["id"]: t for t in module["types"]}
destroy = [t for t in targets if t.get("cName") == "xl6_xrt_buffer_destroy"]
assert len(destroy) == 1
factories = [t for t in targets if t.get("cName") in
             ("xl6BufferFromHexV1", "xl6BufferFromBase64V1")]
assert len(factories) == (4 if case == "xlir_native_static_factory" else 3 if case == "xlir_buffer_codec_oom" else 2)
for target in factories:
    assert target["kind"] == 3
    assert target["parameterCount"] == (3 if target["cName"] == "xl6BufferFromHexV1" else 5)
    assert target["resultPhysicalAbiName"] == "native-class-field-take"
    assert target["resultPhysicalDestroySymbol"] == destroy[0]["symbol"]
    assert target["resultPhysicalFieldDeclaration"] == destroy[0]["parameterPhysicalFieldDeclarations"][0]
    assert target["returnType"] == destroy[0]["parameterTypes"][0]
    result = types[target["returnType"]]
    assert result["name"] == "Buffer" and result["cAbi"] == 26
    assert result["ownershipAbi"] == 2 and result["nullAbi"] == 2
if case == "xlir_native_static_factory":
    static = [t for t in targets if t.get("cName") == "xl6TextByteLengthV1"]
    assert len(static) == 1 and static[0]["kind"] == 2 and static[0]["parameterCount"] == 2
print(f"{case}: exact static/free factory class/field/destructor ABI")
