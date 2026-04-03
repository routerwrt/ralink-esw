// SPDX-License-Identifier: GPL-2.0
#ifndef _RALINK_ESW_DSA_H_
#define _RALINK_ESW_DSA_H_

#define RALINK_ESW_MDIO_TIMEOUT_US    1000

#define RALINK_ESW_PCR0                   0xc0
#define   RALINK_ESW_PCR0_PHY_ADDR        GENMASK(4, 0)
#define   RALINK_ESW_PCR0_PHY_REG         GENMASK(12, 8)
#define   RALINK_ESW_PCR0_WT_PHY_CMD      BIT(13)
#define   RALINK_ESW_PCR0_RD_PHY_CMD      BIT(14)
#define   RALINK_ESW_PCR0_WT_DATA         GENMASK(31, 16)

#define RALINK_ESW_PCR1                   0xc4
#define   RALINK_ESW_PCR1_WT_DONE         BIT(0)
#define   RALINK_ESW_PCR1_RD_RDY          BIT(1)
#define   RALINK_ESW_PCR1_RD_DATA         GENMASK(31, 16)

#define RALINK_ESW_FPA                    0x84
#define RALINK_ESW_FPA_FORCE_MODE_SHIFT	27
#define RALINK_ESW_FPA_FORCE_LNK_SHIFT	22
#define RALINK_ESW_FPA_FORCE_XFC_SHIFT	16
#define RALINK_ESW_FPA_FORCE_DPX_SHIFT	8
#define RALINK_ESW_FPA_FORCE_SPD_SHIFT	0

#define RALINK_ESW_FPA1                   0xc8
#define RALINK_ESW_FPA1_FORCE_LNK0		BIT(12)
#define RALINK_ESW_FPA1_FORCE_LNK1		BIT(13)
#define RALINK_ESW_FPA1_FORCE_EN0		BIT(10)
#define RALINK_ESW_FPA1_FORCE_EN1		BIT(11)
#define RALINK_ESW_FPA1_FORCE_DPX0		BIT(4)
#define RALINK_ESW_FPA1_FORCE_DPX1		BIT(5)

#define RALINK_ESW_FPA1_FORCE_XFC0_SHIFT	6
#define RALINK_ESW_FPA1_FORCE_XFC1_SHIFT	8
#define RALINK_ESW_FPA1_FORCE_SPD0_SHIFT	0
#define RALINK_ESW_FPA1_FORCE_SPD1_SHIFT	2

#define RALINK_ESW_FPA1_FORCE_XFC0		GENMASK(7, 6)
#define RALINK_ESW_FPA1_FORCE_XFC1		GENMASK(9, 8)
#define RALINK_ESW_FPA1_FORCE_SPD0		GENMASK(1, 0)
#define RALINK_ESW_FPA1_FORCE_SPD1		GENMASK(3, 2)

struct ralink_esw {
	struct device *dev;
	void __iomem *base;

	struct clk *clk;
	struct reset_control *rst_esw;
	struct reset_control *rst_ephy;

         /* MDIO */
        struct mutex mdio_lock;
        struct mii_bus *mdio_bus;

	struct dsa_switch *ds;
};

#endif /* _RALINK_ESW_DSA_H_ */
