import os
import sys
import shutil
import importlib.util
import tempfile
import yaml
from pathlib import Path

try:
    from idf_component_manager.dependencies import download_project_dependencies
    from idf_component_tools.manager import ManifestManager
    from idf_component_tools.utils import ProjectRequirements
except ImportError:
    download_project_dependencies = ManifestManager = ProjectRequirements = None

def _bmgr_config_callback(target_name: str, ctx, args, **kwargs) -> None:
    raise Exception("should not be called")

_FAKE_ACTIONS = {
    "actions": {
        "gen-bmgr-config": {
            "callback": _bmgr_config_callback,
            "options": [],
            "short_help": "Generate ESP Board Manager configuration files",
        },
    }
}

def _load_module(file_path, module_name=None):
    module_name = module_name or file_path.stem
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    if spec is None or spec.loader is None:
        raise ImportError(f'Failed to create spec for {file_path}')
    mod = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = mod
    spec.loader.exec_module(mod)
    return mod

def _download_bmgr_component(project_path: str) -> None:
    """Download the esp_board_manager component.

    Tries to read the dependency version from the manifest file in the project's
    main directory. Falls back to the latest version ("*") when the manifest or
    the esp_board_manager entry cannot be found.
    """
    if not all([download_project_dependencies, ManifestManager, ProjectRequirements]):
        print("Warning: idf-component-manager is not available, cannot download component")
        return

    MANIFEST_NAMES = ["idf_component.yml", "idf_component.yaml"]
    BMGR_KEYS = ["espressif/esp_board_manager", "esp_board_manager"]
    DEFAULT_BMGR_KEY = BMGR_KEYS[0]
    DEFAULT_BMGR_VERSION = "*"

    proj = Path(project_path)
    main_dir = proj / "main"
    managed_path = proj / "managed_components"
    lock_path = proj / "dependencies.lock"

    # Resolve the dependency entry from the manifest; fall back to defaults when
    # the manifest or the relevant key is absent.
    bmgr_key = DEFAULT_BMGR_KEY
    bmgr_dep_value = DEFAULT_BMGR_VERSION

    manifest_path = next((p for f in MANIFEST_NAMES if (p := main_dir / f).exists()), None)
    if not manifest_path:
        print(
            f"Warning: Manifest file not found in {main_dir}, "
            f"falling back to downloading latest {DEFAULT_BMGR_KEY}"
        )
    else:
        try:
            with open(manifest_path, 'r', encoding='utf-8') as f:
                deps = (yaml.safe_load(f) or {}).get('dependencies', {})

            found_key = next((k for k in BMGR_KEYS if k in deps), None)
            if found_key:
                bmgr_key = found_key
                bmgr_dep_value = deps[found_key]
            else:
                print(
                    f"Warning: esp_board_manager dependency not found in {manifest_path}, "
                    f"falling back to downloading latest {DEFAULT_BMGR_KEY}"
                )
        except Exception as e:
            print(f"Warning: Failed to parse {manifest_path}: {e}, falling back to latest {DEFAULT_BMGR_KEY}")

    managed_path.mkdir(parents=True, exist_ok=True)

    try:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_manifest = Path(temp_dir) / "idf_component.yml"
            with open(temp_manifest, 'w', encoding='utf-8') as f:
                yaml.dump({'dependencies': {bmgr_key: bmgr_dep_value}}, f)

            manifest = ManifestManager(str(temp_dir), "temp").load()
            download_project_dependencies(
                ProjectRequirements([manifest]),
                str(lock_path),
                str(managed_path),
            )
            print(f"Successfully downloaded esp_board_manager component to {managed_path}")
    except Exception as e:
        print(f"Error downloading esp_board_manager component: {e}")
        raise


def _get_hal_adaptor_override_path(project_path: str):
    """Return the resolved absolute path of the espressif/brookesia_hal_adaptor
    override_path declared in main/idf_component.yml, or None if not found."""
    yml_path = Path(project_path) / 'main' / 'idf_component.yml'
    if not yml_path.exists():
        return None
    try:
        with open(yml_path, encoding='utf-8') as f:
            manifest = yaml.safe_load(f)
        deps = (manifest or {}).get('dependencies', {})
        entry = deps.get('espressif/brookesia_hal_adaptor', {}) or {}
        override_path = entry.get('override_path')
        if not override_path:
            return None
        # override_path is relative to the main/ directory
        return (yml_path.parent / override_path).resolve()
    except Exception:
        return None


def _inject_boards_dir(project_path: str) -> None:
    """Inject '-c <boards_dir>' into sys.argv when not already present.

    Checks the project-local ``boards/`` directory first; falls back to the
    ``brookesia_hal_boards`` directory that sits alongside the
    ``brookesia_hal_adaptor`` override_path declared in idf_component.yml.
    """
    if '-c' in sys.argv:
        return  # already supplied by the caller, nothing to do
    local_boards = Path(project_path) / 'boards'
    boards_dir = None
    if local_boards.is_dir():
        boards_dir = local_boards
        print(f'== idf_ext: using local boards directory: {boards_dir}')
    else:
        hal_adaptor_path = _get_hal_adaptor_override_path(project_path)
        if hal_adaptor_path is not None:
            # Derive boards directory as a sibling of the hal_adaptor directory
            boards_dir = hal_adaptor_path.parent / 'brookesia_hal_boards/boards'
            print(f'== idf_ext: using shared boards directory: {boards_dir}')
    if boards_dir is not None:
        sys.argv.extend(['-c', str(boards_dir)])
    else:
        print(f'== idf_ext: no boards directory found, skipping boards dir injection')
        return


def action_extensions(base_actions: dict, project_path: str) -> dict:
    bmgr_path = Path(project_path) / "managed_components" / "espressif__esp_board_manager"

    if len(sys.argv) > 2 and sys.argv[1] == "gen-bmgr-config":
        _inject_boards_dir(project_path)
        is_updating = os.getenv("_IDF_EXT_UPDATING_DEPS") == "1"
        shutil.rmtree(Path(project_path) / "components" / "gen_bmgr_codes", ignore_errors=True)
        if not bmgr_path.exists() and not is_updating:
            os.environ["_IDF_EXT_UPDATING_DEPS"] = "1"
            os.environ.setdefault("IDF_TARGET", "esp32")
            try:
                _download_bmgr_component(project_path)
            finally:
                os.environ.pop("_IDF_EXT_UPDATING_DEPS", None)

    if not bmgr_path.exists():
        return _FAKE_ACTIONS
    else:
        bmgr_config = _load_module(bmgr_path / "idf_ext.py")
        return bmgr_config.action_extensions(base_actions, project_path)
