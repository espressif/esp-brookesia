# ChangeLog

## v0.6.0-beta2 - 2025-07-23

### Enhancements:

* feat(ai_framework): agent support configure peripheral outside
* feat(systems): speaker prevents infinite audio playback
* feat(systems): speaker add invalid file animation
* feat(gui): animation player supports loading animation from file system

### Bugfixes:

* fix(ai_framework): prevent Agent from being awakened within apps
* fix(systems): shorten animation filename to avoid warning
* fix(services): only erase NVS namespace instead of full partition

## v0.6.0-beta1 - 2025-07-04

### Breaking Changes:

* break(component): rename from 'esp-brookesia' to 'brookesia_core', optimize code structure
* break(component): add new dependencies

### Enhancements:

* feat(component): add ai_framework and services
* feat(gui): add animation player
* feat(systems): add speaker

## v0.5.2 - 2025-06-27

### Enhancements:

* feat(examples): add echoear_factory_demo

## v0.5.1 - 2025-06-17

### Bugfixes:

* fix(examples): Spelling error for M5Stack CoreS33 @netseye (#27)
* fix(systems): update phone app_examples
* feat(ci): remove lib version check

## v0.5.0 - 2025-05-30

### Breaking Changes:

* feat(dependencies): upgrade LVGL dependency to v9.2
* feat(dependencies): upgrade IDF dependency to v5.3 and above
* feat(repo): remove Arduino support
* feat(docs): update library architecture diagram

## v0.4.3 - 2025-01-20

### Enhancements:

* feat(pre-commit): check lib versions

### Bugfixes:

* fix(status_bar): display 1-12 with AM/PM indicator @rysmith0315 (#19)

## v0.4.2 - 2024-12-11

### Enhancements:

* feat(assets): update large images
* feat(phone): support 24h format for clock of status bar
* feat(phone): disable manager gesture navigation back by default
* feat(phone): add stylesheet 720x1280
* feat(examples): update arduino Phone

### Bugfixes:

* fix(examples): PSRAM use QUAD mode for M5Stack CoreS3 @georgik (#16)

## v0.4.1 - 2024-10-25

### Enhancements:

* feat(assets): refactor assets and update stylesheet
* feat(phone): add default image for app launcher in stylesheet
* feat(phone): enable recents screen hide when no snapshot by default
* feat(widgets): avoid click the next page icon when the app launcher scroll is not finished

### Bugfixes:

* fix(examples): fix arduino build error

## v0.4.0 - 2024-10-23

### Enhancements:

* feat(core): support compilation on non-Unix/Linux systems
* feat(core): add event module
* feat(core): add lvgl lock & unlock
* feat(core): add stylesheet template
* feat(phone): public app function 'getPhone()'
* feat(phone): only change the app's visible area size when the status bar is completely opaque
* feat(phone): cancel navigation bar and enable gesture-based navigation by default
* feat(phone): add stylesheet 320x480, 800x1280, 1280x800
* feat(squareline): add ui_comp
* feat(examples): update bsp of esp_brookesia_phone_p4_function_ev_board

### Bugfixes:

* fix(cmake): remove unused code
* fix(conf): fix missing macros definition

## v0.3.1 - 2024-09-25

### Enhancements:

* feat(core): support compilation on non-Unix/Linux systems

## v0.3.0 - 2024-09-23

### Enhancements:

* feat(repo): rename repository from 'esp-ui' to 'esp-brookesia'
* feat(systems): add existing stylesheets for 'Phone'

### Bugfixes:

* fix(phone): resolve begin failure when no stylesheet is applied

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

* feat(examples): 'systems::phone::Phone' add partition requirement
* feat(examples): 'esp-idf' update stylesheet
* feat(examples): 'esp-idf' add esp_brookesia_phone_s3_box
* feat(examples): 'esp-idf' add esp_brookesia_phone_s3_box_3
* feat(examples): 'esp-idf' add esp_brookesia_phone_m5stace_core_s3
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
* fix(examples): correct 'esp_brookesia_phone_s3_lcd_ev_board' README resolution
* fix(examples): disable 'systems::phone::Phone' external stylesheet by default
* fix(examples): fix 'esp-idf' CMakeLists.txt to use custom memory functions
* fix(app_examples): modify 'squareline' ui to use LV_FONT_DEFAULT

## v0.1.0 - 2024-08-12

### Enhancements:

* feat(repo): init project
* feat(systems): add 'Phone' system
* feat(widgets): support 'Status Bar' and 'Navigation Bar' visual mode
