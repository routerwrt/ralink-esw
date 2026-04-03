
// SPDX-License-Identifier: GPL-2.0
/*
 * DSA switch driver for the classic Ralink/MediaTek embedded switch (ESW)
 * found in RT5350/MT76x8 class SoCs.
 *
 */

#include <linux/clk.h>
#include <linux/of_mdio.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <net/dsa.h>

#include "ralink_esw.h"

static inline u32 ralink_esw_r32(struct ralink_esw *esw, u32 reg)
{
    return readl_relaxed(esw->base + reg);
}

static inline void ralink_esw_w32(struct ralink_esw *esw, u32 reg, u32 val)
{
    writel_relaxed(val, esw->base + reg);
}

static inline void ralink_esw_rmw(struct ralink_esw *esw, u32 reg, u32 mask, u32 set)
{
	u32 val = ralink_esw_r32(esw, reg);

	val &= ~mask;
	val |= (set & mask);

	ralink_esw_w32(esw, val, reg);
}

/*
 * PCR1.RD_RDY and PCR1.WT_DONE are read-clear completion bits.
 * Read PCR1 before starting a transaction to drop any stale completion
 * state, then read it again after completion to fetch data / acknowledge
 * completion.
 */
static inline void ralink_esw_mdio_ack(struct ralink_esw *esw)
{
    ralink_esw_r32(esw, RALINK_ESW_PCR1);
}

static int ralink_esw_mdio_wait(struct ralink_esw *esw, u32 mask)
{
    u32 val;

    return readl_poll_timeout(esw->base + RALINK_ESW_PCR1, val,
                  val & mask, 1,
                  RALINK_ESW_MDIO_TIMEOUT_US);
}

static int ralink_esw_phy_read(struct ralink_esw *esw, int phy, int reg, u16 *val)
{
    u32 pcr1;
    int ret;

    mutex_lock(&esw->mdio_lock);

    ralink_esw_mdio_ack(esw);

    ralink_esw_w32(esw, RALINK_ESW_PCR0,
        RALINK_ESW_PCR0_RD_PHY_CMD |
        FIELD_PREP(RALINK_ESW_PCR0_PHY_REG, reg) |
        FIELD_PREP(RALINK_ESW_PCR0_PHY_ADDR, phy));

    ret = ralink_esw_mdio_wait(esw, RALINK_ESW_PCR1_RD_RDY);
    if (ret) {
        dev_err(esw->dev, "MDIO read timeout: phy=%d reg=%d\n",
            phy, reg);
        goto out;
    }

    pcr1 = ralink_esw_r32(esw, RALINK_ESW_PCR1);
    *val = FIELD_GET(RALINK_ESW_PCR1_RD_DATA, pcr1);

out:
    mutex_unlock(&esw->mdio_lock);

    return ret;
}

static int ralink_esw_phy_write(struct ralink_esw *esw, int phy, int reg, u16 val)
{
    int ret;

    mutex_lock(&esw->mdio_lock);

    ralink_esw_mdio_ack(esw);

    ralink_esw_w32(esw, RALINK_ESW_PCR0,
        RALINK_ESW_PCR0_WT_PHY_CMD |
        FIELD_PREP(RALINK_ESW_PCR0_WT_DATA, val) |
        FIELD_PREP(RALINK_ESW_PCR0_PHY_REG, reg) |
        FIELD_PREP(RALINK_ESW_PCR0_PHY_ADDR, phy));

    ret = ralink_esw_mdio_wait(esw, RALINK_ESW_PCR1_WT_DONE);
    if (ret) {
        dev_err(esw->dev, "MDIO write timeout: phy=%d reg=%d\n",
            phy, reg);
        goto out;
    }

    ralink_esw_mdio_ack(esw);

out:
    mutex_unlock(&esw->mdio_lock);

    return ret;
}

static int ralink_esw_mdio_bus_read(struct mii_bus *bus, int addr, int regnum)
{
    struct ralink_esw *esw = bus->priv;
    u16 val;
    int ret;

    ret = ralink_esw_phy_read(esw, addr, regnum, &val);
    if (ret)
        return ret;

    return val;
}

