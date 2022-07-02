#!/usr/bin/env python3
# ---------------------------------------------------------------------------- #

from __future__ import annotations

from dataclasses import dataclass
import json
import os
import os.path
import subprocess
import sys
import re
from argparse import ArgumentParser, Namespace, RawDescriptionHelpFormatter
from multiprocessing import Pool
from pathlib import Path
from typing import (
    Any,
    Callable,
    Dict,
    Iterable,
    List,
    NoReturn,
    Optional,
    Mapping,
    Sequence,
    Tuple,
)

# ---------------------------------------------------------------------------- #

from clang.cindex import (  # type: ignore
    Cursor,
    CursorKind,
    Diagnostic,
    StorageClass,
    TranslationUnit,
    TranslationUnitLoadError,
    TypeKind,
)

Cursor.__hash__ = lambda self: self.hash  # so `Cursor`s can be dict keys

# ---------------------------------------------------------------------------- #
# Usage


def parse_args() -> Namespace:

    available_checks = "\n".join(f"  {name}" for (name, _) in CHECKS)

    parser = ArgumentParser(
        formatter_class=RawDescriptionHelpFormatter,
        epilog=f"""
available checks:
{available_checks}

exit codes:
  0  No problems found.
  1  Analyzer failure.
  2  Bad usage.
  3  Problems found in a translation unit.
""",
    )

    parser.add_argument("build_dir", type=Path)

    parser.add_argument(
        "translation_units",
        type=Path,
        nargs="*",
        help=(
            "Analyze only translation units whose root source file matches or"
            " is under one of the given paths."
        ),
    )

    parser.add_argument(
        "-c",
        "--check",
        metavar="CHECK",
        dest="check_names",
        choices=[name for (name, _) in CHECKS],
        action="append",
        help=(
            "Enable the given check. Can be given multiple times. If not given,"
            " all checks are enabled."
        ),
    )

    parser.add_argument(
        "-j",
        "--jobs",
        dest="threads",
        type=int,
        help=(
            "Number of threads to employ. Defaults to the number of logical"
            " processors."
        ),
    )

    parser.add_argument(
        "--profile",
        action="store_true",
        help="Profile execution. Forces single-threaded execution.",
    )

    return parser.parse_args()


# ---------------------------------------------------------------------------- #
# Main


def main() -> NoReturn:

    args = parse_args()

    compile_commands = load_compilation_database(args)
    contexts = get_translation_unit_contexts(args, compile_commands)

    analyze_translation_units(args, contexts)


def load_compilation_database(args: Namespace) -> Sequence[Mapping[str, str]]:

    # clang.cindex.CompilationDatabase.getCompileCommands() apparently produces
    # entries for files not listed in compile_commands.json in a best-effort
    # manner, which we don't want, so we parse the JSON ourselves instead.

    path = args.build_dir / "compile_commands.json"

    try:
        with path.open("r") as f:
            return json.load(f)
    except FileNotFoundError:
        fatal(f"{path} does not exist")


def get_translation_unit_contexts(
    args: Namespace, compile_commands: Iterable[Mapping[str, str]]
) -> Sequence[TranslationUnitContext]:

    system_include_paths = get_clang_system_include_paths()
    check_names = args.check_names or [name for (name, _) in CHECKS]

    contexts = (
        TranslationUnitContext(
            absolute_path=str(Path(cmd["directory"], cmd["file"]).resolve()),
            compilation_working_dir=cmd["directory"],
            compilation_command=cmd["command"],
            system_include_paths=system_include_paths,
            check_names=check_names,
        )
        for cmd in compile_commands
    )

    if args.translation_units:

        allowed_prefixes = [
            # ensure path exists and is slash-terminated (even if it is a file)
            os.path.join(path.resolve(strict=True), "")
            for path in args.translation_units
        ]

        contexts = (
            ctx
            for ctx in contexts
            if any(
                (ctx.absolute_path + "/").startswith(prefix)
                for prefix in allowed_prefixes
            )
        )

    context_list = list(contexts)

    if not context_list:
        fatal("No translation units to analyze")

    return context_list


