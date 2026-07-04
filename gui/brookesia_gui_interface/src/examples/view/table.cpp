/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.table: table with rows, columns and cells.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_TABLE
class ViewTableExample final : public detail::JsonExample {
public:
    ViewTableExample()
        : JsonExample(
              ExampleInfo{"view.table", "View", "Table", "Table with rows, columns and cells"},
              "examples/view/table.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_table",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "table",
                    "id": "table",
                    "tableProps": {
                        "rows": 2,
                        "columns": 2,
                        "cells": [
                            { "row": 0, "column": 0, "text": "Name" },
                            { "row": 0, "column": 1, "text": "Value" },
                            { "row": 1, "column": 0, "text": "Temp" },
                            { "row": 1, "column": 1, "text": "25C" }
                        ]
                    },
                    "placement": { "mode": "relative", "align": "center", "width": "220dp", "height": "100dp" }
                }
            ]
        }
    ]
})json",
              "/view_table")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.require_view("table");
        checker.check_frame_nonempty("table");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewTableExample, "view.table");
#endif

} // namespace esp_brookesia::gui::examples
