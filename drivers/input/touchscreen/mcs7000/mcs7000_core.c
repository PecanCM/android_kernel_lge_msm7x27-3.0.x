/*
 * Melfas MCS7000 Touchscreen driver
 * Copyright (C)2013 by Sergio Aguayo <sergioag@fuelcontrol.com.pe>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include "mcs7000.h"

static irqreturn_t mcs7000_irq_handler(int irq, void *handle)
{
	struct mcs7000_device	*dev		= handle;
	int			irq_status;
	int			pressed;
	char			i2c_command;
	unsigned char		response_buffer[MCS7000_INPUT_INFO_LENGTH];
	int			x1, y1, z1;
	int			x2, y2, z2;
	int			input_event;
	static int		old_x1=-1, old_y1=-1, old_z1=-1;
	static int		old_x2=-1, old_y2=-1, old_z2=-1;

	irq_status = irq_read_line(irq);
	pressed = !irq_status;

	i2c_command = MCS7000_CMD_INPUT_INFO;
	if(i2c_master_send(dev->client, &i2c_command, 1) < 0) {
		printk(KERN_ERR "%s: Error sending input info command.\n", __FUNCTION__);
		return IRQ_HANDLED;
	}

	if(i2c_master_recv(dev->client, response_buffer, MCS7000_INPUT_INFO_LENGTH) < 0) {
		printk(KERN_ERR "%s: Error receiving input info data.\n", __FUNCTION__);
		return IRQ_HANDLED;
	}

	input_event = response_buffer[0] & 0x0f;

	x1 = response_buffer[2] | ((response_buffer[1] & 0xf0) << 4);
	y1 = response_buffer[3] | ((response_buffer[1] & 0x0f) << 8);
	z1 = z2 = response_buffer[4];
	x2 = response_buffer[6] | ((response_buffer[5] & 0xf0) << 4);
	y2 = response_buffer[7] | ((response_buffer[5] & 0x0f) << 8);

	if(pressed) {
		switch(input_event) {
			case MCS7000_INPUT_NOT_TOUCHED:
				break;
			case MCS7000_INPUT_MULTI_POINT_TOUCH:
				input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, z2);
				input_report_abs(dev->input, ABS_MT_PRESSURE, z2);
				input_report_abs(dev->input, ABS_MT_POSITION_X, x2);
				input_report_abs(dev->input, ABS_MT_POSITION_Y, y2);
				input_mt_sync(dev->input);
				old_x2 = x2;
				old_y2 = y2;
				old_z2 = z2;
				/* fall-through */
			case MCS7000_INPUT_SINGLE_POINT_TOUCH:
				input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, z1);
				input_report_abs(dev->input, ABS_MT_PRESSURE, z1);
				input_report_abs(dev->input, ABS_MT_POSITION_X, x1);
				input_report_abs(dev->input, ABS_MT_POSITION_Y, y1);
				input_mt_sync(dev->input);
				input_sync(dev->input);
				old_x1 = x1;
				old_y1 = y1;
				old_z1 = z1;
				break;
			default:
				printk(KERN_WARNING "%s: Unknown input event %i.\n", __FUNCTION__, input_event);
				break;
		}
	}
	else {
		switch(input_event) {
			case MCS7000_INPUT_NOT_TOUCHED:
				break;
			case MCS7000_INPUT_MULTI_POINT_TOUCH:
				if(old_z2 >= 0 && old_y2 >= 0 && old_x2 >= 0) {
					input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, 0);
					input_report_abs(dev->input, ABS_MT_PRESSURE, old_z2);
					input_report_abs(dev->input, ABS_MT_POSITION_X, old_x2);
					input_report_abs(dev->input, ABS_MT_POSITION_Y, old_y2);
					input_mt_sync(dev->input);
					old_x2 = old_y2 = old_z2 = -1;
				}
				/* fall-through */
			case MCS7000_INPUT_SINGLE_POINT_TOUCH:
				if(old_z1 >= 0 && old_y1 >= 0 && old_x1 >= 0) {
					input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, 0);
					input_report_abs(dev->input, ABS_MT_PRESSURE, old_z1);
					input_report_abs(dev->input, ABS_MT_POSITION_X, old_x1);
					input_report_abs(dev->input, ABS_MT_POSITION_Y, old_y1);
					input_mt_sync(dev->input);
					input_sync(dev->input);
					old_x1 = old_y1 = old_z1 = -1;
				}
				break;
			default:
				printk(KERN_WARNING "%s: Unknown input event %i.\n", __FUNCTION__, input_event);
				break;
		}
	}

