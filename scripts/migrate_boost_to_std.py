#!/usr/bin/env python3
"""Mechanically migrate native C++ sources from Boost to std / standalone Asio.

Skips src/DotNet/**. Preserves per-file newline style (CRLF vs LF).
Does not touch boost::asio::placeholders or Boost.Test.
"""
from __future__ import annotations

import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

DIRS = [
    ROOT / "src" / "Application",
    ROOT / "src" / "Codecs",
    ROOT / "src" / "Common",
    ROOT / "src" / "Communication",
    ROOT / "src" / "Messages",
    ROOT / "src" / "Examples",
    ROOT / "src" / "Tests",
]

EXTENSIONS = {".h", ".hpp", ".cpp"}

# Include replacements: more specific first. Value None means remove the line.
INCLUDE_REPLACEMENTS: list[tuple[str, str | None]] = [
    ("#include <boost/asio/io_context.hpp>", "#include <asio/io_context.hpp>"),
    ("#include <boost/asio.hpp>", "#include <asio.hpp>"),
    ("#include <boost/shared_ptr.hpp>", "#include <memory>"),
    ("#include <boost/scoped_ptr.hpp>", "#include <memory>"),
    ("#include <boost/scoped_array.hpp>", "#include <memory>"),
    ("#include <boost/shared_array.hpp>", "#include <memory>"),
    ("#include <boost/weak_ptr.hpp>", "#include <memory>"),
    ("#include <boost/enable_shared_from_this.hpp>", "#include <memory>"),
    ("#include <boost/intrusive_ptr.hpp>", "#include <memory>"),
    ("#include <boost/function.hpp>", "#include <functional>"),
    ("#include <boost/bind/bind.hpp>", None),
    ("#include <boost/bind.hpp>", None),
    ("#include <boost/thread/mutex.hpp>", "#include <mutex>"),
    ("#include <boost/thread.hpp>", "#include <thread>"),
    ("#include <boost/cstdint.hpp>", "#include <cstdint>"),
    ("#include <boost/filesystem.hpp>", "#include <filesystem>"),
    ("#include <boost/lexical_cast.hpp>", None),
    ("#include <boost/operators.hpp>", None),
    ("#include <boost/algorithm/string/trim.hpp>", None),
]

DATE_TIME_INCLUDE_RE = re.compile(
    r"^\s*#\s*include\s*<boost/date_time/[^>]+>\s*$"
)

# Type/name replacements: more specific first.
# Protect boost::asio::placeholders from boost::asio:: → asio::
PLACEHOLDERS_SENTINEL = "___BOOST_ASIO_PLACEHOLDERS___"

SIMPLE_REPLACEMENTS: list[tuple[str, str]] = [
    ("boost::asio::placeholders", PLACEHOLDERS_SENTINEL),
    ("boost::system::error_code", "asio::error_code"),
    ("boost::asio::", "asio::"),
    ("boost::mutex::scoped_lock", "std::unique_lock<std::mutex>"),
    ("boost::recursive_mutex::scoped_lock", "std::unique_lock<std::recursive_mutex>"),
    ("boost::enable_shared_from_this", "std::enable_shared_from_this"),
    ("boost::shared_ptr", "std::shared_ptr"),
    ("boost::weak_ptr", "std::weak_ptr"),
    ("boost::scoped_ptr", "std::unique_ptr"),
    ("boost::function", "std::function"),
    ("boost::recursive_mutex", "std::recursive_mutex"),
    ("boost::condition_variable", "std::condition_variable"),
    ("boost::lock_guard", "std::lock_guard"),
    ("boost::mutex", "std::mutex"),
    ("boost::thread", "std::thread"),
    ("boost::filesystem::", "std::filesystem::"),
    ("boost::int8_t", "std::int8_t"),
    ("boost::int16_t", "std::int16_t"),
    ("boost::int32_t", "std::int32_t"),
    ("boost::int64_t", "std::int64_t"),
    ("boost::uint8_t", "std::uint8_t"),
    ("boost::uint16_t", "std::uint16_t"),
    ("boost::uint32_t", "std::uint32_t"),
    ("boost::uint64_t", "std::uint64_t"),
]


def find_matching_angle(s: str, open_idx: int) -> int:
    """Return index of '>' matching '<' at open_idx, respecting nesting."""
    depth = 0
    i = open_idx
    n = len(s)
    while i < n:
        c = s[i]
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def replace_array_smart_ptr(text: str, boost_name: str, std_name: str) -> str:
    """boost::scoped_array<T> → std::unique_ptr<T[]>, etc."""
    needle = f"boost::{boost_name}<"
    out: list[str] = []
    i = 0
    while True:
        j = text.find(needle, i)
        if j < 0:
            out.append(text[i:])
            break
        out.append(text[i:j])
        open_lt = j + len(needle) - 1  # points at '<'
        close_gt = find_matching_angle(text, open_lt)
        if close_gt < 0:
            # Malformed; leave rest unchanged
            out.append(text[j:])
            break
        inner = text[open_lt + 1 : close_gt]
        out.append(f"std::{std_name}<{inner}[]>")
        i = close_gt + 1
    return "".join(out)


