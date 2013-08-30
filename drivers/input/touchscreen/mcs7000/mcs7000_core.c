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
#include <linux/workqueue.h>

#include "mcs7000.h"

#define SAMPLE_RATE_HZ		100

/* Auto-select input protocol for modern kernels */
#ifdef ABS_MT_SLOT
#define INPUT_PROTOCOL_A
#else
#define INPUT_PROTOCOL_A
#endif

static irqreturn_t mcs7000_irq_handler(int irq, void *handle)
{
	struct mcs7000_device	*dev		= handle;

	if(dev->platform->irq_read_line(dev) == 0) {
		printk(KERN_INFO "%s: Scheduling work to do.\n", __FUNCTION__);
		disable_irq_nosync(irq);
		queue_delayed_work(dev->queue, &dev->work, msecs_to_jiffies(HZ/SAMPLE_RATE_HZ));
	}

	return IRQ_HANDLED;
}

static void mcs7000_input_report_touch(struct mcs7000_device *dev, int slot, int x, int y, int z)
{
#ifdef INPUT_PROTOCOL_B
	if(x != dev->old_x[slot] || y != dev->old_y[slot] || z != dev->old_z[slot]) {
#endif
/*		printk(KERN_DEBUG "%s: Touch event, slot %i, X = %i, Y = %i, Z = %i.\n",
			__FUNCTION__, slot, x, y, z);*/

#ifdef INPUT_PROTOCOL_B
		input_report_abs(dev->input, ABS_MT_SLOT, slot);
		input_report_abs(dev->input, ABS_MT_TRACKING_ID, slot);
		input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, 9);

		if(x != dev->old_x[slot]) {
			input_report_abs(dev->input, ABS_MT_POSITION_X, x);
			dev->old_x[slot] = x;
		}
		if(y != dev->old_y[slot]) {
			input_report_abs(dev->input, ABS_MT_POSITION_Y, y);
			dev->old_y[slot] = y;
		}
		if(z != dev->old_z[slot]) {
			input_report_abs(dev->input, ABS_MT_PRESSURE, z);
			dev->old_z[slot] = z;
		}

		input_sync(dev->input);

#endif
#ifdef INPUT_PROTOCOL_A
		input_report_abs(dev->input, ABS_MT_TRACKING_ID, slot);
		input_report_abs(dev->input, ABS_MT_TOUCH_MAJOR, 9);
		input_report_abs(dev->input, ABS_MT_POSITION_X, x);
		input_report_abs(dev->input, ABS_MT_POSITION_Y, y);
		input_report_abs(dev->input, ABS_MT_PRESSURE, z);
		input_mt_sync(dev->input);

		dev->old_x[slot] = x;
		dev->old_y[slot] = y;
		dev->old_z[slot] = z;
#endif

		dev->old_touch_valid[slot] = 1;
#ifdef INPUT_PROTOCOL_B
	}
#endif
}

#ifdef INPUT_PROTOCOL_B
static void mcs7000_input_report_untouch(struct mcs7000_device *dev, int slot)
{
	if(dev->old_touch_valid[slot]) {
/*		printk(KERN_DEBUG "%s: Touch release event, slot %i.\n", __FUNCTION__, slot);*/

		input_report_abs(dev->input, ABS_MT_SLOT, slot);
		input_report_abs(dev->input, ABS_MT_TRACKING_ID, -1);
		input_sync(dev->input);
		dev->old_x[slot] = dev->old_y[slot] = dev->old_z[slot] = -1;
		dev->old_touch_valid[slot] = 0;
	}
}
#endif

