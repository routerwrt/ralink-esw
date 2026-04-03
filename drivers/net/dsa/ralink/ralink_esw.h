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
