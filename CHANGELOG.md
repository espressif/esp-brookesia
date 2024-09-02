# ChangeLog

## v0.2.1 - 2024-09-02

### Enhancements:

* feat(core): add support for getting app visual area post-installation
* feat(core): improve app resource clean operations

### Bugfixes:

* fix(docs): fix the video that cannot be previewed
* fix(version): fix the macro definitions that are out of sync with the version
* fix(app_examples): fix the incorrect version in the 'complex_conf' yml file by @isthaison (#8)
* fix(phone): set default app launcher icon when no custom icon is provided
* fix(status_bar): use correct state when set battery charging flag
* fix(phone): correct the typo from 'drak' to 'dark'

## v0.2.0 - 2024-08-20

### Enhancements:

* feat(examples): 'ESP_UI_Phone' add partition requirement
* feat(examples): 'esp-idf' update stylesheet
* feat(examples): 'esp-idf' add esp_ui_phone_s3_box
* feat(examples): 'esp-idf' add esp_ui_phone_s3_box_3
* feat(examples): 'esp-idf' add esp_ui_phone_m5stace_core_s3
* feat(widgets): 'Navigation Bar' support flex visual mode
* feat(widgets): 'Gesture' support indicator bar and avoid accidental touch
* feat(core_app): improve resource clean and screen resize operations

## v0.1.1~1 - 2024-08-16

### Bugfixes:

* fix(idf): bump version to fix idf component build issue

## v0.1.1 - 2024-08-12

### Enhancements:

* feat(examples): replace 'esp-idf' stylesheet with Espressif Registry package

### Bugfixes:

* fix(core & recents_app): fix build missing LV_USE_SNAPSHOT error
* fix(examples): correct 'esp_ui_phone_s3_lcd_ev_board' README resolution
* fix(examples): disable 'ESP_UI_Phone' external stylesheet by default
* fix(examples): fix 'esp-idf' CMakeLists.txt to use custom memory functions
* fix(app_examples): modify 'squareline' ui to use LV_FONT_DEFAULT

## v0.1.0 - 2024-08-12

### Enhancements:

* feat(repo): init project
* feat(systems): add 'Phone' system
* feat(widgets): support 'Status Bar' and 'Navigation Bar' visual mode
