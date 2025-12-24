from typing import Any
import click
import subprocess, sys
import os
from pathlib import Path
import yaml
import shutil
import importlib.util

IDF_PATH = os.getenv("IDF_PATH", "")

def get_target_name(**kwargs) -> str:
    yml_path = f"{kwargs['customer_path']}/{kwargs['board']}/board_info.yaml"
    with open(yml_path, encoding="utf-8") as f:
        data = yaml.safe_load(f)
    return data["chip"]

def load_from_path(file_path, module_name=None):
    """file_path: 任意 .py 文件；返回模块对象"""
    module_name = module_name or file_path.stem
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    if spec is None or spec.loader is None:
        raise ImportError(f'无法为 {file_path} 创建 spec')
    mod = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = mod
    spec.loader.exec_module(mod)
    return mod

def action_extensions(base_actions: dict, project_path: str) -> dict:

    def esp_gen_bmgr_config_callback(target_name: str, ctx, args, **kwargs) -> None:

        shutil.rmtree("components/gen_bmgr_codes", ignore_errors=True)

        bmgr = Path("managed_components/espressif__esp_board_manager")
        if not bmgr.exists():
            subprocess.run([sys.executable, f"{IDF_PATH}/tools/idf.py", "set-target", get_target_name(**kwargs)])

        file_path = bmgr / "idf_ext.py"
        bmgr_config = load_from_path(file_path)
        act = bmgr_config.action_extensions(base_actions, project_path)
        act["actions"]["gen-bmgr-config"]["callback"](target_name, ctx, args, **kwargs)

    # Define command options
    gen_bmgr_config_options = [
        {
            "names": ["-l", "--list-boards"],
            "help": "List all available boards and exit",
            "is_flag": True,
        },
        {
            "names": ["-b", "--board"],
            "help": "Specify board name directly (bypasses sdkconfig reading)",
            "type": str,
        },
        {
            "names": ["-c", "--customer-path", "--custom"],
            "help": 'Path to customer boards directory (use "NONE" to skip)',
            "type": str,
        },
        {
            "names": ["--peripherals-only"],
            "help": "Only process peripherals (skip devices)",
            "is_flag": True,
        },
        {
            "names": ["--devices-only"],
            "help": "Only process devices (skip peripherals)",
            "is_flag": True,
        },
        {
            "names": ["--kconfig-only"],
            "help": "Generate Kconfig menu system for board and component selection (default enabled)",
            "is_flag": True,
        },
        {
            "names": ["--sdkconfig-only"],
            "help": "Only process sdkconfig features without generating Kconfig",
            "is_flag": True,
        },
        {
            "names": ["--disable-sdkconfig-auto-update"],
            "help": "Disable automatic sdkconfig feature enabling (default is enabled)",
            "is_flag": True,
        },
        {
            "names": ["--log-level"],
            "help": "Set the log level (DEBUG, INFO, WARNING, ERROR)",
            "type": str,
            "default": "INFO",
        },
    ]
    # Define the actions
    esp_actions = {
        "actions": {
            "gen-bmgr-config": {
                "callback": esp_gen_bmgr_config_callback,
                "options": gen_bmgr_config_options,
                "short_help": "Generate ESP Board Manager configuration files",
                "help": """Generate ESP Board Manager configuration files for board peripherals and devices.

This command generates C configuration files based on YAML configuration files in the board directories. It can process peripherals, devices, generate Kconfig menus, and update SDK configuration automatically.

For usage examples, see the README.md file.""",
            },
        }
    }

    return esp_actions
