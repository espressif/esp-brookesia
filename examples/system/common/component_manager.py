# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Keep IDF Component Manager state in the selected system build directory."""

import hashlib
import runpy
import shutil
import sys
from pathlib import Path

import yaml


def local_manifest_hash(lock):
    """Track nested local overrides and the output's dependency configuration."""
    digest = hashlib.sha256()
    locked = yaml.safe_load(lock.read_text())
    manifests = []
    for dependency in locked.get("dependencies", {}).values():
        source = dependency.get("source", {})
        if source.get("type") == "local" and source.get("path"):
            manifests.append((lock.parent / source["path"]).resolve() / "idf_component.yml")
    # Conditions in transitive manifests may use config keys absent from the
    # top-level manifest hash. Keep those changes local to this output too.
    # Record the final JSON after IDF finishes configuring. The manager's
    # preliminary JSON can change as newly resolved components add Kconfig.
    manifests.append(lock.parent / "config/sdkconfig.json")
    for manifest in sorted(set(manifests)):
        digest.update(str(manifest).encode() + b"\0")
        digest.update(manifest.read_bytes() if manifest.exists() else b"")
        digest.update(b"\0")
    return digest.hexdigest()


def finalize_state(lock):
    """Record successful resolution only after the final Kconfig is available."""
    if not lock.is_file() or not (lock.parent / "config/sdkconfig.json").is_file():
        raise FileNotFoundError(f"Final dependency lock and Kconfig JSON required in {lock.parent}")
    (lock.parent / "dependencies/local_manifests.sha256").write_text(local_manifest_hash(lock))


def main():
    """Relocate manager state, then run the original IDF manager unchanged."""
    module, *args = sys.argv[1:]
    options = dict(arg[2:].split("=", 1) for arg in args if arg.startswith("--") and "=" in arg)
    project = Path(options["project_dir"])
    # IDF 6.0 prepare_dependencies has no --build_dir option. Both supported
    # interfaces identify the output through its Kconfig JSON path.
    build = (Path(options["build_dir"]) if "build_dir" in options else
             Path(options["sdkconfig_json_file"]).parent.parent)
    state = build / "dependencies"
    lock = Path(options["lock_path"])
    if not state.exists():
        state.mkdir(parents=True)
        # Seed from existing downloads, never move or edit them. The original
        # manager validates hashes and resolves the selected backend as usual.
        cached_components = project / "managed_components"
        if cached_components.is_dir():
            shutil.copytree(cached_components, state / "managed_components")
        if not lock.exists() and (project / "dependencies.lock").is_file():
            locked = yaml.safe_load((project / "dependencies.lock").read_text())
            for dependency in locked.get("dependencies", {}).values():
                source = dependency.get("source", {})
                if source.get("type") == "local" and source.get("path"):
                    source["path"] = str((project / source["path"]).resolve())
            # The imported lock predates this output's local override manifests
            # and configuration. Re-resolve with the official manager once;
            # keep the copied downloads as its validated cache.
            locked["manifest_hash"] = "0" * 64
            lock.write_text(yaml.safe_dump(locked, sort_keys=False))
    # IDF can reuse a lock when a nested override's dependency constraints
    # change without a component version bump. Refresh only this output's
    # generated solution with a valid-length stale hash; the official resolver
    # still validates all component download hashes.
    manifest_stamp = state / "local_manifests.sha256"
    if ("prepare_dependencies" in args and lock.exists() and
            (not manifest_stamp.exists() or manifest_stamp.read_text() != local_manifest_hash(lock))):
        locked = yaml.safe_load(lock.read_text())
        locked["manifest_hash"] = "0" * 64
        lock.write_text(yaml.safe_dump(locked, sort_keys=False))
    sys.argv = [module, *[
        f"--project_dir={state}" if arg.startswith("--project_dir=") else arg for arg in args
    ]]
    preserve_stamp = False
    try:
        runpy.run_module(module, run_name="__main__")
        preserve_stamp = "prepare_dependencies" not in args
    except SystemExit as error:
        # Exit 10 requests another discovery pass with sdkconfig.json. IDF 6.0
        # also injects requirements between passes; keep its retry cache too.
        preserve_stamp = error.code == 10 or (error.code in (None, 0) and "prepare_dependencies" not in args)
        if error.code not in (None, 0):
            raise
    finally:
        # Successful discovery is recorded again after final Kconfig; real
        # failures must not leave an earlier configuration's success stamp.
        if not preserve_stamp:
            manifest_stamp.unlink(missing_ok=True)


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--finalize":
        finalize_state(Path(sys.argv[2]))
    else:
        main()
