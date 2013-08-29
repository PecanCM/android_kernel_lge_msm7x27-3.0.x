/*
 * Melfas MCS7000 Touchscreen driver
 * Copyright (C)2013 by Sergio Aguayo <sergioag@fuelcontrol.com.pe>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <mach/board_lge.h>

#include "mcs7000.h"

#define NO_KEY_TOUCHED			0
#define KEY_MENU_TOUCHED		1
#define KEY_HOME_TOUCHED		2
#define KEY_BACK_TOUCHED		3
#define KEY_SEARCH_TOUCHED		4

int mcs7000_pecan_probe(struct mcs7000_device *dev)
{
	gpio_tlmm_config(GPIO_CFG(28, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	dev->touch_data = dev->client->dev.platform_data;

	input_set_abs_params(dev->input, ABS_MT_POSITION_X, dev->touch_data->ts_x_min, dev->touch_data->ts_x_max, 0, 0);
	input_set_abs_params(dev->input, ABS_MT_POSITION_Y, dev->touch_data->ts_y_min, dev->touch_data->ts_y_max, 0, 0);
	return 0;
}

int mcs7000_pecan_power_on(struct mcs7000_device *dev)
{
	int err;

	gpio_set_value(28, 1);

	err = dev->touch_data->power(1);
	if(err < 0) {
		printk(KERN_ERR "%s: Power on failed.\n", __FUNCTION__);
		goto _err;
	}

	msleep(10);

	return 0;

_err:
	return err;
}

int mcs7000_pecan_power_off(struct mcs7000_device *dev)
{
	int err;

	gpio_set_value(28, 0);

	err = dev->touch_data->power(0);
	if(err < 0) {
		printk(KERN_ERR "%s: Power off failed.\n", __FUNCTION__);
		goto _err;
	}

	msleep(10);

	return 0;

_err:
	return err;
}

static void mcs7000_pecan_report_key(struct mcs7000_device *dev, int key, int status)
{
	int input_key;

	switch(key) {
		case KEY_BACK_TOUCHED:
			input_key = KEY_BACK;
			break;
		case KEY_MENU_TOUCHED:
			input_key = KEY_MENU;
			break;
		case KEY_HOME_TOUCHED:
			input_key = KEY_HOME;
			break;
		case KEY_SEARCH_TOUCHED:
			input_key = KEY_SEARCH;
			break;
		default:
			printk(KERN_WARNING "%s: Unknown key %i\n", __FUNCTION__, key);
			return;
	}

	input_report_key(dev->input, input_key, status);
	input_sync(dev->input);
}

void mcs7000_pecan_input_event(struct mcs7000_device *dev, unsigned char *response_buffer)
{
	int irq_status = irq_read_line(dev->client->irq);
	int pressed = !irq_status;

	int key = (response_buffer[0] & 0xf0) >> 4;
	static int old_key = -1;

	if(pressed) {
		if(key && key != old_key) {
			if(old_key > 0) mcs7000_pecan_report_key(dev, old_key, 0);
			mcs7000_pecan_report_key(dev, key, 1);
			old_key = key;
		}
	}
	else {
		if(key) {
			mcs7000_pecan_report_key(dev, key, 0);
			old_key = 0;
		}
	}
}
