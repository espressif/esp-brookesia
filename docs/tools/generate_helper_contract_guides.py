from __future__ import annotations

import copy
import html
import json
import re
import unicodedata
from pathlib import Path

from tools.helper_contract_registry import HELPER_CONTRACTS


HEADINGS = {
    "en": {
        "overview": "Overview",
        "include": "Standard Include / Helper Class",
        "functions": "Functions",
        "events": "Events",
        "related": "Related Types and Configs",
        "scheduler_yes": "Required",
        "scheduler_no": "Not required",
        "execution": "Execution",
        "description": "Description",
        "parameters": "Parameters",
        "items": "Items",
        "schema_json": "Schema JSON",
        "schema_json_summary": "Show raw JSON",
        "no_parameters": "No parameters.",
        "no_items": "No payload items.",
        "no_description": "No description provided.",
        "no_functions": "This contract does not expose standard function schemas.",
        "no_events": "This contract does not publish standard event schemas.",
        "api_reference": "Raw helper API reference",
        "include_label": "Standard include",
        "helper_label": "Helper class",
        "scheduler_label": "Requires scheduler",
        "type_label": "Type",
        "required_label": "Required",
        "description_label": "Description",
        "required": "required",
        "optional": "optional",
        "default": "Default",
        "cli_command": "CLI Command",
        "related_text": (
            "For structs, enums, helper methods, and the full Doxygen-rendered API surface, see "
            ":doc:`{label} <{doc}>`."
        ),
    },
    "zh_CN": {
        "overview": "概览",
        "include": "标准头文件 / Helper 类",
        "functions": "函数",
        "events": "事件",
        "related": "相关类型与配置",
        "scheduler_yes": "需要",
        "scheduler_no": "不需要",
        "execution": "执行要求",
        "description": "描述",
        "parameters": "参数",
        "items": "参数",
        "schema_json": "Schema JSON",
        "schema_json_summary": "展开查看 JSON",
        "no_parameters": "无。",
        "no_items": "无。",
        "no_description": "无。",
        "no_functions": "无。",
        "no_events": "无。",
        "api_reference": "helper 原始 API 参考",
        "include_label": "标准头文件",
        "helper_label": "Helper 类",
        "scheduler_label": "是否需要调度器",
        "type_label": "类型",
        "required_label": "是否必填",
        "description_label": "描述",
        "required": "必填",
        "optional": "可选",
        "default": "默认值",
        "cli_command": "CLI 命令",
        "related_text": "结构体、枚举、helper 方法以及完整 Doxygen API 请参考 :doc:`{label} <{doc}>`。",
    },
}

DEFAULT_INTERFACE_TITLE = {
    "en": "Service Interfaces",
    "zh_CN": "服务接口",
}


def _display_width(text: str) -> int:
    width = 0
    for char in text:
        width += 2 if unicodedata.east_asian_width(char) in {"F", "W"} else 1
    return width


def _section(title: str, marker: str) -> list[str]:
    return [title, marker * max(_display_width(title), 3), ""]


def _slugify_anchor(text: str) -> str:
    slug = re.sub(r"[^0-9a-zA-Z]+", "-", text).strip("-").lower()
    return slug or "entry"


def _service_interface_subsection_anchor(contract: dict, subsection: str) -> str:
    """Stable anchor for the Functions / Events headings (for :ref: / HTML #fragment)."""
    return f"helper-contract-{contract['category']}-{contract['slug']}-{subsection}"


def _entry_anchor(contract: dict, section_key: str, entry_kind: str, entry_name: str) -> str:
    return (
        f"{contract['category']}-"
        f"{contract['slug']}-"
        f"{_slugify_anchor(section_key)}-"
        f"{entry_kind}-"
        f"{_slugify_anchor(entry_name)}"
    )


