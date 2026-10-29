"""验证嵌套 VFS 只引用本次输出目录，不回退到默认构建目录。"""

from pathlib import Path
import tempfile
import unittest

import gen_tcc_vfs_resources as vfs


class OutputDirectoryTests(unittest.TestCase):
    def test_relative_resource(self):
        root = Path.cwd()
        self.assertEqual(
            vfs.resolve_manifest_path("include/xl.h", root, root, root, root / "out"),
            root / "include" / "xl.h",
        )

    def test_relocated_resource(self):
        root = Path.cwd()
        output = root / "build" / "候选 runtime"
        for separator in ("/", "\\"):
            self.assertEqual(
                vfs.resolve_manifest_path(
                    "%XL6_VFS_OUTPUT_DIR%" + separator + "tcc_builtin_resources_runtime.c",
                    root, root, root, output,
                ),
                output / "tcc_builtin_resources_runtime.c",
            )

    def test_missing_sibling_never_uses_old_pack(self):
        # TemporaryDirectory 仅拥有本测试创建的目录，退出时由标准库清理。
        with tempfile.TemporaryDirectory(prefix="xl-vfs-path-") as directory:
            root = Path(directory).resolve()
            old = root / "build" / "tmp"
            old.mkdir(parents=True)
            (old / "tcc_builtin_resources_runtime.c").write_text("stale", encoding="utf-8")
            output = root / "candidate"
            manifest = root / "manifest.txt"
            manifest.write_text(
                "file|nested/runtime.c|%XL6_VFS_OUTPUT_DIR%/tcc_builtin_resources_runtime.c\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(RuntimeError, "missing VFS resource source"):
                vfs.load_resources(manifest, root, root, root, output)
            output.mkdir()
            current = output / "tcc_builtin_resources_runtime.c"
            current.write_text("current", encoding="utf-8")
            resources = vfs.load_resources(manifest, root, root, root, output)
            self.assertEqual(len(resources), 1)
            self.assertEqual(resources[0].source, current)


if __name__ == "__main__":
    unittest.main()
