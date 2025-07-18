SET(SOURCES screens/ui_Screen_watch_digital.c
    screens/ui_Screen_watch_analog.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_flash_png.c
    images/ui_img_step_png.c
    images/ui_img_daily_mission_png.c
    images/ui_img_sec_dot_png.c
    images/ui_img_clockwise_sec_png.c
    images/ui_img_clockwise_min_png.c
    images/ui_img_clockwise_hour_png.c
    fonts/ui_font_H1.c
    fonts/ui_font_Number_big.c
    fonts/ui_font_Number_extra.c
    fonts/ui_font_Subtitle.c
    fonts/ui_font_Title.c)

add_library(ui ${SOURCES})
