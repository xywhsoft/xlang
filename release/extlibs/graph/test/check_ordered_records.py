"""枚举所有简单路径及合法排列，独立验证完整结果，不复用 BFS/Kahn/Kosaraju。"""
import argparse
import itertools
import json
from pathlib import Path

NAMES = [
    [b"z", b"a", b"m", b"c"],
    [b"", b"a\0b", b"a", b"b"],
    ["中".encode(), b"a", "文".encode(), "😀".encode()],
    [b"A", b"a", b"A\0", b"A\0B"],
]


def edges(sample, reverse):
    count = 3 if sample < 512 else 4
    pairs = list(itertools.product(range(count), repeat=2)) if count == 3 else list(itertools.combinations(range(count), 2))
    mask = sample if count == 3 else sample - 512
    selected = [edge for bit, edge in enumerate(pairs) if mask & (1 << bit)]
    if reverse:
        selected.reverse()
        selected = [edge for index, edge in enumerate(selected) for _ in range(2 if index % 2 == 0 else 1)]
    return count, selected


def paths(count, adjacent, start, target):
    if start == target:
        return [(start,)]
    middle = set(range(count)) - {start, target}
    result = []
    for size in range(len(middle) + 1):
        for inner in itertools.permutations(sorted(middle), size):
            path = (start, *inner, target)
            if all(adjacent[a][b] for a, b in zip(path, path[1:])):
                result.append(path)
    return result


def answers(sample, reverse):
    count, selected = edges(sample, reverse)
    input_order = list(reversed(range(count))) if reverse else list(range(count))
    input_position = {node: index for index, node in enumerate(input_order)}
    adjacent = [[(a, b) in selected for b in range(count)] for a in range(count)]
    directed = {(a, b): paths(count, adjacent, a, b) for a in range(count) for b in range(count)}
    weak = [[adjacent[a][b] or adjacent[b][a] for b in range(count)] for a in range(count)]
    ranks = {}
    for rank, edge in enumerate(selected):
        ranks.setdefault(edge, rank)

    def best(a, b):
        return min(directed[a, b], key=lambda path: (len(path), tuple(ranks[edge] for edge in zip(path, path[1:]))), default=None)

    def groups(strong):
        result, remaining = [], set(range(count))
        while remaining:
            first = min(remaining, key=input_position.get)
            group = [node for node in sorted(remaining, key=input_position.get) if
                     (directed[first, node] and directed[node, first] if strong else paths(count, weak, first, node))]
            result.append(group)
            remaining.difference_update(group)
        return result

    dag = not any(directed[b, a] for a, b in selected)
    topological = []
    if dag:
        # 对全部合法拓扑排列检查入队时刻约束，而不是重写产品的队列。
        for order in itertools.permutations(range(count)):
            position = {node: index for index, node in enumerate(order)}
            if any(position[a] >= position[b] for a, b in selected):
                continue

            def ready(node):
                incoming = [(position[parent], rank) for rank, (parent, child) in enumerate(selected) if child == node]
                return max(incoming, default=(-1, input_position[node]))

            if list(order) == sorted(range(count), key=ready):
                topological.append(list(order))
        if len(topological) != 1:
            raise ValueError(("non-unique declarative queue solution", sample, reverse, topological))
    yield [sample, reverse, "dag", -1, -1, dag]
    yield [sample, reverse, "topo", -1, -1, topological[0] if dag else "value-error"]
    yield [sample, reverse, "components", -1, -1, groups(False)]
    yield [sample, reverse, "scc", -1, -1, groups(True)]
    yield [sample, reverse, "nodes", -1, -1, sorted(range(count), key=lambda node: NAMES[sample % 4][node])]
    for start in range(count):
        neighbors = list(dict.fromkeys(b for a, b in selected if a == start))
        chosen = {target: best(start, target) for target in range(count)}
        bfs = sorted((node for node, path in chosen.items() if path is not None),
                     key=lambda node: (len(chosen[node]), tuple(ranks[edge] for edge in zip(chosen[node], chosen[node][1:]))))
        distances = [min((sum((a * 3 + b) % 3 for a, b in zip(path, path[1:])) for path in directed[start, target]), default=None)
                     for target in range(count)]
        yield [sample, reverse, "neighbors", start, -1, neighbors]
        yield [sample, reverse, "bfs", start, -1, bfs]
        yield [sample, reverse, "distance", start, -1, distances]
        for target in range(count):
            yield [sample, reverse, "path", start, target, list(chosen[target]) if chosen[target] is not None else "value-error"]


def validate(actual, expected):
    if len(actual) != 27776 or len(expected) != 27776:
        raise ValueError(("incomplete ordered corpus", len(actual), len(expected)))
    for index, (got, want) in enumerate(zip(actual, expected)):
        # JSON 的布尔与整数不能借用 Python 的 True == 1 绕过类型校验。
        if json.dumps(got) != json.dumps(want):
            raise ValueError(("ordered result differs", index, got, want))


def controls(expected):
    # 这些结果仍满足旧的成员/距离/拓扑性质检查，完整合同必须能够拒绝。
    changes = [
        (next(i for i, row in enumerate(expected) if row[2] == "neighbors" and isinstance(row[5], list) and len(row[5]) > 1), None),
        (next(i for i, row in enumerate(expected) if row[2] == "scc" and len(row[5]) > 1), None),
        (next(i for i, row in enumerate(expected) if row[:5] == [563, 0, "path", 0, 3]), [0, 2, 3]),
        (next(i for i, row in enumerate(expected) if row[:5] == [512, 0, "topo", -1, -1]), [1, 0, 2, 3]),
    ]
    for index, replacement in changes:
        changed = list(expected)
        row = list(expected[index])
        row[5] = list(reversed(row[5])) if replacement is None else replacement
        if row == expected[index]:
            raise ValueError("ineffective oracle negative control")
        changed[index] = row
        try:
            validate(changed, expected)
        except ValueError:
            continue
        raise ValueError("oracle accepted incorrect order")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("records", type=Path)
    args = parser.parse_args()
    expected = [row for sample in range(576) for reverse in range(2) for row in answers(sample, reverse)]
    actual = [json.loads(line) for line in args.records.read_text(encoding="utf8").splitlines()]
    validate(actual, expected)
    controls(expected)
    print("graph ordered oracle: 1152 graphs, 27776 records, 4 negative controls PASS")


if __name__ == "__main__":
    main()
