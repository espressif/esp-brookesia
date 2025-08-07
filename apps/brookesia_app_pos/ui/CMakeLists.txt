SET(SOURCES screens/ui_Screen_POS_S1.c
    screens/ui_Screen_POS_S2.c
    screens/ui_Screen_POS_S3.c
    screens/ui_Screen_POS_S4.c
    screens/ui_Screen_POS_S5.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_img_bg_1_png.c
    images/ui_img_img_app_pos_png.c
    images/ui_img_img_arrow_right_png.c
    images/ui_img_img_btc_png.c
    images/ui_img_img_card_png.c
    images/ui_img_img_qr_png.c
    images/ui_img_ok_png.c
    fonts/ui_font_Big_Number.c
    fonts/ui_font_Bold_Font.c)

add_library(ui ${SOURCES})