static int ralink_esw_mdio_bus_write(struct mii_bus *bus, int addr, int regnum,
                  u16 val)
{
    struct ralink_esw *esw = bus->priv;

    return ralink_esw_phy_write(esw, addr, regnum, val);
}

static int ralink_esw_mdio_register(struct ralink_esw *esw)
{
    struct device_node *mdio_np;
    struct mii_bus *bus;
    int ret;

    mdio_np = of_get_child_by_name(esw->dev->of_node, "mdio");
    if (!mdio_np)
        return 0;

    bus = devm_mdiobus_alloc(esw->dev);
    if (!bus) {
        of_node_put(mdio_np);
        return -ENOMEM;
    }

    bus->name = "ralink-esw-mdio";
    bus->read = ralink_esw_mdio_bus_read;
    bus->write = ralink_esw_mdio_bus_write;
    bus->parent = esw->dev;
    bus->priv = esw;

    strscpy(bus->id, dev_name(esw->dev), MII_BUS_ID_SIZE);

    ret = devm_of_mdiobus_register(esw->dev, bus, mdio_np);
    of_node_put(mdio_np);
    if (ret)
        return dev_err_probe(esw->dev, ret,
                     "failed to register MDIO bus\n");

    esw->mdio_bus = bus;

    return 0;
}

static inline u32 ralink_esw_port_bit(unsigned int shift, unsigned int port)
{
	return BIT(shift + port);
}

static inline void
ralink_esw_set_port_bit(struct ralink_esw *esw, u32 reg, unsigned int shift,
			unsigned int port, bool enable)
{
	u32 mask = ralink_esw_port_bit(shift, port);

	ralink_esw_rmw(esw, reg, mask, enable ? mask : 0);
}

static inline void
ralink_esw_fpa_set_force_mode(struct ralink_esw *esw, unsigned int port, bool en)
{
	ralink_esw_set_port_bit(esw, RALINK_ESW_FPA,
				RALINK_ESW_FPA_FORCE_MODE_SHIFT,
				port, en);
}

static inline void
ralink_esw_fpa1_set_force_mode(struct ralink_esw *esw, unsigned int port,
			       bool enable)
{
	u32 bit = (port == 5) ? RALINK_ESW_FPA1_FORCE_EN0
			      : RALINK_ESW_FPA1_FORCE_EN1;

	ralink_esw_rmw(esw, RALINK_ESW_FPA1, bit, enable ? bit : 0);
}


static inline void
ralink_esw_fpa_set_link(struct ralink_esw *esw, unsigned int port, bool up)
{
	ralink_esw_set_port_bit(esw, RALINK_ESW_FPA,
				RALINK_ESW_FPA_FORCE_LNK_SHIFT,
				port, up);
}

static inline void
ralink_esw_fpa1_set_link(struct ralink_esw *esw, unsigned int port, bool up)
{
	u32 bit = (port == 5) ? RALINK_ESW_FPA1_FORCE_LNK0
			      : RALINK_ESW_FPA1_FORCE_LNK1;

	ralink_esw_rmw(esw, RALINK_ESW_FPA1, bit, up ? bit : 0);
}


static inline void
ralink_esw_fpa_set_speed(struct ralink_esw *esw, unsigned int port, int speed)
{
	ralink_esw_set_port_bit(esw, RALINK_ESW_FPA,
				RALINK_ESW_FPA_FORCE_SPD_SHIFT,
				port, speed == SPEED_100);
}

static inline void
ralink_esw_fpa1_set_speed(struct ralink_esw *esw, unsigned int port, int speed)
{
	u32 mask  = (port == 5) ? RALINK_ESW_FPA1_FORCE_SPD0
			       : RALINK_ESW_FPA1_FORCE_SPD1;
	u32 shift = (port == 5) ? RALINK_ESW_FPA1_FORCE_SPD0_SHIFT
			       : RALINK_ESW_FPA1_FORCE_SPD1_SHIFT;
	u32 val;

	switch (speed) {
	case SPEED_1000:
		val = 2;
		break;
	case SPEED_100:
		val = 1;
		break;
	default:
		val = 0;
		break;
	}

	ralink_esw_rmw(esw, RALINK_ESW_FPA1, mask, (val << shift) & mask);
}

