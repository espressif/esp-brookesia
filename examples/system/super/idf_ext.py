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

MANIFEST_NAMES = ["idf_component.yml", "idf_component.yaml"]
BMGR_KEYS = ["espressif/esp_board_manager", "esp_board_manager"]
HAL_ADAPTOR_KEYS = ["espressif/brookesia_hal_adaptor", "brookesia_hal_adaptor"]
HAL_BOARDS_KEYS = ["espressif/brookesia_hal_boards", "brookesia_hal_boards"]

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

def _resolve_manifest_dependency(project_path: str, component_keys):
    """Resolve the dependency key/value for a component from the project manifest."""
    default_key = component_keys[0]
    default_version = "*"
    component_name = default_key.split("/")[-1]
    manifest_path = next(
        (p for f in MANIFEST_NAMES if (p := Path(project_path) / "main" / f).exists()),
        None,
    )

    if not manifest_path:
        print(
            f"Warning: Manifest file not found in {Path(project_path) / 'main'}, "
            f"falling back to downloading latest {default_key}"
        )
        return default_key, default_version

    try:
        with open(manifest_path, 'r', encoding='utf-8') as f:
            deps = (yaml.safe_load(f) or {}).get('dependencies', {})

        found_key = next((k for k in component_keys if k in deps), None)
        if found_key:
            return found_key, deps[found_key]

        print(
            f"Warning: {component_name} dependency not found in {manifest_path}, "
            f"falling back to downloading latest {default_key}"
        )
    except Exception as e:
        print(f"Warning: Failed to parse {manifest_path}: {e}, falling back to latest {default_key}")

    return default_key, default_version

def _resolve_dependency_from_component_manifest(component_path: Path, component_keys):
    """Resolve a dependency key/value from an existing component manifest."""
    manifest_path = next((p for f in MANIFEST_NAMES if (p := component_path / f).exists()), None)
    if not manifest_path:
        return None

    try:
        with open(manifest_path, encoding='utf-8') as f:
            deps = (yaml.safe_load(f) or {}).get('dependencies', {})
        found_key = next((k for k in component_keys if k in deps), None)
        if found_key is None:
            return None

        dep_value = deps[found_key]
        if isinstance(dep_value, dict) and "version" in dep_value:
            dep_value = {"version": dep_value["version"]}
        return found_key, dep_value
    except Exception as e:
        print(f"Warning: Failed to parse {manifest_path}: {e}")
        return None

def _resolve_bmgr_dependency_for_project(project_path: str):
    """Resolve esp_board_manager version needed by the project's HAL adaptor."""
    default_dependency = (BMGR_KEYS[0], "*")
    deps = _load_manifest_dependencies(project_path)
    direct_key = next((key for key in BMGR_KEYS if key in deps), None)
    if direct_key is not None:
        return direct_key, deps[direct_key]

    hal_adaptor_path = _get_component_override_path(project_path, HAL_ADAPTOR_KEYS)
    if hal_adaptor_path is None:
        hal_adaptor_path = _get_managed_component_path(project_path, HAL_ADAPTOR_KEYS)
    if hal_adaptor_path is None and any(key in deps for key in HAL_ADAPTOR_KEYS):
        print("== idf_ext: downloading brookesia_hal_adaptor to resolve esp_board_manager dependency")
        _download_component(project_path, HAL_ADAPTOR_KEYS)
        hal_adaptor_path = _get_managed_component_path(project_path, HAL_ADAPTOR_KEYS)
    if hal_adaptor_path is None:
        return default_dependency

    resolved_dependency = _resolve_dependency_from_component_manifest(hal_adaptor_path, BMGR_KEYS)
    if resolved_dependency is None:
        return default_dependency

    print(f"== idf_ext: using esp_board_manager dependency from {hal_adaptor_path / 'idf_component.yml'}")
    return resolved_dependency

