"""Assert the sealed Buffer class/field/destructor and byte-width contracts."""
import json
import sys

case, path = sys.argv[1:]
with open(path, encoding="utf-8-sig") as stream:
    module = json.load(stream)
targets = module["callTargets"]
types = {t["id"]: t for t in module["types"]}
destroy = [t for t in targets if t.get("cName") == "xl6BufferDestroyV1"]
assert len(destroy) == 1 and destroy[0]["errorAbiName"] == "no-raise"
owner = destroy[0]
for name, count in (("xl6BufferCreateV1", 0), ("xl6BufferCreateReserveV1", 1), ("xl6BufferAdoptV1", 1)):
    found = [t for t in targets if t.get("cName") == name]
    assert len(found) == 1
    target = found[0]
    assert target["kind"] == 3 and target["parameterCount"] == count
    assert target["errorAbiName"] == "may-raise"
    assert target["resultPhysicalAbiName"] == "native-class-field-take"
    assert target["resultPhysicalDestroySymbol"] == owner["symbol"]
    assert target["resultPhysicalFieldDeclaration"] == owner["parameterPhysicalFieldDeclarations"][0]
    assert target["returnType"] == owner["parameterTypes"][0]
for name, count, handles in (("xl6BufferCopyFromV1",5,(0,2)), ("xl6BufferCopyWithinV1",4,(0,)),
                              ("xl6BufferFillV1",4,(0,)), ("xl6BufferCompareV1",4,(0,1)),
                              ("xl6BufferIndexOfV1",5,(0,1)), ("xl6BufferByteAtV1",2,(0,))):
    found = [t for t in targets if t.get("cName") == name]
    assert len(found) == 1
    target = found[0]
    assert target["kind"] == 2 and target["parameterCount"] == count
    assert target["errorPhysicalAbiName"] == "xrt-thread-error"
    for slot in handles:
        assert target["parameterTypes"][slot] == owner["parameterTypes"][0]
        assert target["parameterPhysicalAbis"][slot] == "native-class-handle-borrow"
        assert target["parameterPhysicalFieldDeclarations"][slot] == owner["parameterPhysicalFieldDeclarations"][0]
    if name == "xl6BufferByteAtV1":
        assert types[target["returnType"]]["integerTargetName"] == "u8"
assert not any(t.get("runtimeBridgeName") == "buffer" for t in targets)
print(f"{case}: sealed Buffer factory/owner/byte-query ABI")