static inline void
ralink_esw_fpa_set_duplex(struct ralink_esw *esw, unsigned int port, bool full)
{
	ralink_esw_set_port_bit(esw, RALINK_ESW_FPA,
				RALINK_ESW_FPA_FORCE_DPX_SHIFT,
				port, full);
}

static inline void
ralink_esw_fpa1_set_duplex(struct ralink_esw *esw, unsigned int port,
			   bool full)
{
	u32 bit = (port == 5) ? RALINK_ESW_FPA1_FORCE_DPX0
			      : RALINK_ESW_FPA1_FORCE_DPX1;

	ralink_esw_rmw(esw, RALINK_ESW_FPA1, bit, full ? bit : 0);
}

static inline void
ralink_esw_fpa_set_pause(struct ralink_esw *esw, unsigned int port, bool en)
{
	ralink_esw_set_port_bit(esw, RALINK_ESW_FPA,
				RALINK_ESW_FPA_FORCE_XFC_SHIFT,
				port, en);
}

static inline void
ralink_esw_fpa1_set_pause(struct ralink_esw *esw, unsigned int port,
			  bool tx_pause, bool rx_pause)
{
	u32 mask  = (port == 5) ? RALINK_ESW_FPA1_FORCE_XFC0
			       : RALINK_ESW_FPA1_FORCE_XFC1;
	u32 shift = (port == 5) ? RALINK_ESW_FPA1_FORCE_XFC0_SHIFT
			       : RALINK_ESW_FPA1_FORCE_XFC1_SHIFT;
	u32 val = (tx_pause ? 0x2 : 0) | (rx_pause ? 0x1 : 0);

	ralink_esw_rmw(esw, RALINK_ESW_FPA1, mask, (val << shift) & mask);
}

static void ralink_esw_port_set_force_mode(struct ralink_esw *esw, int port,
					   bool enable)
{
	if (port <= 4)
		ralink_esw_fpa_set_force_mode(esw, port, enable);
	else
		ralink_esw_fpa1_set_force_mode(esw, port, enable);
}

static void ralink_esw_port_set_link(struct ralink_esw *esw, int port, bool up)
{
	if (port <= 4)
		ralink_esw_fpa_set_link(esw, port, up);
	else
		ralink_esw_fpa1_set_link(esw, port, up);
}

static void ralink_esw_port_set_speed(struct ralink_esw *esw, int port, int speed)
{
	if (port <= 4)
		ralink_esw_fpa_set_speed(esw, port, speed);
	else
		ralink_esw_fpa1_set_speed(esw, port, speed);
}

static void ralink_esw_port_set_duplex(struct ralink_esw *esw, int port, bool full)
{
	if (port <= 4)
		ralink_esw_fpa_set_duplex(esw, port, full);
	else
		ralink_esw_fpa1_set_duplex(esw, port, full);
}

static void ralink_esw_port_set_pause(struct ralink_esw *esw, int port,
				      bool tx_pause, bool rx_pause)
{
	if (port <= 4)
		ralink_esw_fpa_set_pause(esw, port, tx_pause && rx_pause);
	else
		ralink_esw_fpa1_set_pause(esw, port, tx_pause, rx_pause);
}

static void ralink_esw_phylink_get_caps(struct dsa_switch *ds, int port,
					struct phylink_config *config)
{
	switch (port) {
	case 0 ... 4:
		config->mac_capabilities = MAC_10 | MAC_100 | MAC_SYM_PAUSE;
		break;
	case 6:
		config->mac_capabilities = MAC_10 | MAC_100 | MAC_1000FD |
					   MAC_SYM_PAUSE | MAC_ASYM_PAUSE;
		break;
	default:
		config->mac_capabilities = 0;
		break;
	}
}

static void ralink_esw_mac_config(struct phylink_config *config,
				  unsigned int mode,
				  const struct phylink_link_state *state)
{
	/* Nothing to program until link-up/link-down. */
}

