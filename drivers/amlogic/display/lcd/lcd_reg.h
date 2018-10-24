/*
 * drivers/amlogic/display/lcd/lcd_reg.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#ifndef __LCD_REG_H__
#define __LCD_REG_H__
#include <linux/amlogic/iomap.h>

/* ********************************
 * register offset address define
 * ********************************* */
/* base & offset */
#if 0
#define LCD_REG_BASE_PERIPHS                      (0xc8834400L)
#define LCD_REG_BASE_CBUS                         (0xc1100000L)
#define LCD_REG_BASE_HIU                          (0xc883c000L)
#define LCD_REG_BASE_VCBUS                        (0xd0100000L)
#endif
#define LCD_REG_OFFSET_PERIPHS(reg)               ((reg << 2))
#define LCD_REG_OFFSET_CBUS(reg)                  ((reg << 2))
#define LCD_REG_OFFSET_HIU(reg)                   ((reg << 2))
#define LCD_REG_OFFSET_VCBUS(reg)                 ((reg << 2))

/* ********************************
 * PERIPHS: 0xc8834400
 * ******************************** */
#define PREG_PAD_GPIO1_EN_N                        0x0f
#define PREG_PAD_GPIO1_O                           0x10
#define PREG_PAD_GPIO1_I                           0x11
#define PREG_PAD_GPIO2_EN_N                        0x12
#define PREG_PAD_GPIO2_O                           0x13
#define PREG_PAD_GPIO2_I                           0x14
#define PREG_PAD_GPIO3_EN_N                        0x15
#define PREG_PAD_GPIO3_O                           0x16
#define PREG_PAD_GPIO3_I                           0x17
#define PREG_PAD_GPIO4_EN_N                        0x18
#define PREG_PAD_GPIO4_O                           0x19
#define PREG_PAD_GPIO4_I                           0x1a
#define PREG_PAD_GPIO5_EN_N                        0x1b
#define PREG_PAD_GPIO5_O                           0x1c
#define PREG_PAD_GPIO5_I                           0x1d

#define PERIPHS_PIN_MUX_0                          0x2c
#define PERIPHS_PIN_MUX_1                          0x2d
#define PERIPHS_PIN_MUX_2                          0x2e
#define PERIPHS_PIN_MUX_3                          0x2f
#define PERIPHS_PIN_MUX_4                          0x30
#define PERIPHS_PIN_MUX_5                          0x31
#define PERIPHS_PIN_MUX_6                          0x32
#define PERIPHS_PIN_MUX_7                          0x33
#define PERIPHS_PIN_MUX_8                          0x34
#define PERIPHS_PIN_MUX_9                          0x35
#define PERIPHS_PIN_MUX_10                         0x36
#define PERIPHS_PIN_MUX_11                         0x37
#define PERIPHS_PIN_MUX_12                         0x38

#define LCD_PERIPHS_REG_GX(addr)                  (addr & 0xff)

/* ********************************
 * HIU:  HHI_CBUS_BASE = 0x10
 * ******************************** */
#define HHI_GCLK_MPEG0                             0x1050
#define HHI_GCLK_MPEG1                             0x1051
#define HHI_GCLK_MPEG2                             0x1052
#define HHI_GCLK_OTHER                             0x1054

#define HHI_VIID_PLL_CNTL4                         0x1046
#define HHI_VIID_PLL_CNTL                          0x1047
#define HHI_VIID_PLL_CNTL2                         0x1048
#define HHI_VIID_PLL_CNTL3                         0x1049
#define HHI_VIID_CLK_DIV                           0x104a
    #define DAC0_CLK_SEL           28
    #define DAC1_CLK_SEL           24
    #define DAC2_CLK_SEL           20
    #define VCLK2_XD_RST           17
    #define VCLK2_XD_EN            16
    #define ENCL_CLK_SEL           12
    #define VCLK2_XD                0
#define HHI_VIID_CLK_CNTL                          0x104b
    #define VCLK2_EN               19
    #define VCLK2_CLK_IN_SEL       16
    #define VCLK2_SOFT_RST         15
    #define VCLK2_DIV12_EN          4
    #define VCLK2_DIV6_EN           3
    #define VCLK2_DIV4_EN           2
    #define VCLK2_DIV2_EN           1
    #define VCLK2_DIV1_EN           0
#define HHI_VIID_DIVIDER_CNTL                      0x104c
    #define DIV_CLK_IN_EN          16
    #define DIV_CLK_SEL            15
    #define DIV_POST_TCNT          12
    #define DIV_LVDS_CLK_EN        11
    #define DIV_LVDS_DIV2          10
    #define DIV_POST_SEL            8
    #define DIV_POST_SOFT_RST       7
    #define DIV_PRE_SEL             4
    #define DIV_PRE_SOFT_RST        3
    #define DIV_POST_RST            1
    #define DIV_PRE_RST             0
#define HHI_VID_CLK_DIV                            0x1059
    #define ENCI_CLK_SEL           28
    #define ENCP_CLK_SEL           24
    #define ENCT_CLK_SEL           20
    #define VCLK_XD_RST            17
    #define VCLK_XD_EN             16
    #define ENCL_CLK_SEL           12
    #define VCLK_XD1                8
    #define VCLK_XD0                0
#define HHI_VID_CLK_CNTL                           0x105f
#define HHI_VID_CLK_CNTL2                          0x1065
    #define HDMI_TX_PIXEL_GATE_VCLK  5
    #define VDAC_GATE_VCLK           4
    #define ENCL_GATE_VCLK           3
    #define ENCP_GATE_VCLK           2
    #define ENCT_GATE_VCLK           1
    #define ENCI_GATE_VCLK           0
#define HHI_VID_DIVIDER_CNTL                       0x1066
#define HHI_VID_PLL_CLK_DIV                        0x1068
#define HHI_EDP_APB_CLK_CNTL                       0x107b
#define HHI_EDP_APB_CLK_CNTL_M8M2                  0x1082
#define HHI_EDP_TX_PHY_CNTL0                       0x109c
#define HHI_EDP_TX_PHY_CNTL1                       0x109d
/* m8b */
#define HHI_VID_PLL_CNTL                           0x10c8
#define HHI_VID_PLL_CNTL2                          0x10c9
#define HHI_VID_PLL_CNTL3                          0x10ca
#define HHI_VID_PLL_CNTL4                          0x10cb
#define HHI_VID_PLL_CNTL5                          0x10cc
#define HHI_VID_PLL_CNTL6                          0x10cd
/* g9tv */
#define HHI_HDMI_PLL_CNTL                          0x10c8
#define HHI_HDMI_PLL_CNTL2                         0x10c9
#define HHI_HDMI_PLL_CNTL3                         0x10ca
#define HHI_HDMI_PLL_CNTL4                         0x10cb
#define HHI_HDMI_PLL_CNTL5                         0x10cc
#define HHI_HDMI_PLL_CNTL6                         0x10cd

#define HHI_DSI_LVDS_EDP_CNTL0                     0x10d1
#define HHI_DSI_LVDS_EDP_CNTL1                     0x10d2
#define HHI_DIF_CSI_PHY_CNTL0                      0x10d8
#define HHI_DIF_CSI_PHY_CNTL1                      0x10d9
#define HHI_DIF_CSI_PHY_CNTL2                      0x10da
#define HHI_DIF_CSI_PHY_CNTL3                      0x10db
#define HHI_DIF_CSI_PHY_CNTL4                      0x10dc
#define HHI_DIF_CSI_PHY_CNTL5                      0x10dd
#define HHI_LVDS_TX_PHY_CNTL0                      0x10de
#define HHI_LVDS_TX_PHY_CNTL1                      0x10df
#define HHI_VID2_PLL_CNTL                          0x10e0
#define HHI_VID2_PLL_CNTL2                         0x10e1
#define HHI_VID2_PLL_CNTL3                         0x10e2
#define HHI_VID2_PLL_CNTL4                         0x10e3
#define HHI_VID2_PLL_CNTL5                         0x10e4
#define HHI_VID2_PLL_CNTL6                         0x10e5
#define HHI_VID_LOCK_CLK_CNTL                      0x10f2

