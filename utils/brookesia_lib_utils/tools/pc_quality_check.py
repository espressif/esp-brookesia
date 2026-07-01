#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import concurrent.futures
import json
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_BUILD_ROOT = REPO_ROOT / "build" / "pc_quality"
DEFAULT_CHECKS = ",".join([
    "clang-analyzer-*",
    "bugprone-*",
    "performance-*",
    "portability-*",
    "-bugprone-easily-swappable-parameters",
    "-bugprone-unchecked-optional-access",
    "-performance-enum-size",
    "-performance-no-int-to-ptr",
    "-performance-unnecessary-value-param",
    "-clang-analyzer-optin.cplusplus.VirtualCall",
    "-clang-analyzer-cplusplus.NewDelete",
    "-clang-analyzer-cplusplus.NewDeleteLeaks",
])
ASAN_OPTIONS = "detect_leaks=1:halt_on_error=1:fast_unwind_on_malloc=0:malloc_context_size=50"
LSAN_OPTIONS = "report_objects=1:print_suppressions=0"
CLANG_TIDY_TIMEOUT_SECONDS = 300
CLANG_TIDY_COMPAT_NOISE = "warning: macro replacement list should be enclosed in parentheses [bugprone-macro-parentheses]"


@dataclass
class Tool:
    name: str
    path: Path | None
    error: str | None = None

    @property
    def available(self) -> bool:
        return self.path is not None and self.error is None


def log(message: str) -> None:
    print(f"[pc-quality] {message}", flush=True)


def resolve_repo_path(path: Path) -> Path:
    if path.is_absolute():
        return path.resolve()
    return (REPO_ROOT / path).resolve()


def to_repo_path(path: Path) -> Path:
    return path.resolve().relative_to(REPO_ROOT)


def display_path(path: Path) -> str:
    try:
        return str(to_repo_path(path))
    except ValueError:
        return str(path)


def is_relative_to(path: Path, prefix: Path) -> bool:
    try:
        path.relative_to(prefix)
        return True
    except ValueError:
        return False


def matches_prefix(path: Path, prefix: Path) -> bool:
    path = path.resolve()
    prefix = prefix.resolve()
    return path == prefix or is_relative_to(path, prefix)


def is_build_or_generated_path(path: Path) -> bool:
    relative = path
    try:
        relative = to_repo_path(path)
    except ValueError:
        pass
    return "build" in relative.parts or relative.parts[:1] in ((".cache",), (".deps",))


def file_in_scope(file_path: Path, scope_prefixes: list[Path], exclude_prefixes: list[Path]) -> bool:
    file_path = file_path.resolve()
    try:
        to_repo_path(file_path)
    except ValueError:
        return False
    if is_build_or_generated_path(file_path):
        return False
    if any(matches_prefix(file_path, prefix) for prefix in exclude_prefixes):
        return False
    if not scope_prefixes:
        return True
    return any(matches_prefix(file_path, prefix) for prefix in scope_prefixes)


def run_command(
    command: list[str],
    *,
    cwd: Path = REPO_ROOT,
    env: dict[str, str] | None = None,
    stdout: Path | None = None,
    stderr: Path | None = None,
    timeout: int | None = None,
    check: bool = False,
) -> subprocess.CompletedProcess:
    log("$ " + " ".join(command))
    if stdout is None and stderr is None:
        return subprocess.run(command, cwd=cwd, env=env, text=True, timeout=timeout, check=check)

    if stdout is not None:
        stdout.parent.mkdir(parents=True, exist_ok=True)
    if stderr is not None:
        stderr.parent.mkdir(parents=True, exist_ok=True)
    stdout_handle = stdout.open("w", encoding="utf-8") if stdout is not None else subprocess.PIPE
    stderr_handle = stderr.open("w", encoding="utf-8") if stderr is not None else subprocess.PIPE
    try:
        return subprocess.run(
            command,
            cwd=cwd,
            env=env,
            text=True,
            stdout=stdout_handle,
            stderr=stderr_handle,
            timeout=timeout,
            check=check,
        )
    finally:
        if stdout is not None:
            stdout_handle.close()
        if stderr is not None:
            stderr_handle.close()


