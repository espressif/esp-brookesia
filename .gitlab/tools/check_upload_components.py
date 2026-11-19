# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Validate `.github/workflows/upload_component.yml` component completeness and upload order."""

import argparse
from pathlib import Path
import re
import sys
from typing import Dict, List, Optional, Set, Tuple

from idf_component_tools import semver
import yaml


DEFAULT_WORKFLOW_REL_PATH = Path('.github/workflows/upload_component.yml')
DEFAULT_REGISTRY_NAMESPACE = 'espressif'
UPLOAD_ACTION_PREFIX = 'espressif/upload-components-ci-action'
EXCLUDED_DIR_NAMES = {'components', 'managed_components', 'build', 'main', 'examples', 'test_apps', 'host_test', 'tests'}


def _normalize_registry_id(dep_key: str) -> Tuple[str, str, str]:
    """Normalize a Component Manager dependency key to (namespace, name, 'namespace/name')."""
    if '/' in dep_key:
        raw_ns, raw_name = dep_key.split('/', 1)
        namespace = raw_ns.strip().lower()
        name = raw_name.strip().lower()
    else:
        namespace = DEFAULT_REGISTRY_NAMESPACE
        name = dep_key.strip().lower()
    return namespace, name, f'{namespace}/{name}'


def parse_upload_config(workflow_path: Path) -> Tuple[str, List[str]]:
    """Parse upload namespace and semicolon-separated directories from upload_component.yml.

    Mirrors `espressif/upload-components-ci-action` (`upload.sh`): YAML parses the workflow first
    (preserving any `#` text inside folded/literal scalars as literal string content), then all
    whitespace is stripped and the string is split on `;`.
    """
    data = yaml.safe_load(workflow_path.read_text(encoding='utf-8'))
    if not isinstance(data, dict):
        raise ValueError(f'{workflow_path}: workflow root must be a YAML mapping')

    jobs = data.get('jobs')
    if not isinstance(jobs, dict):
        raise ValueError(f'{workflow_path}: missing "jobs" mapping')

    upload_steps: List[dict] = []
    for job_cfg in jobs.values():
        if not isinstance(job_cfg, dict):
            continue
        steps = job_cfg.get('steps')
        if not isinstance(steps, list):
            continue
        for step in steps:
            if not isinstance(step, dict):
                continue
            uses = str(step.get('uses') or '')
            if uses.startswith(UPLOAD_ACTION_PREFIX):
                upload_steps.append(step)

    if not upload_steps:
        raise ValueError(f'{workflow_path}: no "{UPLOAD_ACTION_PREFIX}" step found')
    if len(upload_steps) > 1:
        raise ValueError(f'{workflow_path}: multiple "{UPLOAD_ACTION_PREFIX}" steps are not supported')

    with_cfg = upload_steps[0].get('with')
    if not isinstance(with_cfg, dict):
        raise ValueError(f'{workflow_path}: upload step is missing "with" mapping')

    raw_namespace = with_cfg.get('namespace')
    if raw_namespace is None or (isinstance(raw_namespace, str) and raw_namespace.strip() == ''):
        namespace = DEFAULT_REGISTRY_NAMESPACE
    elif isinstance(raw_namespace, str):
        namespace = raw_namespace.strip().lower()
    else:
        raise ValueError(f'{workflow_path}: upload step "with.namespace" must be a string')

    raw_directories = with_cfg.get('directories')
    if not isinstance(raw_directories, str) or not raw_directories.strip():
        raise ValueError(f'{workflow_path}: upload step "with.directories" must be a non-empty string')

    compact = re.sub(r'\s+', '', raw_directories)
    trimmed = compact[:-1] if compact.endswith(';') else compact
    directories = trimmed.split(';')
    return namespace, directories


def _validate_optional_str_field(manifest_path: Path, dep_key: str, field_name: str, value: object) -> Optional[str]:
    if value is None:
        return None
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f'{manifest_path}: dependency "{dep_key}" has invalid {field_name} {value!r}')
    return value.strip()


