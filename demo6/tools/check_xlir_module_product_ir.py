#!/usr/bin/env python3
"""Inspect canonical IR binding identities, not generated C variable spelling."""
import json
import sys

name, path = sys.argv[1:]
with open(path, encoding="utf-8-sig") as source:
    module = json.load(source)
nodes = {node["id"]: node for node in module["nodes"]}
types = {item["id"]: item for item in module["types"]}
storage = {item["symbol"]: item for item in module["storages"]}
root = next(node for node in nodes.values() if node["kindName"] == "module")
root_products = 0
local_product_symbols = set()
for declaration in nodes.values():
    if declaration["kindName"] != "value-declaration":
        continue
    type_ = types[declaration["storageType"]]
    if type_["kind"] == 5 and type_["cAbi"] == 32:
        children = module["nodeChildren"]
        bindings = nodes[children[declaration["firstChild"]]]
        fields = [nodes[item] for item in children[bindings["firstChild"]:
                                                  bindings["firstChild"] + bindings["childCount"]]]
        assert len(fields) == len(type_["params"])
        if declaration["parent"] == root["id"]:
            root_products += 1
            for field in fields:
                if not field["symbol"]:
                    continue  # Explicit discard has no storage identity.
                item = storage[field["symbol"]]
                assert item["kindName"] == "global", item
                assert item["declaration"] == field["id"]
        else:
            local_product_symbols.update(field["symbol"] for field in fields if field["symbol"])
    elif declaration["parent"] == root["id"] and declaration["symbol"]:
        item = storage[declaration["symbol"]]
        assert item["kindName"] == "global", item
assert root_products == (4 if name == "xlir_module_product_globals" else 1)
globals_ = {item["symbol"] for item in module["storages"] if item["kindName"] == "global"}
prints = [target for target in module["callTargets"] if target.get("cName") == "xl6_xrt_print"]
if name != "xlir_module_product_initializer":
    assert len(prints) == 1
for target in prints:
    assert target["runtimeBridgeName"] == "print" and target["errorAbiName"] == "may-raise"
    assert target["effectAbiName"] == "conservative"
    assert target["executionBoundaryAbiName"] == "foreign-may-block"
    assert target["parameterAbis"] == [2, 1] and target["parameterPhysicalAbis"] == ["direct", "direct"]
assert all(capture["sourceSymbol"] not in globals_ for capture in module["closureCaptures"])
assert not any(item["kindName"] == "cell-new" and item.get("targetSymbol") in globals_
               for item in module["instructions"])
if name == "xlir_module_product_closure":
    captures = {capture["sourceSymbol"]: capture for capture in module["closureCaptures"]}
    assert set(captures) == local_product_symbols and len(captures) == 2
    text_symbol = next(symbol for symbol, item in captures.items() if item["abiName"] == "retain-cell")
    bytes_symbol = next(symbol for symbol, item in captures.items() if item["abiName"] == "retain-xvalue")
    assert captures[text_symbol]["abiName"] == "retain-cell"
    assert storage[text_symbol]["type"] == captures[text_symbol]["type"]
    assert captures[bytes_symbol]["abiName"] == "retain-xvalue"
    allocations = [item for item in module["instructions"]
                   if item["kindName"] == "cell-new" and item.get("targetSymbol") == text_symbol]
    assert len(allocations) == 1 and allocations[0]["type"] == captures[text_symbol]["cellType"]
    initializations = [item for item in module["instructions"]
                       if item["kindName"] == "cell-init" and item.get("targetSymbol") == text_symbol]
    assert len(initializations) == 1 and initializations[0]["type"] == allocations[0]["type"]
print("module product canonical global/local capture IR ok:", name)
