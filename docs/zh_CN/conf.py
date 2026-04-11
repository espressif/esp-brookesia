# -*- coding: utf-8 -*-
#
# Chinese language Sphinx configuration.
#

import datetime
import os
import sys

sys.path.insert(0, os.path.abspath("../"))
from conf_common import *  # noqa: F403,F401

current_year = datetime.datetime.now().year

project = "ESP-Brookesia 编程指南"
copyright = f"{current_year}, 乐鑫信息科技（上海）股份有限公司"
pdf_title = "ESP-Brookesia 编程指南"
language = "zh_CN"

latex_documents = [
    (
        "index",
        "ReadtheDocsTemplate.tex",
        "ESP-Brookesia",
        f"{current_year}, 乐鑫信息科技（上海）股份有限公司",
        "manual",
    ),
]
