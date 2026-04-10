from __future__ import annotations

import sys
from pathlib import Path

from sphinx.util import logging

DOCS_ROOT = Path(__file__).resolve().parent
if str(DOCS_ROOT) not in sys.path:
    sys.path.insert(0, str(DOCS_ROOT))

from tools.export_helper_schemas import export_helper_schemas
from tools.generate_helper_contract_guides import generate_helper_contract_guides

# Contract .inc files place explicit anchors before the Functions / Events headings:
#   .. _helper-contract-<category>-<slug>-functions:
#   .. _helper-contract-<category>-<slug>-events:
# Use :ref:`… <helper-contract-service-wifi-functions>` or HTML #helper-contract-service-wifi-functions
# after a Sphinx build (see generate_helper_contract_guides._service_interface_subsection_anchor).

LOGGER = logging.getLogger(__name__)

SECTION_ORDER = {
    "Header File": 0,
    "Classes": 1,
    "Functions": 2,
    "Macros": 3,
}


def _is_section_heading(lines: list[str], index: int) -> bool:
    if index + 1 >= len(lines):
        return False
    title = lines[index].strip()
    underline = lines[index + 1].strip()
    return bool(title) and len(underline) >= 3 and len(set(underline)) == 1 and underline[0] in "^~-=" and title != "Header File"


def _collect_section_blocks(lines: list[str]) -> tuple[list[str], list[tuple[str, int, list[str]]]]:
    prefix: list[str] = []
    blocks: list[tuple[str, int, list[str]]] = []

    index = 0
    while index < len(lines):
        if index + 1 < len(lines) and lines[index].strip() == "Header File":
            start = index
            index += 2
            while index < len(lines) and not _is_section_heading(lines, index):
                index += 1
            blocks.append(("Header File", 0, lines[start:index]))
            continue

        if _is_section_heading(lines, index):
            title = lines[index].strip()
            start = index
            order = len(blocks)
            index += 2
            while index < len(lines) and not _is_section_heading(lines, index) and lines[index].strip() != "Header File":
                index += 1
            blocks.append((title, order, lines[start:index]))
            continue

        if not blocks:
            prefix.append(lines[index])
        else:
            # Preserve any trailing text that is not part of a section block.
            blocks[-1][2].append(lines[index])
        index += 1

    return prefix, blocks


def _reorder_inc_file(path: Path) -> None:
    if not path.is_file():
        return

    lines = path.read_text(encoding="utf-8").splitlines()
    prefix, blocks = _collect_section_blocks(lines)
    if not blocks:
        return

    header_blocks = [block for block in blocks if block[0] == "Header File"]
    other_blocks = [block for block in blocks if block[0] != "Header File"]
    other_blocks.sort(key=lambda item: (SECTION_ORDER.get(item[0], 100), item[1]))

    reordered = list(prefix)
    for block in header_blocks + other_blocks:
        reordered.extend(block[2])

    path.write_text("\n".join(reordered), encoding="utf-8")


def _reorder_doxygen_include_files(include_root: Path) -> None:
    if not include_root.exists():
        return
    for path in include_root.rglob("*.inc"):
        _reorder_inc_file(path)


def _build_helper_contract_docs(app) -> None:
    docs_root = Path(__file__).resolve().parent
    repo_root = docs_root.parent
    build_dir = Path(app.outdir).parent
    language = getattr(app.config, "language", "en") or "en"

    LOGGER.info("Generating helper-first contract guides for %s", language)
    export_helper_schemas(repo_root=repo_root, build_dir=build_dir, output_dir=build_dir / "helper_schema")
    generate_helper_contract_guides(build_dir=build_dir, language=language)
    _reorder_doxygen_include_files(build_dir / "inc")


def setup(app):
    app.connect("builder-inited", _build_helper_contract_docs)
    return {
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
