/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#define ESP_FOURCC_VERSION ("v1.0.0")

/**
# Changelog

## [v1.0.0]
    - Add audio, video and pixel format with fourcc(four character code)
*/

typedef uint32_t esp_fourcc_t;  // 32-bit FOURCC code

#define ESP_FOURCC_TO_INT(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

// Convert 32-bit FOURCC to string
static inline void gmf_fourcc_to_str(uint32_t fourcc, char out[5]) {
    for (int i = 0; i < 4; i++) {
        out[i] = (char)((fourcc >> (i * 4)) & 0xFF);
    }
    out[4] = '\0';
}

// Macro to convert an FOURCC code to a string
#define ESP_FOURCC_TO_STR(fourcc)  ({ \
    static char fourcc_str[5]; \
    fourcc_str[0] = fourcc_str[1] = fourcc_str[2] = fourcc_str[3] = fourcc_str[4] = '\0'; \
    gmf_fourcc_to_str(fourcc, fourcc_str); \
    fourcc_str; \
})

/***************************************************************/
/*                         Video codec                         */
/***************************************************************/
#define ESP_FOURCC_H263             ESP_FOURCC_TO_INT('H', '2', '6', '3') /* H263 */
#define ESP_FOURCC_H264             ESP_FOURCC_TO_INT('H', '2', '6', '4') /* H264 with start code */
#define ESP_FOURCC_AVC1             ESP_FOURCC_TO_INT('A', 'V', 'C', '1') /* H264 without start code */
#define ESP_FOURCC_H265             ESP_FOURCC_TO_INT('H', '2', '6', '5') /* HEVC */
#define ESP_FOURCC_VP8              ESP_FOURCC_TO_INT('V', 'P', '8', ' ') /* VP8 */
#define ESP_FOURCC_MJPG             ESP_FOURCC_TO_INT('M', 'J', 'P', 'G') /* Motion-JPEG */


/***************************************************************/
/*                          Container                          */
/***************************************************************/
#define ESP_FOURCC_WAV              ESP_FOURCC_TO_INT('W', 'A', 'V', ' ') /* WAV container */
#define ESP_FOURCC_MP4              ESP_FOURCC_TO_INT('M', 'P', '4', ' ') /* MPEG-4 container */
#define ESP_FOURCC_M2TS             ESP_FOURCC_TO_INT('M', '2', 'T', 'S') /* MPEG-2 Transport Stream */
#define ESP_FOURCC_FLV              ESP_FOURCC_TO_INT('F', 'L', 'V', '1') /* Flash Video */
#define ESP_FOURCC_AVI              ESP_FOURCC_TO_INT('A', 'V', 'I', ' ') /* Audio Video Interleave */
#define ESP_FOURCC_OGG              ESP_FOURCC_TO_INT('O', 'G', 'G', ' ') /* Ogg container */
#define ESP_FOURCC_WEBM             ESP_FOURCC_TO_INT('W', 'E', 'B', 'M') /* WebM container */


/***************************************************************/
/*                          Image codec                        */
/***************************************************************/
#define ESP_FOURCC_PNG              ESP_FOURCC_TO_INT('P', 'N', 'G', ' ') /* Portable Network Graphics */
#define ESP_FOURCC_JPEG             ESP_FOURCC_TO_INT('J', 'P', 'E', 'G') /* JPEG File Interchange Format (JFIF) */
#define ESP_FOURCC_GIF              ESP_FOURCC_TO_INT('G', 'I', 'F', ' ') /* Graphics Interchange Format */
#define ESP_FOURCC_WEBP             ESP_FOURCC_TO_INT('W', 'E', 'B', 'P') /* WebP */
#define ESP_FOURCC_BMP              ESP_FOURCC_TO_INT('B', 'M', 'P', ' ') /* Bitmap */

