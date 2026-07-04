from __future__ import annotations

import argparse
import html
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MERMAID_ROOT = PROJECT_ROOT / "docs" / "_static" / "mermaid"


def _find_chrome() -> str | None:
    for candidate in (
        os.environ.get("PUPPETEER_EXECUTABLE_PATH"),
        shutil.which("google-chrome"),
        shutil.which("google-chrome-stable"),
        shutil.which("chromium"),
        shutil.which("chromium-browser"),
    ):
        if candidate:
            return candidate
    return None


def _base_command() -> list[str]:
    mmdc = shutil.which("mmdc")
    if mmdc:
        return [mmdc]
    npx = shutil.which("npx")
    if npx:
        return [npx, "-y", "@mermaid-js/mermaid-cli"]
    raise RuntimeError("Neither 'mmdc' nor 'npx' was found in PATH")


def _render_one(source: Path, output: Path, command: list[str], puppeteer_config: Path | None) -> None:
    args = [*command, "-i", str(source), "-o", str(output), "-b", "transparent"]
    if puppeteer_config is not None:
        args.extend(["-p", str(puppeteer_config)])
    subprocess.run(args, check=True, cwd=PROJECT_ROOT)


def _write_html_wrapper(source: Path) -> None:
    output = source.with_suffix(".html")
    diagram = html.escape(source.read_text(encoding="utf-8"))
    output.write_text(
        '<pre class="mermaid">\n'
        f"{diagram}\n"
        "</pre>\n"
        '<script type="module">\n'
        "if (!window.__brookesiaMermaidRunner) {\n"
        "  window.__brookesiaMermaidRunner = true;\n"
        '  import("https://cdn.jsdelivr.net/npm/mermaid@11.12.1/dist/mermaid.esm.min.mjs").then(({ default: mermaid }) => {\n'
        '    const run = () => {\n'
        '      mermaid.initialize({ startOnLoad: false, theme: "default" });\n'
        '      mermaid.run({ querySelector: ".mermaid" });\n'
        "    };\n"
        '    if (document.readyState === "loading") {\n'
        '      document.addEventListener("DOMContentLoaded", run, { once: true });\n'
        "    } else {\n"
        "      run();\n"
        "    }\n"
        "  });\n"
        "}\n"
        "</script>\n",
        encoding="utf-8",
    )


def export_mermaid_diagrams(root: Path, formats: list[str]) -> None:
    command = _base_command()
    chrome = _find_chrome()
    sources = sorted(root.rglob("*.mmd"))
    if not sources:
        raise RuntimeError(f"No Mermaid sources found under {root}")

    with tempfile.TemporaryDirectory(prefix="brookesia-mermaid-") as tmp:
        puppeteer_config = None
        if chrome:
            puppeteer_config = Path(tmp) / "puppeteer-config.json"
            puppeteer_config.write_text(
                "{\n"
                f'  "executablePath": "{chrome}",\n'
                '  "args": ["--no-sandbox", "--disable-setuid-sandbox"]\n'
                "}\n",
                encoding="utf-8",
            )

        for source in sources:
            _write_html_wrapper(source)
            for fmt in formats:
                output = source.with_suffix(f".{fmt}")
                print(f"Export {source.relative_to(PROJECT_ROOT)} -> {output.relative_to(PROJECT_ROOT)}")
                _render_one(source, output, command, puppeteer_config)


def main() -> int:
    parser = argparse.ArgumentParser(description="Export Mermaid diagrams used by the ESP-Brookesia docs")
    parser.add_argument(
        "--root",
        type=Path,
        default=MERMAID_ROOT,
        help="Root directory containing .mmd files",
    )
    parser.add_argument(
        "--format",
        dest="formats",
        action="append",
        choices=("png", "svg", "pdf"),
        help="Output format to generate. Can be repeated. Defaults to png and svg.",
    )
    args = parser.parse_args()
    export_mermaid_diagrams(args.root.resolve(), args.formats or ["png", "svg"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