static void ralink_esw_mac_link_down(struct phylink_config *config,
				     unsigned int mode,
				     phy_interface_t interface)
{
	struct dsa_port *dp = dsa_phylink_to_port(config);
	struct ralink_esw *esw = dp->ds->priv;

	ralink_esw_port_set_force_mode(esw, dp->index, true);
	ralink_esw_port_set_link(esw, dp->index, false);
}

static void ralink_esw_mac_link_up(struct phylink_config *config,
				   struct phy_device *phydev,
				   unsigned int mode,
				   phy_interface_t interface,
				   int speed, int duplex,
				   bool tx_pause, bool rx_pause)
{
	struct dsa_port *dp = dsa_phylink_to_port(config);
	struct ralink_esw *esw = dp->ds->priv;

	ralink_esw_port_set_force_mode(esw, dp->index, true);
	ralink_esw_port_set_speed(esw, dp->index, speed);
	ralink_esw_port_set_duplex(esw, dp->index, duplex == DUPLEX_FULL);
	ralink_esw_port_set_pause(esw, dp->index, tx_pause, rx_pause);
	ralink_esw_port_set_link(esw, dp->index, true);
}

static const struct phylink_mac_ops ralink_esw_phylink_mac_ops = {
	.mac_config	= ralink_esw_mac_config,
	.mac_link_down	= ralink_esw_mac_link_down,
	.mac_link_up	= ralink_esw_mac_link_up,
};

static int ralink_esw_port_enable(struct dsa_switch *ds, int port,
				  struct phy_device *phy)
{
	struct ralink_esw *esw = ds->priv;
	u32 mask;

	if (!dsa_is_user_port(ds, port))
		return 0;

	mask = BIT(RALINK_ESW_POC0_DIS_PORT_SHIFT + port);
	ralink_esw_rmw(esw, RALINK_ESW_POC0, mask, 0);

	return 0;
}

static void ralink_esw_port_disable(struct dsa_switch *ds, int port)
{
	struct ralink_esw *esw = ds->priv;
	u32 mask;

	if (!dsa_is_user_port(ds, port))
		return;

	mask = BIT(RALINK_ESW_POC0_DIS_PORT_SHIFT + port);
	ralink_esw_rmw(esw, RALINK_ESW_POC0, mask, mask);
}

static const struct dsa_switch_ops ralink_esw_ops = {
        .port_enable         = ralink_esw_port_enable,
        .port_disable        = ralink_esw_port_disable,

        /* phylink */
        .phylink_get_caps    = ralink_esw_phylink_get_caps,
};

static void ralink_esw_phylink_mac_change(struct ralink_esw *esw, int port,
					  bool up)
{
	struct dsa_port *dp = dsa_to_port(esw->ds, port);

	phylink_mac_change(dp->pl, up);
}

static irqreturn_t ralink_esw_irq_thread(int irq, void *data)
{
	struct ralink_esw *esw = data;
	u32 stat, link, change;
	int port;

	stat = ralink_esw_r32(esw, RALINK_ESW_ISR);
	if (!(stat & RALINK_ESW_PORT_ST_CHG))
		return IRQ_NONE;

	link = ralink_esw_r32(esw, RALINK_ESW_POA) >>
	       RALINK_ESW_POA_LINK_SHIFT;
	change = link ^ esw->link_state;

	for (port = 0; port < RALINK_ESW_NUM_PORTS; port++) {
		if (change & BIT(port))
			ralink_esw_phylink_mac_change(esw, port,
						      !!(link & BIT(port)));
	}

	esw->link_state = link;

	/* Ack interrupt after sampling link state */
	ralink_esw_w32(esw, RALINK_ESW_ISR, stat);

	return IRQ_HANDLED;
}

