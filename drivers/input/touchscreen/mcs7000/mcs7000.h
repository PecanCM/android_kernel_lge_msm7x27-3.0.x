/*
 * Melfas MCS7000 Touchscreen driver
 * Copyright (C)2013 by Sergio Aguayo <sergioag@fuelcontrol.com.pe>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __MCS7000_H__
#define __MCS7000_H__

/* MCS7000 Commands */
#define MCS7000_CMD_INPUT_INFO			0x10
#define MCS7000_CMD_INT_CONTROL			0x1d
#define MCS7000_CMD_RESET			0x1e

/*
 * Input buffer fields explanation:
 * Byte 0: Event type information
 *  - Bits 0-3: Touch event type
 *  - Bits 4-7: Key event type
 * Byte 1: High bits of first X and Y coordinates
 *  - Bits 0-3: Higher 4 bits of first Y coordinate (bits 8-11)
 *  - Bits 4-7: Higher 4 bits of first X coordinate (bits 8-11)
 * Byte 2: Lower 8 bits of first X coordinate (bits 0-7)
 * Byte 3: Lower 8 bits of first Y coordinate (bits 0-7)
 * Byte 4: First touch intensity (Z1)
 * Byte 5: High bits of second X and Y coordinates
 *  - Bits 0-3: Higher 4 bits of second Y coordinate (bits 8-11)
 *  - Bits 4-7: Higher 4 bits of second X coordinate (bits 8-11)
 * Byte 6: Lower 8 bits of second X coordinate (bits 0-7)
 * Byte 7: Lower 8 bits of second Y coordinate (bits 0-7)
 * Byte 8: Second touch intensity (Z2)
 */
#define MCS7000_INPUT_INFO_LENGTH		((MAX_TOUCH_POINTS*4)+1)

/* MCS7000 Input Event types */
#define MCS7000_INPUT_NOT_TOUCHED		0
#define MCS7000_INPUT_SINGLE_POINT_TOUCH	1
#define MCS7000_INPUT_MULTI_POINT_TOUCH		2

#define MAX_TOUCH_POINTS			2

struct mcs7000_device;

struct mcs7000_platform
{
	int				(*probe)(struct mcs7000_device *dev);
	int				(*power_on)(struct mcs7000_device *dev);
	int				(*power_off)(struct mcs7000_device *dev);
	void				(*input_event)(struct mcs7000_device *dev, unsigned char *response_buffer);
	int				(*irq_read_line)(struct mcs7000_device *dev);
	void				(*remove)(struct mcs7000_device *dev);
};


struct mcs7000_device
{
	struct i2c_client		*client;
	struct input_dev		*input;
	struct mcs7000_platform		*platform;
	struct delayed_work		work;
	struct workqueue_struct		*queue;

	int				old_x[MAX_TOUCH_POINTS];
	int				old_y[MAX_TOUCH_POINTS];
	int				old_z[MAX_TOUCH_POINTS];
	int				old_touch_valid[MAX_TOUCH_POINTS];

	void				*platform_data;
};

void mcs7000_set_platform_data(struct mcs7000_device *dev, void *platform_data);
void *mcs7000_get_platform_data(struct mcs7000_device *dev);

#ifdef CONFIG_MCS7000_PECAN
struct mcs7000_platform *mcs7000_pecan_get_platform(void);
#endif /* CONFIG_MCS7000_PECAN */

#endif /* !__MCS7000_H__ */