/* ********************************
 * HIU:  GXBB
 * ******************************** */
#if 0
#define HHI_VIID_CLK_DIV                           0x4a
#define HHI_VIID_CLK_CNTL                          0x4b
#define HHI_VIID_DIVIDER_CNTL                      0x4c
#define HHI_VID_CLK_DIV                            0x59
#define HHI_VID_CLK_CNTL                           0x5f
#define HHI_VID_CLK_CNTL2                          0x65
#define HHI_VID_DIVIDER_CNTL                       0x66
#define HHI_VID_PLL_CLK_DIV                        0x68
#define HHI_EDP_APB_CLK_CNTL                       0x82
#define HHI_EDP_TX_PHY_CNTL0                       0x9c
#define HHI_EDP_TX_PHY_CNTL1                       0x9d

#define HHI_HDMI_PLL_CNTL                          0xc8
#define HHI_HDMI_PLL_CNTL2                         0xc9
#define HHI_HDMI_PLL_CNTL3                         0xca
#define HHI_HDMI_PLL_CNTL4                         0xcb
#define HHI_HDMI_PLL_CNTL5                         0xcc
#define HHI_HDMI_PLL_CNTL6                         0xcd
#define HHI_HDMI_PLL_CNTL_I                        0xce
#define HHI_HDMI_PLL_CNTL7                         0xcf

#define HHI_DSI_LVDS_EDP_CNTL0                     0xd1
#define HHI_DSI_LVDS_EDP_CNTL1                     0xd2
#define HHI_DIF_CSI_PHY_CNTL0                      0xd8
#define HHI_DIF_CSI_PHY_CNTL1                      0xd9
#define HHI_DIF_CSI_PHY_CNTL2                      0xda
#define HHI_DIF_CSI_PHY_CNTL3                      0xdb
#define HHI_DIF_CSI_PHY_CNTL4                      0xdc
#define HHI_DIF_CSI_PHY_CNTL5                      0xdd
#define HHI_LVDS_TX_PHY_CNTL0                      0xde
#define HHI_LVDS_TX_PHY_CNTL1                      0xdf

#define HHI_VID2_PLL_CNTL                          0xe0
#define HHI_VID2_PLL_CNTL2                         0xe1
#define HHI_VID2_PLL_CNTL3                         0xe2
#define HHI_VID2_PLL_CNTL4                         0xe3
#define HHI_VID2_PLL_CNTL5                         0xe4
#define HHI_VID2_PLL_CNTL_I                        0xe5
#define HHI_VID_LOCK_CLK_CNTL                      0xf2
#else
#define LCD_HIU_REG_GX(addr)                       (addr & 0xff)
#endif

/* ********************************
 * Global control:  RESET_CBUS_BASE = 0x11
 * ******************************** */
#define VERSION_CTRL                               0x1100
#define RESET0_REGISTER                            0x1101
#define RESET1_REGISTER                            0x1102
#define RESET2_REGISTER                            0x1103
#define RESET3_REGISTER                            0x1104
#define RESET4_REGISTER                            0x1105
#define RESET5_REGISTER                            0x1106
#define RESET6_REGISTER                            0x1107
#define RESET7_REGISTER                            0x1108
#define RESET0_MASK                                0x1110
#define RESET1_MASK                                0x1111
#define RESET2_MASK                                0x1112
#define RESET3_MASK                                0x1113
#define RESET4_MASK                                0x1114
#define RESET5_MASK                                0x1115
#define RESET6_MASK                                0x1116
#define CRT_MASK                                   0x1117
#define RESET7_MASK                                0x1118

/* ********************************
 * TCON:  VCBUS_BASE = 0x14
 * ******************************** */
/* TCON_L register */
#define L_GAMMA_CNTL_PORT                          0x1400
#define L_GAMMA_DATA_PORT                          0x1401
#define L_GAMMA_ADDR_PORT                          0x1402
#define L_GAMMA_VCOM_HSWITCH_ADDR                  0x1403
#define L_RGB_BASE_ADDR                            0x1405
#define L_RGB_COEFF_ADDR                           0x1406
#define L_POL_CNTL_ADDR                            0x1407
#define L_DITH_CNTL_ADDR                           0x1408
#define L_GAMMA_PROBE_CTRL                         0x1409
/* read only */
#define L_GAMMA_PROBE_COLOR_L                      0x140a
#define L_GAMMA_PROBE_COLOR_H                      0x140b
#define L_GAMMA_PROBE_HL_COLOR                     0x140c
#define L_GAMMA_PROBE_POS_X                        0x140d
#define L_GAMMA_PROBE_POS_Y                        0x140e
#define L_STH1_HS_ADDR                             0x1410
#define L_STH1_HE_ADDR                             0x1411
#define L_STH1_VS_ADDR                             0x1412
#define L_STH1_VE_ADDR                             0x1413
#define L_STH2_HS_ADDR                             0x1414
#define L_STH2_HE_ADDR                             0x1415
#define L_STH2_VS_ADDR                             0x1416
#define L_STH2_VE_ADDR                             0x1417
#define L_OEH_HS_ADDR                              0x1418
#define L_OEH_HE_ADDR                              0x1419
#define L_OEH_VS_ADDR                              0x141a
#define L_OEH_VE_ADDR                              0x141b
#define L_VCOM_HSWITCH_ADDR                        0x141c
#define L_VCOM_VS_ADDR                             0x141d
#define L_VCOM_VE_ADDR                             0x141e
#define L_CPV1_HS_ADDR                             0x141f
#define L_CPV1_HE_ADDR                             0x1420
#define L_CPV1_VS_ADDR                             0x1421
#define L_CPV1_VE_ADDR                             0x1422
#define L_CPV2_HS_ADDR                             0x1423
#define L_CPV2_HE_ADDR                             0x1424
#define L_CPV2_VS_ADDR                             0x1425
#define L_CPV2_VE_ADDR                             0x1426
#define L_STV1_HS_ADDR                             0x1427
#define L_STV1_HE_ADDR                             0x1428
#define L_STV1_VS_ADDR                             0x1429
#define L_STV1_VE_ADDR                             0x142a
#define L_STV2_HS_ADDR                             0x142b
#define L_STV2_HE_ADDR                             0x142c
#define L_STV2_VS_ADDR                             0x142d
#define L_STV2_VE_ADDR                             0x142e
#define L_OEV1_HS_ADDR                             0x142f
#define L_OEV1_HE_ADDR                             0x1430
#define L_OEV1_VS_ADDR                             0x1431
#define L_OEV1_VE_ADDR                             0x1432
#define L_OEV2_HS_ADDR                             0x1433
#define L_OEV2_HE_ADDR                             0x1434
#define L_OEV2_VS_ADDR                             0x1435
#define L_OEV2_VE_ADDR                             0x1436
#define L_OEV3_HS_ADDR                             0x1437
#define L_OEV3_HE_ADDR                             0x1438
#define L_OEV3_VS_ADDR                             0x1439
#define L_OEV3_VE_ADDR                             0x143a
#define L_LCD_PWR_ADDR                             0x143b
#define L_LCD_PWM0_LO_ADDR                         0x143c
#define L_LCD_PWM0_HI_ADDR                         0x143d
#define L_LCD_PWM1_LO_ADDR                         0x143e
#define L_LCD_PWM1_HI_ADDR                         0x143f
#define L_INV_CNT_ADDR                             0x1440
#define L_TCON_MISC_SEL_ADDR                       0x1441
#define L_DUAL_PORT_CNTL_ADDR                      0x1442
#define MLVDS_CLK_CTL1_HI                          0x1443
#define MLVDS_CLK_CTL1_LO                          0x1444
/* [31:30] enable mlvds clocks
 * [24]    mlvds_clk_half_delay       24 // Bit 0
 * [23:0]  mlvds_clk_pattern           0 // Bit 23:0    */