/***************************************************************/
/*                          Audio codec                        */
/***************************************************************/
#define ESP_FOURCC_MP2              ESP_FOURCC_TO_INT('M', 'P', '2', ' ') /* MPEG Layer II */
#define ESP_FOURCC_MP3              ESP_FOURCC_TO_INT('M', 'P', '3', ' ') /* MPEG Layer III */
#define ESP_FOURCC_AAC              ESP_FOURCC_TO_INT('A', 'A', 'C', ' ') /* Advanced Audio Codec */
#define ESP_FOURCC_FLAC             ESP_FOURCC_TO_INT('F', 'L', 'A', 'C') /* Free Lossless Audio Codec */
#define ESP_FOURCC_M4A              ESP_FOURCC_TO_INT('M', '4', 'A', 'A') /* MPEG-4 Audio */
#define ESP_FOURCC_VORBIS           ESP_FOURCC_TO_INT('V', 'O', 'B', 'S') /* Vorbis */
#define ESP_FOURCC_AMRNB            ESP_FOURCC_TO_INT('A', 'M', 'R', 'N') /* Adaptive Multi-Rate Narrowband */
#define ESP_FOURCC_AMRWB            ESP_FOURCC_TO_INT('A', 'M', 'R', 'W') /* Adaptive Multi-Rate Wideband */
#define ESP_FOURCC_ALAC             ESP_FOURCC_TO_INT('A', 'L', 'A', 'C') /* Apple Lossless Audio Codec */
#define ESP_FOURCC_ALAW             ESP_FOURCC_TO_INT('A', 'L', 'A', 'W') /* G.711 Audio Compressed */
#define ESP_FOURCC_ULAW             ESP_FOURCC_TO_INT('U', 'L', 'A', 'W') /* G.711 Audio Compressed */
#define ESP_FOURCC_ADPCM            ESP_FOURCC_TO_INT('A', 'D', 'P', 'C') /* IMA-ADPCM Audio */
#define ESP_FOURCC_PCM_U08          ESP_FOURCC_TO_INT('U', '0', '8', 'L') /* 8-bit PCM Audio */
#define ESP_FOURCC_PCM_S16          ESP_FOURCC_TO_INT('S', '1', '6', 'L') /* 16-bit PCM Audio little endian */
#define ESP_FOURCC_PCM_S24          ESP_FOURCC_TO_INT('S', '2', '4', 'L') /* 24-bit PCM Audio little endian */
#define ESP_FOURCC_PCM_S32          ESP_FOURCC_TO_INT('S', '3', '2', 'L') /* 32-bit PCM Audio little endian */
#define ESP_FOURCC_LYRA             ESP_FOURCC_TO_INT('L', 'Y', 'R', 'A') /* Lyra Audio */
#define ESP_FOURCC_LC3              ESP_FOURCC_TO_INT('L', 'C', '3', '0') /* Low Complexity Communication Codec */
#define ESP_FOURCC_SBC              ESP_FOURCC_TO_INT('S', 'B', 'C', ' ') /* Sub-Band Codec */
#define ESP_FOURCC_OPUS             ESP_FOURCC_TO_INT('O', 'P', 'U', 'S') /* Raw Opus */
#define ESP_FOURCC_SPEEX            ESP_FOURCC_TO_INT('S', 'P', 'E', 'X') /* Speex Audio */


/***************************************************************/
/*                      RGB Pixel formats                      */
/***************************************************************/

/**
 * RGB555
 * Memory Layout:
 *            | bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0 |
 *    Byte 1: |  0    R4   R3   R2   R1   R0 | G4   G3  |
 *    Byte 0: |  G2   G1   G0 | B4   B3   B2   B1   B0  |
 */
#define ESP_FOURCC_RGB15            ESP_FOURCC_TO_INT('R', 'G', '1', '5') /* 16 bpp(bits per pixel),  RGB-5-5-5 */

/**
 * BGR555
 * Memory Layout:
 *            | bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0 |
 *    Byte 1: |  0    B4   B3   B2   B1   B0 | G4   G3  |
 *    Byte 0: |  G2   G1   G0 | R4   R3   R2   R1   R0  |
 */
#define ESP_FOURCC_BGR15            ESP_FOURCC_TO_INT('B', 'G', '1', '5') /* 16 bpp BGR-5-5-5 */

/**
 * RGB565
 * Memory Layout:
 *            | bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0 |
 *    Byte 1: |  R4   R3   R2   R1   R0 | G5   G4   G3  |
 *    Byte 0: |  G2   G1   G0 | B4   B3   B2   B1   B0  |
 */
#define ESP_FOURCC_RGB16            ESP_FOURCC_TO_INT('R', 'G', 'B', 'L') /* 16 bpp RGB-5-6-5, little endian */

/**
 * BGR565
 * Memory Layout:
 *            | bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0 |
 *    Byte 1: |  B4   B3   B2   B1   B0 | G5   G4   G3  |
 *    Byte 0: |  G2   G1   G0 | R4   R3   R2   R1   R0  |
 */
