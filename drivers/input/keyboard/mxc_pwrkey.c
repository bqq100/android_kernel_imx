/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc.
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
 *
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/kd.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/pmic_external.h>
#include "../arch/arm/mach-mx5/mx51_pins.h"
#define KEYCODE_ONOFF            KEY_F4

#if 1
#define pk_printf printk
#else
#define pk_printf(...)
#endif

/*! Input device structure. */
struct input_dev *mxcpwrkey_dev = NULL;
static bool key_press=false;
static void power_key_event_handler(void)
{
	if (!key_press) {
		key_press = true;
		input_report_key(mxcpwrkey_dev, KEYCODE_ONOFF,  1);
		pk_printf("%s_KeyUP\n",__func__);
	}else {
		if (key_press){
			key_press = false;
			input_report_key(mxcpwrkey_dev, KEYCODE_ONOFF,  0);
			pk_printf("%s_KeyDown\n",__func__);
		}
	}
	
}
static int mxcpwrkey_probe(struct platform_device *pdev)
{
	int retval,err;
	pmic_event_callback_t power_key_event;

	printk("I.MX5 powerkey probe\n");
/*
	init_completion(&pk_cmd_done);
	pwrkey_task = kthread_run(powerkey_thread, pdev, "powerkey");
	if (IS_ERR(pwrkey_task)) {
		err = PTR_ERR(pwrkey_task);
		printk(KERN_ERR "powerkey: Failed to start powerkey_thread, err: %d\n", err);
		goto err1;
	}
*/
	mxcpwrkey_dev = input_allocate_device();
	if (!mxcpwrkey_dev) {
		printk(KERN_ERR
		       "mxcpwrkey_dev: not enough memory for input device\n");
		retval = -ENOMEM;
		goto err1;
	}
    
	mxcpwrkey_dev->name = "mxc_power_key";
	mxcpwrkey_dev->phys = "mxcpwrkey/input0";;
	mxcpwrkey_dev->id.bustype = BUS_HOST ;
	mxcpwrkey_dev->evbit[0] = BIT_MASK(EV_KEY) ;
    
	
	power_key_event.param = NULL;
	power_key_event.func = (void *)power_key_event_handler;
	pmic_event_subscribe(EVENT_PWRONI, power_key_event);

	input_set_capability(mxcpwrkey_dev, EV_KEY, KEYCODE_ONOFF);
	
	retval = input_register_device(mxcpwrkey_dev);
	if (retval < 0) {
		printk(KERN_ERR
		       "mxcpwrkey_dev: failed to register input device\n");
		goto err2;
	}
	return 0;
	
      err2:
	input_free_device(mxcpwrkey_dev);
      err1:
	return retval;
}
static int mxcpwrkey_remove(struct platform_device *pdev)
{
	return 0;
} 
static int mxc_pwrkey_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}
static int mxc_pwrkey_resume(struct platform_device *pdev)
{
	return 0;
}
static struct platform_driver mxcpwrkey_driver = {
	.driver = {
		.name = "mxcpwrkey",
	},
	.probe = mxcpwrkey_probe,
	.remove = mxcpwrkey_remove,
	.suspend = mxc_pwrkey_suspend,
	.resume = mxc_pwrkey_resume,
};
static int __init mxcpwrkey_init(void)
{
	platform_driver_register(&mxcpwrkey_driver);
	return 0;
}

static void __exit mxcpwrkey_exit(void)
{
	platform_driver_unregister(&mxcpwrkey_driver);
}
module_init(mxcpwrkey_init);
module_exit(mxcpwrkey_exit);


MODULE_AUTHOR("eben");
MODULE_DESCRIPTION("MXC power key  Driver");
MODULE_LICENSE("GPL");