#ifdef CONFIG_MCS7000_PECAN
	mcs7000_pecan_input_event(dev, response_buffer);
#endif

	return IRQ_HANDLED;
}

void mcs7000_power_on(struct mcs7000_device *dev)
{
#ifdef CONFIG_MCS7000_PECAN
	mcs7000_pecan_power_on(dev);
#endif
	enable_irq(dev->client->irq);
}

void mcs7000_power_off(struct mcs7000_device *dev)
{
#ifdef CONFIG_MCS7000_PECAN
	mcs7000_pecan_power_off(dev);
#endif
	disable_irq(dev->client->irq);
}

static int mcs7000_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mcs7000_device	*dev;
	int			err;

	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto _cleanup_check_functionality;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if(dev == NULL)
		return -ENOMEM;

	i2c_set_clientdata(client, dev);

	dev->input = input_allocate_device();
	if(dev->input == NULL) {
		err = -ENOMEM;
		goto _cleanup_input_allocate;
	}

	dev->input->name = "mcs7000";

	err = input_register_device(dev->input);
	if(err < 0) {
		printk(KERN_ERR "%s: Error registering input device\n", __FUNCTION__);
		goto _cleanup_input_register;
	}

	input_set_abs_params(dev->input, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(dev->input, ABS_MT_TOUCH_MINOR, 0, 15, 0, 0);
	input_set_abs_params(dev->input, ABS_MT_TOUCH_MAJOR, 0, 15, 0, 0);
	input_set_abs_params(dev->input, ABS_MT_TRACKING_ID, 0, 9, 0, 0);

#ifdef CONFIG_MCS7000_PECAN
	err = mcs7000_pecan_probe(dev);
	if(err < 0) {
		goto _cleanup_pecan_probe;
	}
#endif

	err = request_threaded_irq(client->irq, NULL, mcs7000_irq_handler,
		IRQF_TRIGGER_LOW | IRQF_TRIGGER_HIGH, "mcs7000", dev);
	if(err < 0) {
		printk(KERN_ERR "%s: Error requesting IRQ.\n", __FUNCTION__);
		goto _cleanup_request_irq;
	}

	mcs7000_power_off(dev);
	mdelay(10);
	mcs7000_power_on(dev);
	return 0;

_cleanup_request_irq:

#ifdef CONFIG_MCS7000_PECAN
_cleanup_pecan_probe:
	input_unregister_device(dev->input);
#endif

_cleanup_input_register:
	input_free_device(dev->input);

_cleanup_input_allocate:
	kfree(dev);
	i2c_set_clientdata(client, NULL);

_cleanup_check_functionality:
	return err;
}

static int mcs7000_i2c_remove(struct i2c_client *client)
{
	struct mcs7000_device	*dev;

	dev = i2c_get_clientdata(client);

	input_free_device(dev->input);

	kfree(dev);

	i2c_set_clientdata(client, NULL);

	return 0;
}

static const struct i2c_device_id mcs7000_i2c_ids[] =
{
	{ "mcs7000", 1 },
	{ }
};

static struct i2c_driver mcs7000_i2c_driver =
{
	.probe		= mcs7000_i2c_probe,
	.remove		= mcs7000_i2c_remove,
	.id_table	= mcs7000_i2c_ids,
	.driver		= {
		.name	= "mcs7000",
		.owner	= THIS_MODULE,
	},
};

static int __devinit mcs7000_init(void)
{
	return i2c_add_driver(&mcs7000_i2c_driver);
}

static void __exit mcs7000_exit(void)
{
	i2c_del_driver(&mcs7000_i2c_driver);
}

module_init(mcs7000_init);
module_exit(mcs7000_exit);

MODULE_DESCRIPTION("Melfas MCS7000 Touchscreen driver");
MODULE_LICENSE("GLPv2");