def detect_newline(data: bytes) -> bytes:
    if b"\r\n" in data:
        return b"\r\n"
    return b"\n"


def process_text(text: str) -> str:
    lines = text.splitlines(keepends=True)
    new_lines: list[str] = []
    for line in lines:
        stripped = line.lstrip()
        # Preserve indentation prefix
        indent = line[: len(line) - len(stripped)]
        # Detect end-of-line
        if line.endswith("\r\n"):
            eol = "\r\n"
            core = line[len(indent) : -2]
        elif line.endswith("\n"):
            eol = "\n"
            core = line[len(indent) : -1]
        else:
            eol = ""
            core = line[len(indent) :]

        # date_time includes → remove
        if DATE_TIME_INCLUDE_RE.match(core.strip() if False else core):
            # Match full line content without indent consideration: check core
            pass
        core_stripped = core.strip()
        if DATE_TIME_INCLUDE_RE.match(core_stripped):
            continue

        matched_include = False
        for old_inc, new_inc in INCLUDE_REPLACEMENTS:
            if core_stripped == old_inc:
                matched_include = True
                if new_inc is not None:
                    new_lines.append(f"{indent}{new_inc}{eol}")
                break
        if matched_include:
            continue

        new_lines.append(line)

    text = "".join(new_lines)

    # Array smart pointers before generic scoped_ptr / shared_ptr
    text = replace_array_smart_ptr(text, "scoped_array", "unique_ptr")
    text = replace_array_smart_ptr(text, "shared_array", "shared_ptr")

    for old, new in SIMPLE_REPLACEMENTS:
        text = text.replace(old, new)

    text = text.replace(PLACEHOLDERS_SENTINEL, "boost::asio::placeholders")
    return text


def iter_source_files():
    for d in DIRS:
        if not d.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(d):
            # Skip DotNet
            dirnames[:] = [x for x in dirnames if x != "DotNet"]
            if "DotNet" in Path(dirpath).parts:
                continue
            for name in filenames:
                if Path(name).suffix in EXTENSIONS:
                    yield Path(dirpath) / name


def count_remaining(root_dirs: list[Path]) -> tuple[int, int, list[str]]:
    boost_ns = 0
    boost_inc = 0
    sample: list[str] = []
    ns_re = re.compile(r"boost::")
    inc_re = re.compile(r"#\s*include\s*<boost")
    for d in root_dirs:
        if not d.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(d):
            dirnames[:] = [x for x in dirnames if x != "DotNet"]
            if "DotNet" in Path(dirpath).parts:
                continue
            for name in filenames:
                if Path(name).suffix not in EXTENSIONS:
                    continue
                path = Path(dirpath) / name
                try:
                    data = path.read_text(encoding="utf-8", errors="replace")
                except OSError:
                    continue
                for i, line in enumerate(data.splitlines(), 1):
                    if ns_re.search(line):
                        boost_ns += line.count("boost::")
                        if len(sample) < 40:
                            sample.append(f"{path.relative_to(ROOT)}:{i}:{line.strip()}")
                    if inc_re.search(line):
                        boost_inc += 1
                        if len(sample) < 40:
                            sample.append(f"{path.relative_to(ROOT)}:{i}:{line.strip()}")
    return boost_ns, boost_inc, sample


def main() -> int:
    changed: list[str] = []
    for path in sorted(iter_source_files()):
        raw = path.read_bytes()
        nl = detect_newline(raw)
        # Decode conservatively
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            text = raw.decode("latin-1")

        # Normalize to \n for processing; restore later
        had_crlf = nl == b"\r\n"
        work = text.replace("\r\n", "\n").replace("\r", "\n")
        new_work = process_text(work)
        if new_work == work:
            continue

        if had_crlf:
            out = new_work.replace("\n", "\r\n").encode("utf-8")
        else:
            # Keep trailing newline as-is from new_work
            out = new_work.encode("utf-8")

        path.write_bytes(out)
        changed.append(str(path.relative_to(ROOT)))

    print(f"Files changed: {len(changed)}")
    for c in changed:
        print(f"  {c}")

    boost_ns, boost_inc, sample = count_remaining(DIRS)
    print()
    print(f"Remaining boost:: occurrences (outside DotNet): {boost_ns}")
    print(f"Remaining #include <boost ...> lines (outside DotNet): {boost_inc}")
    if sample:
        print("Sample remaining references:")
        for s in sample:
            print(f"  {s}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
