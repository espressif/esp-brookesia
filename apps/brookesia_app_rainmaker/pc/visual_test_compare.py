#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""
Visual regression test comparison script for RainMaker UI.

Compares screenshots captured at different resolutions against baseline images.
Supports PPM, PNG, and other common image formats.

Usage:
    python3 visual_test_compare.py baseline_dir output_dir [options]

Examples:
    python3 visual_test_compare.py visual_test_baseline visual_test_output
    python3 visual_test_compare.py visual_test_baseline visual_test_output --threshold 0.95
"""

import argparse
from html import escape as html_escape
import os
import shutil
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple
import subprocess

try:
    from PIL import Image, ImageChops, ImageStat
    HAS_PIL = True
except ImportError:
    HAS_PIL = False
    print("Warning: Pillow not installed. Install with: pip3 install Pillow")
    print("Falling to basic file comparison...")

# Supported screens
SCREENS = [
    "Login", "Home", "Schedules", "CreateSchedule", "CreateScene",
    "SelectDevices", "SelectActions", "Scenes", "User",
    "LightDetail", "SwitchDetail", "FanDetail", "DeviceSetting"
]

LANGUAGES = ["en", "zh"]

# Default test resolutions
RESOLUTIONS = [
    (320, 240),
    (480, 272),
    (800, 480),
    (1024, 600),
    (1280, 720),
    (1920, 1080),
]


def find_screenshot_files(directory: str, width: int, height: int) -> Dict[str, str]:
    """Find screenshot files for a given resolution.

    Uses exact filenames (no glob) so names like ``*.png.ppm`` match reliably
    and different resolutions do not collide.
    """
    files: Dict[str, str] = {}
    dir_path = Path(directory)
    if not dir_path.is_dir():
        return files

    for screen in SCREENS:
        for lang in LANGUAGES:
            key = f"{screen}_{lang}"
            base = f"{width}x{height}_{screen}_{lang}"
            for name in (
                f"{base}.png",
                f"{base}.png.ppm",
                f"{base}.ppm",
            ):
                p = dir_path / name
                if p.is_file():
                    files[key] = str(p)
                    break
    return files


def load_image(path: str) -> Image.Image:
    """Load an image file."""
    if not HAS_PIL:
        return None
    return Image.open(path)


def ensure_png_for_html(img_path: str) -> str:
    """Return an absolute path to a PNG usable in ``<img>`` for HTML reports.

    Browsers do not reliably render PPM in ``<img>`` (including names like
    ``1024x600_Login_en.png.ppm``). For paths ending in ``.ppm``, write a ``.png``
    beside the source (strip the trailing ``.ppm``) and return that path.
    The PNG is regenerated when missing or older than the PPM.
    """
    path = os.path.abspath(img_path)
    if not HAS_PIL or not path.lower().endswith('.ppm'):
        return path
    # Strip ".ppm". Names like foo.png.ppm → foo.png; plain foo.ppm → foo.png
    base = path[:-4]
    png_path = base if base.lower().endswith('.png') else base + '.png'
    try:
        stale = not os.path.isfile(png_path)
        if not stale:
            stale = os.path.getmtime(path) > os.path.getmtime(png_path)
        if stale:
            with Image.open(path) as im:
                out = im.convert('RGB') if im.mode not in ('RGB', 'RGBA') else im
                out.save(png_path, format='PNG')
    except OSError:
        return path
    return png_path


def compare_images(
    baseline: Image.Image,
    current: Image.Image,
    threshold: float = 0.99
) -> Tuple[bool, float, Image.Image]:
    """
    Compare two images and return (passed, similarity, diff_image).

    Returns:
        (bool, float, Image): (whether test passed, similarity score 0-1, diff image)
    """
    if not baseline or not current:
        return False, 0.0, None

    # Ensure images are same size
    if baseline.size != current.size:
        # Resize current to match baseline
        current = current.resize(baseline.size)

    # Calculate difference
    diff = ImageChops.difference(baseline, current)

    # Get statistics of the difference
    stat = ImageStat.Stat(diff)

    # Mean difference (0 = identical, 255 = completely different)
    mean_diff = stat.mean[0]  # RGB mean

    # Calculate similarity (1.0 = identical, 0.0 = completely different)
    similarity = 1.0 - (mean_diff / 255.0)

    # Create colored diff image for visualization
    # Red areas show differences
    diff_colored = diff.convert("RGB")
    pixels = diff_colored.load()

    for i in range(diff_colored.size[0]):
        for j in range(diff_colored.size[1]):
            pixel = pixels[i, j]
            if sum(pixel) > 30:  # If significant difference
                pixels[i, j] = (255, 0, 0)  # Red
            elif sum(pixel) > 10:
                pixels[i, j] = (255, 255, 0)  # Yellow

    passed = similarity >= threshold
    return passed, similarity, diff_colored


def generate_html_report(
    results: List[Dict],
    report_path: str,
    threshold: float
) -> str:
    """Generate an HTML report.

    Image ``src`` paths are relative to the report file's directory so that
    opening the HTML via file:// or http:// resolves images correctly.
    PPM sources get a companion PNG for thumbnails because browsers do not
    display PPM in ``<img>``.
    """
    report_dir = os.path.dirname(os.path.abspath(report_path)) or os.getcwd()
    total = len(results)
    passed = sum(1 for r in results if r['passed'])
    failed = total - passed

    html = f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>RainMaker Visual Test Report</title>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background: #f5f5f5;
        }}
        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            border-radius: 10px;
            margin-bottom: 30px;
        }}
        .summary {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-top: 20px;
        }}
        .summary-item {{
            background: rgba(255,255,255,0.2);
            padding: 20px;
            border-radius: 8px;
            text-align: center;
        }}
        .summary-item h3 {{
            margin: 0 0 10px 0;
            font-size: 32px;
        }}
        .summary-item p {{
            margin: 0;
            opacity: 0.9;
        }}
        .resolution-section {{
            margin-bottom: 40px;
            background: white;
            border-radius: 10px;
            overflow: hidden;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }}
        .resolution-header {{
            background: #4a5568;
            color: white;
            padding: 20px;
            font-size: 18px;
            font-weight: bold;
        }}
        .test-row {{
            display: flex;
            padding: 15px 20px;
            border-bottom: 1px solid #e2e8f0;
            align-items: center;
        }}
        .test-row:last-child {{
            border-bottom: none;
        }}
        .test-row:hover {{
            background: #f7fafc;
        }}
        .test-info {{
            flex: 1;
        }}
        .test-name {{
            font-weight: 500;
            color: #2d3748;
        }}
        .test-lang {{
            color: #718096;
            font-size: 12px;
            text-transform: uppercase;
            margin-top: 4px;
        }}
        .similarity {{
            text-align: center;
            padding: 8px 20px;
            border-radius: 6px;
            font-weight: 500;
            min-width: 80px;
        }}
        .similarity.high {{
            background: #c6f6d5;
            color: #22543d;
        }}
        .similarity.medium {{
            background: #fefcbf;
            color: #744210;
        }}
        .similarity.low {{
            background: #fed7d7;
            color: #822727;
        }}
        .status {{
            text-align: center;
            padding: 8px 20px;
            border-radius: 6px;
            font-weight: 600;
            min-width: 80px;
        }}
        .status.pass {{
            background: #c6f6d5;
            color: #22543d;
        }}
        .status.fail {{
            background: #fed7d7;
            color: #822727;
        }}
        .status.skip {{
            background: #e2e8f0;
            color: #4a5568;
        }}
        .thumb-missing {{
            display: inline-flex;
            align-items: center;
            justify-content: center;
            width: 100px;
            height: 75px;
            border-radius: 4px;
            border: 1px dashed #cbd5e0;
            color: #718096;
            font-size: 12px;
        }}
        .test-note {{
            color: #c05621;
            font-size: 12px;
            margin-top: 6px;
        }}
        .thumbnails {{
            display: flex;
            gap: 10px;
            margin-left: 20px;
        }}
        .thumbnail {{
            width: 100px;
            height: 75px;
            object-fit: contain;
            border-radius: 4px;
            border: 1px solid #e2e8f0;
            cursor: pointer;
            transition: transform 0.2s;
        }}
        .thumbnail:hover {{
            transform: scale(1.05);
            border-color: #667eea;
        }}
        .modal {{
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0,0,0,0.9);
            z-index: 1000;
            align-items: center;
            justify-content: center;
        }}
        .modal.show {{
            display: flex;
        }}
        .modal-content {{
            max-width: 90%;
            max-height: 90%;
            border-radius: 10px;
            overflow: hidden;
        }}
        .modal-close {{
            position: absolute;
            top: 20px;
            right: 20px;
            color: white;
            font-size: 30px;
            cursor: pointer;
            background: rgba(0,0,0,0.5);
            width: 50px;
            height: 50px;
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
        }}
    </style>
</head>
<body>
    <div class="header">
        <h1>RainMaker Visual Test Report</h1>
        <div class="summary">
            <div class="summary-item">
                <h3>{total}</h3>
                <p>Total Tests</p>
            </div>
            <div class="summary-item">
                <h3>{passed}</h3>
                <p>Passed</p>
            </div>
            <div class="summary-item">
                <h3>{failed}</h3>
                <p>Failed</p>
            </div>
            <div class="summary-item">
                <h3>{threshold*100:.1f}%</h3>
                <p>Threshold</p>
            </div>
        </div>
    </div>
"""

    # Group results by resolution
    by_resolution: Dict[str, List[Dict]] = {}
    for r in results:
        key = f"{r['width']}x{r['height']}"
        if key not in by_resolution:
            by_resolution[key] = []
        by_resolution[key].append(r)

    # Generate report for each resolution
    for res_key in sorted(by_resolution.keys()):
        html += f'<div class="resolution-section">\n'
        html += f'<div class="resolution-header">{res_key}</div>\n'

        for r in by_resolution[res_key]:
            if r.get('status') == 'MISSING':
                sim_class = 'low'
                sim_text = '—'
                status_class = 'skip'
                status_text = 'MISSING'
            else:
                sim_class = 'high' if r['similarity'] >= 0.99 else ('medium' if r['similarity'] >= 0.95 else 'low')
                sim_text = f"{r['similarity']*100:.2f}%"
                status_class = 'pass' if r['passed'] else 'fail'
                status_text = 'PASS' if r['passed'] else 'FAIL'

            note_html = ''
            if r.get('note'):
                note_html = f'<div class="test-note">{html_escape(r["note"])}</div>'

            def _thumb(path: Optional[str], title: str) -> str:
                if not path:
                    return f'<span class="thumb-missing" title="{title}">无</span>'
                src = os.path.relpath(ensure_png_for_html(path), report_dir)
                return (
                    f'<img src="{src}" class="thumbnail" title="{title}" '
                    f'onclick="showImage(this.src)">'
                )

            baseline_thumb = _thumb(r.get('baseline'), 'Baseline')
            current_thumb = _thumb(r.get('current'), 'Current')

            html += f'''
    <div class="test-row">
        <div class="test-info">
            <div class="test-name">{r['screen']}</div>
            <div class="test-lang">{r['lang']}</div>
            {note_html}
        </div>
        <div class="similarity {sim_class}">{sim_text}</div>
        <div class="status {status_class}">{status_text}</div>
        <div class="thumbnails">
            {baseline_thumb}
            {current_thumb}'''

            if r.get('diff'):
                diff_src = os.path.relpath(os.path.abspath(r['diff']), report_dir)
                html += f'<img src="{diff_src}" class="thumbnail" title="Difference" onclick="showImage(this.src)">'

            html += '''
        </div>
    </div>'''

        html += '</div>\n'

    html += '''
    <div id="modal" class="modal" onclick="closeModal(event)">
        <div class="modal-close" onclick="closeModal(event)">&times;</div>
        <img id="modalImg" class="modal-content">
    </div>
    <script>
        function showImage(src) {
            document.getElementById('modalImg').src = src;
            document.getElementById('modal').classList.add('show');
        }
        function closeModal(e) {
            if (e.target !== document.getElementById('modalImg')) {
                document.getElementById('modal').classList.remove('show');
            }
        }
    </script>
</body>
</html>'''

    return html


