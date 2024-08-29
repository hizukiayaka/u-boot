// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc. All Rights Reserved.
 *
 * Author: Weijie Gao <weijie.gao@mediatek.com>
 *
 * Generic autoboot menu command
 */

#include <command.h>
#include <button.h>
#include <dm.h>
#include <env.h>
#include <led.h>
#include <linux/delay.h>
#include <stdio.h>
#include <vsprintf.h>
#include <linux/types.h>

#include "autoboot_helper.h"

static const struct bootmenu_entry *menu_entries;
static u32 menu_count;

static int do_mtkautoboot(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	int i;
	char key[16];
	char val[256];
	char cmd[32];
	int delay = -1;
#ifdef CONFIG_MEDIATEK_BOOTMENU_COUNTDOWN
	const char *delay_str;
#endif

#ifdef CONFIG_MTK_WEB_FAILSAFE
	struct udevice *btn_dev;

	if (!button_get_by_label("reset", &btn_dev)) {
		struct udevice *dev, *led0, *led1;

		led0 = NULL;
		/* Turn off all the LEDs first */
		for (uclass_first_device(UCLASS_LED, &dev);
		     dev; uclass_next_device(&dev)) {
			if (led_get_state(dev))
				led0 = dev;
			else if (!led0)
				led0 = dev;
			led_set_state(dev, LEDST_OFF);
		}

		dev = led0;
#ifdef CONFIG_MTK_WEB_FAILSAFE_INDICATOR_LED_LABEL
		if (sizeof(CONFIG_MTK_WEB_FAILSAFE_INDICATOR_LED_LABEL) > 1) {
			if (led_get_by_label
			    (CONFIG_MTK_WEB_FAILSAFE_INDICATOR_LED_LABEL,
			     &led0))
				led0 = dev;
		}
#else
		if (led_get_by_label(env_get("bootled_pwr"), &led0))
			led0 = dev;
#endif
		if (led_get_by_label(env_get("bootled_rec"), &led1))
			led1 = led0;

		printf("Hold the reset button to enter into httpd failsafe\n");

		i = 0;
		delay = 5;

		do {
			if (button_get_state(btn_dev)) {
				schedule();

				if (led0) {
					led_set_state(led0, LEDST_OFF);
					led_set_state(led1, LEDST_OFF);
					mdelay(5);
					led_set_state(led1, LEDST_ON);
					mdelay(25);
					led_set_state(led1, LEDST_OFF);
					mdelay(20);
					led_set_state(led0, LEDST_ON);
					led_set_state(led1, LEDST_ON);
					mdelay(10);
					led_set_state(led1, LEDST_OFF);
					mdelay(20);
					led_set_state(led0, LEDST_OFF);
					led_set_state(led1, LEDST_ON);
					mdelay(20);
				} else {
					mdelay(100);
				}

				delay--;
				i++;
			} else {
				schedule();

				if (led0) {
					led_set_state(led1, LEDST_OFF);
					led_set_state(led0, LEDST_ON);
					mdelay(50);
					led_set_state(led0, LEDST_OFF);
					mdelay(50);
				} else {
					mdelay(100);
				}

				delay--;
				i--;
			}

			if (i > 10)
				break;
		} while ((i > 0) || (delay > 0));

		if (i > 1) {
			schedule();

			printf("will enter into httpd failsafe\n");
			for (i = 20; led0 && (i > 1); i--) {
				led_set_state(led0, LEDST_OFF);
				mdelay(5);
				led_set_state(led1, LEDST_ON);
				mdelay(5);
				led_set_state(led1, LEDST_OFF);
			}
			schedule();
			if (led0) {
				led_set_state(led0, LEDST_ON);
				led_set_state(led1, LEDST_ON);
			}

			run_command("httpd", 0);
			return CMD_RET_SUCCESS;
		} else {
			if (led0) {
				led_set_state(led1, LEDST_OFF);
				mdelay(10);
				led_set_state(led0, LEDST_ON);
			}
			delay = -1;
		}
	}
#endif

	board_bootmenu_entries(&menu_entries, &menu_count);

	if (!menu_entries || !menu_count) {
		printf("mtkautoboot is not configured!\n");
		return CMD_RET_FAILURE;
	}

	for (i = 0; i < menu_count; i++) {
		snprintf(key, sizeof(key), "bootmenu_%d", i);
		snprintf(val, sizeof(val), "%s=%s",
			 menu_entries[i].desc, menu_entries[i].cmd);
		env_set(key, val);
	}

	/*
	 * Remove possibly existed `next entry` to force bootmenu command to
	 * stop processing
	 */
	snprintf(key, sizeof(key), "bootmenu_%d", i);
	env_set(key, NULL);

#ifdef CONFIG_MEDIATEK_BOOTMENU_COUNTDOWN
	delay_str = env_get("bootmenu_delay");
	if (delay_str)
		delay = simple_strtol(delay_str, NULL, 10);
	else
		delay = CONFIG_MEDIATEK_BOOTMENU_DELAY;
#endif

	snprintf(cmd, sizeof(cmd), "bootmenu %d", delay);
	run_command(cmd, 0);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(mtkautoboot, 1, 0, do_mtkautoboot, "Display MediaTek bootmenu", "");
