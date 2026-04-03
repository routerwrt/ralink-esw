// SPDX-License-Identifier: GPL-2.0
#ifndef _RALINK_ESW_DSA_H_
#define _RALINK_ESW_DSA_H_

#include <linux/if_vlan.h>

#define RALINK_ESW_MDIO_TIMEOUT_US         1000
#define RALINK_ESW_NUM_PORTS               7
#define RALINK_ESW_MAX_FRAME_LEN	1522
#define RALINK_ESW_MAX_MTU \
	(RALINK_ESW_MAX_FRAME_LEN - ETH_HLEN - VLAN_HLEN)

#define RALINK_ESW_ISR                     0x00
#define RALINK_ESW_IMR                     0x04
#define   RALINK_ESW_PORT_ST_CHG           BIT(26)

#define RALINK_ESW_PCR0                    0xc0
#define   RALINK_ESW_PCR0_PHY_ADDR         GENMASK(4, 0)
#define   RALINK_ESW_PCR0_PHY_REG          GENMASK(12, 8)
#define   RALINK_ESW_PCR0_WT_PHY_CMD       BIT(13)
#define   RALINK_ESW_PCR0_RD_PHY_CMD       BIT(14)
#define   RALINK_ESW_PCR0_WT_DATA          GENMASK(31, 16)

#define RALINK_ESW_PCR1                    0xc4
#define   RALINK_ESW_PCR1_WT_DONE          BIT(0)
#define   RALINK_ESW_PCR1_RD_RDY           BIT(1)
#define   RALINK_ESW_PCR1_RD_DATA          GENMASK(31, 16)

#define RALINK_ESW_FPA                     0x84
#define   RALINK_ESW_FPA_FORCE_MODE_SHIFT  27
#define   RALINK_ESW_FPA_FORCE_LNK_SHIFT   22
#define   RALINK_ESW_FPA_FORCE_XFC_SHIFT   16
#define   RALINK_ESW_FPA_FORCE_DPX_SHIFT   8
#define   RALINK_ESW_FPA_FORCE_SPD_SHIFT   0

#define RALINK_ESW_FPA1                    0xc8
#define   RALINK_ESW_FPA1_FORCE_LNK0	   BIT(12)
#define   RALINK_ESW_FPA1_FORCE_LNK1	   BIT(13)
#define   RALINK_ESW_FPA1_FORCE_EN0	   BIT(10)
#define   RALINK_ESW_FPA1_FORCE_EN1	   BIT(11)
#define   RALINK_ESW_FPA1_FORCE_DPX0	   BIT(4)
#define   RALINK_ESW_FPA1_FORCE_DPX1	   BIT(5)
#define   RALINK_ESW_FPA1_FORCE_XFC0_SHIFT 6
#define   RALINK_ESW_FPA1_FORCE_XFC1_SHIFT 8
#define   RALINK_ESW_FPA1_FORCE_SPD0_SHIFT 0
#define   RALINK_ESW_FPA1_FORCE_SPD1_SHIFT 2
#define   RALINK_ESW_FPA1_FORCE_XFC0	   GENMASK(7, 6)
#define   RALINK_ESW_FPA1_FORCE_XFC1	   GENMASK(9, 8)
#define   RALINK_ESW_FPA1_FORCE_SPD0	   GENMASK(1, 0)
#define   RALINK_ESW_FPA1_FORCE_SPD1	   GENMASK(3, 2)

#define RALINK_ESW_POA                     0x80
#define RALINK_ESW_POA_LINK_SHIFT          25

#define RALINK_ESW_POC0			   0x90
#define RALINK_ESW_POC0_DIS_PORT_SHIFT	   23

/* ---- Packed table registers (2 entries per 32-bit register) ---- */
#define RALINK_ESW_PVIDC_BASE             0x40  /* port PVID table */
#define RALINK_ESW_VLANI_BASE             0x50  /* VLAN ID (VID) table */
#define RALINK_ESW_VMSC_BASE              0x70  /* VLAN member */
#define RALINK_ESW_VUB_BASE               0x100 /* VLAN untag */

/* Packed lane helper (idx selects lane 0/1 within a 32-bit register) */
static inline u32 ralink_esw_tbl_reg(u32 base, u16 idx, u16 per_reg)
{
	return base + (idx / per_reg) * 4;
}

static inline u32 ralink_esw_tbl_mask(u16 idx, u16 per_reg, u16 width)
{
	u16 shift = (idx % per_reg) * width;

	return GENMASK(width - 1, 0) << shift;
}

/* Common packed widths */
#define RALINK_ESW_TBL_PER_REG_2          2
#define RALINK_ESW_TBL_WID_VID            16 /* 12-bit used, 4 reserved */
#define RALINK_ESW_TBL_WID_MSC            16 /* 8-bit used, 8 reserved */
#define RALINK_ESW_TBL_WID_UTG            16 /* 7-bit used, 9 reserved */

/* PVID: per port */
static inline u32 ralink_esw_pvidc_reg(unsigned int port)
{
	return ralink_esw_tbl_reg(RALINK_ESW_PVIDC_BASE, port, RALINK_ESW_TBL_PER_REG_2);
}
static inline u32 ralink_esw_pvidc_mask(unsigned int port)
{
	return ralink_esw_tbl_mask(port, RALINK_ESW_TBL_PER_REG_2, RALINK_ESW_TBL_WID_VID);
}

/* VLANI/VMSC/VUB: per VLAN table slot 0..15 */
static inline u32 ralink_esw_vlani_reg(unsigned int slot)
{
	return ralink_esw_tbl_reg(RALINK_ESW_VLANI_BASE, slot, RALINK_ESW_TBL_PER_REG_2);
}
static inline u32 ralink_esw_vlani_mask(unsigned int slot)
{
	return ralink_esw_tbl_mask(slot, RALINK_ESW_TBL_PER_REG_2, RALINK_ESW_TBL_WID_VID);
}

static inline u32 ralink_esw_vmsc_reg(unsigned int slot)
{
	return ralink_esw_tbl_reg(RALINK_ESW_VMSC_BASE, slot, RALINK_ESW_TBL_PER_REG_2);
}
static inline u32 ralink_esw_vmsc_mask(unsigned int slot)
{
	return ralink_esw_tbl_mask(slot, RALINK_ESW_TBL_PER_REG_2, RALINK_ESW_TBL_WID_MSC);
}

static inline u32 ralink_esw_vub_reg(unsigned int slot)
{
	return ralink_esw_tbl_reg(RALINK_ESW_VUB_BASE, slot, RALINK_ESW_TBL_PER_REG_2);
}
static inline u32 ralink_esw_vub_mask(unsigned int slot)
{
	return ralink_esw_tbl_mask(slot, RALINK_ESW_TBL_PER_REG_2, RALINK_ESW_TBL_WID_UTG);
}

struct ralink_esw {
	struct device *dev;
	void __iomem *base;

	struct clk *clk;
	struct reset_control *rst_esw;
	struct reset_control *rst_ephy;

         /* MDIO */
        struct mutex mdio_lock;
        struct mii_bus *mdio_bus;

        u32 link_state;

	struct dsa_switch *ds;
};

#endif /* _RALINK_ESW_DSA_H_ */