static int ralink_esw_irq_init(struct ralink_esw *esw)
{
	int irq, ret;

        irq = platform_get_irq_optional(to_platform_device(esw->dev), 0);
        if (irq == -ENXIO)
	        return 0;
        if (irq < 0)
	        return irq;

	esw->link_state = ralink_esw_r32(esw, RALINK_ESW_POA) >>
			  RALINK_ESW_POA_LINK_SHIFT;

	ret = devm_request_threaded_irq(esw->dev, irq, NULL,
					ralink_esw_irq_thread,
					IRQF_ONESHOT,
					dev_name(esw->dev), esw);
	if (ret) {
		dev_warn(esw->dev,
			 "failed to request link IRQ, falling back to polling\n");
		return 0;
	}

	/* Unmask switch link-change interrupt only */
	ralink_esw_w32(esw, RALINK_ESW_IMR, RALINK_ESW_PORT_ST_CHG);

	return 0;
}

static int ralink_esw_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct ralink_esw *esw;
    int ret;

    esw = devm_kzalloc(dev, sizeof(*esw), GFP_KERNEL);
    if (!esw)
        return -ENOMEM;

    esw->dev = dev;

    esw->base = devm_platform_ioremap_resource(pdev, 0);
    if (IS_ERR(esw->base))
        return dev_err_probe(dev, PTR_ERR(esw->base),
                     "failed to map registers\n");

    esw->clk = devm_clk_get_optional_enabled(dev, "ephy");
    if (IS_ERR(esw->clk))
        return dev_err_probe(dev, PTR_ERR(esw->clk),
                     "failed to enable EPHY clock\n");

    esw->rst_esw = devm_reset_control_get_optional_exclusive(dev, "esw");
    if (IS_ERR(esw->rst_esw))
        return dev_err_probe(dev, PTR_ERR(esw->rst_esw),
                     "failed to get ESW reset\n");

    esw->rst_ephy = devm_reset_control_get_optional_exclusive(dev, "ephy");
    if (IS_ERR(esw->rst_ephy))
        return dev_err_probe(dev, PTR_ERR(esw->rst_ephy),
                     "failed to get EPHY reset\n");

    /*
     * Use a normal reset sequence. Any PHY quirks can be handled later
     * in dedicated PHY driver code.
     */
    if (esw->rst_esw) {
        ret = reset_control_reset(esw->rst_esw);
        if (ret)
            return dev_err_probe(dev, ret,
                         "failed to reset ESW\n");
    }

    if (esw->rst_ephy) {
        ret = reset_control_reset(esw->rst_ephy);
        if (ret)
            return dev_err_probe(dev, ret,
                         "failed to reset EPHY\n");
    }

    esw->ds = devm_kzalloc(dev, sizeof(*esw->ds), GFP_KERNEL);
    if (!esw->ds)
        return -ENOMEM;

    esw->ds->dev = dev;
    esw->ds->priv = esw;
    esw->ds->ops = &ralink_esw_ops;
    esw->ds->phylink_mac_ops = &ralink_esw_phylink_mac_ops;

    platform_set_drvdata(pdev, esw);

    mutex_init(&esw->mdio_lock);
    ret = ralink_esw_mdio_register(esw);
    if (ret)
        return ret;

    ret = dsa_register_switch(esw->ds);
    if (ret)
        return dev_err_probe(dev, ret,
                     "failed to register DSA switch\n");

    ret = ralink_esw_irq_init(esw);
    if (ret)
        dev_warn(dev, "IRQ init failed: %d\n", ret);

    return 0;
}

static void ralink_esw_remove(struct platform_device *pdev)
{
    struct ralink_esw *esw = platform_get_drvdata(pdev);
   
    dsa_unregister_switch(esw->ds);
}

static const struct of_device_id ralink_esw_of_match[] = {
    { .compatible = "ralink,rt5350-esw" },
    { .compatible = "mediatek,mt7628-esw" },
    { }
};
MODULE_DEVICE_TABLE(of, ralink_esw_of_match);

static struct platform_driver ralink_esw_driver = {
    .probe  = ralink_esw_probe,
    .remove = ralink_esw_remove,
    .driver = {
        .name = "ralink-esw",
        .of_match_table = ralink_esw_of_match,
    },
};

module_platform_driver(ralink_esw_driver);

MODULE_AUTHOR("Richard van Schagen <richard@routerwrt.org>");
MODULE_DESCRIPTION("DSA driver for Ralink/MediaTek embedded switch (ESW)");
MODULE_LICENSE("GPL");