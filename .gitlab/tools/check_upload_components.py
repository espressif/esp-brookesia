# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Validate `.github/workflows/upload_component.yml` component completeness and upload order."""

import argparse
from pathlib import Path
import re
import sys
from typing import Dict, List, Optional, Tuple


DEFAULT_WORKFLOW_REL_PATH = Path('.github/workflows/upload_component.yml')
EXCLUDED_DIR_NAMES = {'components', 'managed_components', 'build', 'main', 'examples', 'test_apps', 'host_test', 'tests'}
KEY_VALUE_RE = re.compile(r'^(?P<indent>[ \t]*)(?P<key>[^:#\s][^:#]*?)\s*:\s*(?P<value>.*)$')


def _strip_quotes(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ('"', "'"):
        return value[1:-1]
    return value


def _strip_inline_comment(value: str) -> str:
    in_quote: Optional[str] = None
    for index, ch in enumerate(value):
        if ch in ('"', "'"):
            if in_quote is None:
                in_quote = ch
            elif in_quote == ch:
                in_quote = None
        elif ch == '#' and in_quote is None and (index == 0 or value[index - 1].isspace()):
            return value[:index].rstrip()
    return value.strip()


def parse_upload_directories(workflow_path: Path) -> List[str]:
    """Extract semicolon-separated component directories from upload_component.yml."""
    lines = workflow_path.read_text(encoding='utf-8').splitlines()
    for idx, line in enumerate(lines):
        stripped = line.lstrip()
        if not stripped or stripped.startswith('#'):
            continue
        match = KEY_VALUE_RE.match(line)
        if not match or match.group('key').strip() != 'directories':
            continue

        base_indent = len(match.group('indent'))
        raw_value = _strip_inline_comment(match.group('value'))
        chunks: List[str] = []
        if raw_value in ('>', '>-', '>+', '|', '|-', '|+'):
            for subline in lines[idx + 1:]:
                sub_stripped = subline.strip()
                if not sub_stripped:
                    continue
                sub_indent = len(subline) - len(subline.lstrip())
                if sub_indent <= base_indent:
                    break
                if sub_stripped.startswith('#'):
                    continue
                chunks.append(_strip_inline_comment(sub_stripped))
        elif raw_value:
            chunks.append(_strip_quotes(raw_value))

        joined = ''.join(ch for ch in ''.join(chunks) if not ch.isspace())
        return [item.strip('/') for item in joined.split(';') if item.strip('/')]

    return []


def parse_component_manifest(manifest_path: Path) -> Tuple[Optional[str], Dict[str, Dict[str, Optional[str]]]]:
    """Parse top-level version and dependencies (with optional override_path) using stdlib only."""
    lines = manifest_path.read_text(encoding='utf-8').splitlines()
    version: Optional[str] = None
    dependencies: Dict[str, Dict[str, Optional[str]]] = {}

    in_dependencies = False
    dep_section_indent = -1
    dep_item_indent: Optional[int] = None
    dep_field_indent: Optional[int] = None
    current_dep: Optional[str] = None

    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith('#'):
            continue

        indent = len(line) - len(line.lstrip())
        if in_dependencies and indent <= dep_section_indent:
            in_dependencies = False
            dep_item_indent = None
            dep_field_indent = None
            current_dep = None

        if not in_dependencies:
            if indent == 0:
                match = KEY_VALUE_RE.match(line)
                if not match:
                    continue
                key = _strip_quotes(match.group('key').strip())
                raw_value = _strip_inline_comment(match.group('value'))
                if key == 'version':
                    version = _strip_quotes(raw_value)
                elif key == 'dependencies':
                    in_dependencies = True
                    dep_section_indent = indent
            continue

        if stripped.startswith('-'):
            continue

        match = KEY_VALUE_RE.match(line)
        if not match:
            continue

        if dep_item_indent is None:
            dep_item_indent = indent

        key = _strip_quotes(match.group('key').strip())
        raw_value = _strip_inline_comment(match.group('value'))

        if indent == dep_item_indent:
            current_dep = key
            dep_field_indent = None
            dependencies[current_dep] = {
                'version': _strip_quotes(raw_value) if raw_value else None,
                'override_path': None,
            }
        elif current_dep is not None and indent > dep_item_indent:
            if dep_field_indent is None:
                dep_field_indent = indent
            if indent == dep_field_indent:
                if key == 'override_path':
                    dependencies[current_dep]['override_path'] = _strip_quotes(raw_value)
                elif key == 'version':
                    dependencies[current_dep]['version'] = _strip_quotes(raw_value)

    return version, dependencies


def discover_repo_components(repo_root: Path) -> Dict[str, str]:
    """Return mapping from component name to repo-relative directory for top-level components."""
    components: Dict[str, str] = {}
    for manifest in sorted(repo_root.rglob('idf_component.yml')):
        rel_dir = manifest.parent.relative_to(repo_root)
        if any(part in EXCLUDED_DIR_NAMES or part.startswith('build_') for part in rel_dir.parts):
            continue
        components[rel_dir.name] = rel_dir.as_posix()
    return components


def _split_dep_name(dep_key: str) -> Tuple[str, str]:
    if '/' in dep_key:
        namespace, name = dep_key.split('/', 1)
        return namespace, name
    return '', dep_key


def check_upload_components(repo_root: Path, workflow_rel_path: Path = DEFAULT_WORKFLOW_REL_PATH) -> int:
    workflow_path = repo_root / workflow_rel_path
    if not workflow_path.is_file():
        print(f'ERROR: Upload workflow not found: {workflow_path}', file=sys.stderr)
        return 1

    upload_dirs = parse_upload_directories(workflow_path)
    if not upload_dirs:
        print(f'ERROR: No component directories found in {workflow_rel_path}', file=sys.stderr)
        return 1

    repo_components = discover_repo_components(repo_root)
    violations: List[str] = []

    seen_dirs: Dict[str, int] = {}
    name_to_index: Dict[str, int] = {}
    name_to_dir: Dict[str, str] = {}

    for idx, comp_dir in enumerate(upload_dirs):
        pos = idx + 1
        if comp_dir in seen_dirs:
            violations.append(f'Duplicate directory entry "{comp_dir}" at #{pos} (first seen at #{seen_dirs[comp_dir] + 1})')
            continue
        seen_dirs[comp_dir] = idx

        full_dir = repo_root / comp_dir
        manifest_path = full_dir / 'idf_component.yml'
        if not full_dir.is_dir():
            violations.append(f'#{pos} "{comp_dir}": directory does not exist')
            continue
        if not manifest_path.is_file():
            violations.append(f'#{pos} "{comp_dir}": missing idf_component.yml')
            continue

        comp_name = full_dir.name
        if comp_name in name_to_index:
            violations.append(
                f'Duplicate component name "{comp_name}" at #{pos} "{comp_dir}" '
                f'(already listed at #{name_to_index[comp_name] + 1} "{name_to_dir[comp_name]}")'
            )
            continue
        name_to_index[comp_name] = idx
        name_to_dir[comp_name] = comp_dir

    for idx, comp_dir in enumerate(upload_dirs):
        pos = idx + 1
        full_dir = repo_root / comp_dir
        manifest_path = full_dir / 'idf_component.yml'
        if not manifest_path.is_file():
            continue

        version, dependencies = parse_component_manifest(manifest_path)
        if not version:
            violations.append(f'#{pos} "{comp_dir}": idf_component.yml is missing a version field')

        for dep_key, dep_meta in dependencies.items():
            if dep_key == 'idf':
                continue

            namespace, dep_name = _split_dep_name(dep_key)
            override_path = dep_meta.get('override_path')
            is_internal = (
                override_path is not None
                or (namespace in ('', 'espressif') and (dep_name in repo_components or dep_name.startswith('brookesia_')))
            )
            if not is_internal:
                continue

            if dep_name not in name_to_index:
                hint = f' (found in repo at "{repo_components[dep_name]}")' if dep_name in repo_components else ''
                violations.append(
                    f'#{pos} "{comp_dir}" depends on "{dep_key}", which is missing from {workflow_rel_path}{hint}'
                )
                continue

            dep_idx = name_to_index[dep_name]
            dep_dir = name_to_dir[dep_name]
            if dep_idx >= idx:
                violations.append(
                    f'#{pos} "{comp_dir}" depends on "{dep_key}" (#{dep_idx + 1} "{dep_dir}"), '
                    f'which must appear earlier in {workflow_rel_path}'
                )

            if override_path is not None:
                resolved_override = (full_dir / override_path).resolve()
                expected_dir = (repo_root / dep_dir).resolve()
                if resolved_override != expected_dir:
                    violations.append(
                        f'#{pos} "{comp_dir}" dependency "{dep_key}" has override_path "{override_path}" '
                        f'resolving to "{resolved_override}", expected "{expected_dir}"'
                    )

    if violations:
        print(f'Component upload workflow validation failed ({len(violations)} issue(s)):', file=sys.stderr)
        for violation in violations:
            print(f'  - {violation}', file=sys.stderr)
        return 1

    print(f'Upload component workflow check passed ({len(upload_dirs)} components)')
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
