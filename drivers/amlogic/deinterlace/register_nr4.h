/* ========== nr4_drt_regs register begi= */
#define NR4_DRT_CTRL                   ((0x2da4))
#define NR4_DRT_YSAD_GAIN               ((0x2da5))
#define NR4_DRT_CSAD_GAIN               ((0x2da6))
#define NR4_DRT_SAD_ALP_CORE            ((0x2da7))
#define NR4_DRT_ALP_MINMAX              ((0x2da8))
#define NR4_SNR_CTRL_REG                ((0x2da9))
#define NR4_SNR_ALPHA0_MAX_MIN          ((0x2daa))
#define NR4_ALP0C_ERR2CURV_LIMIT0       ((0x2dab))
#define NR4_ALP0C_ERR2CURV_LIMIT1       ((0x2dac))
#define NR4_ALP0Y_ERR2CURV_LIMIT0       ((0x2dad))
#define NR4_ALP0Y_ERR2CURV_LIMIT1       ((0x2dae))
#define NR4_SNR_ALPA1_RATE_AND_OFST     ((0x2daf))
#define NR4_SNR_ALPHA1_MAX_MIN          ((0x2db0))
#define NR4_ALP1C_ERR2CURV_LIMIT0       ((0x2db1))
#define NR4_ALP1C_ERR2CURV_LIMIT1       ((0x2db2))
#define NR4_ALP1Y_ERR2CURV_LIMIT0       ((0x2db3))
#define NR4_ALP1Y_ERR2CURV_LIMIT1       ((0x2db4))
#define NR4_MTN_CTRL                    ((0x2db5))
#define NR4_MTN_REF_PAR0                ((0x2db6))
#define NR4_MTN_REF_PAR1                ((0x2db7))
#define NR4_MCNR_LUMA_ENH_CTRL          ((0x2db8))
#define NR4_MCNR_LUMA_STAT_LIMTX        ((0x2db9))
#define NR4_MCNR_LUMA_STAT_LIMTY        ((0x2dba))
#define NR4_MCNR_LUMA_DIF_CALC          ((0x2dbb))
#define NR4_MCNR_LUMAPRE_CAL_PRAM       ((0x2dbc))
#define NR4_MCNR_LUMACUR_CAL_PRAM       ((0x2dbd))
#define NR4_MCNR_MV_CTRL_REG            ((0x2dbe))
#define NR4_MCNR_MV_GAIN0               ((0x2dbf))
#define NR4_MCNR_LMV_PARM               ((0x2dc0))
#define   NR4_MCNR_ALP0_REG             (0x2dc1)
#define   NR4_MCNR_ALP1_AND_BET0_REG    (0x2dc2)
#define   NR4_MCNR_BET1_AND_BET2_REG    (0x2dc3)
#define   NR4_MCNR_AC_DC_CRTL           (0x2dc4)
#define   NR4_MCNR_CM_CTRL0             (0x2dc5)
#define   NR4_MCNR_CM_PRAM              (0x2dc6)
#define   NR4_MCNR_CM_RSHFT_ALP0        (0x2dc7)
#define   NR4_MCNR_BLUE_CENT            (0x2dc8)
#define   NR4_MCNR_BLUE_GAIN_PAR0       (0x2dc9)
#define   NR4_MCNR_BLUE_GAIN_PAR1       (0x2dca)
#define   NR4_MCNR_CM_BLUE_CLIP0        (0x2dcb)
#define   NR4_MCNR_CM_BLUE_CLIP1        (0x2dcc)
#define   NR4_MCNR_GREEN_CENT           (0x2dcd)
#define   NR4_MCNR_GREEN_GAIN_PAR0      (0x2dce)
#define   NR4_MCNR_GREEN_GAIN_PAR1      (0x2dcf)
#define   NR4_MCNR_GREEN_CLIP0          (0x2dd0)
#define   NR4_MCNR_GREEN_CLIP2          (0x2dd1)
#define   NR4_MCNR_SKIN_CENT            (0x2dd2)
#define   NR4_MCNR_SKIN_GAIN_PAR0       (0x2dd3)
#define   NR4_MCNR_SKIN_GAIN_PAR1       (0x2dd4)
#define   NR4_MCNR_SKIN_CLIP0           (0x2dd5)
#define   NR4_MCNR_SKIN_CLIP1           (0x2dd6)
#define   NR4_MCNR_ALP1_GLB_CTRL        (0x2dd7)
#define   NR4_MCNR_DC2NORM_LUT0         (0x2dd8)
#define   NR4_MCNR_DC2NORM_LUT1         (0x2dd9)
#define   NR4_MCNR_DC2NORM_LUT2         (0x2dda)
#define   NR4_MCNR_AC2NORM_LUT0         (0x2ddb)
#define   NR4_MCNR_AC2NORM_LUT1         (0x2ddc)
#define   NR4_MCNR_AC2NORM_LUT2         (0x2ddd)
#define   NR4_MCNR_SAD2ALP0_LUT0        (0x2dde)
#define   NR4_MCNR_SAD2ALP0_LUT1        (0x2ddf)
#define   NR4_MCNR_SAD2ALP0_LUT2        (0x2de0)
#define   NR4_MCNR_SAD2ALP0_LUT3        (0x2de1)
#define   NR4_MCNR_SAD2ALP1_LUT0        (0x2de2)
#define   NR4_MCNR_SAD2ALP1_LUT1        (0x2de3)
#define   NR4_MCNR_SAD2ALP1_LUT2        (0x2de4)
#define   NR4_MCNR_SAD2ALP1_LUT3        (0x2de5)
#define   NR4_MCNR_SAD2BET0_LUT0        (0x2de6)
#define   NR4_MCNR_SAD2BET0_LUT1        (0x2de7)
#define   NR4_MCNR_SAD2BET0_LUT2        (0x2de8)
#define   NR4_MCNR_SAD2BET0_LUT3        (0x2de9)
#define   NR4_MCNR_SAD2BET1_LUT0        (0x2dea)
#define   NR4_MCNR_SAD2BET1_LUT1        (0x2deb)
#define   NR4_MCNR_SAD2BET1_LUT2        (0x2dec)
#define   NR4_MCNR_SAD2BET1_LUT3        (0x2ded)
#define   NR4_MCNR_SAD2BET2_LUT0        (0x2dee)
#define   NR4_MCNR_SAD2BET2_LUT1        (0x2def)
#define   NR4_MCNR_SAD2BET2_LUT2        (0x2df0)
#define   NR4_MCNR_SAD2BET2_LUT3        (0x2df1)
#define   NR4_MCNR_RO_U_SUM             (0x2df2)
#define   NR4_MCNR_RO_V_SUM             (0x2df3)
#define   NR4_MCNR_RO_GRDU_SUM          (0x2df4)
#define   NR4_MCNR_RO_GRDV_SUM          (0x2df5)
#define   NR4_TOP_CTRL                  (0x2dff)
#define   NR4_MCNR_SAD_GAIN             (0x3700)
#define   NR4_MCNR_LPF_CTRL             (0x3701)
#define   NR4_MCNR_BLD_VS3LUT0          (0x3702)
#define   NR4_MCNR_BLD_VS3LUT1          (0x3703)
#define   NR4_MCNR_BLD_VS3LUT2          (0x3704)
#define   NR4_MCNR_BLD_VS2LUT0          (0x3705)
#define   NR4_MCNR_BLD_VS2LUT1          (0x3706)
#define   NR4_COEFBLT_LUT10             (0x3707)
#define   NR4_COEFBLT_LUT11             (0x3708)
#define   NR4_COEFBLT_LUT12             (0x3709)
#define   NR4_COEFBLT_LUT20             (0x370a)
#define   NR4_COEFBLT_LUT21             (0x370b)
#define   NR4_COEFBLT_LUT22             (0x370c)
#define   NR4_COEFBLT_LUT30             (0x370d)
#define   NR4_COEFBLT_LUT31             (0x370e)
#define   NR4_COEFBLT_LUT32             (0x370f)
#define   NR4_COEFBLT_CONV              (0x3710)
#define   NR4_DBGWIN_YX0                (0x3711)
#define   NR4_DBGWIN_YX1                (0x3712)
#define   NR4_NM_X_CFG                  (0x3713)
#define   NR4_NM_Y_CFG                  (0x3714)
#define   NR4_NM_SAD_THD                (0x3715)
#define   NR4_MCNR_BANDSPLIT_PRAM       (0x3716)
#define   NR4_MCNR_ALP1_SGN_COR         (0x3717)
#define   NR4_MCNR_ALP1_SGN_PRAM        (0x3718)
#define   NR4_MCNR_ALP1_MVX_LUT1        (0x3719)
#define   NR4_MCNR_ALP1_MVX_LUT2        (0x371a)
#define   NR4_MCNR_ALP1_MVX_LUT3        (0x371b)
#define   NR4_MCNR_ALP1_LP_PRAM         (0x371c)
#define   NR4_MCNR_ALP1_SGN_LUT1        (0x371d)
#define   NR4_MCNR_ALP1_SGN_LUT2        (0x371e)
#define   NR4_RO_NM_SAD_SUM             (0x371f)
#define   NR4_RO_NM_SAD_CNT             (0x3720)
#define   NR4_RO_NM_VAR_SUM             (0x3721)
#define   NR4_RO_NM_VAR_SCNT            (0x3722)
#define   NR4_RO_NM_VAR_MIN_MAX         (0x3723)
#define   NR4_RO_NR4_DBGPIX_NUM         (0x3724)
#define   NR4_RO_NR4_BLDVS2_SUM         (0x3725)
#define   NR4_BLDVS3_SUM                (0x3726)
#define   NR4_COEF12_SUM                (0x3727)
#define   NR4_COEF123_SUM               (0x3728)
#define   NR_DB_FLT_CTRL                (0x3738)
#define   NR_DB_FLT_YC_THRD             (0x3739)
#define   NR_DB_FLT_RANDLUT             (0x373a)
#define   NR_DB_FLT_PXI_THRD            (0x373b)
#define   NR_DB_FLT_SEED_Y              (0x373c)
#define   NR_DB_FLT_SEED_V              (0x373e)
#define   NR_DB_FLT_SEED3               (0x373f)
#define   LBUF_TOP_CTRL                 (0x2fff)
