# -*- coding: utf-8 -*-
#
# English language Sphinx configuration.
#

import datetime
import os
import sys

sys.path.insert(0, os.path.abspath("../"))
from conf_common import *  # noqa: F403,F401

current_year = datetime.datetime.now().year

project = "ESP-Brookesia"
copyright = f"{current_year}, Espressif Systems (Shanghai) CO., LTD"
pdf_title = "ESP-Brookesia Programming Guide"
language = "en"

latex_documents = [
    (
        "index",
        "ReadtheDocsTemplate.tex",
        "ESP-Brookesia",
        f"{current_year}, Espressif Systems (Shanghai) CO., LTD",
        "manual",
    ),
]