#define ESP_FOURCC_BGR16            ESP_FOURCC_TO_INT('B', 'G', 'R', 'L') /* 16 bpp BGR-5-6-5, little endian */

/**
 * RGB565_BE
 * Memory Layout:
 *            | bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0 |
 *    Byte 1: |  G2   G1   G0 | B4   B3   B2   B1   B0  |
 *    Byte 0: |  R4   R3   R2   R1   R0 | G5   G4   G3  |
 */
#define ESP_FOURCC_RGB16_BE         ESP_FOURCC_TO_INT('R', 'G', 'B', 'B') /* 16 bpp RGB-5-6-5, big endian */

/**
 * BGR565_BE
 * Memory Layout:
 *            | bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0 |
 *    Byte 1: |  G2   G1   G0 | R4   R3   R2   R1   R0  |
 *    Byte 0: |  B4   B3   B2   B1   B0 | G5   G4   G3  |
 */
#define ESP_FOURCC_BGR16_BE         ESP_FOURCC_TO_INT('B', 'G', 'R', 'B') /* 16 bpp BGR-5-6-5, big endian */

/**
 * RGB24
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 2: |  B7  -  B0  |
 *    Byte 1: |  G7  -  G0  |
 *    Byte 0: |  R7  -  R0  |
 */
#define ESP_FOURCC_RGB24            ESP_FOURCC_TO_INT('R', 'G', 'B', '3') /* 24 bpp RGB-8-8-8 */

/**
 * BGR24
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 2: |  R7  -  R0  |
 *    Byte 1: |  G7  -  G0  |
 *    Byte 0: |  B7  -  B0  |
 */
#define ESP_FOURCC_BGR24            ESP_FOURCC_TO_INT('B', 'G', 'R', '3') /* 24 bpp BGR-8-8-8 */

/**
 * RGBA32
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 3: |  A7  -  A0  |
 *    Byte 2: |  B7  -  B0  |
 *    Byte 1: |  G7  -  G0  |
 *    Byte 0: |  R7  -  R0  |
 */
#define ESP_FOURCC_RGBA32           ESP_FOURCC_TO_INT('R', 'A', '2', '4') /* 32 bpp RGBA-8-8-8-8 */

/**
 * RGBX32
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 3: |  X7  -  X0  |
 *    Byte 2: |  B7  -  B0  |
 *    Byte 1: |  G7  -  G0  |
 *    Byte 0: |  R7  -  R0  |
 */
#define ESP_FOURCC_RGBX32           ESP_FOURCC_TO_INT('R', 'X', '2', '4') /* 32 bpp RGBX-8-8-8-8 */

/**
 * ARGB32
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 3: |  B7  -  B0  |
 *    Byte 2: |  G7  -  G0  |
 *    Byte 1: |  R7  -  R0  |
*     Byte 0: |  A7  -  A0  |
 */
#define ESP_FOURCC_ARGB32           ESP_FOURCC_TO_INT('A', 'B', '2', '4') /* 32 bpp ARGB-8-8-8-8 */

/**
 * XRGB32
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 3: |  B7  -  B0  |
 *    Byte 2: |  G7  -  G0  |
 *    Byte 1: |  R7  -  R0  |
*     Byte 0: |  X7  -  X0  |
 */
#define ESP_FOURCC_XRGB32           ESP_FOURCC_TO_INT('X', 'B', '2', '4') /* 32 bpp XRGB-8-8-8-8 */

/**
 * ABGR32
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 3: |  R7  -  R0  |
 *    Byte 2: |  G7  -  G0  |
 *    Byte 1: |  B7  -  B0  |
 *    Byte 0: |  A7  -  A0  |
 */
#define ESP_FOURCC_ABGR32           ESP_FOURCC_TO_INT('A', 'R', '2', '4') /* 32 bpp ABGR-8-8-8-8 */

/**
 * XBGR32
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 3: |  R7  -  R0  |
 *    Byte 2: |  G7  -  G0  |
 *    Byte 1: |  B7  -  B0  |
 *    Byte 0: |  X7  -  X0  |
 */
#define ESP_FOURCC_XBGR32           ESP_FOURCC_TO_INT('X', 'R', '2', '4') /* 32 bpp XBGR-8-8-8-8 */

