/*******************************************************************************
 * Size: 8 px
 * Bpp: 4
 * Opts: --bpp 4 --size 8 --no-compress --font MaisonNeue-Book.ttf --range 32-127 --format lvgl -o esp_brookesia_font_maison_neue_book_8.c
 ******************************************************************************/

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#ifndef ESP_BROOKESIA_FONT_MAISON_NEUE_BOOK_8
#define ESP_BROOKESIA_FONT_MAISON_NEUE_BOOK_8 1
#endif

#if ESP_BROOKESIA_FONT_MAISON_NEUE_BOOK_8

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0xa0, 0xa0, 0xa0, 0x60, 0xc0,

    /* U+0022 "\"" */
    0x48, 0x62, 0x53,

    /* U+0023 "#" */
    0x0, 0x90, 0x90, 0x8, 0xd9, 0xb5, 0x1, 0x84,
    0x50, 0x3a, 0xbb, 0x93, 0x7, 0x29, 0x0,

    /* U+0024 "$" */
    0x0, 0x60, 0x2, 0xab, 0xa2, 0x56, 0x61, 0x40,
    0x4b, 0x82, 0x62, 0x60, 0xa1, 0xab, 0x96, 0x0,
    0x30, 0x0,

    /* U+0025 "%" */
    0x0, 0x0, 0x2, 0x0, 0x39, 0xa1, 0x9, 0x0,
    0x72, 0x64, 0x82, 0x0, 0x18, 0x85, 0x68, 0x81,
    0x0, 0x19, 0x36, 0x27, 0x0, 0x91, 0xa, 0x93,
    0x0, 0x30, 0x0, 0x0,

    /* U+0026 "&" */
    0x9, 0x88, 0x0, 0xa, 0x1, 0x0, 0x19, 0x93,
    0x50, 0x73, 0x9, 0x80, 0x2a, 0x89, 0x50,

    /* U+0027 "'" */
    0x45, 0x23,

    /* U+0028 "(" */
    0x1, 0x60, 0x91, 0x19, 0x3, 0x60, 0x37, 0x0,
    0xa0, 0x6, 0x40, 0x3,

    /* U+0029 ")" */
    0x60, 0x2, 0x80, 0xa, 0x0, 0x82, 0x8, 0x20,
    0xa0, 0x55, 0x3, 0x0,

    /* U+002A "*" */
    0x26, 0x52, 0xc, 0xb0, 0x13, 0x40,

    /* U+002B "+" */
    0x0, 0x50, 0x0, 0x9, 0x0, 0x37, 0xc7, 0x10,
    0x9, 0x0,

    /* U+002C "," */
    0x39, 0x14,

    /* U+002D "-" */
    0x58, 0x40,

    /* U+002E "." */
    0x39,

    /* U+002F "/" */
    0x0, 0x2, 0x0, 0x71, 0x1, 0x70, 0x7, 0x10,
    0x17, 0x0, 0x70, 0x0, 0x10, 0x0,

    /* U+0030 "0" */
    0x9, 0x89, 0x7, 0x30, 0x36, 0x90, 0x0, 0x97,
    0x30, 0x36, 0x9, 0x89, 0x0,

    /* U+0031 "1" */
    0x4c, 0x12, 0x81, 0x8, 0x10, 0x81, 0x8, 0x10,

    /* U+0032 "2" */
    0x19, 0x89, 0x5, 0x10, 0x91, 0x1, 0x86, 0x2,
    0x91, 0x0, 0x7a, 0x88, 0x0,

    /* U+0033 "3" */
    0x19, 0x87, 0x3, 0x10, 0x90, 0x1, 0xa7, 0x5,
    0x0, 0x90, 0x29, 0x99, 0x0,

    /* U+0034 "4" */
    0x0, 0xb6, 0x0, 0x84, 0x60, 0x54, 0x36, 0x9,
    0x89, 0xb0, 0x0, 0x36, 0x0,

    /* U+0035 "5" */
    0x1a, 0x86, 0x5, 0xa8, 0x50, 0x21, 0x8, 0x25,
    0x0, 0x72, 0x2a, 0x88, 0x0,

    /* U+0036 "6" */
    0x8, 0x89, 0x16, 0x77, 0x70, 0x96, 0x5, 0x57,
    0x40, 0x36, 0x9, 0x89, 0x0,

    /* U+0037 "7" */
    0x79, 0x99, 0x0, 0x55, 0x0, 0xa0, 0x6, 0x40,
    0xa, 0x0,

    /* U+0038 "8" */
    0x29, 0x86, 0x7, 0x20, 0xa0, 0x2c, 0xb7, 0x9,
    0x0, 0x90, 0x59, 0x98, 0x0,

    /* U+0039 "9" */
    0x49, 0x86, 0x9, 0x0, 0x91, 0x29, 0x8a, 0x23,
    0x20, 0xa0, 0x19, 0x87, 0x0,

    /* U+003A ":" */
    0x39, 0x0, 0x39,

    /* U+003B ";" */
    0x39, 0x0, 0x39, 0x14,

    /* U+003C "<" */
    0x0, 0x5, 0x6, 0x92, 0x69, 0x0, 0x3, 0x94,
    0x0, 0x2,

    /* U+003D "=" */
    0x48, 0x87, 0x48, 0x87,

    /* U+003E ">" */
    0x32, 0x0, 0x7, 0x81, 0x0, 0x4c, 0x19, 0x70,
    0x11, 0x0,

    /* U+003F "?" */
    0x1a, 0x99, 0x3, 0x20, 0xa0, 0x0, 0x94, 0x0,
    0x14, 0x0, 0x4, 0x80, 0x0,

    /* U+0040 "@" */
    0x2, 0x77, 0x60, 0x18, 0x58, 0x47, 0x61, 0x49,
    0x68, 0x61, 0xa5, 0x67, 0x27, 0x67, 0x75, 0x2,
    0x77, 0x50,

    /* U+0041 "A" */
    0x2, 0xc0, 0x0, 0x74, 0x40, 0x8, 0x8, 0x5,
    0xa8, 0xb1, 0xa0, 0x2, 0x80,

    /* U+0042 "B" */
    0x5b, 0x9a, 0x25, 0x50, 0x46, 0x5b, 0x8c, 0x25,
    0x50, 0x9, 0x5b, 0x9a, 0x40,

    /* U+0043 "C" */
    0x9, 0x9a, 0x36, 0x40, 0x8, 0x91, 0x0, 0x6,
    0x40, 0x9, 0x9, 0x9a, 0x30,

    /* U+0044 "D" */
    0x5b, 0x99, 0x15, 0x50, 0x19, 0x55, 0x0, 0xa5,
    0x50, 0x19, 0x5b, 0x9a, 0x20,

    /* U+0045 "E" */
    0x5b, 0x99, 0x5, 0x50, 0x0, 0x5b, 0x87, 0x5,
    0x50, 0x0, 0x5b, 0x99, 0x0,

    /* U+0046 "F" */
    0x5b, 0x99, 0x5, 0x50, 0x0, 0x5b, 0x87, 0x5,
    0x50, 0x0, 0x55, 0x0, 0x0,

    /* U+0047 "G" */
    0x9, 0x9a, 0x46, 0x40, 0x5, 0x91, 0x19, 0x96,
    0x40, 0xa, 0x9, 0x9a, 0x30,

    /* U+0048 "H" */
    0x55, 0x2, 0x85, 0x50, 0x28, 0x5b, 0x89, 0x85,
    0x50, 0x28, 0x55, 0x2, 0x80,

    /* U+0049 "I" */
    0x55, 0x55, 0x55, 0x55, 0x55,

    /* U+004A "J" */
    0x0, 0x28, 0x0, 0x28, 0x0, 0x28, 0x50, 0x37,
    0x5a, 0xa2,

    /* U+004B "K" */
    0x55, 0x9, 0x25, 0x59, 0x30, 0x5c, 0xb0, 0x5,
    0x64, 0x80, 0x55, 0x7, 0x50,

    /* U+004C "L" */
    0x55, 0x0, 0x55, 0x0, 0x55, 0x0, 0x55, 0x0,
    0x5b, 0x98,

    /* U+004D "M" */
    0x5c, 0x0, 0xf, 0x15, 0xb4, 0x7, 0xb1, 0x55,
    0x90, 0x99, 0x15, 0x57, 0x83, 0x91, 0x55, 0xb,
    0x9, 0x10,

    /* U+004E "N" */
    0x5c, 0x0, 0xa5, 0xa6, 0xa, 0x55, 0x91, 0xa5,
    0x51, 0x9a, 0x55, 0x6, 0xa0,

    /* U+004F "O" */
    0x9, 0x9a, 0x30, 0x64, 0x0, 0xa0, 0x91, 0x0,
    0x90, 0x64, 0x0, 0xa0, 0x9, 0x9a, 0x30,

    /* U+0050 "P" */
    0x5b, 0x99, 0x45, 0x50, 0xa, 0x5b, 0x99, 0x25,
    0x50, 0x0, 0x55, 0x0, 0x0,

    /* U+0051 "Q" */
    0x9, 0x9a, 0x30, 0x64, 0x0, 0xa0, 0x91, 0x0,
    0x90, 0x64, 0x19, 0xa0, 0x9, 0x9a, 0x90,

    /* U+0052 "R" */
    0x5b, 0x9a, 0x35, 0x50, 0x28, 0x5b, 0xac, 0x25,
    0x52, 0x80, 0x55, 0x9, 0x20,

    /* U+0053 "S" */
    0x2a, 0x99, 0x5, 0x60, 0x32, 0x4, 0x87, 0x7,
    0x10, 0x37, 0x2a, 0x9a, 0x20,

    /* U+0054 "T" */
    0x69, 0xd9, 0x40, 0xa, 0x0, 0x0, 0xa0, 0x0,
    0xa, 0x0, 0x0, 0xa0, 0x0,

    /* U+0055 "U" */
    0x54, 0x0, 0xa5, 0x40, 0xa, 0x54, 0x0, 0xa5,
    0x50, 0x19, 0xa, 0x9a, 0x30,

    /* U+0056 "V" */
    0xa0, 0x1, 0x95, 0x50, 0x82, 0xa, 0xa, 0x0,
    0x86, 0x50, 0x2, 0xe0, 0x0,

    /* U+0057 "W" */
    0xa0, 0x1d, 0x2, 0x78, 0x27, 0xa3, 0x64, 0x55,
    0xa1, 0x89, 0x11, 0xa8, 0xa, 0xa0, 0xe, 0x20,
    0x6a, 0x0,

    /* U+0058 "X" */
    0x83, 0xa, 0x10, 0xa7, 0x40, 0x5, 0xc0, 0x0,
    0xa6, 0x50, 0x93, 0xa, 0x10,

    /* U+0059 "Y" */
    0x92, 0x8, 0x31, 0xa3, 0x80, 0x5, 0xc0, 0x0,
    0xa, 0x0, 0x0, 0xa0, 0x0,

    /* U+005A "Z" */
    0x49, 0x9d, 0x40, 0x3, 0x80, 0x1, 0xa0, 0x0,
    0xa1, 0x0, 0x8b, 0x99, 0x20,

    /* U+005B "[" */
    0x5a, 0x25, 0x50, 0x55, 0x5, 0x50, 0x55, 0x5,
    0xb2,

    /* U+005C "\\" */
    0x20, 0x0, 0x71, 0x0, 0x7, 0x0, 0x6, 0x10,
    0x0, 0x70, 0x0, 0x62, 0x0, 0x1,

    /* U+005D "]" */
    0x7b, 0xa, 0xa, 0xa, 0xa, 0x7b,

    /* U+005E "^" */
    0x4, 0xa0, 0x9, 0x72, 0x37, 0x19,

    /* U+005F "_" */
    0x48, 0x88, 0x30,

    /* U+0060 "`" */
    0x28,

    /* U+0061 "a" */
    0x3a, 0x94, 0x22, 0x79, 0x76, 0x19, 0x59, 0x89,

    /* U+0062 "b" */
    0x72, 0x0, 0x7, 0xa8, 0x80, 0x73, 0x6, 0x37,
    0x30, 0x63, 0x7a, 0x88, 0x0,

    /* U+0063 "c" */
    0x19, 0x88, 0x8, 0x10, 0x40, 0x81, 0x4, 0x1,
    0x99, 0x80,

    /* U+0064 "d" */
    0x0, 0x7, 0x21, 0x98, 0xc2, 0x81, 0x8, 0x28,
    0x10, 0x82, 0x19, 0x8c, 0x20,

    /* U+0065 "e" */
    0x18, 0x88, 0x8, 0x77, 0x92, 0x82, 0x3, 0x1,
    0xa8, 0x80,

    /* U+0066 "f" */
    0xa, 0x37, 0xc4, 0x18, 0x1, 0x80, 0x18, 0x0,

    /* U+0067 "g" */
    0x19, 0x8c, 0x28, 0x10, 0x82, 0x81, 0x8, 0x21,
    0x98, 0xc2, 0x14, 0x9, 0x10, 0x78, 0x60,

    /* U+0068 "h" */
    0x72, 0x0, 0x79, 0x87, 0x73, 0xa, 0x72, 0xa,
    0x72, 0xa,

    /* U+0069 "i" */
    0x51, 0x72, 0x72, 0x72, 0x72,

    /* U+006A "j" */
    0x4, 0x20, 0x63, 0x6, 0x30, 0x63, 0x6, 0x30,
    0x91,

    /* U+006B "k" */
    0x72, 0x0, 0x72, 0x75, 0x7a, 0x70, 0x75, 0xa0,
    0x72, 0x28,

    /* U+006C "l" */
    0x72, 0x72, 0x72, 0x72, 0x72,

    /* U+006D "m" */
    0x7a, 0xaa, 0x98, 0x73, 0x19, 0xa, 0x72, 0x8,
    0xa, 0x72, 0x8, 0xa,

    /* U+006E "n" */
    0x79, 0x87, 0x73, 0xa, 0x72, 0xa, 0x72, 0xa,

    /* U+006F "o" */
    0x19, 0x88, 0x8, 0x10, 0x64, 0x81, 0x6, 0x41,
    0x98, 0x80,

    /* U+0070 "p" */
    0x7a, 0x88, 0x7, 0x30, 0x63, 0x73, 0x6, 0x37,
    0xa8, 0x80, 0x72, 0x0, 0x4, 0x10, 0x0,

    /* U+0071 "q" */
    0x19, 0x8c, 0x28, 0x10, 0x82, 0x81, 0x8, 0x21,
    0x98, 0xc2, 0x0, 0x7, 0x20, 0x0, 0x41,

    /* U+0072 "r" */
    0x79, 0x37, 0x20, 0x72, 0x7, 0x20,

    /* U+0073 "s" */
    0x3a, 0xa3, 0x48, 0x31, 0x40, 0x58, 0x49, 0x96,

    /* U+0074 "t" */
    0x36, 0x8, 0xb3, 0x36, 0x3, 0x60, 0x1b, 0x10,

    /* U+0075 "u" */
    0x81, 0xa, 0x81, 0xa, 0x72, 0xa, 0x3a, 0x8c,

    /* U+0076 "v" */
    0xa0, 0x9, 0x4, 0x62, 0x80, 0xa, 0x81, 0x0,
    0x59, 0x0,

    /* U+0077 "w" */
    0x90, 0x77, 0x9, 0x62, 0x88, 0x27, 0x29, 0x55,
    0x93, 0xd, 0x0, 0xd0,

    /* U+0078 "x" */
    0x83, 0x64, 0xa, 0x80, 0xa, 0x90, 0x82, 0x65,

    /* U+0079 "y" */
    0x90, 0xa, 0x3, 0x65, 0x50, 0x9, 0xa0, 0x0,
    0x66, 0x0, 0x9, 0x0, 0x0, 0x40, 0x0,

    /* U+007A "z" */
    0x58, 0xa9, 0x1, 0xa0, 0xa, 0x10, 0xa9, 0x86,

    /* U+007B "{" */
    0xa, 0x70, 0xa0, 0x59, 0x0, 0xa0, 0xa, 0x0,
    0x97,

    /* U+007C "|" */
    0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,

    /* U+007D "}" */
    0x79, 0x0, 0xa0, 0xa, 0x40, 0xa0, 0xa, 0x7,
    0x90,

    /* U+007E "~" */
    0x19, 0x51, 0x54, 0x24, 0x91
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 35, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 50, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 5, .adv_w = 50, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 8, .adv_w = 101, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 23, .adv_w = 84, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 41, .adv_w = 128, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 69, .adv_w = 86, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 84, .adv_w = 33, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 86, .adv_w = 47, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 98, .adv_w = 47, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 110, .adv_w = 63, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 116, .adv_w = 76, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 126, .adv_w = 39, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 128, .adv_w = 46, .box_w = 3, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 130, .adv_w = 39, .box_w = 2, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 131, .adv_w = 57, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 145, .adv_w = 80, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 158, .adv_w = 47, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 192, .adv_w = 69, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 205, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 218, .adv_w = 76, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 231, .adv_w = 65, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 241, .adv_w = 69, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 254, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 267, .adv_w = 39, .box_w = 2, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 270, .adv_w = 39, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 274, .adv_w = 71, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 284, .adv_w = 71, .box_w = 4, .box_h = 2, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 288, .adv_w = 71, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 298, .adv_w = 76, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 311, .adv_w = 103, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 329, .adv_w = 76, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 342, .adv_w = 81, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 355, .adv_w = 83, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 368, .adv_w = 83, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 381, .adv_w = 72, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 394, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 407, .adv_w = 84, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 420, .adv_w = 83, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 433, .adv_w = 33, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 438, .adv_w = 67, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 448, .adv_w = 77, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 66, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 471, .adv_w = 108, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 489, .adv_w = 86, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 502, .adv_w = 87, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 517, .adv_w = 79, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 530, .adv_w = 87, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 545, .adv_w = 78, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 558, .adv_w = 77, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 571, .adv_w = 75, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 584, .adv_w = 85, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 597, .adv_w = 77, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 610, .adv_w = 108, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 628, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 641, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 654, .adv_w = 76, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 667, .adv_w = 40, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 676, .adv_w = 57, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 690, .adv_w = 40, .box_w = 2, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 696, .adv_w = 71, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 702, .adv_w = 77, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 705, .adv_w = 38, .box_w = 2, .box_h = 1, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 706, .adv_w = 65, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 714, .adv_w = 75, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 727, .adv_w = 72, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 737, .adv_w = 75, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 750, .adv_w = 73, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 760, .adv_w = 43, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 768, .adv_w = 74, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 783, .adv_w = 71, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 793, .adv_w = 27, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 798, .adv_w = 28, .box_w = 3, .box_h = 6, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 807, .adv_w = 65, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 817, .adv_w = 27, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 822, .adv_w = 104, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 834, .adv_w = 71, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 842, .adv_w = 76, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 852, .adv_w = 75, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 867, .adv_w = 75, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 882, .adv_w = 42, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 888, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 896, .adv_w = 43, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 904, .adv_w = 70, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 912, .adv_w = 69, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 922, .adv_w = 97, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 934, .adv_w = 60, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 942, .adv_w = 65, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 957, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 965, .adv_w = 47, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 974, .adv_w = 33, .box_w = 2, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 982, .adv_w = 47, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 991, .adv_w = 81, .box_w = 5, .box_h = 2, .ofs_x = 0, .ofs_y = 2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Map glyph_ids to kern left classes*/
static const uint8_t kern_left_class_mapping[] =
{
    0, 1, 2, 3, 0, 0, 0, 0,
    3, 4, 4, 5, 0, 6, 7, 6,
    8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 9, 0, 0, 0, 0, 0,
    2, 0, 18, 19, 20, 21, 22, 23,
    21, 24, 24, 25, 26, 27, 24, 24,
    21, 28, 29, 30, 31, 32, 25, 33,
    34, 26, 35, 36, 37, 38, 37, 0,
    6, 0, 39, 40, 40, 41, 40, 42,
    43, 39, 44, 44, 45, 41, 39, 39,
    40, 40, 0, 46, 47, 48, 49, 50,
    51, 45, 52, 53, 37, 0, 37, 0
};

