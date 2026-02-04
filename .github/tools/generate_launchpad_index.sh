#!/bin/bash
# Copyright 2022 Espressif Systems (Shanghai) PTE LTD
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script generates the index.html page for ESP-Brookesia Launchpad
# Usage: generate_launchpad_index.sh <git_remote> <output_dir>
# Example: generate_launchpad_index.sh espressif/esp-brookesia ./images

git_remote="$1"
output_dir="${2:-.}"

# Build base URL
base_url="$(echo $git_remote | sed 's|\(.*\)/\(.*\)|https://\1.github.io/\2/|')"
launchpad_base="https://espressif.github.io/esp-launchpad/?flashConfigURL="

OUT_FILE="$output_dir/index.html"

cat <<EOF > $OUT_FILE
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP-Brookesia Launchpad</title>
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
      max-width: 900px;
      margin: 0 auto;
      padding: 40px 20px;
      background: #f5f5f5;
    }
    h1 {
      color: #1a1a1a;
      text-align: center;
      margin-bottom: 10px;
    }
    .subtitle {
      text-align: center;
      color: #666;
      margin-bottom: 40px;
    }
    .cards {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 20px;
    }
    .card {
      background: white;
      border-radius: 12px;
      padding: 24px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.1);
      transition: transform 0.2s, box-shadow 0.2s;
    }
    .card:hover {
      transform: translateY(-4px);
      box-shadow: 0 4px 16px rgba(0,0,0,0.15);
    }
    .card h2 {
      margin: 0 0 12px 0;
      color: #333;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .card p {
      color: #666;
      margin: 0 0 16px 0;
      line-height: 1.5;
    }
    .button {
      display: inline-block;
      background: linear-gradient(135deg, #e74c3c, #c0392b);
      color: white;
      padding: 12px 24px;
      text-decoration: none;
      border-radius: 8px;
      font-weight: 500;
      transition: opacity 0.2s;
    }
    .button:hover {
      opacity: 0.9;
    }
    .button.blue {
      background: linear-gradient(135deg, #3498db, #2980b9);
    }
    .footer {
      text-align: center;
      margin-top: 40px;
      color: #999;
      font-size: 14px;
    }
    .footer a {
      color: #666;
    }
  </style>
</head>
<body>
  <h1>🚀 ESP-Brookesia Launchpad</h1>
  <p class="subtitle">One-click flash firmware to your ESP32 device via Web Serial</p>

  <div class="cards">
    <div class="card">
      <h2>🔊 Speaker</h2>
      <p>ESP-Brookesia Speaker product firmware. A complete AI voice assistant solution.</p>
      <a class="button" href="${launchpad_base}${base_url}speaker/launchpad.toml">Flash Speaker</a>
    </div>

    <div class="card">
      <h2>🎛️ Service Console</h2>
      <p>Interactive console for testing ESP-Brookesia services. Supports multiple development boards.</p>
      <a class="button blue" href="${launchpad_base}${base_url}service_console/launchpad.toml">Flash Service Console</a>
    </div>
  </div>

  <div class="footer">
    <p>Powered by <a href="https://github.com/espressif/esp-launchpad">ESP-Launchpad</a> |
       <a href="https://github.com/${git_remote}">GitHub Repository</a></p>
  </div>
</body>
</html>
EOF

echo "Generated $OUT_FILE"
