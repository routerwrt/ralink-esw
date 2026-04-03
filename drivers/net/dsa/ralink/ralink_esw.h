// SPDX-License-Identifier: GPL-2.0
#ifndef _RALINK_ESW_DSA_H_
#define _RALINK_ESW_DSA_H_

struct ralink_esw {
	struct device *dev;
	void __iomem *base;

	struct clk *clk;
	struct reset_control *rst_esw;
	struct reset_control *rst_ephy;

	struct dsa_switch *ds;
};

#endif /* _RALINK_ESW_DSA_H_ */
