#!/usr/bin/env python3
"""Migrate tests/ from Boost.Test to GoogleTest.

Preserves per-file newline style (CRLF vs LF). Deletes tests/main.cpp.
Does not touch Catch2.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TESTS = ROOT / "tests"


def detect_newline(raw: bytes) -> str:
    return "\r\n" if b"\r\n" in raw else "\n"


def split_top_level_args(args: str) -> list[str]:
    """Split macro argument list on top-level commas (paren/string aware)."""
    parts: list[str] = []
    depth = 0
    in_str: str | None = None
    start = 0
    i = 0
    while i < len(args):
        c = args[i]
        if in_str:
            if c == "\\" and i + 1 < len(args):
                i += 2
                continue
            if c == in_str:
                in_str = None
        else:
            if c in "\"'":
                in_str = c
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            elif c == "," and depth == 0:
                parts.append(args[start:i].strip())
                start = i + 1
        i += 1
    parts.append(args[start:].strip())
    return parts


def find_macro_call(text: str, name: str, pos: int) -> tuple[int, int, str] | None:
    """Find macro NAME(...) starting at/after pos. Returns (start, end, args)."""
    pattern = re.compile(rf"\b{re.escape(name)}\s*\(")
    m = pattern.search(text, pos)
    if not m:
        return None
    start = m.start()
    i = m.end()
    depth = 1
    in_str: str | None = None
    while i < len(text) and depth:
        c = text[i]
        if in_str:
            if c == "\\" and i + 1 < len(text):
                i += 2
                continue
            if c == in_str:
                in_str = None
        else:
            if c in "\"'":
                in_str = c
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
        i += 1
    if depth != 0:
        raise ValueError(f"Unbalanced parens for {name} at {start}")
    args = text[m.end() : i - 1]
    return start, i, args


def replace_two_arg(
    text: str,
    boost_name: str,
    gtest_fmt: str,
) -> str:
    """Replace BOOST_*(a, b) with gtest_fmt.format(a=..., b=...)."""
    out: list[str] = []
    pos = 0
    while True:
        found = find_macro_call(text, boost_name, pos)
        if not found:
            out.append(text[pos:])
            break
        start, end, args = found
        parts = split_top_level_args(args)
        if len(parts) != 2:
            raise ValueError(
                f"{boost_name} expected 2 args, got {len(parts)}: {args!r}"
            )
        a, b = parts
        out.append(text[pos:start])
        out.append(gtest_fmt.format(a=a, b=b))
        pos = end
    return "".join(out)


def replace_one_arg(text: str, boost_name: str, gtest_fmt: str) -> str:
    out: list[str] = []
    pos = 0
    while True:
        found = find_macro_call(text, boost_name, pos)
        if not found:
            out.append(text[pos:])
            break
        start, end, args = found
        out.append(text[pos:start])
        out.append(gtest_fmt.format(x=args.strip()))
        pos = end
    return "".join(out)


def replace_zero_arg(text: str, boost_name: str, replacement: str) -> str:
    return re.sub(rf"\b{re.escape(boost_name)}\s*\(\s*\)", replacement, text)


def migrate_content(text: str) -> str:
    # Header: remove Boost.Test bootstrap, add gtest.
    text = re.sub(
        r"#\s*define\s+BOOST_TEST_NO_MAIN[^\n]*\n",
        "",
        text,
    )
    text = re.sub(
        r"#\s*include\s*<boost/test/unit_test\.hpp>\s*\n",
        "#include <gtest/gtest.h>\n",
        text,
    )
    # Also handle BOOST_TEST_MODULE if present (main.cpp style).
    text = re.sub(
        r"#\s*define\s+BOOST_TEST_MODULE[^\n]*\n",
        "",
        text,
    )

    # filesystem
    text = text.replace("#include <boost/filesystem.hpp>", "#include <filesystem>")
    text = text.replace("boost::filesystem::", "std::filesystem::")

    # More-specific macros first.
    text = replace_two_arg(
        text, "BOOST_CHECK_EQUAL", "EXPECT_EQ(({a}), ({b}))"
    )
    text = replace_two_arg(
        text, "BOOST_REQUIRE_EQUAL", "ASSERT_EQ(({a}), ({b}))"
    )
    text = replace_two_arg(
        text, "BOOST_CHECK_THROW", "EXPECT_THROW({a}, {b})"
    )
    text = replace_two_arg(
        text, "BOOST_REQUIRE_THROW", "ASSERT_THROW({a}, {b})"
    )
    text = replace_two_arg(text, "BOOST_CHECK_GT", "EXPECT_GT({a}, {b})")
    text = replace_two_arg(text, "BOOST_CHECK_GE", "EXPECT_GE({a}, {b})")
    text = replace_two_arg(text, "BOOST_CHECK_LT", "EXPECT_LT({a}, {b})")
    text = replace_two_arg(text, "BOOST_CHECK_LE", "EXPECT_LE({a}, {b})")
    text = replace_two_arg(text, "BOOST_CHECK_NE", "EXPECT_NE({a}, {b})")
    text = replace_two_arg(
        text, "BOOST_REQUIRE_GT", "ASSERT_GT({a}, {b})"
    )
    text = replace_two_arg(
        text, "BOOST_CHECK_CLOSE", "EXPECT_NEAR({a}, {b}, 1e-5 * std::fabs({b}))"
    )
    text = replace_two_arg(
        text, "BOOST_CHECK_MESSAGE", "EXPECT_TRUE({a}) << {b}"
    )
    text = replace_two_arg(
        text, "BOOST_REQUIRE_MESSAGE", "ASSERT_TRUE({a}) << {b}"
    )

    text = replace_one_arg(text, "BOOST_FAIL", "FAIL() << {x}")
    text = replace_one_arg(
        text, "BOOST_TEST_CHECKPOINT", "SCOPED_TRACE({x})"
    )
    text = replace_zero_arg(text, "BOOST_TEST_PASSPOINT", 'SCOPED_TRACE("")')

    text = replace_one_arg(text, "BOOST_CHECK", "EXPECT_TRUE({x})")
    text = replace_one_arg(text, "BOOST_REQUIRE", "ASSERT_TRUE({x})")

    text = replace_one_arg(
        text, "BOOST_AUTO_TEST_CASE", "TEST(QuickFAST, {x})"
    )

    return text


def migrate_file(path: Path) -> bool:
    raw = path.read_bytes()
    newline = detect_newline(raw)
    # Decode as latin-1 to preserve Non-ISO extended-ASCII bytes.
    text = raw.decode("latin-1")
    # Normalize to \n for processing
    text_lf = text.replace("\r\n", "\n").replace("\r", "\n")
    new_lf = migrate_content(text_lf)
    if new_lf == text_lf:
        return False
    out = new_lf.replace("\n", newline).encode("latin-1")
    path.write_bytes(out)
    return True


def main() -> int:
    changed = 0
    for path in sorted(TESTS.glob("test*.cpp")):
        if migrate_file(path):
            print(f"migrated: {path.relative_to(ROOT)}")
            changed += 1
        else:
            print(f"unchanged: {path.relative_to(ROOT)}")

    main_cpp = TESTS / "main.cpp"
    if main_cpp.exists():
        main_cpp.unlink()
        print(f"deleted: {main_cpp.relative_to(ROOT)}")

    # Verify
    leftovers: list[str] = []
    for path in TESTS.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix not in {".cpp", ".h", ".hpp"}:
            continue
        body = path.read_text(encoding="latin-1", errors="replace")
        if "BOOST_AUTO_TEST" in body or "boost/test" in body:
            leftovers.append(str(path.relative_to(ROOT)))
        # Any remaining Boost.Test macros
        if re.search(r"\bBOOST_(CHECK|REQUIRE|FAIL|TEST_)", body):
            leftovers.append(f"{path.relative_to(ROOT)} (BOOST_* macro)")

    if leftovers:
        print("ERROR: leftovers remain:", file=sys.stderr)
        for item in leftovers:
            print(f"  {item}", file=sys.stderr)
        return 1

    print(f"OK: migrated {changed} files; no BOOST_AUTO_TEST / boost/test left")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
