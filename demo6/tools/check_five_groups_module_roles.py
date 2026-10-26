#!/usr/bin/env python3
"""Lint the approved module inventory, not implementation or runtime acceptance."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from collections import Counter
from pathlib import Path


# This independent baseline is the user's approved scope, not a generated API count.
REQUESTED = {
    "concurrency": """atomic spin channel channel_cancel channel_select channel_select_cancel
        channel_coroutine wait once thread_key thread sync mutex cond cancel future future_bridge
        future_continue future_combine task task_group task_pool executor task_group_pool coroutine
        coroutine_scheduler coroutine_event future_coroutine task_coroutine task_group_coroutine sem rwlock event""",
    "crypto_asn1": """asn1_der pem crypto_core crypto_int31 crypto_rsa crypto_rsa_private
        crypto_rsa_pss crypto_rsa_pss_sign crypto_rsa_pkcs1 crypto_rsa_pkcs1_sign crypto_ecdsa_core
        crypto_ecdsa_der crypto_nist crypto_p256 crypto_p384 crypto_nist_keypair crypto_p256_keypair
        crypto_p384_keypair crypto_ecdsa_math crypto_ecdsa_verify crypto_ecdsa_p256 crypto_ecdsa_p384
        crypto_ecdsa_verify_der crypto_ecdsa_p256_der crypto_ecdsa_p384_der crypto_ecdsa_sign
        crypto_ecdsa_p256_sign crypto_ecdsa_p384_sign crypto_ecdsa_sign_der crypto_ecdsa_p256_sign_der
        crypto_ecdsa_p384_sign_der crypto_sha1 crypto_sha256 crypto_sha224 crypto_sha512 crypto_sha512_256
        crypto_md5 crypto_hmac_sha256 crypto_hmac_sha512 crypto_pbkdf2_sha256 crypto_pbkdf2_sha512
        crypto_hkdf_sha256 crypto_hkdf_sha512 crypto_chacha20 crypto_poly1305 crypto_chacha20_poly1305
        crypto_aes crypto_aes_gcm crypto_curve25519 crypto_x25519 crypto_ed25519 crypto_ed25519_sign
        crypto_ed25519_verify crypto_ed25519_keypair crypto_x448 crypto_x25519_keypair crypto_x448_keypair""",
    "x509": """x509_parse x509_crl x509_signature x509_digest x509_verify x509_crl_verify
        x509_verify_rsa x509_verify_ecdsa x509_verify_ed25519 x509_profile x509_name x509_distribution
        x509_crl_profile x509_crl_policy x509_name_constraints x509_path x509_path_build x509_store
        x509_store_file x509_store_system x509_identity""",
    "tls": """tls tls_resume tls_handshake tls_handshake_reader tls_hello tls_negotiate tls_hello_write
        tls_psk tls_psk_write tls_key_exchange tls_policy tls_context tls_key_exchange_x25519
        tls_key_exchange_x448 tls_key_exchange_p256 tls_key_exchange_p384 tls_messages tls_messages_write
        tls_auth_messages tls_auth_messages_write tls_schedule tls_schedule_sha256 tls_schedule_sha384
        tls_record tls_record_aes tls_record_chacha tls_session tls_verify tls_client tls_client_verify
        tls_client_resume tls_identity tls_identity_rsa tls_identity_ec tls_identity_p256 tls_identity_p384
        tls_identity_ed25519 tls_server tls_server_resume tls_stream tls_stream_future tls_stream_listener
        tls_stream_listener_future tls_stream_listener_sync tls_stream_dial tls_stream_dial_proxy tls_stream_dial_future""",
    "network": """net net_interface net_interface_text net_dns net_resolver net_resolver_future net_socket
        net_buffer net_frame net_frame_line net_frame_length net_proxy net_proxy_handshake net_proxy_socks5
        net_proxy_http_connect net_proxy_dial net_port net_port_select net_port_epoll net_port_uring
        net_port_kqueue net_port_iocp net_engine net_file task_net task_group_net net_tcp net_tcp_file
        net_tcp_server net_tcp_server_future net_tcp_server_sync net_sync net_tcp_dial net_tcp_dial_future
        net_tcp_future net_tcp_sync net_tcp_dial_sync net_udp net_udp_future net_udp_sync""",
}
EXPECTED = {(group, module) for group, names in REQUESTED.items() for module in names.split()}
COUNTS = {group: len(names.split()) for group, names in REQUESTED.items()}
DISPOSITIONS = {"internal", "language-mechanism", "public-type", "public-namespace", "composition"}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def contained_file(root: Path, relative: str) -> Path:
    require(isinstance(relative, str) and bool(relative), "empty file witness")
    path = (root / relative).resolve()
    require(not Path(relative).is_absolute() and path.is_relative_to(root), "file witness escapes root")
    require(path.is_file(), "missing witness: " + relative)
    return path


def validate(data: dict, fixtures: dict, root: Path) -> dict:
    require(set(data) == {"SchemaVersion", "Purpose", "RequiredGroups", "OpenQuestions", "Profiles", "Modules"},
            "unknown inventory fields; role inventory must not claim runtime acceptance")
    require(type(data.get("SchemaVersion")) is int and data["SchemaVersion"] == 1, "unsupported schema")
    require(isinstance(data.get("Purpose"), str) and bool(data["Purpose"].strip()), "missing purpose")
    require(data.get("RequiredGroups") == COUNTS, "approved group counts changed")
    rows = data.get("Modules", [])
    require(isinstance(rows, list) and len(rows) == 198, "inventory must contain exactly 198 modules")
    pairs = [(row.get("Group"), row.get("Module")) for row in rows]
    require(len(set(pairs)) == len(pairs), "duplicate module")
    require(set(pairs) == EXPECTED, "approved module scope changed")
    cases = fixtures.get("Cases", [])
    names = [case["Name"] for case in cases]
    require(len(set(names)) == len(names), "duplicate product case")
    for case in cases:
        require(isinstance(case.get("Negative"), bool), "product case needs explicit polarity")
        contained_file(root, case["Source"])
    positive = {case["Name"] for case in cases if not case["Negative"]}
    profiles = data.get("Profiles", {})
    require(isinstance(profiles, dict), "profiles must be an object")
    for name, profile in profiles.items():
        require(set(profile) == {"Surface", "Reason", "AcceptanceBoundary", "SourceWitnesses",
                                "DecisionWitnesses", "NormalProductCases"}, name + ": unknown profile fields")
        for field in ("Reason", "AcceptanceBoundary"):
            require(isinstance(profile.get(field), str) and bool(profile[field].strip()), name + ": " + field)
        require(isinstance(profile.get("Surface"), list), name + ": Surface must be an array")
        for field in ("SourceWitnesses", "DecisionWitnesses"):
            witnesses = profile.get(field, [])
            require(isinstance(witnesses, list) and bool(witnesses), name + ": missing " + field)
            for witness in witnesses:
                contained_file(root, witness)
        selected = profile.get("NormalProductCases", [])
        require(isinstance(selected, list) and bool(selected), name + ": no selected product cases")
        require(len(set(selected)) == len(selected) and set(selected) <= positive, name + ": unknown product case")
    used = set()
    for row in rows:
        require(set(row) == {"Group", "Module", "Disposition", "Profile", "Review"}, "unknown module fields")
        if row.get("Review") == "pending":
            require(row.get("Disposition") == "pending" and row.get("Profile") is None,
                    row["Module"] + ": pending must not claim a reviewed role")
        else:
            require(row.get("Review") == "role-reviewed", row["Module"] + ": unknown review status")
            require(row.get("Disposition") in DISPOSITIONS, row["Module"] + ": unknown disposition")
            require(row.get("Profile") in profiles, row["Module"] + ": unknown profile")
            used.add(row["Profile"])
    require(used == set(profiles), "unused profile")
    questions = data.get("OpenQuestions", [])
    question_ids = [q["Id"] for q in questions]
    require(len(set(question_ids)) == len(question_ids), "duplicate open question")
    modules = {module for _, module in EXPECTED}
    for question in questions:
        require(set(question) == {"Id", "Modules", "State", "Question"}, "unknown question fields")
        require(bool(question.get("Question", "").strip()), "empty open question")
        require(question.get("State") == "awaiting-api-scope-confirmation", "unknown question state")
        require(bool(question.get("Modules")) and set(question["Modules"]) <= modules, "unknown question module")
    return dict(InventoryValid=True, RequestedModules=len(rows), Groups=COUNTS,
                RoleReviews=dict(Counter(row["Review"] for row in rows)),
                Dispositions=dict(Counter(row["Disposition"] for row in rows)),
                Profiles=len(profiles), OpenQuestions=question_ids,
                RuntimeAcceptanceEvaluated=False, FullFiveGroupsAccepted=False, FullGoalComplete=False)


def self_test(data: dict, fixtures: dict, root: Path) -> int:
    mutations = []
    def check(name, mutate):
        candidate = copy.deepcopy(data)
        mutate(candidate)
        try:
            validate(candidate, fixtures, root)
        except ValueError:
            mutations.append(name)
        else:
            raise AssertionError("invalid inventory accepted: " + name)
    check("missing module", lambda d: d["Modules"].pop())
    check("duplicate module", lambda d: d["Modules"].__setitem__(-1, d["Modules"][0]))
    check("renamed module", lambda d: d["Modules"][0].__setitem__("Module", "not_approved"))
    check("wrong group", lambda d: d["Modules"][0].__setitem__("Group", "network"))
    check("unknown profile", lambda d: d["Modules"][0].__setitem__("Profile", "not_defined"))
    check("invented acceptance status", lambda d: d["Modules"][0].__setitem__("Review", "complete"))
    reviewed = next(i for i, row in enumerate(data["Modules"]) if row["Review"] == "role-reviewed")
    check("unreviewed role claimed", lambda d: d["Modules"][reviewed].__setitem__("Review", "pending"))
    first = next(iter(data["Profiles"]))
    check("unknown case", lambda d: d["Profiles"][first].__setitem__("NormalProductCases", ["not_defined"]))
    check("missing witness", lambda d: d["Profiles"][first].__setitem__("SourceWitnesses", ["not_present.c"]))
    check("escaping witness", lambda d: d["Profiles"][first].__setitem__("SourceWitnesses", ["../README.md"]))
    check("runtime acceptance claim", lambda d: d.__setitem__("Complete", True))
    return len(mutations)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[1]
    parser.add_argument("--roles", type=Path, default=root / "tools/five_groups_module_roles.json")
    parser.add_argument("--fixtures", type=Path, nargs="+", default=[
        root / "tools/concurrency_public_fixtures.json", root / "tools/crypto_surface_public_fixtures.json",
        root / "tools/x509_public_fixtures.json", root / "tools/x509_policy_public_fixtures.json",
        root / "tools/network_surface_public_fixtures.json"])
    parser.add_argument("--report", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    paths = [Path(__file__).resolve(), args.roles.resolve(), *[p.resolve() for p in args.fixtures]]
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    inputs = {str(path): sha(path) for path in paths}
    try:
        data = json.loads(paths[1].read_text(encoding="utf-8-sig"))
        fixtures = {"Cases": []}
        for path in paths[2:]:
            fixtures["Cases"].extend(json.loads(path.read_text(encoding="utf-8-sig"))["Cases"])
        result = validate(data, fixtures, root)
        result["RejectedInvalidInventories"] = self_test(data, fixtures, root) if args.self_test else 0
        require(inputs == {str(path): sha(path) for path in paths}, "inputs changed during review")
        result["Inputs"] = inputs
    except (ValueError, KeyError, TypeError, OSError) as error:
        parser.exit(1, "module-role inventory rejected: " + str(error) + "\n")
    if args.report:
        args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in result.items() if k != "Inputs"}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
