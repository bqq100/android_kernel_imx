/*
 * Freescale touchscreen driver
 *
 * Copyright (C) 2007-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/*!
 * @file mxc_ts.c
 *
 * @brief Driver for the Freescale Semiconductor MXC touchscreen with calibration support.
 *
 * The touchscreen driver is designed as a standard input driver which is a
 * wrapper over low level PMIC driver. Most of the hardware configuration and
 * touchscreen functionality is implemented in the low level PMIC driver. During
 * initialization, this driver creates a kernel thread. This thread then calls
 * PMIC driver to obtain touchscreen values continously. These values are then
 * passed to the input susbsystem.
 *
 * @ingroup touchscreen
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/pmic_external.h>
#include <linux/pmic_adc.h>
#include <linux/kthread.h>
#ifdef CONFIG_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define MXC_TS_NAME	"mxc_ts"

static struct input_dev *mxc_inputdev;
static struct task_struct *tstask;
/**
 * calibration array refers to
 * (delta_x[0], delta_x[1], delta_x[2], delta_y[0], delta_y[1], delta_y[2], delta).
 * Which generated by calibration service.
 * In this driver when we got touch pointer (x', y') from PMIC ADC,
 * we calculate the display pointer (x,y) by:
 * x = (delta_x[0] * x' + delta_x[1] * y' + delta_x[2]) / delta;
 * y = (delta_y[0] * x' + delta_y[1] * y' + delta_y[2]) / delta;
 */
static int calibration[7];
module_param_array(calibration, int, NULL, S_IRUGO | S_IWUSR);

#ifdef CONFIG_EARLYSUSPEND

static wait_queue_head_t ts_wait;
static int ts_suspend;

static void stop_ts_early_suspend(struct early_suspend *h)
{
	ts_suspend = 1;
}

static void start_ts_late_resume(struct early_suspend *h)
{
	ts_suspend = 0;
	wake_up_interruptible(&ts_wait);
}

static struct early_suspend stop_ts_early_suspend_desc = {
	.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING,
	.suspend = stop_ts_early_suspend,
	.resume = start_ts_late_resume,
};

#endif

static int ts_thread(void *arg)
{
	t_touch_screen ts_sample;
	s32 wait = 0;

	do {
		int x, y;
		static int last_x = -1, last_y = -1, last_press = -1;

#ifdef CONFIG_EARLYSUSPEND
		wait_event_interruptible(ts_wait, !ts_suspend);
#endif
		memset(&ts_sample, 0, sizeof(t_touch_screen));
		if (0 != pmic_adc_get_touch_sample(&ts_sample, !wait))
			continue;
		if (!(ts_sample.contact_resistance || wait))
			continue;

		if (ts_sample.x_position == 0 && ts_sample.y_position == 0 &&
		    ts_sample.contact_resistance == 0) {
			x = last_x;
			y = last_y;
		} else if (calibration[6] == 0) {
			x = ts_sample.x_position;
			y = ts_sample.y_position;
		} else {
			x = calibration[0] * (int)ts_sample.x_position +
			    calibration[1] * (int)ts_sample.y_position +
			    calibration[2];
			x /= calibration[6];
			if (x < 0)
				x = 0;
			y = calibration[3] * (int)ts_sample.x_position +
			    calibration[4] * (int)ts_sample.y_position +
			    calibration[5];
			y /= calibration[6];
			if (y < 0)
				y = 0;
		}

		if (x != last_x) {
			input_report_abs(mxc_inputdev, ABS_X, x);
			last_x = x;
		}
		if (y != last_y) {
			input_report_abs(mxc_inputdev, ABS_Y, y);
			last_y = y;
		}

		/* report pressure */
		input_report_abs(mxc_inputdev, ABS_PRESSURE,
				 ts_sample.contact_resistance);
#ifdef CONFIG_MXC_PMIC_MC13892
		/* workaround for aplite ADC resistance large range value */
		if (ts_sample.contact_resistance > 22)
			ts_sample.contact_resistance = 1;
		else
			ts_sample.contact_resistance = 0;
#endif
		/* report the BTN_TOUCH */
		if (ts_sample.contact_resistance != last_press)
			input_event(mxc_inputdev, EV_KEY,
				    BTN_TOUCH, ts_sample.contact_resistance);

		input_sync(mxc_inputdev);
		last_press = ts_sample.contact_resistance;

		wait = ts_sample.contact_resistance;
		msleep(20);

	} while (!kthread_should_stop());

	return 0;
}

static int __init mxc_ts_init(void)
{
	int retval;

	if (!is_pmic_adc_ready())
		return -ENODEV;

	mxc_inputdev = input_allocate_device();
	if (!mxc_inputdev) {
		printk(KERN_ERR "mxc_ts_init: not enough memory\n");
		return -ENOMEM;
	}

	mxc_inputdev->name = MXC_TS_NAME;
	mxc_inputdev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	mxc_inputdev->keybit[BIT_WORD(BTN_TOUCH)] |= BIT_MASK(BTN_TOUCH);
	mxc_inputdev->absbit[0] =
	    BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) | BIT_MASK(ABS_PRESSURE);
	retval = input_register_device(mxc_inputdev);
	if (retval < 0) {
		input_free_device(mxc_inputdev);
		return retval;
	}

	tstask = kthread_run(ts_thread, NULL, "mxc_ts");
	if (IS_ERR(tstask)) {
		printk(KERN_ERR "mxc_ts_init: failed to create kthread");
		tstask = NULL;
		return -1;
	}
#ifdef CONFIG_EARLYSUSPEND
	init_waitqueue_head(&ts_wait);
	register_early_suspend(&stop_ts_early_suspend_desc);
#endif
	printk("mxc input touchscreen loaded\n");
	return 0;
}

static void __exit mxc_ts_exit(void)
{
	if (tstask)
		kthread_stop(tstask);

	input_unregister_device(mxc_inputdev);

	if (mxc_inputdev) {
		input_free_device(mxc_inputdev);
		mxc_inputdev = NULL;
	}
#ifdef CONFIG_EARLYSUSPEND
	unregister_early_suspend(&stop_ts_early_suspend_desc);
#endif
}

late_initcall(mxc_ts_init);
module_exit(mxc_ts_exit);

MODULE_DESCRIPTION("MXC touchscreen driver with calibration");
MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_LICENSE("GPL");