#define L_TCON_DOUBLE_CTL                          0x1449
#define L_TCON_PATTERN_HI                          0x144a
#define L_TCON_PATTERN_LO                          0x144b
#define LDIM_BL_ADDR_PORT                          0x144e
#define LDIM_BL_DATA_PORT                          0x144f
#define L_DE_HS_ADDR                               0x1451
#define L_DE_HE_ADDR                               0x1452
#define L_DE_VS_ADDR                               0x1453
#define L_DE_VE_ADDR                               0x1454
#define L_HSYNC_HS_ADDR                            0x1455
#define L_HSYNC_HE_ADDR                            0x1456
#define L_HSYNC_VS_ADDR                            0x1457
#define L_HSYNC_VE_ADDR                            0x1458
#define L_VSYNC_HS_ADDR                            0x1459
#define L_VSYNC_HE_ADDR                            0x145a
#define L_VSYNC_VS_ADDR                            0x145b
#define L_VSYNC_VE_ADDR                            0x145c
/* bit 8 -- vfifo_mcu_enable
 * bit 7 -- halt_vs_de
 * bit 6 -- R8G8B8_format
 * bit 5 -- R6G6B6_format (round to 6 bits)
 * bit 4 -- R5G6B5_format
 * bit 3 -- dac_dith_sel
 * bit 2 -- lcd_mcu_enable_de     -- ReadOnly
 * bit 1 -- lcd_mcu_enable_vsync  -- ReadOnly
 * bit 0 -- lcd_mcu_enable */
#define L_LCD_MCU_CTL                              0x145d

/* **************************************************************************
 *  Dual port mLVDS registers
 ************************************************************************** */
/* bit 3 - enable_u_dual_mlvds_dp_clk
 * bit 2 - enable_u_map_mlvds_r_clk
 * bit 1 - enable_u_map_mlvds_l_clk
 * bit 0 - dual_mlvds_en */
#define DUAL_MLVDS_CTL                             0x1460
/* bit[12:0] - dual_mlvds_line_start */
#define DUAL_MLVDS_LINE_START                      0x1461
/* bit[12:0] - dual_mlvds_line_end */
#define DUAL_MLVDS_LINE_END                        0x1462
/* bit[12:0] - dual_mlvds_w_pixel_start_l */
#define DUAL_MLVDS_PIXEL_W_START_L                 0x1463
/* bit[12:0] - dual_mlvds_w_pixel_end_l */
#define DUAL_MLVDS_PIXEL_W_END_L                   0x1464
/* bit[12:0] - dual_mlvds_w_pixel_start_r */
#define DUAL_MLVDS_PIXEL_W_START_R                 0x1465
/* bit[12:0] - dual_mlvds_w_pixel_end_r */
#define DUAL_MLVDS_PIXEL_W_END_R                   0x1466
/* bit[12:0] - dual_mlvds_r_pixel_start_l */
#define DUAL_MLVDS_PIXEL_R_START_L                 0x1467
/* bit[12:0] - dual_mlvds_r_pixel_cnt_l */
#define DUAL_MLVDS_PIXEL_R_CNT_L                   0x1468
/* bit[12:0] - dual_mlvds_r_pixel_start_r */
#define DUAL_MLVDS_PIXEL_R_START_R                 0x1469
/* bit[12:0] - dual_mlvds_r_pixel_cnt_r */
#define DUAL_MLVDS_PIXEL_R_CNT_R                   0x146a
/* bit[15]   - v_inversion_en
 * bit[12:0] - v_inversion_pixel */
#define V_INVERSION_PIXEL                          0x1470
/* bit[15]   - v_inversion_sync_en
 * bit[12:0] - v_inversion_line */
#define V_INVERSION_LINE                           0x1471
/* bit[15:12]  - v_loop_r
 * bit[11:10]  - v_pattern_1_r
 * bit[9:8]    - v_pattern_0_r
 * bit[7:4]    - v_loop_l
 * bit[3:2]    - v_pattern_1_l
 * bit[1:0]    - v_pattern_0_l */
#define V_INVERSION_CONTROL                        0x1472
#define MLVDS2_CONTROL                             0x1474
   #define     mLVDS2_RESERVED  15
   #define     mLVDS2_double_pattern  14
   /* 13:8  // each channel has one bit */
   #define     mLVDS2_ins_reset  8
   #define     mLVDS2_dual_gate  7
   /* 0=6Bits, 1=8Bits */
   #define     mLVDS2_bit_num    6
   /* 0=3Pairs, 1=6Pairs */
   #define     mLVDS2_pair_num   5
   #define     mLVDS2_msb_first  4
   #define     mLVDS2_PORT_SWAP  3
   #define     mLVDS2_MLSB_SWAP  2
   #define     mLVDS2_PN_SWAP    1
   #define     mLVDS2_en         0
#define MLVDS2_CONFIG_HI                           0x1475
#define MLVDS2_CONFIG_LO                           0x1476
   /* Bit 31:29 */
   #define     mLVDS2_reset_offset         29
   /* Bit 28:23 */
   #define     mLVDS2_reset_length         23
   /* Bit 22:20 */
   #define     mLVDS2_config_reserved      20
   #define     mLVDS2_reset_start_bit12    19
   #define     mLVDS2_data_write_toggle    18
   #define     mLVDS2_data_write_ini       17
   #define     mLVDS2_data_latch_1_toggle  16
   #define     mLVDS2_data_latch_1_ini     15
   #define     mLVDS2_data_latch_0_toggle  14
   #define     mLVDS2_data_latch_0_ini     13
   /* 0=same as reset_0, 1=1 clock delay of reset_0 */
   #define     mLVDS2_reset_1_select       12
   /* Bit 11:0 */
   #define     mLVDS2_reset_start           0
#define MLVDS2_DUAL_GATE_WR_START                  0x1477
   /* Bit 12:0 */
   #define     mlvds2_dual_gate_wr_start    0
#define MLVDS2_DUAL_GATE_WR_END                    0x1478
   /* Bit 12:0 */
   #define     mlvds2_dual_gate_wr_end      0
#define MLVDS2_DUAL_GATE_RD_START                  0x1479
   /* Bit 12:0 */
   #define     mlvds2_dual_gate_rd_start    0
#define MLVDS2_DUAL_GATE_RD_END                    0x147a
   /* Bit 12:0 */
   #define     mlvds2_dual_gate_rd_end      0
#define MLVDS2_SECOND_RESET_CTL                    0x147b
   /* Bit 12:0 */
   #define     mLVDS2_2nd_reset_start       0
#define MLVDS2_DUAL_GATE_CTL_HI                    0x147c
#define MLVDS2_DUAL_GATE_CTL_LO                    0x147d
   /* Bit 7:0 */
   #define     mlvds2_tcon_field_en        24
   /* Bit 2:0 */
   #define     mlvds2_dual_gate_reserved   21
   #define     mlvds2_scan_mode_start_line_bit12 20
   /* Bit 3:0 */
   #define     mlvds2_scan_mode_odd        16
   /* Bit 3:0 */
   #define     mlvds2_scan_mode_even       12
   /* Bit 11:0 */
   #define     mlvds2_scan_mode_start_line  0
