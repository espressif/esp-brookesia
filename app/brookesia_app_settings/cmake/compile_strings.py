#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Compile the shared Settings translations for the GSP firmware."""

import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    tables = {}
    for locale in ("en", "zh_CN"):
        tables[locale] = json.loads((args.source / f"{locale}.json").read_text())["strings"]
    if tables["en"].keys() != tables["zh_CN"].keys():
        raise ValueError("Settings English and Chinese translation keys must match")
    lines = [
        "// Generated from Settings i18n resources. Do not edit.",
        "#pragma once",
        "#include <string_view>",
        "namespace esp_brookesia::app::settings::detail {",
        "struct CompiledString { std::string_view key, english, chinese; };",
        "inline constexpr CompiledString COMPILED_STRINGS[] = {",
    ]
    for key in sorted(tables["en"]):
        values = (key, tables["en"][key], tables["zh_CN"][key])
        lines.append("    {" + ", ".join(json.dumps(v, ensure_ascii=False) for v in values) + "},")
    lines.extend(["};", "} // namespace esp_brookesia::app::settings::detail", ""])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines))


if __name__ == "__main__":
    main()
