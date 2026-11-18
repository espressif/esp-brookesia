var PACKAGE_VERSION = "0.1.0";
var outputId = 0;
var brightnessSubscription = 0;
var page = 0;
var cachedBrightness = null;
var pageRequestedAt = 0;

var KEY_OPEN_BRIGHTNESS = fnv1a("open_brightness");
var KEY_HEADER_BACK = fnv1a("header_back");
var KEY_EXIT = fnv1a("exit_app");
var KEY_BRIGHTNESS = fnv1a("brightness");

function fnv1a(name) {
    var key = 2166136261;
    for (var i = 0; i < name.length; i++) {
        key = Math.imul(key ^ name.charCodeAt(i), 16777619) >>> 0;
    }
    return key;
}

function callService(name, fn, params) {
    var raw = brookesia.call_service_function(name, fn, JSON.stringify(params || {}), 3000);
    return JSON.parse(raw);
}

function setText(name, text) {
    brookesia.gsp_set_text(name, String(text));
}

function setValue(name, value) {
    brookesia.gsp_set_value(name, value | 0);
}

function nowMs() {
    try {
        return Date.now();
    } catch (error) {
        return 0;
    }
}

function showPage(scene) {
    pageRequestedAt = nowMs();
    brookesia.print("page request", scene);
    brookesia.gsp_show_page(scene);
}

function paintCachedBrightness() {
    var text = (cachedBrightness === null || cachedBrightness === undefined) ? "--" : String(cachedBrightness | 0);
    if (page === 0) {
        setText("brightness_readback", "Brightness " + text + "%");
        return;
    }
    setText("brightness_value", text + "%");
    if (cachedBrightness !== null && cachedBrightness !== undefined) {
        setValue("brightness_slider", cachedBrightness | 0);
    }
}

function showStatus(text) {
    try {
        setText("service_status", text);
    } catch (error) {
        brookesia.print("status update failed", String(error));
    }
}

function rememberBrightness(value) {
    cachedBrightness = (value === null || value === undefined) ? null : (value | 0);
    paintCachedBrightness();
}

function findBacklightOutput() {
    var result = callService("Display", "GetOutputs", {});
    if (!result.success || !result.data) {
        showStatus(result.error || "GetOutputs failed");
        return false;
    }
    var outputs = result.data;
    for (var i = 0; i < outputs.length; i++) {
        if (outputs[i] && outputs[i].backlight) {
            outputId = outputs[i].id | 0;
            return true;
        }
    }
    showStatus("Display has no backlight");
    return false;
}

function readBrightness() {
    if (!outputId && !findBacklightOutput()) {
        rememberBrightness(null);
        return null;
    }
    var result = callService("Display", "GetBacklightBrightness", {OutputId: outputId});
    if (!result.success) {
        showStatus(result.error || "GetBacklightBrightness failed");
        rememberBrightness(null);
        return null;
    }
    var value = result.data | 0;
    brookesia.print("Display readback", PACKAGE_VERSION, value);
    showStatus("Package " + PACKAGE_VERSION + " readback " + value + "%");
    rememberBrightness(value);
    return value;
}

function writeBrightness(value) {
    var startedAt = nowMs();
    if (!outputId && !findBacklightOutput()) {
        return;
    }
    var clamped = value | 0;
    if (clamped < 10) {
        clamped = 10;
    }
    if (clamped > 100) {
        clamped = 100;
    }
    var written = callService("Display", "SetBacklightBrightness", {
        OutputId: outputId,
        Brightness: clamped
    });
    if (!written.success) {
        showStatus(written.error || "SetBacklightBrightness failed");
        return;
    }
    readBrightness();
    brookesia.print("slider readback refresh", nowMs() - startedAt);
}

function exitApp() {
    var active = callService("SystemCore", "GetActiveApp", {});
    var appId = active.success && active.data ? (active.data.app_id | 0) : 0;
    if (!appId) {
        showStatus("Cannot resolve this app");
        return;
    }
    var closed = callService("SystemCore", "RequestCloseApp", {AppId: appId});
    if (!closed.success) {
        showStatus(closed.error || "RequestCloseApp failed");
    }
}

function subscribeBrightness() {
    if (brightnessSubscription) {
        return;
    }
    brightnessSubscription = brookesia.subscribe_service_event("Display", "BacklightBrightnessChanged") | 0;
}

function unsubscribeBrightness() {
    if (!brightnessSubscription) {
        return;
    }
    try {
        brookesia.unsubscribe_event(brightnessSubscription);
    } catch (error) {
        brookesia.print("unsubscribe failed", String(error));
    }
    brightnessSubscription = 0;
}

globalThis.brookesia_app = {
    on_start: function() {
        page = 0;
        outputId = 0;
        try {
            subscribeBrightness();
            readBrightness();
        } catch (error) {
            brookesia.print("device control start failed", String(error));
            return false;
        }
        return true;
    },
    on_stop: function() {
        unsubscribeBrightness();
        return true;
    },
    on_action: function(action, path, payloadJson) {
        var payload = {};
        try {
            payload = JSON.parse(payloadJson || "{}");
        } catch (error) {
            showStatus("Bad GSP event");
            return false;
        }
        if (action === "gsp.scene") {
            var scene = payload.scene | 0;
            var elapsed = pageRequestedAt ? (nowMs() - pageRequestedAt) : 0;
            page = scene;
            pageRequestedAt = 0;
            brookesia.print("page ready", scene, elapsed);
            try {
                paintCachedBrightness();
            } catch (error) {
                brookesia.print("page refresh failed", String(error));
                return false;
            }
            return true;
        }
        if (action !== "gsp.call") {
            return true;
        }
        var key = payload.key >>> 0;
        try {
            if (key === KEY_OPEN_BRIGHTNESS) {
                brookesia.print("open brightness page");
                showPage(1);
            } else if (key === KEY_HEADER_BACK) {
                brookesia.print("back to status");
                showPage(0);
            } else if (key === KEY_EXIT) {
                brookesia.print("exit device control");
                exitApp();
            } else if (key === KEY_BRIGHTNESS) {
                brookesia.print("slider", payload.arg | 0);
                writeBrightness(payload.arg | 0);
            }
        } catch (error) {
            showStatus(String(error));
            return false;
        }
        return true;
    },
    on_event: function(serviceName, eventName, itemsJson) {
        if (serviceName !== "Display" || eventName !== "BacklightBrightnessChanged") {
            return true;
        }
        try {
            var items = JSON.parse(itemsJson || "{}");
            if (outputId && ((items.OutputId | 0) !== outputId)) {
                return true;
            }
            cachedBrightness = items.Brightness | 0;
            showStatus("Display event " + cachedBrightness + "%");
            paintCachedBrightness();
            brookesia.print("brightness event refresh", cachedBrightness);
        } catch (error) {
            brookesia.print("brightness event failed", String(error));
        }
        return true;
    }
};