def _download_component(project_path: str, component_keys) -> None:
    """Download a component declared in the project manifest.

    Tries to read the dependency version from the manifest file in the project's
    main directory. Falls back to the latest version ("*") when the manifest or
    the matching dependency entry cannot be found.
    """
    if not all([download_project_dependencies, ManifestManager, ProjectRequirements]):
        print("Warning: idf-component-manager is not available, cannot download component")
        return

    default_key = component_keys[0]
    component_name = default_key.split("/")[-1]

    proj = Path(project_path)
    managed_path = proj / "managed_components"
    lock_path = proj / "dependencies.lock"

    dependencies = {}
    if component_keys == BMGR_KEYS:
        component_key, component_dep_value = _resolve_bmgr_dependency_for_project(project_path)
    else:
        component_key, component_dep_value = _resolve_manifest_dependency(project_path, component_keys)
    dependencies[component_key] = component_dep_value

    # When boards are downloaded on demand, pull esp_board_manager in the same
    # solve so a later dependency refresh does not remove the managed boards.
    if component_keys == HAL_BOARDS_KEYS:
        bmgr_path = _get_available_component_path(project_path, BMGR_KEYS)
        needs_bmgr = _has_component(project_path, BMGR_KEYS) or _has_component(project_path, HAL_ADAPTOR_KEYS)
        if bmgr_path is None and needs_bmgr:
            bmgr_key, bmgr_dep_value = _resolve_bmgr_dependency_for_project(project_path)
            dependencies[bmgr_key] = bmgr_dep_value

    managed_path.mkdir(parents=True, exist_ok=True)

    try:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_manifest = Path(temp_dir) / "idf_component.yml"
            with open(temp_manifest, 'w', encoding='utf-8') as f:
                yaml.dump({'dependencies': dependencies}, f)

            manifest = ManifestManager(str(temp_dir), "temp").load()
            had_idf_target = "IDF_TARGET" in os.environ
            if not had_idf_target:
                os.environ["IDF_TARGET"] = "esp32"
            try:
                download_project_dependencies(
                    ProjectRequirements([manifest]),
                    str(lock_path),
                    str(managed_path),
                )
            finally:
                if not had_idf_target:
                    os.environ.pop("IDF_TARGET", None)
            print(f"Successfully downloaded {component_name} component to {managed_path}")
    except Exception as e:
        print(f"Error downloading {component_name} component: {e}")
        raise


def _load_manifest_dependencies(project_path: str):
    """Load dependencies from the project manifest, returning an empty dict on failure."""
    main_dir = Path(project_path) / 'main'
    manifest_path = next((p for f in MANIFEST_NAMES if (p := main_dir / f).exists()), None)
    if not manifest_path:
        return {}
    try:
        with open(manifest_path, encoding='utf-8') as f:
            manifest = yaml.safe_load(f) or {}
        return manifest.get('dependencies', {}) or {}
    except Exception:
        return {}

def _get_component_override_path(project_path: str, component_keys):
    """Return the resolved absolute override_path for the given component keys, or None."""
    yml_path = Path(project_path) / 'main' / 'idf_component.yml'
    deps = _load_manifest_dependencies(project_path)
    component_key = next((key for key in component_keys if key in deps), None)
    if component_key is None:
        return None
    entry = deps.get(component_key, {}) or {}
    override_path = entry.get('override_path')
    if not override_path:
        return None
    return (yml_path.parent / override_path).resolve()


def _get_option_value(argv, short_name: str, long_name: str):
    for i, arg in enumerate(argv):
        if arg in {short_name, long_name} and i + 1 < len(argv):
            return argv[i + 1]
        if arg.startswith(f'{long_name}='):
            return arg.split('=', 1)[1]
        if arg.startswith(short_name) and arg != short_name:
            return arg[len(short_name):]
    return None


def _set_option_value(argv, short_name: str, long_name: str, value: str) -> bool:
    for i, arg in enumerate(argv):
        if arg in {short_name, long_name} and i + 1 < len(argv):
            argv[i + 1] = value
            return True
        if arg.startswith(f'{long_name}='):
            argv[i] = f'{long_name}={value}'
            return True
        if arg.startswith(short_name) and arg != short_name:
            argv[i] = f'{short_name}{value}'
            return True
    return False


def _is_valid_hal_boards_path(path) -> bool:
    path = Path(path)
    return path.is_dir() and (path / "boards").is_dir()


def _resolve_hal_boards_path(project_path: str):
    override_path = _get_component_override_path(project_path, HAL_BOARDS_KEYS)
    if override_path is not None and _is_valid_hal_boards_path(override_path):
        return override_path

    managed_path = _get_managed_component_path(project_path, HAL_BOARDS_KEYS)
    if managed_path is not None and _is_valid_hal_boards_path(managed_path):
        return managed_path

    return None