def _emit_schema_items(
    lines: list[str],
    items: list[dict],
    entry_kind: str,
    language: str,
) -> None:
    text = HEADINGS[language]
    section_key = "parameters" if entry_kind == "functions" else "items"
    lines.extend(_section(text[section_key], '"'))

    if not items:
        lines.append(f"- {text['no_parameters' if entry_kind == 'functions' else 'no_items']}")
        lines.append("")
        return

    for item in items:
        item_name = item.get("name") or ("unnamed" if language == "en" else "未命名")
        lines.append(f"- ``{item_name}``")
        metadata_lines = [f"**{text['type_label']}**: ``{item['type']}``"]
        if entry_kind == "functions":
            metadata_lines.append(
                f"**{text['required_label']}**: {text['required'] if item['required'] else text['optional']}"
            )
            default_value = item.get("default_value")
            if isinstance(default_value, dict) and default_value.get("display"):
                metadata_lines.append(f"**{text['default']}**: ``{default_value['display']}``")
        description = (item.get("description", "") or "").strip()
        metadata_lines.append(
            f"**{text['description_label']}**: {description if description else text['no_description']}"
        )

        lines.append("")
        for metadata_line in metadata_lines:
            lines.append(f"  - {metadata_line}")
        lines.append("")
    lines.append("")


def _emit_schema_entries(
    lines: list[str],
    contract: dict,
    section_key: str,
    entries: list[dict],
    entry_kind: str,
    language: str,
    entry_title_marker: str,
    service_name: str,
) -> None:
    text = HEADINGS[language]
    if not entries:
        lines.append(text["no_functions" if entry_kind == "functions" else "no_events"])
        lines.append("")
        return

    for entry in entries:
        anchor = _entry_anchor(contract, section_key, entry_kind, entry["name"])
        lines.append(f".. _{anchor}:")
        lines.append("")
        lines.extend(_section(f"``{entry['name']}``", entry_title_marker))

        lines.extend(_section(text["description"], '"'))
        description = (entry.get("description", "") or "").strip()
        lines.append(description if description else text["no_description"])
        lines.append("")

        lines.extend(_section(text["execution"], '"'))
        lines.append(
            f"- {text['scheduler_label']}: "
            f"{text['scheduler_yes'] if entry.get('require_scheduler', True) else text['scheduler_no']}"
        )
        lines.append("")

        if entry_kind == "functions":
            _emit_schema_items(lines, entry.get("parameters", []), entry_kind, language)
        else:
            _emit_schema_items(lines, entry.get("items", []), entry_kind, language)

        _emit_schema_json(lines, entry, language)
        _emit_cli_command(lines, entry, entry_kind, service_name, language)


def _emit_schema_json(lines: list[str], entry: dict, language: str) -> None:
    text = HEADINGS[language]
    lines.extend(_section(text["schema_json"], '"'))

    normalized_entry = _normalize_schema_entry_for_display(entry)
    pretty_json = json.dumps(normalized_entry, ensure_ascii=False, indent=2)
    escaped_json = html.escape(pretty_json)

    lines.append(".. raw:: html")
    lines.append("")
    lines.append("   <details class=\"helper-schema-json\">")
    lines.append(f"   <summary>{text['schema_json_summary']}</summary>")
    lines.append("   <pre><code>")
    for json_line in escaped_json.splitlines():
        lines.append(f"   <span class=\"json-line\">{json_line}</span>")
    lines.append("   </code></pre>")
    lines.append("   </details>")
    lines.append("")


def _emit_cli_command(
    lines: list[str],
    entry: dict,
    entry_kind: str,
    service_name: str,
    language: str,
) -> None:
    text = HEADINGS[language]
    lines.extend(_section(text["cli_command"], '"'))

    if entry_kind == "functions":
        params = entry.get("parameters", [])
        if params:
            param_obj = {p["name"]: None for p in params}
            param_json = json.dumps(param_obj, ensure_ascii=False, separators=(",", ":"))
            cmd = f"svc_call {service_name} {entry['name']} {param_json}"
        else:
            cmd = f"svc_call {service_name} {entry['name']}"
    else:
        cmd = f"svc_subscribe {service_name} {entry['name']}"

    lines.append(".. code-block:: bash")
    lines.append("")
    lines.append(f"   {cmd}")
    lines.append("")


def _normalize_schema_entry_for_display(entry: dict) -> dict:
    normalized = copy.deepcopy(entry)
    parameters = normalized.get("parameters")
    if isinstance(parameters, list):
        for parameter in parameters:
            if isinstance(parameter, dict) and "default_value" in parameter:
                parameter["default_value"] = _normalize_default_value(parameter["default_value"])
    return normalized