/**
 * BGRA32
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 3: |  A7  -  A0  |
 *    Byte 2: |  R7  -  R0  |
 *    Byte 1: |  G7  -  G0  |
 *    Byte 0: |  B7  -  B0  |
 */
#define ESP_FOURCC_BGRA32           ESP_FOURCC_TO_INT('B', 'A', '2', '4') /* 32 bpp BGRA-8-8-8-8 */

/**
 * BGRX32
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 3: |  X7  -  X0  |
 *    Byte 2: |  R7  -  R0  |
 *    Byte 1: |  G7  -  G0  |
 *    Byte 0: |  B7  -  B0  |
 */
#define ESP_FOURCC_BGRX32           ESP_FOURCC_TO_INT('B', 'X', '2', '4') /* 32 bpp BGRX-8-8-8-8 */


/***************************************************************/
/*                     Grey Pixel formats                      */
/***************************************************************/
/**
 * Grey8
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 0: |  G7  -  G0  |
 */
#define ESP_FOURCC_GREY             ESP_FOURCC_TO_INT('G', 'R', 'E', 'Y') /* 8 bpp Greyscale */

/**
 * Grey16
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 1: |  G15 -  G8  |
 *    Byte 0: |  G7  -  G0  |
 */
#define ESP_FOURCC_Y16              ESP_FOURCC_TO_INT('Y', '1', '6', 'L') /* 16 bpp Greyscale */

/**
 * Grey16_BE
 * Memory Layout:
 *            | bit7 - bit0 |
 *    Byte 1: |  G7  -  G0  |
 *    Byte 0: |  G15 -  G8  |
 */
#define ESP_FOURCC_Y16_BE           ESP_FOURCC_TO_INT('Y', '1', '6', 'B') /* 16 bpp Greyscale, big endian */


/***************************************************************/
/*                   YCbCr-YUV packed format                   */
/***************************************************************/
/**
 * YUYV422
 * Memory Layout:
 *    +--+--+--+--+ +--+--+--+--+
 *    |Y0|U0|Y1|V0| |Y2|U2|Y3|V2|
 *    +--+--+--+--+ +--+--+--+--+
 */
#define ESP_FOURCC_YUYV            ESP_FOURCC_TO_INT('Y', 'U', 'Y', 'V') /* 16 bpp(bits per pixel), YUYV 4:2:2 */

/**
 * YVYU422
 * Memory Layout:
 *    +--+--+--+--+ +--+--+--+--+
 *    |Y0|V0|Y1|U0| |Y2|V2|Y3|U2|
 *    +--+--+--+--+ +--+--+--+--+
 */
#define ESP_FOURCC_YVYU            ESP_FOURCC_TO_INT('Y', 'V', 'Y', 'U') /* 16 bpp, YVYU 4:2:2 */

/**
 * YYUV422
 * Memory Layout:
 *    +--+--+--+--+ +--+--+--+--+
 *    |Y0|Y1|U0|V0| |Y2|Y3|U2|V2|
 *    +--+--+--+--+ +--+--+--+--+
 */
#define ESP_FOURCC_YYUV            ESP_FOURCC_TO_INT('Y', 'Y', 'U', 'V') /* 16 bpp, YYUV 4:2:2 */

/**
 * UYVY422
 * Memory Layout:
 *    +--+--+--+--+ +--+--+--+--+
 *    |U0|Y0|V0|Y1| |U2|Y2|V2|Y3|
 *    +--+--+--+--+ +--+--+--+--+
 */
#define ESP_FOURCC_UYVY            ESP_FOURCC_TO_INT('U', 'Y', 'V', 'Y') /* 16 bpp, UYVY 4:2:2 */

/**
 * VYUY422
 * Memory Layout:
 *    +--+--+--+--+ +--+--+--+--+
 *    |V0|Y0|U0|Y1| |V2|Y2|U2|Y3|
 *    +--+--+--+--+ +--+--+--+--+
 */
#define ESP_FOURCC_VYUY            ESP_FOURCC_TO_INT('V', 'Y', 'U', 'Y') /* 16 bpp, VYUY 4:2:2 */

/**
 * YUV444
 * Memory Layout:
 *    +--+--+--+ +--+--+--+
 *    |Y0|U0|V0| |Y1|U1|V1|
 *    +--+--+--+ +--+--+--+
 */
