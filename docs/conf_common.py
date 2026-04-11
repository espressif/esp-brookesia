# -*- coding: utf-8 -*-
#
# Common (non-language-specific) configuration for Sphinx.
# This file is imported by language-specific conf.py files
# (for example, docs/en/conf.py and docs/zh_CN/conf.py).
#
import os as _os

# Base configuration from esp-docs (theme, builders, link roles, etc.).
from esp_docs.conf_docs import *  # noqa: F403,F401

# Extra extensions used by this project.
extensions += [
    # Add "copy" buttons for code blocks.
    "sphinx_copybutton",
    # Render WaveDrom diagrams when used in RST.
    "sphinxcontrib.wavedrom",
    # Render Mermaid diagrams (client-side JS, no mmdc required).
    "sphinxcontrib.mermaid",
    # Keep esp-docs build-system compatibility.
    "esp_docs.esp_extensions.dummy_build_system",
    # Run Doxygen during build and generate inc/*.inc fragments.
    "esp_docs.esp_extensions.run_doxygen",
    # Patch duplicate Breathe output for service_helper/base.hpp (after run_doxygen).
    "doxygen_inc_postprocess",
    # Generate helper-first contract guide fragments.
    "helper_contract_docs",
]

# Repository metadata used by link roles and page context.
github_repo = "espressif/esp-brookesia"

# Context values consumed by sphinx_idf_theme.
html_context["github_user"] = "espressif"
html_context["github_repo"] = "esp-brookesia"

# Supported documentation languages.
# idf_targets is intentionally not set: this project builds without a specific
# -t target, and sphinx_idf_theme requires either both idf_target and
# idf_targets to be set, or neither. Omitting idf_targets satisfies this.
languages = ["en", "zh_CN"]

# Project metadata used by the docs theme.
project_homepage = "https://github.com/espressif/esp-brookesia"
project_slug = "esp-brookesia"

# Brand/static assets.
html_static_path = ["../_static"]

# Keep compatibility with esp-docs defaults while appending local CSS.
try:
    html_css_files  # type: ignore[name-defined]  # noqa: B018
except NameError:
    html_css_files = []
html_css_files = html_css_files + ["helper_schema.css"]

# Increase sidebar depth so nested API pages remain visible.
try:
    html_theme_options  # type: ignore[name-defined]  # noqa: B018
except NameError:
    html_theme_options = {}
html_theme_options = dict(html_theme_options)
html_theme_options["navigation_depth"] = max(int(html_theme_options.get("navigation_depth", 4)), 6)

# Ignore generated build output and this folder README during source scan.
exclude_patterns = ["_build", "README.md"]

# Output naming and syntax/highlight assets.
pdf_file_prefix = "esp-brookesia"
pygments_style = "sphinx"

# Mermaid: render diagrams client-side via the Mermaid JS library bundled by
# sphinxcontrib-mermaid. This avoids any dependency on the 'mmdc' CLI tool,
# which is not available in the CI environment.
mermaid_output_format = "raw"

# Override the Sphinx-generated latexmkrc with a custom version that:
#   - Fixes "python.ist not found" when latexmk uses -outdir=build.
#   - Suppresses spurious "uninitialized value" warnings for LATEXOPTS.
latex_additional_files = [
    _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "latexmkrc"),
]