static void mcs7000_work_handler(struct work_struct *work)
{
	struct mcs7000_device	*dev;
	int			irq_status;
	int			pressed;
	char			i2c_command;
	unsigned char		response_buffer[MCS7000_INPUT_INFO_LENGTH];
	int			x[MAX_TOUCH_POINTS], y[MAX_TOUCH_POINTS], z[MAX_TOUCH_POINTS];
	int			input_event;
	int			i;

	dev = container_of(container_of(work, struct delayed_work, work), struct mcs7000_device, work);

	irq_status = dev->platform->irq_read_line(dev);
	pressed = !irq_status;

	i2c_command = MCS7000_CMD_INPUT_INFO;
	if(i2c_master_send(dev->client, &i2c_command, 1) < 0) {
		printk(KERN_ERR "%s: Error sending input info command.\n", __FUNCTION__);
		goto _schedule_next_run;
	}

	if(i2c_master_recv(dev->client, response_buffer, MCS7000_INPUT_INFO_LENGTH) < 0) {
		printk(KERN_ERR "%s: Error receiving input info data.\n", __FUNCTION__);
		goto _schedule_next_run;
	}

	input_event = response_buffer[0] & 0x0f;

/*	printk(KERN_DEBUG "%s: Pressed: %i, Input Event: %i\n", __FUNCTION__, pressed, input_event);*/

	for(i = 0; i < MAX_TOUCH_POINTS; i++) {
		x[i] = response_buffer[(i*4)+2] | ((response_buffer[(i*4)+1] & 0xf0) << 4);
		y[i] = response_buffer[(i*4)+3] | ((response_buffer[(i*4)+1] & 0x0f) << 8);
		z[i] = response_buffer[(i*4)+4];
	}

	if(pressed) {
		switch(input_event) {
			case MCS7000_INPUT_NOT_TOUCHED:
				/* Do nothing */
				break;
			case MCS7000_INPUT_MULTI_POINT_TOUCH:
				for(i = 0; i < MAX_TOUCH_POINTS; i++)
					mcs7000_input_report_touch(dev, i, x[i], y[i], z[i]);
#ifdef INPUT_PROTOCOL_A
				input_sync(dev->input);
#endif
				break;
			case MCS7000_INPUT_SINGLE_POINT_TOUCH:
				mcs7000_input_report_touch(dev, 0, x[0], y[0], z[0]);
#ifdef INPUT_PROTOCOL_B
				for(i = 1; i < MAX_TOUCH_POINTS; i++)
					if(dev->old_touch_valid[i]) mcs7000_input_report_untouch(dev, i);
#else
				input_sync(dev->input);
#endif
				break;
			default:
				printk(KERN_WARNING "%s: Unknown input event %i.\n", __FUNCTION__, input_event);
				break;
		}
	}
	else {
		switch(input_event) {
#ifdef INPUT_PROTOCOL_A
			case MCS7000_INPUT_NOT_TOUCHED:
			case MCS7000_INPUT_SINGLE_POINT_TOUCH:
			case MCS7000_INPUT_MULTI_POINT_TOUCH:
				input_mt_sync(dev->input);
				input_sync(dev->input);
				break;
#else
			case MCS7000_INPUT_NOT_TOUCHED:
			case MCS7000_INPUT_SINGLE_POINT_TOUCH:
			case MCS7000_INPUT_MULTI_POINT_TOUCH:
				for(i = 0; i < MAX_TOUCH_POINTS; i++) {
					mcs7000_input_report_untouch(dev, i);
				}
				break;
#endif
			default:
				printk(KERN_WARNING "%s: Unknown input event %i.\n", __FUNCTION__, input_event);
				break;
		}
	}

	dev->platform->input_event(dev, response_buffer);

_schedule_next_run:
	if(pressed) {
		queue_delayed_work(dev->queue, &dev->work, msecs_to_jiffies(HZ/SAMPLE_RATE_HZ));
	}
	else {
		enable_irq(dev->client->irq);
	}
}

void mcs7000_power_on(struct mcs7000_device *dev)
{
	dev->platform->power_on(dev);
	enable_irq(dev->client->irq);
}

void mcs7000_power_off(struct mcs7000_device *dev)
{
	dev->platform->power_off(dev);
	disable_irq(dev->client->irq);
}