#define ESP_FOURCC_YUV             ESP_FOURCC_TO_INT('V', '3', '0', '8') /* 24 bpp, Y-U-V 4:4:4 */

/**
 * UYV444
 * Memory Layout:
 *   +--+--+--+ +--+--+--+
 *   |U0|Y0|V0| |U1|Y1|V1|
 *   +--+--+--+ +--+--+--+
 */
#define ESP_FOURCC_UYV             ESP_FOURCC_TO_INT('I', 'Y', 'U', '2') /* 24 bpp, U-Y-V 4:4:4 */

/**
 * Espressif YUV420, U00 V10 shared Y00 Y01 Y10 Y11, U02 V12 shared Y02 Y03 Y12 Y13
 * Memory Layout:
 *       +---+---+---+ +---+---+---+
 * Line1 |U00|Y00|Y01| |U02|Y02|Y03|
 *       +---+---+---+ +---+---+---+
 * Line2 |V10|Y10|Y11| |V12|Y12|Y13|
 *       +---+---+---+ +---+---+---+
 */
#define ESP_FOURCC_OUYY_EVYY       ESP_FOURCC_TO_INT('O', 'U', 'E', 'V') /* 12 bpp, Espressif Y-U-V 4:2:0 */

/**
 * Espressif YUV420, U0 V4 shared Y0 Y1 Y2 Y3, U2 V6 shared Y4 Y5 Y6 Y7
 * Memory Layout:
 *    Y, Cb, Y,  Y, Cb, Y,  Y, Cr, Y,  Y, Cr, Y
 *    +--+--+--+ +--+--+--+ +--+--+--+ +--+--+--+
 *    |Y0|U0|Y1| |Y2|U2|Y3| |Y4|V4|Y5| |Y6|V6|Y7|
 *    +--+--+--+ +--+--+--+ +--+--+--+ +--+--+--+
 */
#define ESP_FOURCC_YUY2YVY2        ESP_FOURCC_TO_INT('Y', 'U', 'Y', '2') /* 12 bpp, Espressif LCD_CAM Y-U-V 4:2:0 */

/**
 * Espressif YUV411, U0 V2 shared Y0 Y1 Y2 Y3, U4 V6 shared Y4 Y5 Y6 Y7
 * Memory Layout:
 *    Y,  Cb, Y,  Y, Cb, Y,  Y, Cr, Y,  Y, Cr, Y
 *    +--+--+--+ +--+--+--+ +--+--+--+ +--+--+--+
 *    |Y0|U0|Y1| |Y2|V2|Y3| |Y4|U4|Y5| |Y6|V6|Y7|
 *    +--+--+--+ +--+--+--+ +--+--+--+ +--+--+--+
 */
#define ESP_FOURCC_YUYYVY        ESP_FOURCC_TO_INT('Y', 'U', 'Y', 'Y') /* 12 bpp, Espressif LCD_CAM Y-U-V 4:1:1 */


/***************************************************************/
/*                  YUV(YCbCr) Semi-planar format               */
/*    Two contiguous planes, one Y, one Cr + Cb interleaved    */
/***************************************************************/
/**
 * NV12 (YUV 4:2:0), 1 shared U(Cb), V(Cr) value for 2x2 block pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *    | U00 | V00 | U02 | V02 |
 *    | U20 | V20 | U22 | V22 |
 */
#define ESP_FOURCC_NV12         ESP_FOURCC_TO_INT('N', 'V', '1', '2') /* 12 bpp, YUV 4:2:0  */

/**
 * NV21 (YVU 4:2:0), 1 shared U(Cb), V(Cr) value for 2x2 block pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *    | V00 | U00 | V02 | U02 |
 *    | V20 | U20 | V22 | U22 |
 */
#define ESP_FOURCC_NV21         ESP_FOURCC_TO_INT('N', 'V', '2', '1') /* 12 bpp, YVU 4:2:0  */

/**
 * NV16 (YUV 4:2:2), 1 shared U(Cb), V(Cr) value for 2 pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *    | U00 | V00 | U02 | V02 |
 *    | U10 | V10 | U12 | V12 |
 *    | U20 | V20 | U22 | V22 |
 *    | U30 | V30 | U32 | V32 |
 */
#define ESP_FOURCC_NV16         ESP_FOURCC_TO_INT('N', 'V', '1', '6') /* 16 bpp, YUV 4:2:2  */