#define MLVDS2_RESET_CONFIG_HI                     0x147e
#define MLVDS2_RESET_CONFIG_LO                     0x147f
   #define     mLVDS2_reset_range_enable   31
   #define     mLVDS2_reset_range_inv      30
   #define     mLVDS2_reset_config_res1    29
   /* Bit 11:0 */
   #define     mLVDS2_reset_range_line_0   16
   /* Bit 2:0 */
   #define     mLVDS2_reset_config_res3    13
   /* Bit 11:0 */
   #define     mLVDS2_reset_range_line_1    0

/* ********************************************************************
 *  TCON register
 * ******************************************************************** */
#define GAMMA_CNTL_PORT                            0x1480
   #define  GAMMA_VCOM_POL    7
   #define  GAMMA_RVS_OUT     6
   /* Read Only */
   #define  ADR_RDY           5
   /* Read Only */
   #define  WR_RDY            4
   /* Read Only */
   #define  RD_RDY            3
   #define  GAMMA_TR          2
   #define  GAMMA_SET         1
   #define  GAMMA_EN          0
#define GAMMA_DATA_PORT                            0x1481
#define GAMMA_ADDR_PORT                            0x1482
   #define  H_RD              12
   #define  H_AUTO_INC        11
   #define  H_SEL_R           10
   #define  H_SEL_G           9
   #define  H_SEL_B           8
   /* 7:0 */
   #define  HADR_MSB          7
   #define  HADR              0
#define GAMMA_VCOM_HSWITCH_ADDR                    0x1483
#define RGB_BASE_ADDR                              0x1485
#define RGB_COEFF_ADDR                             0x1486
#define POL_CNTL_ADDR                              0x1487
   /* FOR DCLK OUTPUT */
   #define   DCLK_SEL             14
   /* FOR RGB format DVI output */
   #define   TCON_VSYNC_SEL_DVI   11
   /* FOR RGB format DVI output */
   #define   TCON_HSYNC_SEL_DVI   10
   /* FOR RGB format DVI output */
   #define   TCON_DE_SEL_DVI      9
   #define   CPH3_POL         8
   #define   CPH2_POL         7
   #define   CPH1_POL         6
   #define   TCON_DE_SEL      5
   #define   TCON_VS_SEL      4
   #define   TCON_HS_SEL      3
   #define   DE_POL           2
   #define   VS_POL           1
   #define   HS_POL           0
#define DITH_CNTL_ADDR                             0x1488
   #define  DITH10_EN         10
   #define  DITH8_EN          9
   #define  DITH_MD           8
   /* 7:4 */
   #define  DITH10_CNTL_MSB   7
   #define  DITH10_CNTL       4
   /* 3:0 */
   #define  DITH8_CNTL_MSB    3
   #define  DITH8_CNTL        0
/* Bit 1 highlight_en
 * Bit 0 probe_en */
#define GAMMA_PROBE_CTRL                           0x1489
/* read only
 * Bit [15:0]  probe_color[15:0] */
#define GAMMA_PROBE_COLOR_L                        0x148a
/* Read only
 * Bit 15: if true valid probed color
 * Bit [13:0]  probe_color[29:16] */
#define GAMMA_PROBE_COLOR_H                        0x148b
/* bit 15:0, 5:6:5 color */
#define GAMMA_PROBE_HL_COLOR                       0x148c
/* 12:0 pos_x */
#define GAMMA_PROBE_POS_X                          0x148d
/* 12:0 pos_y */
#define GAMMA_PROBE_POS_Y                          0x148e
#define STH1_HS_ADDR                               0x1490
#define STH1_HE_ADDR                               0x1491
#define STH1_VS_ADDR                               0x1492
#define STH1_VE_ADDR                               0x1493
#define STH2_HS_ADDR                               0x1494
#define STH2_HE_ADDR                               0x1495
#define STH2_VS_ADDR                               0x1496
#define STH2_VE_ADDR                               0x1497
#define OEH_HS_ADDR                                0x1498
#define OEH_HE_ADDR                                0x1499
#define OEH_VS_ADDR                                0x149a
#define OEH_VE_ADDR                                0x149b
#define VCOM_HSWITCH_ADDR                          0x149c
#define VCOM_VS_ADDR                               0x149d
#define VCOM_VE_ADDR                               0x149e
#define CPV1_HS_ADDR                               0x149f
#define CPV1_HE_ADDR                               0x14a0
#define CPV1_VS_ADDR                               0x14a1
#define CPV1_VE_ADDR                               0x14a2
#define CPV2_HS_ADDR                               0x14a3
#define CPV2_HE_ADDR                               0x14a4
#define CPV2_VS_ADDR                               0x14a5
#define CPV2_VE_ADDR                               0x14a6
#define STV1_HS_ADDR                               0x14a7
#define STV1_HE_ADDR                               0x14a8
#define STV1_VS_ADDR                               0x14a9
#define STV1_VE_ADDR                               0x14aa
#define STV2_HS_ADDR                               0x14ab
#define STV2_HE_ADDR                               0x14ac
#define STV2_VS_ADDR                               0x14ad
#define STV2_VE_ADDR                               0x14ae
#define OEV1_HS_ADDR                               0x14af
#define OEV1_HE_ADDR                               0x14b0
#define OEV1_VS_ADDR                               0x14b1
#define OEV1_VE_ADDR                               0x14b2
#define OEV2_HS_ADDR                               0x14b3
#define OEV2_HE_ADDR                               0x14b4
#define OEV2_VS_ADDR                               0x14b5
#define OEV2_VE_ADDR                               0x14b6
#define OEV3_HS_ADDR                               0x14b7
#define OEV3_HE_ADDR                               0x14b8
#define OEV3_VS_ADDR                               0x14b9
#define OEV3_VE_ADDR                               0x14ba
#define LCD_PWR_ADDR                               0x14bb
   #define      LCD_VDD        5
   #define      LCD_VBL        4
   #define      LCD_GPI_MSB    3
   #define      LCD_GPIO       0
#define LCD_PWM0_LO_ADDR                           0x14bc
#define LCD_PWM0_HI_ADDR                           0x14bd
#define LCD_PWM1_LO_ADDR                           0x14be
#define LCD_PWM1_HI_ADDR                           0x14bf
#define INV_CNT_ADDR                               0x14c0
   #define     INV_EN          4
   #define     INV_CNT_MSB     3
   #define     INV_CNT         0
#define TCON_MISC_SEL_ADDR                         0x14c1
   #define     STH2_SEL        12
   #define     STH1_SEL        11
   #define     OEH_SEL         10
   #define     VCOM_SEL         9
   #define     DB_LINE_SW       8
   #define     CPV2_SEL         7
   #define     CPV1_SEL         6
   #define     STV2_SEL         5
   #define     STV1_SEL         4
   #define     OEV_UNITE        3
   #define     OEV3_SEL         2
   #define     OEV2_SEL         1
   #define     OEV1_SEL         0
#define DUAL_PORT_CNTL_ADDR                        0x14c2
   #define     OUTPUT_YUV       15
   /* 14:12 */
   #define     DUAL_IDF         12
   /* 11:9 */
   #define     DUAL_ISF         9
   #define     LCD_ANALOG_SEL_CPH3   8
   #define     LCD_ANALOG_3PHI_CLK_SEL   7
   #define     LCD_LVDS_SEL54   6
   #define     LCD_LVDS_SEL27   5
   #define     LCD_TTL_SEL      4
   #define     DUAL_LVDC_EN     3
   #define     PORT_SWP         2
   #define     RGB_SWP          1
   #define     BIT_SWP          0