def get_clang_system_include_paths() -> Sequence[str]:

    # libclang does not automatically include clang's standard system include
    # paths, so we ask clang what they are and include them ourselves.

    # TODO: Is there a less hacky way to do this?

    result = subprocess.run(
        ["clang", "-E", "-", "-v"],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        universal_newlines=True,  # decode stdout/stderr using default encoding
        check=True,
    )

    # Module `re` does not support repeated group captures.
    pattern = (
        r"#include <...> search starts here:\n"
        r"((?: \S*\n)+)"
        r"End of search list."
    )

    match = re.search(pattern, result.stderr, re.MULTILINE)
    assert match is not None

    return [line[1:] for line in match.group(1).splitlines()]


def fatal(message: str) -> NoReturn:
    print(f"\033[0;31mERROR: {message}\033[0m")
    sys.exit(1)


# ---------------------------------------------------------------------------- #
# Analysis


@dataclass
class TranslationUnitContext:
    absolute_path: str
    compilation_working_dir: str
    compilation_command: str
    system_include_paths: Sequence[str]
    check_names: Sequence[str]


def analyze_translation_units(
    args: Namespace, contexts: Sequence[TranslationUnitContext]
) -> NoReturn:

    results: List[bool]

    if not args.profile:

        with Pool(processes=args.threads) as pool:
            results = pool.map(analyze_translation_unit, contexts)

    else:

        import cProfile
        import pstats

        profile = cProfile.Profile()

        try:
            results = profile.runcall(
                lambda: list(map(analyze_translation_unit, contexts))
            )
        finally:
            stats = pstats.Stats(profile, stream=sys.stderr)
            stats.strip_dirs()
            stats.sort_stats("tottime")
            stats.print_stats()

    print(
        f"\033[0;34mAnalyzed {len(contexts)}"
        f" translation unit{'' if len(contexts) == 1 else 's'}.\033[0m"
    )

    sys.exit(0 if all(results) else 3)


def analyze_translation_unit(context: TranslationUnitContext) -> bool:

    # relative to script's original working directory
    relative_path = os.path.relpath(context.absolute_path)

    # load translation unit

    command = context.compilation_command.split()

    adjusted_command = [
        # keep the original compilation command name
        command[0],
        # ignore unknown GCC warning options
        "-Wno-unknown-warning-option",
        # add clang system include paths
        *(
            arg
            for path in context.system_include_paths
            for arg in ("-isystem", path)
        ),
        # keep all other arguments but the last, which is the file name
        *command[1:-1],
        # replace relative path to get absolute location information
        context.absolute_path,
    ]

    original_cwd = os.getcwd()
    os.chdir(context.compilation_working_dir)  # for options like -I to work

    try:
        tu = TranslationUnit.from_source(filename=None, args=adjusted_command)
    except TranslationUnitLoadError as e:
        raise RuntimeError(f"Failed to load {relative_path}") from e

    os.chdir(original_cwd)  # to have proper relative paths in messages

    # check for fatal diagnostics

    found_problems = False

    for diag in tu.diagnostics:
        # consider only Fatal diagnostics, like missing includes
        if diag.severity >= Diagnostic.Fatal:
            found_problems = True
            location = format_location(diag, default=relative_path)
            print(
                f"\033[0;33m{location}: {diag.spelling}; this may lead to false"
                f" positives and negatives\033[0m"
            )

    # analyze translation unit

    def log(node: Cursor, message: str) -> None:
        nonlocal found_problems
        found_problems = True
        print(f"{format_location(node)}: {message}")

    try:
        for (name, checker) in CHECKS:
            if name in context.check_names:
                checker(tu, context.absolute_path, log)
    except Exception as e:
        raise RuntimeError(f"Error analyzing {relative_path}") from e

    return not found_problems


# obj must have a location field/property that is a `SourceLocation`.
def format_location(obj: Any, *, default: str = "(none)") -> str:

    location = obj.location

    if location.file is None:
        return default
    else:
        abs_path = Path(location.file.name).resolve()
        rel_path = os.path.relpath(abs_path)
        return f"{rel_path}:{location.line}:{location.column}"


# ---------------------------------------------------------------------------- #
# Checks

Checker = Callable[[TranslationUnit, str, Callable[[Cursor, str], None]], None]

