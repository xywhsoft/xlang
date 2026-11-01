"""Reject mixed revisions, corrupted snapshots and checkout-induced drift."""
from pathlib import Path
import hashlib
import json
import tempfile
import unittest

from check_xrt_alignment import check


class XrtAlignmentTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="xrt-alignment-")
        self.addCleanup(self.directory.cleanup)
        base = Path(self.directory.name)
        self.root, self.xge = base / "xlang", base / "xge"
        self.pin = "a" * 40
        self.impl, self.decl = b"implementation\n", b"declarations\n"
        self.write(self.root / "lib/xrt/xrt.h", self.impl)
        self.write(self.root / "release/include/xrt_decl.h", self.decl)
        self.write(self.xge / "extlibs/xrt/xrt.h", self.impl)
        self.write(self.xge / "extlibs/xrt/xrt_decl.h", self.decl)
        self.upstream = {"revision": self.pin, "dirty": False}
        self.native = {"source_commit": self.pin, "overlay_files": {},
                       "header_sha256": self.sha(self.impl), "declaration_header_sha256": self.sha(self.decl)}
        self.snapshot = {"implementation": {"sha256": self.sha(self.impl)},
                         "declarations": {"sha256": self.sha(self.decl)}}
        self.metadata()

    @staticmethod
    def sha(data):
        return hashlib.sha256(data).hexdigest()

    @staticmethod
    def write(path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def metadata(self):
        for path, value in ((self.root / "lib/xrt/upstream.json", self.upstream),
                            (self.root / "lib/xrt/xrt.snapshot.json", self.snapshot),
                            (self.xge / "extlibs/xrt/upstream.json", self.native)):
            self.write(path, json.dumps(value).encode())

    def test_matching_committed_snapshots(self):
        self.assertEqual(check(self.root, self.xge, self.pin)["revision"], self.pin)

    def test_revision_mismatch_with_identical_headers(self):
        self.native["source_commit"] = "b" * 40
        self.metadata()
        with self.assertRaisesRegex(ValueError, "revision mismatch"):
            check(self.root, self.xge)

    def test_unexpected_pin(self):
        with self.assertRaisesRegex(ValueError, "revision mismatch"):
            check(self.root, self.xge, "b" * 40)

    def test_same_revision_does_not_hide_corrupt_bytes(self):
        self.write(self.xge / "extlibs/xrt/xrt.h", self.impl + b"changed")
        with self.assertRaisesRegex(ValueError, "bytes differ"):
            check(self.root, self.xge)

    def test_stale_provenance_is_rejected(self):
        self.native["header_sha256"] = "0" * 64
        self.metadata()
        with self.assertRaisesRegex(ValueError, "digest mismatch"):
            check(self.root, self.xge)

    def test_matching_crlf_snapshots_are_rejected(self):
        data = self.impl.replace(b"\n", b"\r\n")
        self.write(self.root / "lib/xrt/xrt.h", data)
        self.write(self.xge / "extlibs/xrt/xrt.h", data)
        self.native["header_sha256"] = self.snapshot["implementation"]["sha256"] = self.sha(data)
        self.metadata()
        with self.assertRaisesRegex(ValueError, "canonical LF"):
            check(self.root, self.xge)

    def test_development_overlays_are_rejected(self):
        self.native["overlay_files"] = {"src/core.c": "uncommitted"}
        self.metadata()
        with self.assertRaisesRegex(ValueError, "without source overlays"):
            check(self.root, self.xge)


if __name__ == "__main__":
    unittest.main()