#define MLVDS_CONTROL                              0x14c3
   #define     mLVDS_RESERVED   15
   #define     mLVDS_double_pattern  14
   /* 13:8  // each channel has one bit */
   #define     mLVDS_ins_reset  8
   #define     mLVDS_dual_gate  7
   /* 0=6Bits, 1=8Bits */
   #define     mLVDS_bit_num    6
   /* 0=3Pairs, 1=6Pairs */
   #define     mLVDS_pair_num   5
   #define     mLVDS_msb_first  4
   #define     mLVDS_PORT_SWAP  3
   #define     mLVDS_MLSB_SWAP  2
   #define     mLVDS_PN_SWAP    1
   #define     mLVDS_en         0
#define MLVDS_RESET_PATTERN_HI                     0x14c4
#define MLVDS_RESET_PATTERN_LO                     0x14c5
   /* Bit 47:16 */
   #define     mLVDS_reset_pattern      0
#define MLVDS_RESET_PATTERN_EXT                    0x14c6
   /* Bit 15:0 */
   #define     mLVDS_reset_pattern_ext  0
#define MLVDS_CONFIG_HI                            0x14c7
#define MLVDS_CONFIG_LO                            0x14c8
   /* Bit 31:29 */
   #define     mLVDS_reset_offset         29
   /* Bit 28:23 */
   #define     mLVDS_reset_length         23
   /* Bit 22:20 */
   #define     mLVDS_config_reserved      20
   #define     mLVDS_reset_start_bit12    19
   #define     mLVDS_data_write_toggle    18
   #define     mLVDS_data_write_ini       17
   #define     mLVDS_data_latch_1_toggle  16
   #define     mLVDS_data_latch_1_ini     15
   #define     mLVDS_data_latch_0_toggle  14
   #define     mLVDS_data_latch_0_ini     13
   /* 0 - same as reset_0, 1 - 1 clock delay of reset_0 */
   #define     mLVDS_reset_1_select       12
   /* Bit 11:0 */
   #define     mLVDS_reset_start           0
#define TCON_DOUBLE_CTL                            0x14c9
   /* Bit 7:0 */
   #define     tcon_double_ini             8
   /* Bit 7:0 */
   #define     tcon_double_inv             0
#define TCON_PATTERN_HI                            0x14ca
#define TCON_PATTERN_LO                            0x14cb
   /* Bit 15:0 */
   #define     tcon_pattern_loop_data     16
   /* Bit 3:0 */
   #define     tcon_pattern_loop_start    12
   /* Bit 3:0 */
   #define     tcon_pattern_loop_end       8
   /* Bit 7:0 */
   #define     tcon_pattern_enable         0
#define TCON_CONTROL_HI                            0x14cc
#define TCON_CONTROL_LO                            0x14cd
   /* Bit 5:0 (enable pclk on TCON channel 7 to 2) */
   #define     tcon_pclk_enable           26
   /* Bit 1:0 (control phy clok divide 2,4,6,8) */
   #define     tcon_pclk_div              24
   /* Bit 23:0 (3 bit for each channel) */
   #define     tcon_delay                  0
#define LVDS_BLANK_DATA_HI                         0x14ce
#define LVDS_BLANK_DATA_LO                         0x14cf
   /* 31:30 */
   #define     LVDS_blank_data_reserved 30
   /* 29:20 */
   #define     LVDS_blank_data_r        20
   /* 19:10 */
   #define     LVDS_blank_data_g        10
   /*  9:0 */
   #define     LVDS_blank_data_b         0
#define LVDS_PACK_CNTL_ADDR                        0x14d0
   #define     LVDS_USE_TCON    7
   #define     LVDS_DUAL        6
   #define     PN_SWP           5
   #define     LSB_FIRST        4
   #define     LVDS_RESV        3
   #define     ODD_EVEN_SWP     2
   #define     LVDS_REPACK      0
/* New from M3 :
 * Bit 15:12 -- Enable OFFSET Double Generate(TOCN7-TCON4)
 * Bit 11:0 -- de_hs(old tcon) second offset_hs (new tcon) */
#define DE_HS_ADDR                                 0x14d1
/* New from M3 :
 * Bit 15:12 -- Enable OFFSET Double Generate(TOCN3-TCON0) */
#define DE_HE_ADDR                                 0x14d2
#define DE_VS_ADDR                                 0x14d3
#define DE_VE_ADDR                                 0x14d4
#define HSYNC_HS_ADDR                              0x14d5
#define HSYNC_HE_ADDR                              0x14d6
#define HSYNC_VS_ADDR                              0x14d7
#define HSYNC_VE_ADDR                              0x14d8
#define VSYNC_HS_ADDR                              0x14d9
#define VSYNC_HE_ADDR                              0x14da
#define VSYNC_VS_ADDR                              0x14db
#define VSYNC_VE_ADDR                              0x14dc
/* bit 8 -- vfifo_mcu_enable
 * bit 7 -- halt_vs_de
 * bit 6 -- R8G8B8_format
 * bit 5 -- R6G6B6_format (round to 6 bits)
 * bit 4 -- R5G6B5_format
 * bit 3 -- dac_dith_sel
 * bit 2 -- lcd_mcu_enable_de     -- ReadOnly
 * bit 1 -- lcd_mcu_enable_vsync  -- ReadOnly
 * bit 0 -- lcd_mcu_enable */
#define LCD_MCU_CTL                                0x14dd
/* ReadOnly
 *   R5G6B5 when R5G6B5_format
 *   G8R8   when R8G8B8_format
 *   G5R10  Other */
#define LCD_MCU_DATA_0                             0x14de
/* ReadOnly
 *   G8B8   when R8G8B8_format
 *   G5B10  Other */
#define LCD_MCU_DATA_1                             0x14df
/* LVDS */
#define LVDS_GEN_CNTL                              0x14e0
#define LVDS_PHY_CNTL0                             0x14e1
#define LVDS_PHY_CNTL1                             0x14e2
#define LVDS_PHY_CNTL2                             0x14e3
#define LVDS_PHY_CNTL3                             0x14e4
#define LVDS_PHY_CNTL4                             0x14e5
#define LVDS_PHY_CNTL5                             0x14e6
#define LVDS_SRG_TEST                              0x14e8
#define LVDS_BIST_MUX0                             0x14e9
#define LVDS_BIST_MUX1                             0x14ea
#define LVDS_BIST_FIXED0                           0x14eb
#define LVDS_BIST_FIXED1                           0x14ec
#define LVDS_BIST_CNTL0                            0x14ed
#define LVDS_CLKB_CLKA                             0x14ee
#define LVDS_PHY_CLK_CNTL                          0x14ef
#define LVDS_SER_EN                                0x14f0
#define LVDS_PHY_CNTL6                             0x14f1
#define LVDS_PHY_CNTL7                             0x14f2
#define LVDS_PHY_CNTL8                             0x14f3
#define MLVDS_CLK_CTL0_HI                          0x14f4
#define MLVDS_CLK_CTL0_LO                          0x14f5
   #define     mlvds_clk_pattern_reserved 31
   /* Bit 2:0 */
   #define     mpclk_dly                  28
   /* Bit 1:0 (control phy clok divide 2,4,6,8) */
   #define     mpclk_div                  26
   #define     use_mpclk                  25
   #define     mlvds_clk_half_delay       24
   /* Bit 23:0 */
   #define     mlvds_clk_pattern           0