static int mcs7000_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mcs7000_device	*dev;
	int			err, i;

	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		printk(KERN_ERR "%s: I2C device doesn't report needed I2C_FUNC_I2C functionality\n", __FUNCTION__);
		goto _cleanup_check_functionality;
	}

	dev = kzalloc(sizeof(struct mcs7000_device), GFP_KERNEL);
	if(dev == NULL) {
		printk(KERN_ERR "%s: Not enough memory.\n", __FUNCTION__);
		err = -ENOMEM;
		goto _cleanup_alloc_device;
	}

	dev->client = client;

	for(i = 0; i < MAX_TOUCH_POINTS; i++) {
		dev->old_x[i] = -1;
		dev->old_y[i] = -1;
		dev->old_z[i] = -1;
		dev->old_touch_valid[i] = 0;
	}

	i2c_set_clientdata(client, dev);

	dev->input = input_allocate_device();
	if(dev->input == NULL) {
		err = -ENOMEM;
		goto _cleanup_input_allocate;
	}

	dev->input->name = "touch_mcs7000";
	set_bit(EV_SYN, dev->input->evbit);
	set_bit(EV_ABS, dev->input->evbit);
	set_bit(EV_KEY, dev->input->evbit);

	err = input_register_device(dev->input);
	if(err < 0) {
		printk(KERN_ERR "%s: Error registering input device\n", __FUNCTION__);
		goto _cleanup_input_register;
	}

	input_set_abs_params(dev->input, ABS_MT_TOUCH_MAJOR, 0, 9, 0, 0);
	input_set_abs_params(dev->input, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(dev->input, ABS_MT_TRACKING_ID, 0, 1, 0, 0);

#ifdef CONFIG_MCS7000_PECAN
	dev->platform = mcs7000_pecan_get_platform();
#endif

	err = dev->platform->probe(dev);
	if(err < 0) {
		goto _cleanup_platform_probe;
	}

	dev->queue = create_singlethread_workqueue("mcs7000-workqueue");
	if(!dev->queue) {
		printk(KERN_ERR "%s: Cannot create work queue.\n", __FUNCTION__);
		err = -ENOMEM;
		goto _cleanup_create_workqueue;
	}

	INIT_DELAYED_WORK(&dev->work, mcs7000_work_handler);

	err = request_threaded_irq(client->irq, NULL, mcs7000_irq_handler,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, "mcs7000", dev);
	if(err < 0) {
		printk(KERN_ERR "%s: Error requesting IRQ.\n", __FUNCTION__);
		goto _cleanup_request_irq;
	}

	mcs7000_power_off(dev);
	mdelay(10);
	mcs7000_power_on(dev);

	printk(KERN_INFO "%s: MCS7000 Touchscreen registered successfully!\n", __FUNCTION__);

	return 0;

_cleanup_create_workqueue:
	free_irq(client->irq, dev);

_cleanup_request_irq: 
	dev->platform->remove(dev);

_cleanup_platform_probe:
	input_unregister_device(dev->input);

_cleanup_input_register:
	input_free_device(dev->input);

_cleanup_input_allocate:
	kfree(dev);
	i2c_set_clientdata(client, NULL);

_cleanup_alloc_device:
_cleanup_check_functionality:
	return err;
}

static int mcs7000_i2c_remove(struct i2c_client *client)
{
	struct mcs7000_device	*dev;

	dev = i2c_get_clientdata(client);

	mcs7000_power_off(dev);

	destroy_workqueue(dev->queue);

	dev->platform->remove(dev);

	free_irq(dev->client->irq, dev);

	input_unregister_device(dev->input);
	input_free_device(dev->input);

	kfree(dev);

	i2c_set_clientdata(client, NULL);

	return 0;
}

static int mcs7000_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct mcs7000_device	*dev;

	dev = i2c_get_clientdata(client);

	mcs7000_power_off(dev);

	return 0;
}

static int mcs7000_i2c_resume(struct i2c_client *client)
{
	struct mcs7000_device	*dev;

	dev = i2c_get_clientdata(client);

	mcs7000_power_on(dev);

	return 0;
}

void *mcs7000_get_platform_data(struct mcs7000_device *dev)
{
	return dev->platform_data;
}

void mcs7000_set_platform_data(struct mcs7000_device *dev, void *platform_data)
{
	dev->platform_data = platform_data;
}

static const struct i2c_device_id mcs7000_i2c_ids[] =
{
	{ "touch_mcs7000", 1 },
	{ }
};

static struct i2c_driver mcs7000_i2c_driver =
{
	.probe		= mcs7000_i2c_probe,
	.remove		= mcs7000_i2c_remove,
	.suspend	= mcs7000_i2c_suspend,
	.resume		= mcs7000_i2c_resume,
	.id_table	= mcs7000_i2c_ids,
	.driver		= {
		.name	= "mcs7000",
		.owner	= THIS_MODULE,
	},
};

static int __devinit mcs7000_init(void)
{
	printk(KERN_INFO "%s: Melfas MCS7000 driver loaded.\n", __FUNCTION__);
	return i2c_add_driver(&mcs7000_i2c_driver);
}

static void __exit mcs7000_exit(void)
{
	i2c_del_driver(&mcs7000_i2c_driver);
	printk(KERN_INFO "%s: Melfas MCS7000 driver unloaded.\n", __FUNCTION__);
}

module_init(mcs7000_init);
module_exit(mcs7000_exit);

MODULE_DESCRIPTION("Melfas MCS7000 Touchscreen driver");
MODULE_LICENSE("GPL v2");
