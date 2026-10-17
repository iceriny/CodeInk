#!/usr/bin/env python3
"""Shared development commands for the terminal and VS Code (stdlib only)."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent


def configure_tools():
    if os.name == "nt":
        candidates = [Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "LLVM/bin"]
        if os.environ.get("LOCALAPPDATA"):
            candidates.append(Path(os.environ["LOCALAPPDATA"]) / "Programs/LLVM/bin")
        for llvm in candidates:
            if (llvm / "clang++.exe").is_file():
                os.environ["PATH"] = str(llvm) + os.pathsep + os.environ.get("PATH", "")
                break
    os.environ["PYTHONUTF8"] = "1"


def tool(name):
    path = shutil.which(name)
    if not path:
        raise SystemExit(f"Missing tool: {name}. See docs/development.md.")
    return path


def run(*args):
    print(subprocess.list2cmdline([str(arg) for arg in args]), flush=True)
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["doctor", "deps", "build", "dll", "test", "self-test", "verify", "mock", "sanitize"])
    args = parser.parse_args()
    configure_tools()
    if args.command == "doctor":
        print(f"Python: {sys.version.split()[0]} ({sys.executable})")
        for name in ("node", "npm", "clang++", "lld-link"):
            print(f"{name}: {tool(name)}")
        run(tool("node"), "--version")
        run(tool("clang++"), "--version")
    elif args.command == "deps":
        run(tool("npm"), "ci", "--prefix", "engine", "--ignore-scripts", "--no-audit", "--no-fund")
    elif args.command in ("build", "dll"):
        tool("clang++")
        tool("lld-link")
        run(sys.executable, "-X", "utf8", "build.py", *(["--dll-only"] if args.command == "dll" else []))
    elif args.command == "test":
        for path in sorted((ROOT / "engine").glob("*.mjs")):
            run(tool("node"), "--check", path)
        run(tool("node"), "--test", "tests/engine_tests.mjs")
        run(sys.executable, "-X", "utf8", "tests/bridge_logging_smoke.py")
    elif args.command == "self-test":
        run(tool("node"), "engine/bridge.mjs", "--self-test")
        print("PASS: real Shiki / JSONC integration self-test")
    elif args.command == "verify":
        run(sys.executable, "-X", "utf8", "tests/verify_package.py")
    else:
        if not sys.platform.startswith("linux"):
            raise SystemExit("Native host mocks require Linux (or WSL) with clang++ and lld-link. See docs/development.md.")
        build = ROOT / "build"
        build.mkdir(exist_ok=True)
        run(tool("clang++"), "-std=c++17", "-O1", "-g", "-Isrc", "tests/textutil_tests.cpp", "src/textutil.cpp", "-o", build / "textutil_tests")
        run(build / "textutil_tests")
        sanitizer = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if args.command == "sanitize" else []
        run(tool("clang++"), "-std=c++17", "-O1", "-g", *sanitizer, "-Isrc", "tests/basicformat_tests.cpp", "src/basicformat.cpp", "src/textutil.cpp", "-o", build / "basicformat_tests")
        run(build / "basicformat_tests")
        run(sys.executable, "-X", "utf8", "tests/run_basicformat_semantics.py")
        flags = ["--sanitize"] if args.command == "sanitize" else []
        for script in ("tests/run_office_mock.py", "tests/run_addin_mock.py", "tests/run_settings_mock.py"):
            run(sys.executable, "-X", "utf8", script, *flags)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
