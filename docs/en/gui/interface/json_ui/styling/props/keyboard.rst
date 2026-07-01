.. _gui-interface-json-ui-styling-props-keyboard-sec-00:

Keyboardprops
==========================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-styling-props-keyboard-sec-01:

Overview
--------------------

``keyboardProps`` applies to ``keyboard``.

.. _gui-interface-json_ui-styling-props-keyboard-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/keyboard`

.. _gui-interface-json_ui-styling-props-keyboard-sec-03:

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Binding
     - UI Effect / Limits
   * - ``mode``
     - string
     - ``"text"``
     - ``keyboardProps.mode``
     - keyboard mode; the backend maps the string to a supported keyboard mode
   * - ``allowedModes``
     - string[]
     - ``[]``
     - ``keyboardProps.allowedModes``
     - modes allowed to switch to; the binding uses a comma string, such as ``"text,upper,number,special"``
   * - ``popovers``
     - bool
     - ``false``
     - ``keyboardProps.popovers``
     - whether to enable key popovers
   * - ``iconSize``
     - dimension
     - ``0``
     - ``keyboardProps.iconSize``
     - target icon size for keys with an ``image``; ``0`` draws at the image's original size
   * - ``targetTextInput``
     - string
     - ``""``
     - no
     - the target ``textInput`` path; when empty, the keyboard does not auto-type into the text box
   * - ``layouts``
     - object
     - ``{}``
     - no
     - custom layout table; mapped by the backend to the native keyboard map
   * - ``keyStyles``
     - object
     - ``{}``
     - no
     - key style classes; supports ``default`` / ``special`` / ``mode`` / ``action`` / ``disabled``
   * - ``keyStyleRefs``
     - object
     - ``{}``
     - no
     - mapping from key style class to theme style id; supports the same style classes as ``keyStyles``

.. _gui-interface-json_ui-styling-props-keyboard-sec-04:

Mode
--------------------

Current public values:

.. list-table::
   :header-rows: 1

   * - Value
     - Meaning
   * - ``text``
     - text lowercase keyboard
   * - ``upper``
     - text uppercase keyboard
   * - ``number``
     - numeric keypad
   * - ``special``
     - symbol / special-character keyboard

Only ``text``, ``upper``, ``number``, ``special`` modes are supported; an unknown mode raises an error at the parser/validator
stage. If ``allowedModes`` is non-empty and a ``role=mode`` target mode is not in the list, the key uses the disabled style and ignores clicks.

.. _gui-interface-json_ui-styling-props-keyboard-sec-05:

Targettextinput
------------------------------

``targetTextInput`` supports a screen absolute path and also a dotted path relative to the current screen root. Example:

.. code-block:: json

   "keyboardProps": {
       "targetTextInput": "/keyboard_input/panel/input"
   }

.. _gui-interface-json_ui-styling-props-keyboard-sec-06:

Layouts
--------------------

``layouts`` describes key rows in JSON, not runtime controls. It supports the ``text``, ``upper``, ``number``, ``special`` layouts. Each key can be a string or an object:

.. code-block:: json

   {
       "text": "q",
       "width": 1,
       "role": "",
       "mode": "",
       "styleClass": "",
       "image": "${image.icon.backspace}"
   }

A key with a ``role`` may also set ``text``. Here ``role`` decides the key behavior and ``text`` is the text fallback. If ``image`` is set, the backend
draws the corresponding image resource centered in the key draw-task and skips label drawing; it does not create per-key child controls and does not write the icon text into
``textInput``. The icon is scaled by ``keyboardProps.iconSize``; when the image is missing or fails to load, it falls back to showing ``text``.

Supported ``role``:

.. list-table::
   :header-rows: 1

   * - Role
     - Behavior
   * - ``backspace``
     - delete the previous character
   * - ``left`` / ``right``
     - move the cursor
   * - ``space``
     - space
   * - ``ok``
     - trigger ``ready``
   * - ``cancel``
     - trigger ``cancel``
   * - ``mode``
     - switch to the layout specified by the ``mode`` field

``width`` ranges from ``1..15`` and sets the key's relative width.

``keyStyles`` can configure key style classes:

.. code-block:: json

   "keyStyles": {
       "action": {
           "bgColor": "#2563eb",
           "textColor": "#ffffff",
           "pressedBgColor": "#1d4ed8",
           "pressedTextColor": "#ffffff",
           "radius": "8dp"
       }
   }

When ``styleClass`` is not written explicitly, the backend infers it by role: ordinary characters are ``default``, control keys are ``special``,
mode-switch keys are ``mode``, confirm/cancel are ``action``, and unavailable mode keys are ``disabled``.

``keyStyleRefs`` can reference named styles in the current theme. The runtime resolves key colors and corner radius from that style's ``bgColor``, ``textColor``, ``radius`` and
``stateStyles.pressed.bgColor``, ``stateStyles.pressed.textColor``. ``keyStyles`` can still serve as an
inline fallback; when the same style class appears in both places, the result from ``keyStyleRefs`` takes precedence.

.. _gui-interface-json_ui-styling-props-keyboard-sec-07:

Example
--------------------

.. code-block:: json

   "keyboardProps": {
       "mode": "text",
       "allowedModes": ["text", "upper", "number", "special"],
       "popovers": true,
       "iconSize": "${constant.ui.keyboard.iconSize}",
       "targetTextInput": "/keyboard_input/panel/input",
       "keyStyles": {
           "default": {"bgColor": "#ffffff", "textColor": "#111827"},
           "action": {"bgColor": "#2563eb", "textColor": "#ffffff"},
           "disabled": {"bgColor": "#e5e7eb", "textColor": "#9ca3af"}
       },
       "layouts": {
           "text": [
               ["q", "w", "e", {"text": "ABC", "role": "mode", "mode": "upper", "width": 2}],
               [
                   {"text": "Cancel", "role": "cancel", "width": 2, "image": "${image.icon.cancel}"},
                   {"text": "Space", "role": "space", "width": 6, "image": "${image.icon.space}"},
                   {"text": "OK", "role": "ok", "width": 2, "image": "${image.icon.ok}"}
               ]
           ],
           "upper": [
               ["Q", "W", "E", {"role": "backspace", "width": 2}]
           ],
           "number": [
               ["1", "2", "3", {"role": "ok", "width": 2}]
           ]
       }
   }