CHECKS: List[Tuple[str, Checker]] = []


def check(name: str) -> Callable[[Checker], Checker]:
    def decorator(checker: Checker) -> Checker:
        CHECKS.append((name, checker))
        return checker

    return decorator


@check("return-value-never-used")
def check_return_value_never_used(
    translation_unit: TranslationUnit,
    translation_unit_path: str,
    log: Callable[[Cursor, str], None],
) -> None:
    """
    Report static functions with a non-void return value that no caller ever
    uses.

    This check is best effort, but should never report false positives (positive
    being error).
    """

    def function_occurrence_might_use_return_value(
        ancestors: Sequence[Cursor], node: Cursor
    ) -> bool:

        if ancestors[-1].kind.is_statement():

            return False

        elif (
            ancestors[-1].kind == CursorKind.CALL_EXPR
            and ancestors[-1].referenced == node.referenced
        ):

            if not ancestors[-2].kind.is_statement():
                return True

            if ancestors[-2].kind in [
                CursorKind.IF_STMT,
                CursorKind.SWITCH_STMT,
                CursorKind.WHILE_STMT,
                CursorKind.DO_STMT,
                CursorKind.RETURN_STMT,
            ]:
                return True

            if ancestors[-2].kind == CursorKind.FOR_STMT:
                [_init, cond, _adv] = ancestors[-2].get_children()
                if ancestors[-1] == cond:
                    return True

            return False

        else:

            # might be doing something with a pointer to the function
            return True

    # Maps canonical function `Cursor`s to whether we found a place that maybe
    # uses their return value. Only includes static functions that don't return
    # void and belong to the translation unit's root file (i.e, were not brought
    # in by an #include).
    funcs: Dict[Cursor, bool] = {}

    for [*ancestors, node] in all_paths(translation_unit.cursor):

        if (
            node.kind == CursorKind.FUNCTION_DECL
            and node.storage_class == StorageClass.STATIC
            and node.location.file.name == translation_unit_path
            and node.type.get_result().get_canonical().kind != TypeKind.VOID
        ):
            funcs.setdefault(node.canonical, False)

        if (
            node.kind == CursorKind.DECL_REF_EXPR
            and node.referenced.kind == CursorKind.FUNCTION_DECL
            and node.referenced.canonical in funcs
            and function_occurrence_might_use_return_value(ancestors, node)
        ):
            funcs[node.referenced.canonical] = True

    # ---

    for (cursor, return_value_maybe_used) in funcs.items():
        if not return_value_maybe_used:
            log(cursor, f"{cursor.spelling}() return value is never used")


# ---------------------------------------------------------------------------- #
# Traversal

# Hides nodes of kind UNEXPOSED_EXPR.
def all_paths(root: Cursor) -> Iterable[Sequence[Cursor]]:

    path = []

    def aux(node: Cursor) -> Iterable[Sequence[Cursor]]:
        nonlocal path

        if node.kind != CursorKind.UNEXPOSED_EXPR:
            path.append(node)
            yield path

        for child in node.get_children():
            yield from aux(child)

        if node.kind != CursorKind.UNEXPOSED_EXPR:
            path.pop()

    return aux(root)


# ---------------------------------------------------------------------------- #
# Utilities handy for development


def print_node(node: Cursor) -> None:

    print(f"{format_location(node)}: kind = {node.kind.name}", end="")

    if node.spelling:
        print(f", name = {node.spelling}", end="")

    if node.type is not None:
        print(f", type = {node.type.get_canonical().spelling}", end="")

    if node.referenced is not None:
        print(f", referenced = {node.referenced.spelling}", end="")

    print()


def print_tree(
    node: Cursor, *, max_depth: Optional[int] = None, indentation_level: int = 0
) -> None:

    if max_depth is None or max_depth >= 0:

        print("  " * indentation_level, end="")
        print_node(node)

        for child in node.get_children():
            print_tree(
                child,
                max_depth=None if max_depth is None else max_depth - 1,
                indentation_level=indentation_level + 1,
            )


# ---------------------------------------------------------------------------- #

if __name__ == "__main__":
    main()

# ---------------------------------------------------------------------------- #