def capture_command(
    command: list[str],
    *,
    input_text: str | None = None,
    timeout: int | None = None,
) -> subprocess.CompletedProcess:
    return subprocess.run(
        command,
        cwd=REPO_ROOT,
        input=input_text,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
        timeout=timeout,
    )


def candidate_paths(names: list[str], llvm_bin: Path | None) -> list[Path]:
    candidates: list[Path] = []
    if llvm_bin is not None:
        candidates.extend(llvm_bin / name for name in names)
    for name in names:
        found = shutil.which(name)
        if found is not None:
            candidates.append(Path(found))

    seen = set()
    unique = []
    for candidate in candidates:
        key = str(candidate)
        if key in seen:
            continue
        seen.add(key)
        unique.append(candidate)
    return unique


def is_wasi_tool(path: Path) -> bool:
    resolved = path.resolve()
    if "/opt/wasi-sdk/" in str(resolved):
        return True
    version = capture_command([str(resolved), "--version"])
    output = version.stdout.lower()
    return "wasi-sdk" in output or "wasm32" in output or "wasip" in output


def find_tool(name: str, names: list[str], llvm_bin: Path | None = None, required: bool = True) -> Tool:
    rejected = []
    for candidate in candidate_paths(names, llvm_bin):
        if not candidate.exists():
            continue
        if is_wasi_tool(candidate):
            rejected.append(str(candidate))
            continue
        return Tool(name=name, path=candidate.resolve())

    if rejected:
        error = (
            f"Rejected {name} from WASI toolchain: {', '.join(rejected)}. "
            "Install a native LLVM package or pass --llvm-bin to a native LLVM bin directory."
        )
    elif required:
        error = f"Could not find native {name}. Install it or pass --llvm-bin to a native LLVM bin directory."
    else:
        error = None
    return Tool(name=name, path=None, error=error)


def check_expected_compat(clangxx: Tool) -> list[str]:
    if not clangxx.available:
        return []

    source = "#include <expected>\nint main(){std::expected<int,int> x=1; return *x;}\n"
    with tempfile.TemporaryDirectory(prefix="brookesia_expected_") as temp_dir:
        output = Path(temp_dir) / "expected.o"
        result = capture_command([
            str(clangxx.path),
            "-std=gnu++23",
            "-x",
            "c++",
            "-",
            "-c",
            "-o",
            str(output),
        ], input_text=source)
    if result.returncode == 0:
        return []

    log("native clang++ needs std::expected compatibility flags for libstdc++")
    return ["-Wno-builtin-macro-redefined", "-U__cpp_concepts", "-D__cpp_concepts=(202002L)"]


def to_cmake_cxx_flags(args: list[str]) -> str:
    build_args = []
    for arg in args:
        if arg == "-D__cpp_concepts=(202002L)":
            build_args.append("-D__cpp_concepts='(202002L)'")
        else:
            build_args.append(arg)
    return " ".join(build_args)


def get_reports_dir(args: argparse.Namespace) -> Path:
    if args.reports_dir is not None:
        return resolve_repo_path(args.reports_dir)
    return resolve_repo_path(args.build_root) / "reports"


def get_scope_prefixes(args: argparse.Namespace) -> list[Path]:
    return [resolve_repo_path(prefix) for prefix in args.scope_prefix]


def get_exclude_prefixes(args: argparse.Namespace) -> list[Path]:
    return [resolve_repo_path(prefix) for prefix in args.exclude_prefix]