def parse_component_manifest(manifest_path: Path) -> Tuple[Optional[str], Dict[str, Dict[str, Optional[str]]]]:
    """Parse top-level version and dependency metadata (including source type) via PyYAML."""
    try:
        data = yaml.safe_load(manifest_path.read_text(encoding='utf-8'))
    except yaml.YAMLError as exc:
        raise ValueError(f'{manifest_path}: invalid YAML: {exc}') from exc

    if data is None:
        data = {}
    if not isinstance(data, dict):
        raise ValueError(f'{manifest_path}: manifest root must be a YAML mapping')

    raw_version = data.get('version')
    version = raw_version.strip() if isinstance(raw_version, str) else None
    if raw_version is not None and (not version or not semver.validate(version)):
        raise ValueError(f'{manifest_path}: invalid "version" value {raw_version!r} (expected a concrete component version)')

    raw_deps = data.get('dependencies')
    if raw_deps is None:
        return version, {}
    if not isinstance(raw_deps, dict):
        raise ValueError(f'{manifest_path}: "dependencies" must be a YAML mapping')

    dependencies: Dict[str, Dict[str, Optional[str]]] = {}
    for dep_key, dep_cfg in raw_deps.items():
        if not isinstance(dep_key, str) or not dep_key.strip():
            raise ValueError(f'{manifest_path}: invalid dependency key {dep_key!r}')

        is_idf = dep_key.strip().lower() == 'idf'
        if dep_cfg is None:
            dependencies[dep_key] = {
                'source': 'idf' if is_idf else 'registry',
                'version': None,
                'override_path': None,
                'git': None,
                'path': None,
            }
        elif isinstance(dep_cfg, str):
            dependencies[dep_key] = {
                'source': 'idf' if is_idf else 'registry',
                'version': dep_cfg.strip() or None,
                'override_path': None,
                'git': None,
                'path': None,
            }
        elif isinstance(dep_cfg, dict):
            raw_dep_ver = dep_cfg.get('version')
            if raw_dep_ver is not None and (
                isinstance(raw_dep_ver, bool) or not isinstance(raw_dep_ver, (str, int, float))
            ):
                raise ValueError(f'{manifest_path}: dependency "{dep_key}" has invalid version {raw_dep_ver!r}')
            git_url = _validate_optional_str_field(manifest_path, dep_key, 'git', dep_cfg.get('git'))
            local_path = _validate_optional_str_field(manifest_path, dep_key, 'path', dep_cfg.get('path'))
            override_path = _validate_optional_str_field(
                manifest_path, dep_key, 'override_path', dep_cfg.get('override_path')
            )

            if is_idf:
                source = 'idf'
            elif git_url is not None:
                source = 'git'
            elif local_path is not None:
                source = 'path'
            elif override_path is not None:
                source = 'override_path'
            else:
                source = 'registry'

            dependencies[dep_key] = {
                'source': source,
                'version': (str(raw_dep_ver).strip() or None) if raw_dep_ver is not None else None,
                'override_path': override_path,
                'git': git_url,
                'path': local_path,
            }
        else:
            raise ValueError(
                f'{manifest_path}: dependency "{dep_key}" must be a version string or mapping, got {type(dep_cfg).__name__}'
            )

    return version, dependencies


def discover_repo_components(repo_root: Path) -> List[str]:
    """Return sorted repo-relative POSIX paths of publishable top-level components in the repository."""
    components: List[str] = []
    for manifest in sorted(repo_root.rglob('idf_component.yml')):
        rel_dir = manifest.parent.relative_to(repo_root)
        if any(
            part.startswith('.') or part in EXCLUDED_DIR_NAMES or part.startswith('build_')
            for part in rel_dir.parts
        ):
            continue
        components.append(rel_dir.as_posix())
    return components