#define MLVDS_DUAL_GATE_WR_START                   0x14f6
   /* Bit 12:0 */
   #define     mlvds_dual_gate_wr_start    0
#define MLVDS_DUAL_GATE_WR_END                     0x14f7
   /* Bit 12:0 */
   #define     mlvds_dual_gate_wr_end      0
#define MLVDS_DUAL_GATE_RD_START                   0x14f8
   /* Bit 12:0 */
   #define     mlvds_dual_gate_rd_start    0
#define MLVDS_DUAL_GATE_RD_END                     0x14f9
   /* Bit 12:0 */
   #define     mlvds_dual_gate_rd_end      0
#define MLVDS_SECOND_RESET_CTL                     0x14fa
   /* Bit 12:0 */
   #define     mLVDS_2nd_reset_start       0
#define MLVDS_DUAL_GATE_CTL_HI                     0x14fb
#define MLVDS_DUAL_GATE_CTL_LO                     0x14fc
   /* Bit 7:0 */
   #define     mlvds_tcon_field_en        24
   /* Bit 2:0 */
   #define     mlvds_dual_gate_reserved   21
   #define     mlvds_scan_mode_start_line_bit12 20
   /* Bit 3:0 */
   #define     mlvds_scan_mode_odd        16
   /* Bit 3:0 */
   #define     mlvds_scan_mode_even       12
   /* Bit 11:0 */
   #define     mlvds_scan_mode_start_line  0
#define MLVDS_RESET_CONFIG_HI                      0x14fd
#define MLVDS_RESET_CONFIG_LO                      0x14fe
   #define     mLVDS_reset_range_enable   31
   #define     mLVDS_reset_range_inv      30
   #define     mLVDS_reset_config_res1    29
   /* Bit 11:0 */
   #define     mLVDS_reset_range_line_0   16
   /* Bit 2:0 */
   #define     mLVDS_reset_config_res3    13
   /* Bit 11:0 */
   #define     mLVDS_reset_range_line_1    0

/* **************************************************************************
   Vbyone registers  (Note: no MinLVDS in G9tv, share the register)
 ************************************************************************** */
#define VBO_CTRL_L                                 0x1460
#define VBO_CTRL_H                                 0x1461
#define VBO_SOFT_RST                               0x1462
#define VBO_LANES                                  0x1463
#define VBO_VIN_CTRL                               0x1464
#define VBO_ACT_VSIZE                              0x1465
#define VBO_REGION_00                              0x1466
#define VBO_REGION_01                              0x1467
#define VBO_REGION_02                              0x1468
#define VBO_REGION_03                              0x1469
#define VBO_VBK_CTRL_0                             0x146a
#define VBO_VBK_CTRL_1                             0x146b
#define VBO_HBK_CTRL                               0x146c
#define VBO_PXL_CTRL                               0x146d
#define VBO_LANE_SKEW_L                            0x146e
#define VBO_LANE_SKEW_H                            0x146f
#define VBO_GCLK_LANE_L                            0x1470
#define VBO_GCLK_LANE_H                            0x1471
#define VBO_GCLK_MAIN                              0x1472
#define VBO_STATUS_L                               0x1473
#define VBO_STATUS_H                               0x1474
#define VBO_LANE_OUTPUT                            0x1475
#define LCD_PORT_SWAP                              0x1476
#define VBO_TMCHK_THRD_L                           0x1478
#define VBO_TMCHK_THRD_H                           0x1479
#define VBO_FSM_HOLDER_L                           0x147a
#define VBO_FSM_HOLDER_H                           0x147b
#define VBO_INTR_STATE_CTRL                        0x147c
#define VBO_INTR_UNMASK                            0x147d
#define VBO_TMCHK_HSYNC_STATE_L                    0x147e
#define VBO_TMCHK_HSYNC_STATE_H                    0x147f
#define VBO_TMCHK_VSYNC_STATE_L                    0x14f4
#define VBO_TMCHK_VSYNC_STATE_H                    0x14f5
#define VBO_TMCHK_VDE_STATE_L                      0x14f6
#define VBO_TMCHK_VDE_STATE_H                      0x14f7
#define VBO_INTR_STATE                             0x14f8

/* ********************************
 * Video Interface:  VENC_VCBUS_BASE = 0x1b
 * ******************************** */
#define VENC_INTCTRL                               0x1b6e

/* ********************************
 * ENCL:  VCBUS_BASE = 0x1c
 * ******************************** */
/* ENCL */
/* bit 15:8 -- vfifo2vd_vd_sel
 * bit 7 -- vfifo2vd_drop
 * bit 6:1 -- vfifo2vd_delay
 * bit 0 -- vfifo2vd_en */
#define ENCL_VFIFO2VD_CTL                          0x1c90
/* bit 12:0 -- vfifo2vd_pixel_start */
#define ENCL_VFIFO2VD_PIXEL_START                  0x1c91
/* bit 12:00 -- vfifo2vd_pixel_end */
#define ENCL_VFIFO2VD_PIXEL_END                    0x1c92
/* bit 10:0 -- vfifo2vd_line_top_start */
#define ENCL_VFIFO2VD_LINE_TOP_START               0x1c93
/* bit 10:00 -- vfifo2vd_line_top_end */
#define ENCL_VFIFO2VD_LINE_TOP_END                 0x1c94
/* bit 10:00 -- vfifo2vd_line_bot_start */
#define ENCL_VFIFO2VD_LINE_BOT_START               0x1c95
/* bit 10:00 -- vfifo2vd_line_bot_end */
#define ENCL_VFIFO2VD_LINE_BOT_END                 0x1c96
#define ENCL_VFIFO2VD_CTL2                         0x1c97
#define ENCL_TST_EN                                0x1c98
#define ENCL_TST_MDSEL                             0x1c99
#define ENCL_TST_Y                                 0x1c9a
#define ENCL_TST_CB                                0x1c9b
#define ENCL_TST_CR                                0x1c9c
#define ENCL_TST_CLRBAR_STRT                       0x1c9d
#define ENCL_TST_CLRBAR_WIDTH                      0x1c9e
#define ENCL_TST_VDCNT_STSET                       0x1c9f