def configure_build(
    args: argparse.Namespace,
    build_dir: Path,
    *,
    c_compiler: str | None = "/usr/bin/cc",
    cxx_compiler: str | None = "/usr/bin/c++",
    c_flags: str = "",
    cxx_flags: str = "",
    linker_flags: str = "",
) -> int:
    command = [
        "cmake",
        "-S",
        str(resolve_repo_path(args.source_dir)),
        "-B",
        str(build_dir),
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]
    if c_compiler is not None:
        command.append(f"-DCMAKE_C_COMPILER={c_compiler}")
    if cxx_compiler is not None:
        command.append(f"-DCMAKE_CXX_COMPILER={cxx_compiler}")
    if c_flags:
        command.append(f"-DCMAKE_C_FLAGS={c_flags}")
    if cxx_flags:
        command.append(f"-DCMAKE_CXX_FLAGS={cxx_flags}")
    if linker_flags:
        command.append(f"-DCMAKE_EXE_LINKER_FLAGS={linker_flags}")
    command.extend(args.cmake_arg)
    return run_command(command).returncode


def build_target(build_dir: Path, target: str, jobs: int) -> int:
    return run_command(["cmake", "--build", str(build_dir), "--target", target, f"-j{jobs}"]).returncode


def find_target_executable(build_dir: Path, target: str) -> Path | None:
    candidates = [build_dir / target]
    candidates.extend(path for path in build_dir.rglob(target) if path.is_file())
    seen = set()
    for candidate in candidates:
        if candidate in seen:
            continue
        seen.add(candidate)
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None


def filter_compile_commands(
    source: Path,
    output: Path,
    scope_prefixes: list[Path],
    exclude_prefixes: list[Path],
) -> tuple[list[Path], list[Path]]:
    entries = json.loads(source.read_text(encoding="utf-8"))
    filtered = []
    files = []
    rejected_parents = set()
    seen = set()

    for entry in entries:
        file_name = entry.get("file")
        if not file_name:
            continue
        file_path = Path(file_name)
        if not file_path.is_absolute():
            file_path = Path(entry.get("directory", REPO_ROOT)) / file_path
        file_path = file_path.resolve()
        in_scope = file_in_scope(file_path, scope_prefixes, exclude_prefixes)
        if not in_scope:
            try:
                to_repo_path(file_path)
                rejected_parents.add(file_path.parent)
            except ValueError:
                pass
            continue

        key = (entry.get("directory"), str(file_path), entry.get("command"), tuple(entry.get("arguments", [])))
        if key in seen:
            continue
        seen.add(key)
        normalized = dict(entry)
        normalized["file"] = str(file_path)
        filtered.append(normalized)
        files.append(file_path)

    included_files = sorted(set(files))
    safe_rejected_parents = []
    for parent in sorted(rejected_parents):
        if any(matches_prefix(file_path, parent) for file_path in included_files):
            continue
        safe_rejected_parents.append(parent)

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(filtered, indent=2) + "\n", encoding="utf-8")
    return included_files, safe_rejected_parents


def write_tool_error(path: Path, tool: Tool) -> int:
    message = tool.error or f"{tool.name} is unavailable"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(message + "\n", encoding="utf-8")
    log(message)
    return 1


def run_clang_tidy(
    clang_tidy: Tool,
    compile_dir: Path,
    files: list[Path],
    reports_dir: Path,
    jobs: int,
    extra_args: list[str],
) -> int:
    output = reports_dir / "clang-tidy.txt"
    if not clang_tidy.available:
        return write_tool_error(output, clang_tidy)

    log(f"running clang-tidy on {len(files)} file(s)")
    extra_arg_options = []
    for extra_arg in extra_args:
        extra_arg_options.append(f"--extra-arg={extra_arg}")

    def filter_clang_tidy_output(output_text: str) -> str:
        filtered = []
        for line in output_text.splitlines():
            if line == CLANG_TIDY_COMPAT_NOISE:
                continue
            if line.endswith(" warning generated.") or line.endswith(" warnings generated."):
                continue
            filtered.append(line)
        return "\n".join(filtered).strip()

    def run_one(file_path: Path) -> tuple[Path, int, str]:
        command = [
            str(clang_tidy.path),
            "--quiet",
            f"-p={compile_dir}",
            f"-checks={DEFAULT_CHECKS}",
            *extra_arg_options,
            str(file_path),
        ]
        try:
            result = capture_command(command, timeout=CLANG_TIDY_TIMEOUT_SECONDS)
            return file_path, result.returncode, filter_clang_tidy_output(result.stdout)
        except subprocess.TimeoutExpired as e:
            output_text = e.stdout or ""
            timeout_message = (
                f"clang-tidy timed out after {CLANG_TIDY_TIMEOUT_SECONDS}s while analyzing "
                f"{display_path(file_path)}"
            )
            return file_path, 124, f"{filter_clang_tidy_output(output_text)}\n{timeout_message}\n"

    status = 0
    lines = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, jobs)) as executor:
        for file_path, returncode, output_text in executor.map(run_one, files):
            if returncode != 0:
                status = 1
            if output_text.strip():
                lines.append(f"===== {display_path(file_path)} =====\n{output_text.rstrip()}\n")

    output.write_text("\n".join(lines) if lines else "clang-tidy completed with no output\n", encoding="utf-8")
    return status


