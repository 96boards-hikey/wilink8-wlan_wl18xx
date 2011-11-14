/*
 * This file is part of wl18xx
 *
 * Copyright (C) 2011 Texas Instruments Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/wl12xx.h>

#include "../wlcore/wlcore.h"
#include "../wlcore/io.h"
#include "reg.h"

#define WL18XX_FW_NAME  "ti-connectivity/wl18xx-fw-multirole-roc.bin"
#define WL18XX_NVS_NAME "ti-connectivity/wl18xx-nvs.bin"

static const struct wlcore_partition_set wl18xx_ptable[PART_TABLE_LEN] = {
	[PART_TOP_PRCM_ELP_SOC] = {
		.mem  = { .start = 0x00A02000, .size  = 0x00010000 },
		.reg  = { .start = 0x00807000, .size  = 0x00005000 },
		.mem2 = { .start = 0x00800000, .size  = 0x0000B000 },
		.mem3 = { .start = 0x00000000, .size  = 0x00000000 },
	},
	[PART_DOWN] = {
		.mem  = { .start = 0x00000000, .size  = 0x00014000 },
		.reg  = { .start = 0x00810000, .size  = 0x0000BFFF },
		.mem2 = { .start = 0x00000000, .size  = 0x00000000 },
		.mem3 = { .start = 0x00000000, .size  = 0x00000000 },
	},
	[PART_BOOT] = {
		.mem  = { .start = 0x00700000, .size = 0x0000030c },
		.reg  = { .start = 0x00802000, .size = 0x00014578 },
		.mem2 = { .start = 0x00B00404, .size = 0x00001000 },
		.mem3 = { .start = 0x00C00000, .size = 0x00000400 },
	},
};

static int wl18xx_get_chip_id(struct wlcore *wl)
{
	u32 id;

	printk(KERN_DEBUG "wl18xx_get_chip_id\n");

	wlcore_select_partition(wl, PART_BOOT);

	id = wlcore_read32(wl, WL18XX_CHIP_ID_B);

	switch (id) {
	case CHIP_ID_185x_PG10:
		printk(KERN_DEBUG "chip id 0x%x (185x PG10)\n", id);

		wl->fw_name  = WL18XX_FW_NAME;
		wl->nvs_name = WL18XX_NVS_NAME;
		break;
	default:
		printk(KERN_DEBUG "unsupported chip id: 0x%x\n", id);
		id = -ENODEV;
		break;
	}

	return id;
}

static struct wlcore_ops wl18xx_ops = {
	.get_chip_id = wl18xx_get_chip_id,
};

static int __devinit wl18xx_probe(struct platform_device *pdev)
{
	struct wl12xx_platform_data *pdata = pdev->dev.platform_data;
	struct wlcore *wl;
	int ret = -ENODEV;

	printk(KERN_DEBUG "wl18xx_probe\n");

	wl = wlcore_alloc_hw();
	if (IS_ERR(wl)) {
		ret = PTR_ERR(wl);
		goto out;
	}

	/*
	 * HACK! We're casting ops here, it should be fine for now,
	 * because they're the same, but this needs to be properly
	 * fixed
	 */
	wl->if_ops = (struct wlcore_if_ops *) pdata->ops;
	wl->ops = &wl18xx_ops;
	wl->dev = &pdev->dev;
	wl->ptable = &wl18xx_ptable[0];

	platform_set_drvdata(pdev, wl);

	ret = wlcore_register_hw(wl);
	if (ret)
		goto out_free_hw;

	goto out;

out_free_hw:
	wlcore_free_hw(wl);
out:
	return ret;
}

static int __devexit wl18xx_remove(struct platform_device *pdev)
{
	struct wlcore *wl = platform_get_drvdata(pdev);

	printk(KERN_DEBUG "wl18xx_remove\n");

	wlcore_unregister_hw(wl);
	wlcore_free_hw(wl);

	return 0;
}

static const struct platform_device_id wl18xx_id_table[] __devinitconst = {
	{ "wl18xx", 0 },
	{  } /* Terminating Entry */
};
MODULE_DEVICE_TABLE(platform, wl18xx_id_table);

static struct platform_driver wl18xx_driver = {
	.probe		= wl18xx_probe,
	.remove		= __devexit_p(wl18xx_remove),
	.id_table	= wl18xx_id_table,
	.driver = {
		.name	= "wl18xx_driver",
		.owner	= THIS_MODULE,
	}
};

static int __init wl18xx_init(void)
{
	printk(KERN_DEBUG "wl18xx_init\n");

	return platform_driver_register(&wl18xx_driver);
}
module_init(wl18xx_init);

static void __exit wl18xx_exit(void)
{
	platform_driver_unregister(&wl18xx_driver);
}
module_exit(wl18xx_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Luciano Coelho <coelho@ti.com>");
MODULE_FIRMWARE(WL18XX_FW_NAME);
