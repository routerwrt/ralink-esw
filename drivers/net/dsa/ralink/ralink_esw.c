
// SPDX-License-Identifier: GPL-2.0
/*
 * DSA switch driver for the classic Ralink/MediaTek embedded switch (ESW)
 * found in RT5350/MT76x8 class SoCs.
 *
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <net/dsa.h>

#include "ralink_esw.h"


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