def run_cppcheck(cppcheck: Tool, compile_commands: Path, reports_dir: Path) -> int:
    text_output = reports_dir / "cppcheck.txt"
    xml_output = reports_dir / "cppcheck.xml"
    if not cppcheck.available:
        return write_tool_error(text_output, cppcheck)

    base_command = [
        str(cppcheck.path),
        f"--project={compile_commands}",
        "--enable=warning,performance,portability",
        "--inline-suppr",
        "--suppress=missingIncludeSystem",
        "--suppress=unknownMacro",
        "--suppress=wrongPrintfScanfArgNum:*/brookesia_lib_utils/include/brookesia/lib_utils/log.hpp",
        "--suppress=wrongPrintfScanfArgNum:*/brookesia_lib_utils/src/log.cpp",
        "--suppress=pureVirtualCall:*/brookesia_hal_interface/include/brookesia/hal_interface/device.hpp",
        "--error-exitcode=2",
        "--quiet",
    ]
    text_result = run_command([*base_command, f"--output-file={text_output}"])
    xml_result = run_command([*base_command, "--xml", "--xml-version=2"], stderr=xml_output)
    return 1 if text_result.returncode != 0 or xml_result.returncode != 0 else 0


def run_scan_build(
    scan_build: Tool,
    build_dir: Path,
    target: str,
    reports_dir: Path,
    jobs: int,
    exclude_dirs: list[Path],
) -> int:
    scan_dir = reports_dir / "scan-build"
    if not scan_build.available:
        scan_dir.mkdir(parents=True, exist_ok=True)
        return write_tool_error(scan_dir / "scan-build.txt", scan_build)

    run_command(["cmake", "--build", str(build_dir), "--target", "clean"])
    command = [
        str(scan_build.path),
        "--status-bugs",
        "-o",
        str(scan_dir),
        "--html-title",
        "ESP-Brookesia PC static analyzer",
        "--use-cc",
        "/usr/bin/cc",
        "--use-c++",
        "/usr/bin/c++",
    ]
    for exclude in exclude_dirs:
        command.extend(["--exclude", str(exclude)])
    command.extend(["cmake", "--build", str(build_dir), "--target", target, f"-j{jobs}"])
    return run_command(command).returncode