/*Map glyph_ids to kern right classes*/
static const uint8_t kern_right_class_mapping[] =
{
    0, 0, 0, 1, 0, 0, 0, 0,
    1, 2, 2, 3, 0, 4, 5, 4,
    6, 7, 8, 9, 10, 11, 12, 7,
    13, 14, 15, 16, 16, 0, 0, 0,
    17, 0, 18, 19, 20, 19, 19, 19,
    20, 19, 19, 21, 19, 19, 19, 19,
    20, 19, 20, 19, 22, 23, 24, 25,
    26, 27, 28, 29, 30, 31, 30, 0,
    4, 0, 32, 33, 34, 34, 34, 35,
    34, 33, 36, 37, 33, 33, 38, 38,
    34, 38, 34, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 30, 0, 30, 0
};

/*Kern values between classes*/
static const int8_t kern_class_values[] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -3, -5, 0, -10, 0,
    -4, -3, -1, -5, 0, 0, 0, -4,
    0, -4, -3, 0, 0, 0, -3, -3,
    0, -5, -3, -4, -5, 0, -6, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -15,
    0, 0, -3, 0, 0, 0, -9, -3,
    1, -3, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, -1, -5, 0,
    1, 0, 1, 1, 1, 1, 0, 0,
    0, -4, 3, -4, 0, 4, 4, 0,
    0, 0, 0, -3, -1, -3, -3, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -4, 1, 0, 0, 0, -8, 1,
    0, 0, 0, 0, 0, 0, -18, -6,
    0, 0, 0, 0, -5, -13, 0, 0,
    1, -3, -9, 0, -4, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -6, 0, 0,
    0, 0, 2, 0, 0, 0, 5, 4,
    -9, 4, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -19,
    0, 0, 0, -3, 0, 0, 0, 0,
    0, 0, 0, -9, 1, -4, -15, -4,
    4, 4, 4, 3, 4, 4, 1, 0,
    0, -10, 3, -14, 1, 4, 4, -5,
    -10, 0, 0, 0, 0, -6, 0, -4,
    -3, 0, 0, -5, 3, 0, 0, -1,
    -4, -3, -1, -1, -5, -1, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -5, -4, 0, 0, -2, -3, -3,
    0, 0, -1, 0, 0, -4, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -4, -3, -2, -3, -4, -3,
    -5, -1, -1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -3, 0, 0,
    0, -3, -3, 0, 0, 0, -4, 0,
    -1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, 0, 0, -1, -3,
    0, 0, 0, 0, -4, 0, -4, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -4, 0,
    0, 0, 0, 0, 0, -5, -3, -1,
    0, 0, -4, 0, -3, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -4, 0, 0, 0,
    0, 0, -1, -3, -1, 0, 0, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, -23, -12, 0,
    -8, -3, -6, -4, -9, -4, 0, -5,
    -1, 0, 0, -9, 0, -3, -13, -1,
    0, 0, 0, 1, 0, 0, -4, 0,
    0, 0, 0, -7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -3, 0, 0, 0, 3, 0, -1, -3,
    -1, 0, 0, 0, -5, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -13, 0,
    0, 3, -3, 4, 0, 0, 0, -4,
    0, 0, -3, 0, 0, 0, -9, 3,
    0, -3, 0, -4, -13, -5, -9, -5,
    1, -12, 0, -4, 0, 0, 0, -3,
    -5, 0, 0, 0, -3, -4, 0, -6,
    -5, 1, -9, 0, -4, 0, 0, -3,
    0, -3, 0, 0, 0, -3, 0, 0,
    -1, 0, 0, 0, 0, -3, 0, 0,
    0, 0, -4, 0, -5, -1, -4, -5,
    -1, -3, 0, -4, 0, -1, 0, 0,
    0, -1, 0, 0, 0, -1, 0, 0,
    0, 0, -3, -1, 0, -5, 3, -4,
    0, 0, 0, 0, 0, 0, -1, 0,
    0, 0, 1, -3, 0, 0, 0, 0,
    -5, 0, -3, 0, -3, -3, -4, -3,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 3, 3, 1, 3, 0,
    -5, -1, 0, -5, 3, -4, 0, 0,
    0, 0, 0, 0, -5, 0, 0, 0,
    -3, -3, 0, 0, 0, 0, -5, 0,
    -5, 0, -3, -6, -4, -3, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, -3, -1, 0, -1, 0,
    0, 0, 0, 4, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0,
    0, -3, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -3, 0, -3,
    0, 0, 0, 0, -4, -1, 0, -3,
    0, 0, 0, 0, 0, 0, 0, -14,
    0, -12, 0, 0, 0, 0, 0, 0,
    3, 0, 0, 0, 0, -6, 0, -3,
    -13, -3, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -4, 0, -4, -4, 0,
    -1, 0, -5, -5, -3, -5, -5, -5,
    -5, -6, -3, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -3, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -5, 0, -4, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -5, 0, 0, -3, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, -10, 4, 0, 0, 0, -4,
    0, 0, 0, 0, 0, 0, -4, 1,
    0, -4, -4, 0, 0, 0, 0, 0,
    1, 0, 0, 1, 3, -3, 0, -6,
    -5, 0, 0, 0, -4, -4, -3, -8,
    -4, 0, -8, 0, -17, 0, 0, 4,
    -15, 4, 0, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, -6, 3, 0, -4,
    0, 0, -12, -4, -17, -6, 1, -18,
    0, -3, 0, 0, 0, -1, -4, 0,
    0, 0, 0, -4, 0, -12, -6, 1,
    -10, 0, 0, 0, 0, -17, 0, -12,
    0, 0, 0, 0, -4, 0, 0, 0,
    0, 0, 1, -8, 0, 0, -18, 0,
    0, 0, 0, 0, -4, -4, -4, -3,
    0, 0, 0, -4, 0, 0, 0, 0,
    -1, 3, 0, 0, 0, 0, 0, 0,
    -5, -1, 0, -5, 3, -4, 0, 0,
    0, 0, 0, 0, -5, 0, 0, 0,
    -3, -1, 0, 0, 0, 0, -5, 0,
    -5, 0, -3, -6, -3, -3, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, -3, -1, 0, 0, 0,
    0, -3, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -5, 0, -3, 0, -1, 0,
    0, -4, -1, 0, 0, 0, 0, -3,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -5, 0, 0, -3,
    0, 0, 0, 0, 0, 0, 1, 0,
    -4, 0, 0, 0, 0, -3, 0, 0,
    0, 0, -5, 0, -4, 0, 0, -4,
    0, -3, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -2, 0, 0, 0, 0,
    0, 0, 0, 1, 0, -19, -17, -15,
    0, 0, 0, -6, -9, 0, 3, 0,
    0, -13, -1, -13, 0, -5, -19, 0,
    -3, 0, 0, 0, 0, 0, 0, 1,
    2, -20, -3, -17, -6, -3, -3, -18,
    -19, -6, -15, -14, -18, -17, -17, -18,
    0, 1, 0, -17, -6, -10, 0, 0,
    0, 0, -6, 0, 1, 0, 0, 0,
    0, -9, 0, -4, -17, -3, 0, 0,
    3, 0, 0, 3, 0, 1, 0, -8,
    0, -6, 0, 0, 0, -3, -8, -1,
    -5, -1, -3, 0, -1, -4, 0, 1,
    0, -13, -1, -6, 0, 0, 0, 0,
    -4, 0, 1, 0, 0, 0, 0, -5,
    0, 0, -9, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 0, -1, 0, -4,
    0, 0, 0, -1, -4, 0, 0, 0,
    0, 0, -1, 0, 0, 1, 0, -22,
    -13, -17, 0, 0, 0, 0, -12, 0,
    1, 0, 0, 0, 0, -12, 0, -5,
    -18, -3, 0, 0, 3, 0, 0, 0,
    0, 1, 0, -12, 0, -12, -4, 0,
    0, -8, -12, -4, -9, -5, -5, -5,
    -5, -9, 0, 0, 0, 0, -8, 1,
    0, 0, 0, 0, -6, 0, 0, 0,
    0, 0, -3, 0, 0, -8, -1, 0,
    0, 0, 0, 1, 0, 1, 0, 0,
    0, 0, 0, -5, -4, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -4, 0, -3, -8, -3, 1, 0,
    1, 1, 1, 1, 0, 0, 0, -1,
    0, -4, 0, 0, 4, 0, -4, 0,
    0, -4, -1, -3, -3, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 3,
    0, 0, 0, 0, -13, 0, 0, 0,
    1, 0, 0, 0, -19, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 3, 0, 0, -10, -3, -4, 0,
    0, 3, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -3, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -3, 0, 0, 0, 0, -3, 0,
    0, 0, 0, -1, 0, -4, -3, -3,
    -1, 0, -7, -4, -4, -4, 1, -3,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -6, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -4,
    -13, -1, 0, -1, -3, 0, 0, 0,
    -1, -1, 0, -4, -3, -4, -3, -3,
    0, 1, 0, 0, 0, 4, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 3,
    1, -14, 0, -4, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 3, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -4, 0, -3,
    1, 0, 0, 0, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 4, 0, 0, 0, 4,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -3, 0, 0, -9, 4, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -3, -6, -3,
    0, -4, 0, 0, 0, 0, -5, 1,
    0, 0, 0, 1, 0, 0, 0, 0,
    0, -13, -8, -10, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 3, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -3, 0, -5, 0, -4,
    0, 0, 0, 0, -5, 0, 0, 0,
    0, 0, 0, 0, -8, 0, -8, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -4, 0, 0, 0, 0, -3, 0,
    0, 0, -1, -3, 0, -4, -3, -5,
    -4, 0, -1, 0, 0, 0, -9, 4,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 4, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -4,
    0, -4, 0, -4, -1, 0, 0, 0,
    -3, -1, 0, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -5, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -3,
    0, -15, -3, -6, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -4, 0, -3, 0, -5,
    0, 0, 0, 0, -4, 1, 0, 0,
    0, 0, 0, 0, 0, -1, 0, -8,
    -1, -5, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -1, 0, -3, 0, -3, 0, 0,
    0, 0, -3, 0, 0, 0, 0, 0,
    0, 0, 0, -3, 0, -15, -4, -6,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -3,
    0, -4, 0, -4, 1, 0, 0, 0,
    -4, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -5, 4, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -1,
    0, -3, 0, 0, 0, 0, -3, 0,
    0, 0, 0, 0, 0, 0
};


/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t kern_classes =
{
    .class_pair_values   = kern_class_values,
    .left_class_mapping  = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt      = 53,
    .right_class_cnt     = 46,
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_classes,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 1,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t esp_brookesia_font_maison_neue_book_8 = {
#else
lv_font_t esp_brookesia_font_maison_neue_book_8 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 8,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if ESP_BROOKESIA_FONT_MAISON_NEUE_BOOK_8*/
