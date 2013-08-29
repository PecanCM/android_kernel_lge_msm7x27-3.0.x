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
 * Byte 4: Touch intensity
 * Byte 5: High bits of second X and Y coordinates
 *  - Bits 0-3: Higher 4 bits of second Y coordinate (bits 8-11)
 *  - Bits 4-7: Higher 4 bits of second X coordinate (bits 8-11)
 * Byte 6: Lower 8 bits of second X coordinate (bits 0-7)
 * Byte 7: Lower 8 bits of second Y coordinate (bits 0-7)
 */
#define MCS7000_INPUT_INFO_LENGTH		8

/* MCS7000 Input Event types */
#define MCS7000_INPUT_NOT_TOUCHED		0
#define MCS7000_INPUT_SINGLE_POINT_TOUCH	1
#define MCS7000_INPUT_MULTI_POINT_TOUCH		2


struct mcs7000_device
{
	struct i2c_client		*client;
	struct input_dev		*input;

#ifdef CONFIG_MCS7000_PECAN
	struct touch_platform_data	*touch_data;
#endif /* CONFIG_MCS7000_PECAN */

};


#ifdef CONFIG_MCS7000_PECAN
int mcs7000_pecan_probe(struct mcs7000_device *dev);
int mcs7000_pecan_power_off(struct mcs7000_device *dev);
int mcs7000_pecan_power_on(struct mcs7000_device *dev);
void mcs7000_pecan_input_event(struct mcs7000_device *dev, unsigned char *response_buffer);
#endif /* CONFIG_MCS7000_PECAN */

#endif /* !__MCS7000_H__ */