def main():
    parser = argparse.ArgumentParser(
        description='Compare visual regression test screenshots'
    )
    parser.add_argument(
        'baseline_dir',
        help='Directory containing baseline screenshots'
    )
    parser.add_argument(
        'output_dir',
        help='Directory containing current test screenshots'
    )
    parser.add_argument(
        '--threshold', '-t',
        type=float,
        default=0.99,
        help='Similarity threshold for passing (default: 0.99)'
    )
    parser.add_argument(
        '--diff-dir', '-d',
        default='visual_test_diff',
        help='Directory for difference images (default: visual_test_diff)'
    )
    parser.add_argument(
        '--report', '-r',
        default='visual_test_report.html',
        help='Output HTML report file (default: visual_test_report.html)'
    )
    parser.add_argument(
        '--resolutions',
        default=None,
        help='Comma-separated resolutions to test (e.g., "800x480,1024x600")'
    )
    parser.add_argument(
        '--convert',
        action='store_true',
        help='Convert PPM files to PNG using ImageMagick if available'
    )

    args = parser.parse_args()

    if not HAS_PIL:
        print("Error: Pillow is required for image comparison.")
        print("Install with: pip3 install Pillow")
        return 1

    # Parse resolutions
    if args.resolutions:
        try:
            resolutions = []
            for r in args.resolutions.split(','):
                w, h = map(int, r.strip().split('x'))
                resolutions.append((w, h))
        except (ValueError, AttributeError) as e:
            print(f"Error parsing resolutions: {e}")
            return 1
    else:
        resolutions = RESOLUTIONS

    # Fresh diff directory so old *_diff.png files from prior runs are not left behind
    diff_dir = Path(args.diff_dir)
    if diff_dir.exists():
        shutil.rmtree(diff_dir)
    diff_dir.mkdir(parents=True, exist_ok=True)

    # Convert PPM to PNG if requested
    if args.convert:
        try:
            subprocess.run(['convert', '-version'], capture_output=True, check=True)
            has_imagemagick = True
        except (subprocess.CalledProcessError, FileNotFoundError):
            has_imagemagick = False
            print("ImageMagick not found, skipping PPM conversion")

        if has_imagemagick:
            print("Converting PPM files to PNG...")
            for ppm_file in Path(args.output_dir).glob('*.ppm'):
                png_file = ppm_file.with_suffix('.png')
                subprocess.run(['convert', str(ppm_file), str(png_file)], capture_output=True)

    # Collect results: every (resolution × screen × language) gets a row
    results = []
    compare_total = 0
    compare_passed = 0

    for w, h in resolutions:
        print(f"\nTesting {w}x{h}...")

        baseline_files = find_screenshot_files(args.baseline_dir, w, h)
        current_files = find_screenshot_files(args.output_dir, w, h)

        for screen in SCREENS:
            for lang in LANGUAGES:
                key = f"{screen}_{lang}"
                baseline_path = baseline_files.get(key)
                current_path = current_files.get(key)

                if not baseline_path and not current_path:
                    print(f"  ⚠️  {screen} ({lang}): 无 baseline 且无 current")
                    results.append({
                        'width': w,
                        'height': h,
                        'screen': screen,
                        'lang': lang,
                        'baseline': None,
                        'current': None,
                        'similarity': 0.0,
                        'passed': False,
                        'diff': None,
                        'note': '缺少 baseline 与 current 截图',
                        'status': 'MISSING',
                    })
                    continue

                if not baseline_path and current_path:
                    print(f"  ⚠️  {screen} ({lang}): 无 baseline，从 current 复制…")
                    baseline_file = Path(args.baseline_dir) / Path(current_path).name
                    baseline_file.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(current_path, baseline_file)
                    baseline_path = str(baseline_file)
                    baseline_files[key] = baseline_path

                if not current_path:
                    print(f"  ⚠️  {screen} ({lang}): 无 current 截图")
                    results.append({
                        'width': w,
                        'height': h,
                        'screen': screen,
                        'lang': lang,
                        'baseline': baseline_path,
                        'current': None,
                        'similarity': 0.0,
                        'passed': False,
                        'diff': None,
                        'note': '缺少 current 截图',
                        'status': 'MISSING',
                    })
                    continue

                compare_total += 1
                diff_path: Optional[Path] = None

                try:
                    baseline_img = load_image(baseline_path)
                    current_img = load_image(current_path)
                    passed, similarity, diff_img = compare_images(
                        baseline_img,
                        current_img,
                        args.threshold
                    )

                    if passed:
                        compare_passed += 1
                        print(f"  ✓ {screen} ({lang}): {similarity*100:.2f}%")
                    else:
                        print(f"  ✗ {screen} ({lang}): {similarity*100:.2f}% (FAILED)")

                    if not passed and diff_img:
                        diff_filename = f"{w}x{h}_{screen}_{lang}_diff.png"
                        diff_path = diff_dir / diff_filename
                        diff_img.save(diff_path)

                    results.append({
                        'width': w,
                        'height': h,
                        'screen': screen,
                        'lang': lang,
                        'baseline': baseline_path,
                        'current': current_path,
                        'similarity': similarity,
                        'passed': passed,
                        'diff': str(diff_path) if diff_path else None,
                    })

                except Exception as e:
                    print(f"  ❌ {screen} ({lang}): Error comparing - {e}")
                    results.append({
                        'width': w,
                        'height': h,
                        'screen': screen,
                        'lang': lang,
                        'baseline': baseline_path,
                        'current': current_path,
                        'similarity': 0.0,
                        'passed': False,
                        'diff': None,
                        'note': str(e),
                    })

    # Generate report
    print(f"\n{'='*50}")
    print(f"Compared: {compare_passed}/{compare_total} passed")
    print(f"Report rows: {len(results)} (含缺失项)")
    print(f"{'='*50}")

    html = generate_html_report(
        results,
        args.report,
        args.threshold
    )

    with open(args.report, 'w') as f:
        f.write(html)

    print(f"\nHTML report generated: {args.report}")

    all_passed = all(r['passed'] for r in results)
    return 0 if all_passed else 1


if __name__ == '__main__':
    sys.exit(main())