def check_upload_components(repo_root: Path, workflow_rel_path: Path = DEFAULT_WORKFLOW_REL_PATH) -> int:
    repo_root = repo_root.resolve()
    workflow_path = repo_root / workflow_rel_path
    if not workflow_path.is_file():
        print(f'ERROR: Upload workflow not found: {workflow_path}', file=sys.stderr)
        return 1

    try:
        upload_namespace, raw_upload_dirs = parse_upload_config(workflow_path)
    except (ValueError, yaml.YAMLError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        return 1

    violations: List[str] = []

    seen_dirs: Dict[str, int] = {}
    published_id_to_index: Dict[str, int] = {}
    published_id_to_dir: Dict[str, str] = {}
    published_name_to_id: Dict[str, str] = {}
    resolved_dir_to_id: Dict[Path, str] = {}
    normalized_dirs: List[Optional[str]] = []
    local_override_dirs: Set[Path] = set()

    for idx, raw_dir in enumerate(raw_upload_dirs):
        pos = idx + 1
        if not raw_dir:
            violations.append(f'#{pos}: empty directory entry in {workflow_rel_path}')
            normalized_dirs.append(None)
            continue

        if Path(raw_dir).is_absolute():
            violations.append(f'#{pos} "{raw_dir}": must be a repository-relative path')
            normalized_dirs.append(None)
            continue

        comp_dir = raw_dir.rstrip('/')
        if not comp_dir:
            violations.append(f'#{pos} "{raw_dir}": invalid directory entry')
            normalized_dirs.append(None)
            continue

        if comp_dir in seen_dirs:
            violations.append(f'Duplicate directory entry "{comp_dir}" at #{pos} (first seen at #{seen_dirs[comp_dir] + 1})')
            normalized_dirs.append(None)
            continue
        seen_dirs[comp_dir] = idx

        full_dir = repo_root / comp_dir
        try:
            resolved_dir = full_dir.resolve()
            resolved_dir.relative_to(repo_root)
        except ValueError:
            violations.append(f'#{pos} "{comp_dir}": directory resolves outside repository root')
            normalized_dirs.append(None)
            continue

        manifest_path = full_dir / 'idf_component.yml'
        if not full_dir.is_dir():
            violations.append(f'#{pos} "{comp_dir}": directory does not exist')
            normalized_dirs.append(None)
            continue
        if not manifest_path.is_file():
            violations.append(f'#{pos} "{comp_dir}": missing idf_component.yml')
            normalized_dirs.append(None)
            continue

        comp_name = resolved_dir.name.lower()
        published_id = f'{upload_namespace}/{comp_name}'
        if published_id in published_id_to_index:
            prev_idx = published_id_to_index[published_id]
            violations.append(
                f'Duplicate component "{published_id}" at #{pos} "{comp_dir}" '
                f'(already listed at #{prev_idx + 1} "{published_id_to_dir[published_id]}")'
            )
            normalized_dirs.append(None)
            continue

        published_id_to_index[published_id] = idx
        published_id_to_dir[published_id] = comp_dir
        published_name_to_id[comp_name] = published_id
        resolved_dir_to_id[resolved_dir] = published_id
        normalized_dirs.append(comp_dir)

    for idx, comp_dir in enumerate(normalized_dirs):
        if comp_dir is None:
            continue
        pos = idx + 1
        full_dir = repo_root / comp_dir
        manifest_path = full_dir / 'idf_component.yml'

        try:
            version, dependencies = parse_component_manifest(manifest_path)
        except ValueError as exc:
            violations.append(f'#{pos} "{comp_dir}": {exc}')
            continue

        if not version:
            violations.append(f'#{pos} "{comp_dir}": idf_component.yml is missing a version field')

        for dep_key, dep_meta in dependencies.items():
            source = dep_meta.get('source')
            if source == 'idf':
                continue

            if source in ('git', 'path'):
                violations.append(
                    f'#{pos} "{comp_dir}" dependency "{dep_key}" uses unsupported "{source}" source '
                    f'(ESP Component Registry uploads do not support git or local path dependencies)'
                )
                continue

            _, dep_name, dep_id = _normalize_registry_id(dep_key)
            override_path = dep_meta.get('override_path')
            resolved_override = (full_dir / override_path).resolve() if override_path else None

            if dep_id not in published_id_to_index:
                if dep_name in published_name_to_id:
                    expected_id = published_name_to_id[dep_name]
                    violations.append(
                        f'#{pos} "{comp_dir}" dependency "{dep_key}" resolves to Registry ID "{dep_id}", '
                        f'but {workflow_rel_path} publishes "{expected_id}" (namespace mismatch)'
                    )
                elif resolved_override is not None and resolved_override in resolved_dir_to_id:
                    expected_id = resolved_dir_to_id[resolved_override]
                    violations.append(
                        f'#{pos} "{comp_dir}" dependency "{dep_key}" resolves to Registry ID "{dep_id}", '
                        f'but override_path "{override_path}" targets published component "{expected_id}"'
                    )
                elif (
                    resolved_override is not None
                    and repo_root / 'local' in resolved_override.parents
                    and resolved_override.name.lower() in (dep_name, dep_id.replace('/', '__'))
                ):
                    # This local checkout supplies a Registry dependency during development.
                    local_override_dirs.add(resolved_override)
                continue

            dep_idx = published_id_to_index[dep_id]
            dep_dir = published_id_to_dir[dep_id]
            if dep_idx >= idx:
                violations.append(
                    f'#{pos} "{comp_dir}" depends on "{dep_key}" (#{dep_idx + 1} "{dep_dir}"), '
                    f'which must appear earlier in {workflow_rel_path}'
                )

            if resolved_override is not None:
                expected_dir = (repo_root / dep_dir).resolve()
                if resolved_override != expected_dir:
                    violations.append(
                        f'#{pos} "{comp_dir}" dependency "{dep_key}" has override_path "{override_path}" '
                        f'resolving to "{resolved_override}", expected "{expected_dir}"'
                    )

    for repo_comp_dir in discover_repo_components(repo_root):
        resolved_repo_dir = (repo_root / repo_comp_dir).resolve()
        if resolved_repo_dir not in resolved_dir_to_id and resolved_repo_dir not in local_override_dirs:
            comp_name = resolved_repo_dir.name.lower()
            violations.append(
                f'Repository component "{repo_comp_dir}" ("{upload_namespace}/{comp_name}") '
                f'is missing from {workflow_rel_path}'
            )

    if violations:
        print(f'Component upload workflow validation failed ({len(violations)} issue(s)):', file=sys.stderr)
        for violation in violations:
            print(f'  - {violation}', file=sys.stderr)
        return 1

    print(f'Upload component workflow check passed ({len(published_id_to_index)} components, namespace={upload_namespace})')
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description='Validate .github/workflows/upload_component.yml component list and dependency order.'
    )
    parser.add_argument(
        '--repo-root',
        default='.',
        type=Path,
        help='Repository root to scan (default: current directory)',
    )
    parser.add_argument(
        '--workflow',
        default=DEFAULT_WORKFLOW_REL_PATH,
        type=Path,
        help='Path to upload workflow relative to repo-root',
    )
    args = parser.parse_args()
    return check_upload_components(args.repo_root.resolve(), args.workflow)


if __name__ == '__main__':
    sys.exit(main())