def run_static(args: argparse.Namespace) -> int:
    build_root = resolve_repo_path(args.build_root)
    reports_dir = get_reports_dir(args)
    static_build_dir = build_root / "static" / "build"
    compile_dir = build_root / "static" / "compile_commands"
    compile_commands = compile_dir / "compile_commands.json"
    reports_dir.mkdir(parents=True, exist_ok=True)

    llvm_bin = resolve_repo_path(args.llvm_bin) if args.llvm_bin is not None else None
    clangxx = find_tool("clang++", ["clang++-18", "clang++"], llvm_bin, required=False)
    clang_tidy = find_tool("clang-tidy", ["clang-tidy-18", "clang-tidy"], llvm_bin)
    scan_build = find_tool("scan-build", ["scan-build-18", "scan-build"], llvm_bin)
    cppcheck = find_tool("cppcheck", ["cppcheck"])

    compat_args = check_expected_compat(clangxx)
    compat_flags = to_cmake_cxx_flags(compat_args)
    status = configure_build(args, static_build_dir, cxx_flags=compat_flags)
    if status != 0:
        return status

    source_compile_commands = static_build_dir / "compile_commands.json"
    if not source_compile_commands.exists():
        log(f"compile database is missing: {source_compile_commands}")
        return 1

    scope_prefixes = get_scope_prefixes(args)
    exclude_prefixes = get_exclude_prefixes(args)
    files, rejected_dirs = filter_compile_commands(source_compile_commands, compile_commands, scope_prefixes, exclude_prefixes)
    scan_excludes = sorted(set([*exclude_prefixes, *rejected_dirs]))
    log(f"filtered compile database: {len(files)} file(s) -> {compile_commands}")
    if not files:
        log("no files matched the selected scope")
        return 1

    results = [
        run_clang_tidy(clang_tidy, compile_dir, files, reports_dir, args.jobs, compat_args),
        run_scan_build(scan_build, static_build_dir, args.target, reports_dir, args.jobs, scan_excludes),
        run_cppcheck(cppcheck, compile_commands, reports_dir),
    ]
    return 1 if any(result != 0 for result in results) else 0


def asan_environment(args: argparse.Namespace, reports_dir: Path) -> dict[str, str]:
    env = os.environ.copy()
    env["ASAN_OPTIONS"] = f"{ASAN_OPTIONS}:log_path={reports_dir / 'asan'}"
    env["LSAN_OPTIONS"] = LSAN_OPTIONS
    llvm_bin = resolve_repo_path(args.llvm_bin) if args.llvm_bin is not None else None
    symbolizer = find_tool("llvm-symbolizer", ["llvm-symbolizer-18", "llvm-symbolizer"], llvm_bin, required=False)
    if symbolizer.available:
        env["ASAN_SYMBOLIZER_PATH"] = str(symbolizer.path)
    elif symbolizer.error:
        log(symbolizer.error)
    return env


def run_executable(
    executable: Path,
    program_args: list[str],
    reports_dir: Path,
    *,
    env: dict[str, str] | None,
    timeout_seconds: int,
) -> int:
    reports_dir.mkdir(parents=True, exist_ok=True)
    try:
        result = run_command(
            [str(executable), *program_args],
            env=env,
            stdout=reports_dir / "stdout.txt",
            stderr=reports_dir / "stderr.txt",
            timeout=timeout_seconds,
        )
        return result.returncode
    except subprocess.TimeoutExpired as e:
        timeout_note = f"program timed out after {timeout_seconds}s\n"
        (reports_dir / "timeout.txt").write_text(timeout_note, encoding="utf-8")
        if e.stdout:
            (reports_dir / "stdout.txt").write_text(str(e.stdout), encoding="utf-8")
        if e.stderr:
            (reports_dir / "stderr.txt").write_text(str(e.stderr), encoding="utf-8")
        log(timeout_note.strip())
        return 124


def run_asan(args: argparse.Namespace) -> int:
    build_root = resolve_repo_path(args.build_root)
    build_dir = build_root / "asan" / "build"
    reports_dir = get_reports_dir(args) / "asan"
    reports_dir.mkdir(parents=True, exist_ok=True)

    sanitizer_flags = "-fsanitize=address,leak -fno-omit-frame-pointer -g"
    status = configure_build(
        args,
        build_dir,
        c_flags=sanitizer_flags,
        cxx_flags=sanitizer_flags,
        linker_flags="-fsanitize=address,leak",
    )
    if status != 0:
        return status
    status = build_target(build_dir, args.target, args.jobs)
    if status != 0:
        return status

    executable = find_target_executable(build_dir, args.target)
    if executable is None:
        return write_tool_error(reports_dir / "executable.txt", Tool(args.target, None, "target executable not found"))
    return run_executable(
        executable,
        args.program_arg,
        reports_dir,
        env=asan_environment(args, reports_dir),
        timeout_seconds=args.timeout_seconds,
    )