/* ENCL registers */
#define ENCL_VIDEO_EN                              0x1ca0
#define ENCL_VIDEO_Y_SCL                           0x1ca1
#define ENCL_VIDEO_PB_SCL                          0x1ca2
#define ENCL_VIDEO_PR_SCL                          0x1ca3
#define ENCL_VIDEO_Y_OFFST                         0x1ca4
#define ENCL_VIDEO_PB_OFFST                        0x1ca5
#define ENCL_VIDEO_PR_OFFST                        0x1ca6
/* ----- Video mode */
#define ENCL_VIDEO_MODE                            0x1ca7
#define ENCL_VIDEO_MODE_ADV                        0x1ca8
/* --------------- Debug pins */
#define ENCL_DBG_PX_RST                            0x1ca9
#define ENCL_DBG_LN_RST                            0x1caa
#define ENCL_DBG_PX_INT                            0x1cab
#define ENCL_DBG_LN_INT                            0x1cac
/* ----------- Video Advanced setting */
#define ENCL_VIDEO_YFP1_HTIME                      0x1cad
#define ENCL_VIDEO_YFP2_HTIME                      0x1cae
#define ENCL_VIDEO_YC_DLY                          0x1caf
#define ENCL_VIDEO_MAX_PXCNT                       0x1cb0
#define ENCL_VIDEO_HAVON_END                       0x1cb1
#define ENCL_VIDEO_HAVON_BEGIN                     0x1cb2
#define ENCL_VIDEO_VAVON_ELINE                     0x1cb3
#define ENCL_VIDEO_VAVON_BLINE                     0x1cb4
#define ENCL_VIDEO_HSO_BEGIN                       0x1cb5
#define ENCL_VIDEO_HSO_END                         0x1cb6
#define ENCL_VIDEO_VSO_BEGIN                       0x1cb7
#define ENCL_VIDEO_VSO_END                         0x1cb8
#define ENCL_VIDEO_VSO_BLINE                       0x1cb9
#define ENCL_VIDEO_VSO_ELINE                       0x1cba
#define ENCL_VIDEO_MAX_LNCNT                       0x1cbb
#define ENCL_VIDEO_BLANKY_VAL                      0x1cbc
#define ENCL_VIDEO_BLANKPB_VAL                     0x1cbd
#define ENCL_VIDEO_BLANKPR_VAL                     0x1cbe
#define ENCL_VIDEO_HOFFST                          0x1cbf
#define ENCL_VIDEO_VOFFST                          0x1cc0
#define ENCL_VIDEO_RGB_CTRL                        0x1cc1
#define ENCL_VIDEO_FILT_CTRL                       0x1cc2
#define ENCL_VIDEO_OFLD_VPEQ_OFST                  0x1cc3
#define ENCL_VIDEO_OFLD_VOAV_OFST                  0x1cc4
#define ENCL_VIDEO_MATRIX_CB                       0x1cc5
#define ENCL_VIDEO_MATRIX_CR                       0x1cc6
#define ENCL_VIDEO_RGBIN_CTRL                      0x1cc7
#define ENCL_MAX_LINE_SWITCH_POINT                 0x1cc8
#define ENCL_DACSEL_0                              0x1cc9
#define ENCL_DACSEL_1                              0x1cca

/* ********************************
 * ENCT:  VCBUS_BASE = 0x1c
 * ******************************** */
/* ENCT */
/* bit 15:8 -- vfifo2vd_vd_sel
 * bit 7 -- vfifo2vd_drop
 * bit 6:1 -- vfifo2vd_delay
 * bit 0 -- vfifo2vd_en */
#define ENCT_VFIFO2VD_CTL                          0x1c20
/* bit 12:0 -- vfifo2vd_pixel_start */
#define ENCT_VFIFO2VD_PIXEL_START                  0x1c21
/* bit 12:00 -- vfifo2vd_pixel_end */
#define ENCT_VFIFO2VD_PIXEL_END                    0x1c22
/* bit 10:0 -- vfifo2vd_line_top_start */
#define ENCT_VFIFO2VD_LINE_TOP_START               0x1c23
/* bit 10:00 -- vfifo2vd_line_top_end */
#define ENCT_VFIFO2VD_LINE_TOP_END                 0x1c24
/* bit 10:00 -- vfifo2vd_line_bot_start */
#define ENCT_VFIFO2VD_LINE_BOT_START               0x1c25
/* bit 10:00 -- vfifo2vd_line_bot_end */
#define ENCT_VFIFO2VD_LINE_BOT_END                 0x1c26
#define ENCT_VFIFO2VD_CTL2                         0x1c27
#define ENCT_TST_EN                                0x1c28
#define ENCT_TST_MDSEL                             0x1c29
#define ENCT_TST_Y                                 0x1c2a
#define ENCT_TST_CB                                0x1c2b
#define ENCT_TST_CR                                0x1c2c
#define ENCT_TST_CLRBAR_STRT                       0x1c2d
#define ENCT_TST_CLRBAR_WIDTH                      0x1c2e
#define ENCT_TST_VDCNT_STSET                       0x1c2f

/* ENCT registers */
#define ENCT_VIDEO_EN                              0x1c60
#define ENCT_VIDEO_Y_SCL                           0x1c61
#define ENCT_VIDEO_PB_SCL                          0x1c62
#define ENCT_VIDEO_PR_SCL                          0x1c63
#define ENCT_VIDEO_Y_OFFST                         0x1c64
#define ENCT_VIDEO_PB_OFFST                        0x1c65
#define ENCT_VIDEO_PR_OFFST                        0x1c66
/* ----- Video mode */
#define ENCT_VIDEO_MODE                            0x1c67
#define ENCT_VIDEO_MODE_ADV                        0x1c68
/* --------------- Debug pins */
#define ENCT_DBG_PX_RST                            0x1c69
#define ENCT_DBG_LN_RST                            0x1c6a
#define ENCT_DBG_PX_INT                            0x1c6b
#define ENCT_DBG_LN_INT                            0x1c6c
/* ----------- Video Advanced setting */
#define ENCT_VIDEO_YFP1_HTIME                      0x1c6d
#define ENCT_VIDEO_YFP2_HTIME                      0x1c6e
#define ENCT_VIDEO_YC_DLY                          0x1c6f
#define ENCT_VIDEO_MAX_PXCNT                       0x1c70
#define ENCT_VIDEO_HAVON_END                       0x1c71
#define ENCT_VIDEO_HAVON_BEGIN                     0x1c72
#define ENCT_VIDEO_VAVON_ELINE                     0x1c73
#define ENCT_VIDEO_VAVON_BLINE                     0x1c74
#define ENCT_VIDEO_HSO_BEGIN                       0x1c75
#define ENCT_VIDEO_HSO_END                         0x1c76
#define ENCT_VIDEO_VSO_BEGIN                       0x1c77
#define ENCT_VIDEO_VSO_END                         0x1c78
#define ENCT_VIDEO_VSO_BLINE                       0x1c79
#define ENCT_VIDEO_VSO_ELINE                       0x1c7a
#define ENCT_VIDEO_MAX_LNCNT                       0x1c7b
#define ENCT_VIDEO_BLANKY_VAL                      0x1c7c
#define ENCT_VIDEO_BLANKPB_VAL                     0x1c7d
#define ENCT_VIDEO_BLANKPR_VAL                     0x1c7e
#define ENCT_VIDEO_HOFFST                          0x1c7f
#define ENCT_VIDEO_VOFFST                          0x1c80
#define ENCT_VIDEO_RGB_CTRL                        0x1c81
#define ENCT_VIDEO_FILT_CTRL                       0x1c82
#define ENCT_VIDEO_OFLD_VPEQ_OFST                  0x1c83
#define ENCT_VIDEO_OFLD_VOAV_OFST                  0x1c84
#define ENCT_VIDEO_MATRIX_CB                       0x1c85
#define ENCT_VIDEO_MATRIX_CR                       0x1c86
#define ENCT_VIDEO_RGBIN_CTRL                      0x1c87
#define ENCT_MAX_LINE_SWITCH_POINT                 0x1c88
#define ENCT_DACSEL_0                              0x1c89
#define ENCT_DACSEL_1                              0x1c8a

/* ********************************
 * Video post-processing:  VPP_VCBUS_BASE = 0x1d
 * ******************************** */
