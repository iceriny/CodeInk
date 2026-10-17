"""Check that representative C++ code executes identically after cleanup."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = shutil.which("clang++") or "clang++"
with tempfile.TemporaryDirectory(prefix="cleanup-semantics-", dir=root / "build") as directory:
    folder = Path(directory)
    emitter = folder / "emit"
    subprocess.run([compiler, "-std=c++17", "-Isrc", "tests/basicformat_semantics.cpp",
                    "src/basicformat.cpp", "-o", str(emitter)], cwd=root, check=True)
    sources = [folder / "before.cpp", folder / "after.cpp"]
    subprocess.run([str(emitter), *map(str, sources)], check=True)
    results = []
    for source in sources:
        binary = source.with_suffix("")
        subprocess.run([compiler, "-std=c++20", str(source), "-o", str(binary)], check=True)
        results.append(subprocess.run([str(binary)], check=True, capture_output=True).stdout)
    assert results[0] == results[1], "Formatted C++ changed observable output"
print("PASS: original/formatted C++ compile and execute identically (streams, shifts, templates, pointers, literals and macro stringification)")