def _find_board_dir(search_path: Path, board_name: str):
    if not search_path.is_dir():
        return None
    if search_path.name == board_name:
        return search_path
    candidates = sorted(
        path for path in search_path.rglob(board_name)
        if path.is_dir() and path.name == board_name
    )
    return candidates[0] if candidates else None


def _rewrite_bmgr_board_arg(project_path: str) -> None:
    board_arg = _get_option_value(sys.argv, '-b', '--board')
    if not board_arg:
        return

    if Path(board_arg).expanduser().exists():
        return

    customer_path = _get_option_value(sys.argv, '-c', '--customer-path')
    if customer_path:
        search_path = Path(customer_path).expanduser().resolve()
    else:
        hal_boards_path = _resolve_hal_boards_path(project_path)
        if hal_boards_path is None:
            return
        search_path = hal_boards_path / "boards"

    board_path = _find_board_dir(search_path, board_arg)
    if board_path is None:
        return
    board_path = str(board_path)

    if _set_option_value(sys.argv, '-b', '--board', board_path):
        print(f"== idf_ext: using board path for gen-bmgr-config: {board_path}")


def _has_component(project_path: str, component_keys) -> bool:
    """Return whether any of the given component keys is declared in the manifest."""
    deps = _load_manifest_dependencies(project_path)
    return any(key in deps for key in component_keys)


def _get_project_component_path(project_path: str, component_keys):
    """Return a matching component path under the project's components directory."""
    components_root = Path(project_path) / "components"
    for key in component_keys:
        namespace, _, name = key.partition("/")
        candidates = []
        if name:
            candidates.append(f"{namespace}__{name}")
            candidates.append(name)
        else:
            candidates.append(namespace)
        for candidate in candidates:
            candidate_path = components_root / candidate
            if candidate_path.exists():
                return candidate_path

    return None


def _get_managed_component_path(project_path: str, component_keys):
    """Return the downloaded managed component path if present, else None."""
    managed_root = Path(project_path) / "managed_components"
    for key in component_keys:
        namespace, _, name = key.partition("/")
        candidates = []
        if name:
            candidates.append(f"{namespace}__{name}")
            candidates.append(name)
        else:
            candidates.append(namespace)
        for candidate in candidates:
            candidate_path = managed_root / candidate
            if candidate_path.exists():
                return candidate_path

    for child in managed_root.glob("*"):
        if child.is_dir() and any(key.split("/")[-1] in child.name for key in component_keys):
            return child
    return None


def _get_available_component_path(project_path: str, component_keys):
    """Return a local project component path first, then a managed component path."""
    component_path = _get_project_component_path(project_path, component_keys)
    if component_path is not None:
        return component_path
    return _get_managed_component_path(project_path, component_keys)


def action_extensions(base_actions: dict, project_path: str) -> dict:
    bmgr_path = _get_available_component_path(project_path, BMGR_KEYS)

    if len(sys.argv) > 2 and sys.argv[1] == "gen-bmgr-config":
        is_updating = os.getenv("_IDF_EXT_UPDATING_DEPS") == "1"
        shutil.rmtree(Path(project_path) / "components" / "gen_bmgr_codes", ignore_errors=True)
        _rewrite_bmgr_board_arg(project_path)
        needs_bmgr = _has_component(project_path, BMGR_KEYS) or _has_component(project_path, HAL_ADAPTOR_KEYS)
        if needs_bmgr and bmgr_path is None and not is_updating:
            os.environ["_IDF_EXT_UPDATING_DEPS"] = "1"
            os.environ.setdefault("IDF_TARGET", "esp32")
            try:
                _download_component(project_path, BMGR_KEYS)
            finally:
                os.environ.pop("_IDF_EXT_UPDATING_DEPS", None)
            bmgr_path = _get_available_component_path(project_path, BMGR_KEYS)

    if bmgr_path is None:
        return _FAKE_ACTIONS
    else:
        bmgr_config = _load_module(bmgr_path / "idf_ext.py")
        return bmgr_config.action_extensions(base_actions, project_path)