def _normalize_default_value(value):
    if isinstance(value, dict):
        if "json" in value and "display" in value and set(value.keys()) <= {"json", "display"}:
            return _normalize_default_value(value["json"])
        return {key: _normalize_default_value(sub_value) for key, sub_value in value.items()}
    if isinstance(value, list):
        return [_normalize_default_value(item) for item in value]
    return value


def _emit_schema_section(
    lines: list[str],
    contract: dict,
    exported_sections: dict[str, dict],
    entry_kind: str,
    language: str,
    entry_title_marker: str,
) -> None:
    definitions = contract["schema_sections"]
    if len(definitions) == 1:
        section_key = definitions[0]["key"]
        section = exported_sections[section_key]
        _emit_schema_entries(
            lines,
            contract,
            section_key,
            section.get(entry_kind, []),
            entry_kind,
            language,
            entry_title_marker,
            service_name=section.get("service_name", ""),
        )
        return

    # For multi-part schemas (for example Video encoder/decoder), keep a stable
    # hierarchy: Functions/Events -> section group -> concrete schema entry.
    if entry_title_marker == "^":
        marker = "^"
        grouped_entry_marker = "~"
    elif entry_title_marker == "~":
        marker = "~"
        grouped_entry_marker = "'"
    else:
        marker = "~"
        grouped_entry_marker = entry_title_marker
    for definition in definitions:
        title = definition["title"][language]
        lines.extend(_section(title, marker))
        section_key = definition["key"]
        section = exported_sections[section_key]
        _emit_schema_entries(
            lines,
            contract,
            section_key,
            section.get(entry_kind, []),
            entry_kind,
            language,
            grouped_entry_marker,
            service_name=section.get("service_name", ""),
        )


def generate_helper_contract_guides(build_dir: Path, language: str) -> None:
    schema_root = build_dir / "helper_schema"
    guide_root = build_dir / "contract_guides"
    guide_root.mkdir(parents=True, exist_ok=True)

    text = HEADINGS[language]

    for contract in HELPER_CONTRACTS:
        schema_path = schema_root / contract["category"] / f"{contract['slug']}.json"
        with schema_path.open("r", encoding="utf-8") as stream:
            schema = json.load(stream)

        exported_sections = {section["key"]: section for section in schema["sections"]}
        lines: list[str] = []

        omit_overview = contract.get("omit_overview")
        if omit_overview is None:
            # Keep all helper contract guides in a consistent structure by default.
            omit_overview = True
        if not omit_overview:
            lines.extend(_section(text["overview"], "-"))
            lines.append(contract["overview"][language])
            lines.append("")

        if language != "zh_CN":
            lines.extend(_section(text["include"], "-"))
            lines.append(f"- {text['include_label']}: ``#include \\\"{contract['include_header']}\\\"``")
            lines.append(f"- {text['helper_label']}: ``{contract['helper_type']}``")
            lines.append("")

        interface_title = contract.get("interface_title") or DEFAULT_INTERFACE_TITLE
        interface_as_parent = bool(contract.get("interface_as_parent", True))
        has_interface_title = isinstance(interface_title, dict) and bool(interface_title.get(language))
        functions_events_marker = "-"
        entry_title_marker = "^"
        if has_interface_title and interface_as_parent:
            lines.extend(_section(interface_title[language], "-"))
            functions_events_marker = "^"
            entry_title_marker = "~"
        elif has_interface_title:
            lines.extend(_section(interface_title[language], "-"))

        lines.append(f".. _{_service_interface_subsection_anchor(contract, 'functions')}:")
        lines.append("")
        lines.extend(_section(text["functions"], functions_events_marker))
        _emit_schema_section(
            lines,
            contract,
            exported_sections,
            "functions",
            language,
            entry_title_marker,
        )

        lines.append(f".. _{_service_interface_subsection_anchor(contract, 'events')}:")
        lines.append("")
        lines.extend(_section(text["events"], functions_events_marker))
        _emit_schema_section(
            lines,
            contract,
            exported_sections,
            "events",
            language,
            entry_title_marker,
        )

        lines.extend(_section(text["related"], "-"))
        lines.append(
            text["related_text"].format(label=text["api_reference"], doc=contract["api_reference_doc"])
        )
        lines.append("")

        output_path = guide_root / contract["category"] / f"{contract['slug']}.inc"
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text("\n".join(lines), encoding="utf-8")