/**
 * NV61 (YVU 4:2:2), 1 shared U(Cb), V(Cr) value for 2 pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *    | V00 | U00 | V02 | U02 |
 *    | V10 | U10 | V12 | U12 |
 *    | V20 | U20 | V22 | U22 |
 *    | V30 | U30 | V32 | U32 |
 */
#define ESP_FOURCC_NV61         ESP_FOURCC_TO_INT('N', 'V', '6', '1') /* 16 bpp, YVU 4:2:2  */

/**
 * NV24 (YUV 4:4:4)
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *    | U00 | V00 | U01 | V01 | U02 | V02 | U03 | V03 |
 *    | U10 | V10 | U11 | V11 | U12 | V12 | U13 | V13 |
 *    | U20 | V20 | U21 | V21 | U22 | V22 | U23 | V23 |
 *    | U30 | V30 | U31 | V31 | U32 | V32 | U33 | V33 |
 */
#define ESP_FOURCC_NV24         ESP_FOURCC_TO_INT('N', 'V', '2', '4') /* 24 bpp, YUV 4:4:4  */

/**
 * NV42 (YVU 4:4:4)
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *    | V00 | U00 | V01 | U01 | V02 | U02 | V03 | U03 |
 *    | V10 | U10 | V11 | U11 | V12 | U12 | V13 | U13 |
 *    | V20 | U20 | V21 | U21 | V22 | U22 | V23 | U23 |
 *    | V30 | U30 | V31 | U31 | V32 | U32 | V33 | U33 |
 */
#define ESP_FOURCC_NV42         ESP_FOURCC_TO_INT('N', 'V', '4', '2') /* 24 bpp, YVU 4:4:4  */


/*******************************************************************/
/*                     YCbCr-YUV Semi-planar format                */
/*   Two non contiguous planes - one Y, one Cr + Cb interleaved    */
/*******************************************************************/
/**
 * NV12M (YUV 4:2:0), 1 shared U(Cb), V(Cr) value for 2x2 block pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *
 *    | U00 | V00 | U02 | V02 |
 *    | U20 | V20 | U22 | V22 |
 */
#define ESP_FOURCC_NV12M        ESP_FOURCC_TO_INT('N', 'M', '1', '2') /* 12  YUV 4:2:0  */

/**
 * NV21M (YVU 4:2:0), 1 shared U(Cb), V(Cr) value for 2x2 block pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *
 *    | V00 | U00 | V02 | U02 |
 *    | V20 | U20 | V22 | U22 |
 */
#define ESP_FOURCC_NV21M        ESP_FOURCC_TO_INT('N', 'M', '2', '1') /* 12  YVU 4:2:0  */

/**
 * NV16M (YUV 4:2:2), 1 shared U(Cb), V(Cr) value for 2 pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *
 *    | U00 | V00 | U02 | V02 |
 *    | U10 | V10 | U12 | V12 |
 *    | U20 | V20 | U22 | V22 |
 *    | U30 | V30 | U32 | V32 |
 */
#define ESP_FOURCC_NV16M        ESP_FOURCC_TO_INT('N', 'M', '1', '6') /* 16  YUV 4:2:2  */

/**
 * NV61M (YVU 4:2:2), 1 shared U(Cb), V(Cr) value for 2 pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *
 *    | V00 | U00 | V02 | U02 |
 *    | V10 | U10 | V12 | U12 |
 *    | V20 | U20 | V22 | U22 |
 *    | V30 | U30 | V32 | U32 |
 */
#define ESP_FOURCC_NV61M        ESP_FOURCC_TO_INT('N', 'M', '6', '1') /* 16  YVU 4:2:2  */


/*******************************************************************/
/*                     YCbCr-YUV planar format                     */
/* Three contiguous planes, Y, Cb(U), Cr(V), U V is un-interleaved */
/*******************************************************************/
/**
 * YUV410P (YUV 4:1:0), 1 shared U(Cb), V(Cr) value for 16(4x4) pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *    | U00 |
 *    | V00 |
 */
#define ESP_FOURCC_YUV410P      ESP_FOURCC_TO_INT('Y', 'U', 'V', '9') /* 9 bpp, YUV 4:1:0, 1 shared U(Cb), V(Cr) value for 16 pixels */

/**
 * YVU410P (YVU 4:1:0), 1 shared U(Cb), V(Cr) value for 16(4x4) pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *    | V00 |
 *    | U00 |
 */
