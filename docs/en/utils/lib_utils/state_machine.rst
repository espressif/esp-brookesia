.. _utils-lib_utils-state_machine-sec-00:

State Machine
=============

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/lib_utils/state_machine.hpp"``

.. _utils-lib_utils-state_machine-sec-01:

Overview
--------

`state_machine` implements registration, actions, transition callbacks, and runtime control for
multi-state workflows.

.. _utils-lib_utils-state_machine-sec-02:

Features
--------

- Add states and transition rules
- Initial state and forced switches
- Transition callbacks and runtime queries
- Optional integration with `TaskScheduler` for async work

.. _utils-lib_utils-state_machine-sec-03:

API reference
---------------

.. include-build-file:: inc/utils/brookesia_lib_utils/include/brookesia/lib_utils/state_machine.inc
