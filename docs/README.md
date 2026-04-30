# Documentation Source Folder

This directory contains the source files for the ESP-Brookesia RST documentation.

The documentation is built with `esp-docs` and Sphinx. Public API references
for the `utils`, `service`, `agent`, and `expression` components are generated
automatically during the `build-docs` process.

For `service` and `agent`, the user-facing documentation is now helper-first:
`service_helper` and `agent_helper` are treated as the public contracts, while
concrete providers stay available as implementation appendices.

# Hosted Documentation

* English: https://docs.espressif.com/projects/esp-brookesia/en/latest/index.html
* 中文: https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/index.html

The above URLs are all for the master branch latest version. Click the drop-down in the bottom left to choose a stable version or to download a PDF.

## Build Documentation

It is recommended to use a dedicated virtual environment for documentation:

```bash
python3 -m venv .venv-docs
source .venv-docs/bin/activate
python3 -m pip install --upgrade pip
python3 -m pip install -r docs/requirements.txt
```

Make sure the following tools are available in your `PATH`:

- **doxygen** and a host C++ compiler such as `c++`: required by `build-docs`
  to generate API XML and compile the helper-schema exporter.
- **latexmk** and a XeLaTeX distribution (e.g. `texlive-xetex`): required
  only when building PDF output (`-bs latex`). Install on Ubuntu/Debian:

  ```bash
  sudo apt-get install -y latexmk texlive-xetex texlive-fonts-recommended \
      texlive-plain-generic texlive-lang-chinese
  ```

  If you only need HTML output, these are not required.

The exporter now compiles directly against local repository components
(`brookesia_lib_utils`, `brookesia_hal_interface`, `brookesia_service_manager`,
`brookesia_service_helper`, and `brookesia_agent_helper`) without docs-side shim headers.

Run the remaining commands from the `docs/` directory:

```bash
cd docs
```

Build the English HTML docs:

```bash
build-docs -l en -bs html
```

Build the Chinese HTML docs:

```bash
build-docs -l zh_CN -bs html
```

To build PDF output instead, replace `-bs html` with `-bs latex` (requires
`latexmk` and a XeLaTeX distribution, see above).

These commands also run Doxygen, export helper contract schemas, and generate
intermediate artifacts under:

- `_build/<lang>/<target>/xml`
- `_build/<lang>/<target>/inc`
- `_build/<lang>/<target>/helper_schema`
- `_build/<lang>/<target>/contract_guides`

## Preview

Preview the English docs locally:

```bash
python3 -m http.server 8000 --directory _build/en/generic/html
```

Preview the Chinese docs locally:

```bash
python3 -m http.server 8000 --directory _build/zh_CN/generic/html
```

Then open `http://localhost:8000/` in your browser.

## Warning Baselines

Documentation warning baselines are stored in:

- `docs/sphinx-known-warnings.txt`
- `docs/doxygen-known-warnings.txt`

If the API headers change and Doxygen warnings legitimately change with them,
rebuild the docs and update the corresponding baseline file carefully.

## Language Folder Sync

Keep the `docs/en` and `docs/zh_CN` file names synchronized:

```bash
cd docs
./check_lang_folder_sync.sh
```
