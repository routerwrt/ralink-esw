
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

static const struct phylink_mac_ops ralink_esw_phylink_mac_ops = {
        /* stub for phylink_mac_ops */
};

static const struct dsa_switch_ops ralink_esw_ops = {
        /* stub for dsa_switch_ops */
};

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