#define ESP_FOURCC_YVU410P      ESP_FOURCC_TO_INT('Y', 'V', 'U', '9') /* 9 bpp, YVU 4:1:0, 1 shared U(Cb), V(Cr) value for 16 pixels */

/**
 * YUV411P (YUV 4:1:1), 1 shared U(Cb), V(Cr) value for 4 horizontally pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | U00 |
 *    | U10 |
 *    | V00 |
 *    | V10 |
 */
#define ESP_FOURCC_YUV411P      ESP_FOURCC_TO_INT('4', '1', '1', 'P') /* 12 bpp, YVU411 planar, 1 shared U(Cb), V(Cr) value for 4 horizontally pixels */

/**
 * YUV420P (YUV 4:2:0)
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *    | U00 | U02 |
 *    | U20 | U22 |
 *    | V00 | V02 |
 *    | V20 | V22 |
 */
#define ESP_FOURCC_YUV420P      ESP_FOURCC_TO_INT('Y', 'U', '1', '2') /* 12 bpp, YUV 4:2:0, 1 shared U(Cb), V(Cr) value for 2x2 block pixels */

/**
 * YVU420P (YVU 4:2:0)
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *    | V00 | V02 |
 *    | V20 | V22 |
 *    | U00 | U02 |
 *    | U20 | U22 |
 */
#define ESP_FOURCC_YVU420P      ESP_FOURCC_TO_INT('Y', 'V', '1', '2') /* 12 bpp, YVU 4:2:0, 1 shared U(Cb), V(Cr) value for 2x2 block pixels */

/**
 * YUV422P (YUV 4:2:2)
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | U00 | U02 |
 *    | U10 | U12 |
 *    | V00 | V02 |
 *    | V10 | V12 |
 */
#define ESP_FOURCC_YUV422P      ESP_FOURCC_TO_INT('4', '2', '2', 'P') /* 16 bpp, YVU422 planar */

/**
 * YUV444P (YUV 4:4:4)
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | U00 | U01 | U02 | U03 |
 *    | U10 | U11 | U12 | U13 |
 *    | V00 | V01 | V02 | V03 |
 *    | V10 | V11 | V12 | V13 |
 */
#define ESP_FOURCC_YUV444P      ESP_FOURCC_TO_INT('4', '4', '4', 'P') /* 24 bpp, YVU444 planar */


/***********************************************************************/
/*                       YCbCr-YUV planar format                       */
/* Three non contiguous planes, Y, Cb(U), Cr(V), U V is un-interleaved */
/***********************************************************************/

/**
 * YUV420M (YUV 4:2:0), 1 shared U(Cb), V(Cr) value for 2x2 block pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *
 *    | U00 | U02 |
 *    | U20 | U22 |
 *
 *    | V00 | V02 |
 *    | V20 | V22 |
 */
#define ESP_FOURCC_YUV420M      ESP_FOURCC_TO_INT('Y', 'M', '1', '2') /* 12 bpp, YUV420 planar */

/**
 * YVU420M (YUV 4:2:0), 1 shared U(Cb), V(Cr) value for 2x2 block pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *    | Y20 | Y21 | Y22 | Y23 |
 *    | Y30 | Y31 | Y32 | Y33 |
 *
 *    | V00 | V02 |
 *    | V20 | V22 |
 *
 *    | U00 | U02 |
 *    | U20 | U22 |
 */
#define ESP_FOURCC_YVU420M      ESP_FOURCC_TO_INT('Y', 'M', '2', '1') /* 12 bpp, YVU420 planar */

/**
 * YUV422M (YUV 4:2:2), 1 shared U(Cb), V(Cr) value for 2 pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *
 *    | U00 | U02 |
 *    | U10 | U12 |
 *
 *    | V00 | V02 |
 *    | V10 | V12 |
 */
#define ESP_FOURCC_YUV422M      ESP_FOURCC_TO_INT('Y', 'M', '1', '6') /* 16  bpp, YUV422 planar */

/**
 * YVU422M (YUV 4:2:2), 1 shared U(Cb), V(Cr) value for 2 pixels
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *
 *    | V00 | V02 |
 *    | V10 | V12 |
 *
 *    | U00 | U02 |
 *    | U10 | U12 |
 */
