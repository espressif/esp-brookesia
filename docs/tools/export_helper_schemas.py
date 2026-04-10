from __future__ import annotations

import os
import shutil
import subprocess
import textwrap
from pathlib import Path

from tools.helper_contract_registry import HELPER_CONTRACTS


def _cpp_escape(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def _generate_cpp_source(contracts: list[dict]) -> str:
    include_headers = sorted({contract["include_header"] for contract in contracts})
    includes = "\n".join(f'#include "{header}"' for header in include_headers)

    contract_blocks: list[str] = []
    for contract in contracts:
        section_blocks: list[str] = []
        for section in contract["schema_sections"]:
            functions_expr = f'{contract["helper_type"]}::{section["functions_accessor"]}'
            events_expr = f'{contract["helper_type"]}::{section["events_accessor"]}'
            name_type = section.get("name_type") or contract["helper_type"]
            name_expr = f'{name_type}::get_name()'
            section_blocks.append(
                textwrap.dedent(
                    f"""\
                    {{
                        boost::json::object section_obj;
                        section_obj["key"] = "{_cpp_escape(section["key"])}";
                        section_obj["service_name"] = std::string({name_expr});
                        section_obj["functions"] = serialize_functions({functions_expr});
                        section_obj["events"] = serialize_events({events_expr});
                        sections.emplace_back(std::move(section_obj));
                    }}
                    """
                ).rstrip()
            )

        contract_blocks.append(
            textwrap.dedent(
                f"""\
                {{
                    boost::json::array sections;
                {textwrap.indent(chr(10).join(section_blocks), "    ")}

                    boost::json::object contract_obj;
                    contract_obj["category"] = "{_cpp_escape(contract["category"])}";
                    contract_obj["slug"] = "{_cpp_escape(contract["slug"])}";
                    contract_obj["header_path"] = "{_cpp_escape(contract["header_path"])}";
                    contract_obj["include_header"] = "{_cpp_escape(contract["include_header"])}";
                    contract_obj["helper_type"] = "{_cpp_escape(contract["helper_type"])}";
                    contract_obj["sections"] = std::move(sections);
                    write_json_file(out_dir / "{_cpp_escape(contract["category"])}" / "{_cpp_escape(contract["slug"])}.json", contract_obj);
                }}
                """
            ).rstrip()
        )

    return textwrap.dedent(
        f"""\
        #define BOOST_ERROR_CODE_HEADER_ONLY 1
        #define BOOST_JSON_NO_LIB 1
        #include <iostream>
        #include "boost/json/src.hpp"
        #include "schema_json_serializer.hpp"
        {includes}

        using brookesia_host::serialize_functions;
        using brookesia_host::serialize_events;
        using brookesia_host::write_json_file;

        namespace fs = std::filesystem;

        int main(int argc, char **argv)
        {{
            if (argc != 2) {{
                std::cerr << "Usage: " << argv[0] << " <output-dir>" << std::endl;
                return 1;
            }}

            fs::path out_dir = argv[1];
            fs::create_directories(out_dir);

        {textwrap.indent(chr(10).join(contract_blocks), "    ")}

            return 0;
        }}
        """
    )


def _compile_exporter(repo_root: Path, cpp_path: Path, binary_path: Path, context: str = "") -> None:
    include_and_link_args = [
        str(cpp_path),
        "-I",
        "/usr/local/include",
        "-I",
        str(repo_root / "utils" / "brookesia_lib_utils" / "include"),
        "-I",
        str(repo_root / "service" / "brookesia_service_helper" / "include"),
        "-I",
        str(repo_root / "service" / "brookesia_service_helper" / "host_test"),
        "-I",
        str(repo_root / "service" / "brookesia_service_manager" / "include"),
        "-I",
        str(repo_root / "agent" / "brookesia_agent_helper" / "include"),
        "-pthread",
        "-o",
        str(binary_path),
    ]

    preferred = os.environ.get("CXX", "").strip()
    if preferred:
        compiler_candidates = [preferred]
    else:
        compiler_candidates = [
            "c++",
            "g++",
            "clang++",
            "g++-13",
            "g++-12",
            "clang++-17",
            "clang++-16",
        ]
    compilers: list[str] = []
    for candidate in compiler_candidates:
        if candidate in compilers:
            continue
        if shutil.which(candidate):
            compilers.append(candidate)

    standard_flags = ["-std=c++23", "-std=c++2b"]
    errors: list[str] = []
    for compiler in compilers:
        for standard_flag in standard_flags:
            compile_command = [compiler, standard_flag, *include_and_link_args]
            result = subprocess.run(
                compile_command,
                cwd=repo_root,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            if result.returncode == 0:
                return

            stderr_lines = [line.rstrip() for line in result.stderr.splitlines() if line.strip()]
            if stderr_lines:
                if len(stderr_lines) <= 4:
                    summary = " | ".join(stderr_lines)
                else:
                    summary = " | ".join(stderr_lines[:2] + ["..."] + stderr_lines[-2:])
            else:
                summary = f"exit code {result.returncode}"
            errors.append(f"{compiler} {standard_flag}: {summary}")

    if not compilers:
        raise RuntimeError("No C++ compiler found. Set CXX or install c++/g++/clang++ for docs schema export.")

    details = "\n".join(errors[-8:])
    raise RuntimeError(
        f"Failed to compile helper schema exporter with available compilers/standards{f' ({context})' if context else ''}. "
        "Please provide a compiler that supports C++23 (or C++2b with std::expected).\n"
        f"{details}"
    )


def export_helper_schemas(repo_root: Path, build_dir: Path, output_dir: Path) -> None:
    work_dir = build_dir / "contract_tools"
    work_dir.mkdir(parents=True, exist_ok=True)

    for contract in HELPER_CONTRACTS:
        slug = f"{contract['category']}_{contract['slug']}"
        cpp_path = work_dir / f"helper_schema_exporter_{slug}.cpp"
        binary_path = work_dir / f"helper_schema_exporter_{slug}"
        cpp_path.write_text(_generate_cpp_source([contract]), encoding="utf-8")

        _compile_exporter(
            repo_root=repo_root,
            cpp_path=cpp_path,
            binary_path=binary_path,
            context=slug,
        )
        subprocess.run([str(binary_path), str(output_dir)], check=True, cwd=repo_root)