/* Bit 31  vd1_bgosd_exchange_en for preblend
// Bit 30  vd1_bgosd_exchange_en for postblend
// bit 28   color management enable
// Bit 27,  reserved
// Bit 26:18, reserved
// Bit 17, osd2 enable for preblend
// Bit 16, osd1 enable for preblend
// Bit 15, reserved
// Bit 14, vd1 enable for preblend
// Bit 13, osd2 enable for postblend
// Bit 12, osd1 enable for postblend
// Bit 11, reserved
// Bit 10, vd1 enable for postblend
// Bit 9,  if true, osd1 is alpha premultipiled
// Bit 8,  if true, osd2 is alpha premultipiled
// Bit 7,  postblend module enable
// Bit 6,  preblend module enable
// Bit 5,  if true, osd2 foreground compared with osd1 in preblend
// Bit 4,  if true, osd2 foreground compared with osd1 in postblend
// Bit 3,
// Bit 2,  if true, disable resetting async fifo every vsync, otherwise every
//           vsync the aync fifo will be reseted.
// Bit 1,
// Bit 0    if true, the output result of VPP is saturated */
#define VPP2_MISC                                  0x1926
/* Bit 31  vd1_bgosd_exchange_en for preblend
// Bit 30  vd1_bgosd_exchange_en for postblend
// Bit 28   color management enable
// Bit 27,  if true, vd2 use viu2 output as the input, otherwise use normal
//            vd2 from memory
// Bit 26:18, vd2 alpha
// Bit 17, osd2 enable for preblend
// Bit 16, osd1 enable for preblend
// Bit 15, vd2 enable for preblend
// Bit 14, vd1 enable for preblend
// Bit 13, osd2 enable for postblend
// Bit 12, osd1 enable for postblend
// Bit 11, vd2 enable for postblend
// Bit 10, vd1 enable for postblend
// Bit 9,  if true, osd1 is alpha premultipiled
// Bit 8,  if true, osd2 is alpha premultipiled
// Bit 7,  postblend module enable
// Bit 6,  preblend module enable
// Bit 5,  if true, osd2 foreground compared with osd1 in preblend
// Bit 4,  if true, osd2 foreground compared with osd1 in postblend
// Bit 3,
// Bit 2,  if true, disable resetting async fifo every vsync, otherwise every
//           vsync the aync fifo will be reseted.
// Bit 1,
// Bit 0	if true, the output result of VPP is saturated */
#define VPP_MISC                                   0x1d26

#define VPP2_POSTBLEND_H_SIZE                      0x1921
#define VPP_POSTBLEND_H_SIZE                       0x1d21
/* Bit 3	minus black level enable for vadj2
 * Bit 2	Video adjustment enable for vadj2
 * Bit 1	minus black level enable for vadj1
 * Bit 0	Video adjustment enable for vadj1 */
#define VPP_VADJ_CTRL                              0x1d40
/* Bit 16:8  brightness, signed value
 * Bit 7:0	contrast, unsigned value, contrast from  0 <= contrast <2 */
#define VPP_VADJ1_Y                                0x1d41
/* cb' = cb*ma + cr*mb
 * cr' = cb*mc + cr*md
 * all are bit 9:0, signed value, -2 < ma/mb/mc/md < 2 */
#define VPP_VADJ1_MA_MB                            0x1d42
#define VPP_VADJ1_MC_MD                            0x1d43
/* Bit 16:8  brightness, signed value
 * Bit 7:0   contrast, unsigned value, contrast from  0 <= contrast <2 */
#define VPP_VADJ2_Y                                0x1d44
/* cb' = cb*ma + cr*mb
 * cr' = cb*mc + cr*md
 * all are bit 9:0, signed value, -2 < ma/mb/mc/md < 2 */
#define VPP_VADJ2_MA_MB                            0x1d45
#define VPP_VADJ2_MC_MD                            0x1d46

#define VPP_MATRIX_CTRL                            0x1d5f
/* Bit 28:16 coef00 */
/* Bit 12:0  coef01 */
#define VPP_MATRIX_COEF00_01                       0x1d60
/* Bit 28:16 coef02 */
/* Bit 12:0  coef10 */
#define VPP_MATRIX_COEF02_10                       0x1d61
/* Bit 28:16 coef11 */
/* Bit 12:0  coef12 */
#define VPP_MATRIX_COEF11_12                       0x1d62
/* Bit 28:16 coef20 */
/* Bit 12:0  coef21 */
#define VPP_MATRIX_COEF20_21                       0x1d63
#define VPP_MATRIX_COEF22                          0x1d64
/* Bit 26:16 offset0 */
/* Bit 10:0  offset1 */
#define VPP_MATRIX_OFFSET0_1                       0x1d65
/* Bit 10:0  offset2 */
#define VPP_MATRIX_OFFSET2                         0x1d66
/* Bit 26:16 pre_offset0 */
/* Bit 10:0  pre_offset1 */
#define VPP_MATRIX_PRE_OFFSET0_1                   0x1d67
/* Bit 10:0  pre_offset2 */
#define VPP_MATRIX_PRE_OFFSET2                     0x1d68

/* ********************************
 * VPU:  VPU_VCBUS_BASE = 0x27
 * ******************************** */
/* [31:11] Reserved.
 * [10: 8] cntl_viu_vdin_sel_data. Select VIU to VDIN data path,
    must clear it first before changing the path selection:
 *          3'b000=Disable VIU to VDIN path;
 *          3'b001=Enable VIU of ENC_I domain to VDIN;
 *          3'b010=Enable VIU of ENC_P domain to VDIN;
 *          3'b100=Enable VIU of ENC_T domain to VDIN;
 * [ 6: 4] cntl_viu_vdin_sel_clk. Select which clock to VDIN path,
    must clear it first before changing the clock:
 *          3'b000=Disable VIU to VDIN clock;
 *          3'b001=Select encI clock to VDIN;
 *          3'b010=Select encP clock to VDIN;
 *          3'b100=Select encT clock to VDIN;
 * [ 3: 2] cntl_viu2_sel_venc. Select which one of the encI/P/T
    that VIU2 connects to:
 *         0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT.
 * [ 1: 0] cntl_viu1_sel_venc. Select which one of the encI/P/T
    that VIU1 connects to:
 *         0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT. */
#define VPU_VIU_VENC_MUX_CTRL                      0x271a

/* Bit  6 RW, gclk_mpeg_vpu_misc
 * Bit  5 RW, gclk_mpeg_venc_l_top
 * Bit  4 RW, gclk_mpeg_vencl_int
 * Bit  3 RW, gclk_mpeg_vencp_int
 * Bit  2 RW, gclk_mpeg_vi2_top
 * Bit  1 RW, gclk_mpeg_vi_top
 * Bit  0 RW, gclk_mpeg_venc_p_top */
#define VPU_CLK_GATE                               0x2723
#define VPU_MISC_CTRL                              0x2740

#define VPU_VLOCK_CTRL                             0x3000
#define VPU_VLOCK_ADJ_EN_SYNC_CTRL                 0x301d
#define VPU_VLOCK_GCLK_EN                          0x301e
/* ******************************** */

extern int lcd_ioremap(void);
extern unsigned int lcd_vcbus_read(unsigned int _reg);
extern void lcd_vcbus_write(unsigned int _reg, unsigned int _value);
extern void lcd_vcbus_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len);
extern unsigned int lcd_vcbus_getb(unsigned int reg,
		unsigned int _start, unsigned int _len);
extern void lcd_vcbus_set_mask(unsigned int reg, unsigned int _mask);
extern void lcd_vcbus_clr_mask(unsigned int reg, unsigned int _mask);

extern unsigned int lcd_hiu_read(unsigned int _reg);
extern void lcd_hiu_write(unsigned int _reg, unsigned int _value);
extern void lcd_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len);
extern unsigned int lcd_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len);
extern void lcd_hiu_set_mask(unsigned int _reg, unsigned int _mask);
extern void lcd_hiu_clr_mask(unsigned int _reg, unsigned int _mask);

extern unsigned int lcd_cbus_read(unsigned int _reg);
extern void lcd_cbus_write(unsigned int _reg, unsigned int _value);
extern void lcd_cbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len);

extern unsigned int lcd_periphs_read(unsigned int _reg);
extern void lcd_periphs_write(unsigned int _reg, unsigned int _value);
extern void lcd_pinmux_set_mask(unsigned int _reg, unsigned int _mask);
extern void lcd_pinmux_clr_mask(unsigned int _reg, unsigned int _mask);

#endif