#define ESP_FOURCC_YVU422M      ESP_FOURCC_TO_INT('Y', 'M', '6', '1') /* 16 bpp, YVU422 planar */

/**
 * YUV444M (YUV 4:4:4)
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *
 *    | U00 | U01 | U02 | U03 |
 *    | U10 | U11 | U12 | U13 |
 *
 *    | V00 | V01 | V02 | V03 |
 *    | V10 | V11 | V12 | V13 |
 */
#define ESP_FOURCC_YUV444M      ESP_FOURCC_TO_INT('Y', 'M', '2', '4') /* 24 bpp, YUV444 planar */

/**
 * YUV444M (YUV 4:4:4)
 * Memory Layout:
 *    | Y00 | Y01 | Y02 | Y03 |
 *    | Y10 | Y11 | Y12 | Y13 |
 *
 *    | V00 | V01 | V02 | V03 |
 *    | V10 | V11 | V12 | V13 |
 *
 *    | U00 | U01 | U02 | U03 |
 *    | U10 | U11 | U12 | U13 |
 */
#define ESP_FOURCC_YVU444M      ESP_FOURCC_TO_INT('Y', 'M', '4', '2') /* 24 bpp, YVU444 planar */


/**********************************************************************/
/*                             CMYK format                            */
/**********************************************************************/
#define ESP_FOURCC_CMYK         ESP_FOURCC_TO_INT('C', 'M', 'Y', 'K') /* 32 bpp, CMYK */


/**********************************************************************/
/*                             RAW format                            */
/**********************************************************************/
/**
 * RAW8
 * Memory Layout:
 *    Addr0    Addr1    Addr2    Addr3
 *    R0        R1       R2       R3
 */
#define ESP_FOURCC_RAW8         ESP_FOURCC_TO_INT('R', 'A', 'W', '8') /* 8 bpp, raw8 */

/**
 * RAW10
 * Memory Layout:
 *    Addr0      Addr1      Addr2      Addr3
 *    R0[7:0]    R0[1:0]    R1[7:0]    R1[1:0]
 */
#define ESP_FOURCC_RAW10        ESP_FOURCC_TO_INT('R', 'A', 'W', 'A') /* 10 bpp, raw10 */

/**
 * RAW12
 * Memory Layout:
 *    Addr0      Addr1      Addr2      Addr3
 *    R0[7:0]    R0[3:0]    R1[7:0]    R1[3:0]
 */
#define ESP_FOURCC_RAW12        ESP_FOURCC_TO_INT('R', 'A', 'W', 'C') /* 12 bpp, raw12 */

/**
 * RAW16
 * Memory Layout:
 *    Addr0      Addr1      Addr2      Addr3
 *    R0[7:0]    R0[7:0]    R1[7:0]    R1[7:0]
 */
#define ESP_FOURCC_RAW16        ESP_FOURCC_TO_INT('R', 'A', 'W', 'G') /* 16 bpp, raw16 */

/**
 * LUM4 (luminance 4 bits)
 * Memory Layout:
 *    Addr0  Addr1  Addr2  Addr3
 *    L0L1   L2L3   L4L5   L6L7
 */
#define ESP_FOURCC_LUM4         ESP_FOURCC_TO_INT('L', 'U', 'M', '4') /* 4 bpp, an 4-bit luminance-only format */

/**
 * LUM8 (luminance 8 bits)
 * Memory Layout:
 *    Addr0  Addr1  Addr2  Addr3
 *    L0     L1     L2     L3
 */
#define ESP_FOURCC_LUM8         ESP_FOURCC_TO_INT('L', 'U', 'M', '8') /* 8 bpp, an 8-bit luminance-only format */

/**
 * ALPHA4 (Alpha 4 bits)
 * Memory Layout:
 *    Addr0  Addr1  Addr2  Addr3
 *    A0A1   A2A3   A4A5   A6A7
 */
#define ESP_FOURCC_ALPHA4       ESP_FOURCC_TO_INT('A', 'L', 'P', '4') /* 4 bpp, an 4-bit alpha-only format */

/**
 * ALPHA8 (Alpha 8 bits)
 * Memory Layout:
 *    Addr0  Addr1  Addr2  Addr3
 *    A0     A1     A2     A3
 */
#define ESP_FOURCC_ALPHA8       ESP_FOURCC_TO_INT('A', 'L', 'P', '8') /* 8 bpp, an 8-bit alpha-only format */
