# ChangeLog

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

* feat(examples): 'ESP_Brookesia_Phone' add partition requirement
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
* fix(examples): disable 'ESP_Brookesia_Phone' external stylesheet by default
* fix(examples): fix 'esp-idf' CMakeLists.txt to use custom memory functions
* fix(app_examples): modify 'squareline' ui to use LV_FONT_DEFAULT

## v0.1.0 - 2024-08-12

### Enhancements:

* feat(repo): init project
* feat(systems): add 'Phone' system
* feat(widgets): support 'Status Bar' and 'Navigation Bar' visual mode
