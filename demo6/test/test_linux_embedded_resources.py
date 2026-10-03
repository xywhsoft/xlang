"""Small, platform-independent mutations for the native-product resource checker."""
import ctypes
import lzma
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_linux_embedded_resources import BuiltinResource, decode, verify_resource


def entry(source, packed=None, method=0):
    packed = source if packed is None else packed
    backing = ctypes.create_string_buffer(packed)
    resource = BuiltinResource(b"fixture", ctypes.cast(backing,
        ctypes.POINTER(ctypes.c_ubyte)), len(packed), len(source), method)
    resource.backing = backing
    return resource


class ResourceContract(unittest.TestCase):
    def test_stored_binary_and_empty(self):
        for source in (b"", b"a\x00b\xff", b"abcdef"):
            verify_resource(entry(source), "fixture", source)

    def test_independent_lzma(self):
        source = b"a\x00b\xff" * 200
        alone = lzma.compress(source, format=lzma.FORMAT_ALONE)
        packed = alone[:5] + alone[13:]
        self.assertLess(len(packed), len(source))
        self.assertEqual(decode(packed, len(source), 1), source)
        verify_resource(entry(source, packed, 1), "fixture", source)
        with self.assertRaises((ValueError, lzma.LZMAError)):
            decode(packed + b"trailing", len(source), 1)

    def test_identity_and_extent_mutations(self):
        for field, value in (("name", b"other"), ("name", None), ("size", 2),
                             ("packed_size", 200), ("method", 2), ("data", None)):
            with self.subTest(field=field, value=value):
                resource = entry(b"abc")
                setattr(resource, field, value)
                with self.assertRaises(AssertionError):
                    verify_resource(resource, "fixture", b"abc")

    def test_stale_bytes_and_bad_encoding(self):
        with self.assertRaisesRegex(AssertionError, "stale"):
            verify_resource(entry(b"abc", b"abd"), "fixture", b"abc")
        with self.assertRaises(ValueError):
            verify_resource(entry(b"abc", b"x", 1), "fixture", b"abc")
        with self.assertRaises(ValueError):
            decode(b"abc", 1, 0)


if __name__ == "__main__":
    unittest.main()