def run_heaptrack(args: argparse.Namespace) -> int:
    build_root = resolve_repo_path(args.build_root)
    build_dir = build_root / "heaptrack" / "build"
    reports_dir = get_reports_dir(args) / "heaptrack"
    reports_dir.mkdir(parents=True, exist_ok=True)

    heaptrack = find_tool("heaptrack", ["heaptrack"])
    heaptrack_print = find_tool("heaptrack_print", ["heaptrack_print"], required=False)
    if not heaptrack.available:
        return write_tool_error(reports_dir / "heaptrack.txt", heaptrack)

    debug_flags = "-g -fno-omit-frame-pointer"
    status = configure_build(args, build_dir, c_flags=debug_flags, cxx_flags=debug_flags)
    if status != 0:
        return status
    status = build_target(build_dir, args.target, args.jobs)
    if status != 0:
        return status

    executable = find_target_executable(build_dir, args.target)
    if executable is None:
        return write_tool_error(reports_dir / "executable.txt", Tool(args.target, None, "target executable not found"))

    output_base = reports_dir / "heaptrack"
    try:
        result = run_command(
            [str(heaptrack.path), "-o", str(output_base), str(executable), *args.program_arg],
            stdout=reports_dir / "stdout.txt",
            stderr=reports_dir / "stderr.txt",
            timeout=args.timeout_seconds,
        )
        status = result.returncode
    except subprocess.TimeoutExpired as e:
        status = 124
        (reports_dir / "timeout.txt").write_text(
            f"heaptrack run timed out after {args.timeout_seconds}s\n",
            encoding="utf-8",
        )
        if e.stdout:
            (reports_dir / "stdout.txt").write_text(str(e.stdout), encoding="utf-8")
        if e.stderr:
            (reports_dir / "stderr.txt").write_text(str(e.stderr), encoding="utf-8")

    data_files = sorted(
        [*reports_dir.glob("heaptrack*.gz"), *reports_dir.glob("heaptrack*.zst")],
        key=lambda path: path.stat().st_mtime,
    )
    if heaptrack_print.available and data_files:
        run_command(
            [str(heaptrack_print.path), str(data_files[-1])],
            stdout=get_reports_dir(args) / "heaptrack-summary.txt",
        )
    elif heaptrack_print.error:
        (get_reports_dir(args) / "heaptrack-summary.txt").write_text(
            heaptrack_print.error + "\n",
            encoding="utf-8",
        )
    return status


def run_all(args: argparse.Namespace) -> int:
    statuses = []
    for runner in (run_static, run_asan, run_heaptrack):
        status = runner(args)
        statuses.append(status)
        if status != 0 and not args.keep_going:
            break
    return 1 if any(status != 0 for status in statuses) else 0


def add_common_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--build-root", type=Path, default=DEFAULT_BUILD_ROOT)
    parser.add_argument("--reports-dir", type=Path)
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--llvm-bin", type=Path)
    parser.add_argument("--scope-prefix", type=Path, action="append", default=[])
    parser.add_argument("--exclude-prefix", type=Path, action="append", default=[])
    parser.add_argument("--cmake-arg", action="append", default=[])
    parser.add_argument("--program-arg", action="append", default=[])
    parser.add_argument("--timeout-seconds", type=int, default=180)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run generic PC CMake quality checks")
    subparsers = parser.add_subparsers(dest="command", required=True)

    for name in ("static", "asan", "heaptrack"):
        add_common_arguments(subparsers.add_parser(name))

    all_parser = subparsers.add_parser("all")
    add_common_arguments(all_parser)
    all_parser.add_argument("--keep-going", action="store_true")

    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.command == "static":
        return run_static(args)
    if args.command == "asan":
        return run_asan(args)
    if args.command == "heaptrack":
        return run_heaptrack(args)
    if args.command == "all":
        return run_all(args)
    return 2


if __name__ == "__main__":
    sys.exit(main())
