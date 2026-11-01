"""Keep Ribbon generation independent from the Windows build toolchain."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import dev


class BuildTests(unittest.TestCase):
    def load(self):
        spec = importlib.util.spec_from_file_location("codeink_build", ROOT / "build.py")
        module = importlib.util.module_from_spec(spec)
        with patch.object(dev, "configure_tools") as configure, \
                patch("shutil.which") as which, patch.object(Path, "mkdir") as mkdir:
            spec.loader.exec_module(module)
        configure.assert_not_called()
        which.assert_not_called()
        mkdir.assert_not_called()
        return module

    def test_import_does_not_initialize_build(self):
        self.load()

    def test_ribbon_generation_without_compiler_or_linker(self):
        module = self.load()
        with tempfile.TemporaryDirectory(prefix="codeink-build-") as folder:
            module.BUILD = Path(folder) / "build"
            module.DIST = Path(folder) / "dist"
            with patch.object(module, "configure_tools") as configure, \
                    patch.object(module.shutil, "which", return_value=None) as which:
                module.ribbon()
            configure.assert_not_called()
            which.assert_not_called()
            self.assertIn("OnColumnCount", (module.BUILD / "ribbon.generated.h").read_text(encoding="utf-8"))
            self.assertFalse(module.DIST.exists())

    def test_real_build_still_requires_windows_linker(self):
        module = self.load()
        with patch.object(module, "configure_tools") as configure, \
                patch.object(module.shutil, "which", side_effect=lambda name: "clang++" if name == "clang++" else None), \
                patch.object(Path, "mkdir") as mkdir:
            with self.assertRaisesRegex(SystemExit, "lld-link"):
                module.main()
        configure.assert_called_once()
        mkdir.assert_not_called()


if __name__ == "__main__":
    unittest.main()
