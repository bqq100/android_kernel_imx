/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/nodemask.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/ata.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/max17135.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/videodev2.h>
#include <linux/mxcfb.h>
#include <linux/fec.h>
#include <linux/gpmi-nfc.h>
#include <linux/android_pmem.h>
#include <linux/usb/android.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/flash.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/memory.h>
#include <mach/gpio.h>
#include <mach/mmc.h>
#include <mach/mxc_dvfs.h>
#include <mach/iomux-mx50.h>

#include "devices.h"
#include "crm_regs.h"
#include "usb.h"
#include "dma-apbh.h"

#define SD1_WP	(3*32 + 19)	/*GPIO_4_19 */
#define SD1_CD	(0*32 + 27)	/*GPIO_1_27 */
#define SD2_WP	(4*32 + 16)	/*GPIO_5_16 */
#define SD2_CD	(4*32 + 17) /*GPIO_5_17 */
#define SD3_WP	(4*32 + 28) /*GPIO_5_28 */
#define SD3_CD	(3*32 + 4) /*GPIO_4_4 */
#define HP_DETECT	(3*32 + 15)	/*GPIO_4_15 */
#define PWR_INT		(3*32 + 18)	/*GPIO_4_18 */

#define EPDC_D0		(2*32 + 1)	/*GPIO_3_0 */
#define EPDC_D1		(2*32 + 2)	/*GPIO_3_1 */
#define EPDC_D2		(2*32 + 3)	/*GPIO_3_2 */
#define EPDC_D3		(2*32 + 4)	/*GPIO_3_3 */
#define EPDC_D4		(2*32 + 5)	/*GPIO_3_4 */
#define EPDC_D5		(2*32 + 6)	/*GPIO_3_5 */
#define EPDC_D6		(2*32 + 7)	/*GPIO_3_6 */
#define EPDC_D7		(2*32 + 8)	/*GPIO_3_7 */
#define EPDC_GDCLK	(2*32 + 16)	/*GPIO_3_16 */
#define EPDC_GDSP	(2*32 + 17)	/*GPIO_3_17 */
#define EPDC_GDOE	(2*32 + 18)	/*GPIO_3_18 */
#define EPDC_GDRL	(2*32 + 19)	/*GPIO_3_19 */
#define EPDC_SDCLK	(2*32 + 20)	/*GPIO_3_20 */
#define EPDC_SDOE	(2*32 + 23)	/*GPIO_3_23 */
#define EPDC_SDLE	(2*32 + 24)	/*GPIO_3_24 */
#define EPDC_SDSHR	(2*32 + 26)	/*GPIO_3_26 */
#define EPDC_BDR0	(3*32 + 23)	/*GPIO_4_23 */
#define EPDC_SDCE0	(3*32 + 25)	/*GPIO_4_25 */
#define EPDC_SDCE1	(3*32 + 26)	/*GPIO_4_26 */
#define EPDC_SDCE2	(3*32 + 27)	/*GPIO_4_27 */

#define EPDC_PMIC_WAKE		(5*32 + 16)	/*GPIO_6_16 */
#define EPDC_PMIC_INT		(5*32 + 17)	/*GPIO_6_17 */
#define EPDC_VCOM	(3*32 + 21)	/*GPIO_4_21 */
#define EPDC_PWRSTAT	(2*32 + 28)	/*GPIO_3_28 */
#define EPDC_ELCDIF_BACKLIGHT	(1*32 + 18)	/*GPIO_2_18 */
#define CSPI_CS1	(3*32 + 13)	/*GPIO_4_13 */
#define CSPI_CS2	(3*32 + 11) /*GPIO_4_11*/

extern int __init mx50_arm2_init_mc13892(void);
extern struct cpu_wp *(*get_cpu_wp)(int *wp);
extern void (*set_num_cpu_wp)(int num);
static int num_cpu_wp = 2;

static struct pad_desc  mx50_armadillo2[] = {
	/* SD1 */
	MX50_PAD_ECSPI2_SS0__GPIO_4_19,
	MX50_PAD_EIM_CRE__GPIO_1_27,
	MX50_PAD_SD1_CMD__SD1_CMD,

	MX50_PAD_SD1_CLK__SD1_CLK,
	MX50_PAD_SD1_D0__SD1_D0,
	MX50_PAD_SD1_D1__SD1_D1,
	MX50_PAD_SD1_D2__SD1_D2,
	MX50_PAD_SD1_D3__SD1_D3,

	/* SD2 */
	MX50_PAD_SD2_CD__GPIO_5_17,
	MX50_PAD_SD2_WP__GPIO_5_16,
	MX50_PAD_SD2_CMD__SD2_CMD,
	MX50_PAD_SD2_CLK__SD2_CLK,
	MX50_PAD_SD2_D0__SD2_D0,
	MX50_PAD_SD2_D1__SD2_D1,
	MX50_PAD_SD2_D2__SD2_D2,
	MX50_PAD_SD2_D3__SD2_D3,
	MX50_PAD_SD2_D4__SD2_D4,
	MX50_PAD_SD2_D5__SD2_D5,
	MX50_PAD_SD2_D6__SD2_D6,
	MX50_PAD_SD2_D7__SD2_D7,

	/* SD3 */
	MX50_PAD_SD3_WP__GPIO_5_28,
	MX50_PAD_KEY_COL2__GPIO_4_4,
	MX50_PAD_SD3_CMD__SD3_CMD,
	MX50_PAD_SD3_CLK__SD3_CLK,
	MX50_PAD_SD3_D0__SD3_D0,
	MX50_PAD_SD3_D1__SD3_D1,
	MX50_PAD_SD3_D2__SD3_D2,
	MX50_PAD_SD3_D3__SD3_D3,
	MX50_PAD_SD3_D4__SD3_D4,
	MX50_PAD_SD3_D5__SD3_D5,
	MX50_PAD_SD3_D6__SD3_D6,
	MX50_PAD_SD3_D7__SD3_D7,

	MX50_PAD_SSI_RXD__SSI_RXD,
	MX50_PAD_SSI_TXD__SSI_TXD,
	MX50_PAD_SSI_TXC__SSI_TXC,
	MX50_PAD_SSI_TXFS__SSI_TXFS,

	/* LINE1_DETECT (headphone detect) */
	MX50_PAD_ECSPI1_SS0__GPIO_4_15,

	/* PWR_INT */
	MX50_PAD_ECSPI2_MISO__GPIO_4_18,

	/* UART pad setting */
	MX50_PAD_UART1_TXD__UART1_TXD,
	MX50_PAD_UART1_RXD__UART1_RXD,
	MX50_PAD_UART1_CTS__UART1_CTS,
	MX50_PAD_UART1_RTS__UART1_RTS,
	MX50_PAD_UART2_TXD__UART2_TXD,
	MX50_PAD_UART2_RXD__UART2_RXD,
	MX50_PAD_UART2_CTS__UART2_CTS,
	MX50_PAD_UART2_RTS__UART2_RTS,

	MX50_PAD_I2C1_SCL__I2C1_SCL,
	MX50_PAD_I2C1_SDA__I2C1_SDA,
	MX50_PAD_I2C2_SCL__I2C2_SCL,
	MX50_PAD_I2C2_SDA__I2C2_SDA,
	MX50_PAD_I2C3_SCL__I2C3_SCL,
	MX50_PAD_I2C3_SDA__I2C3_SDA,

	/* EPDC pins */
	MX50_PAD_EPDC_D0__EPDC_D0,
	MX50_PAD_EPDC_D1__EPDC_D1,
	MX50_PAD_EPDC_D2__EPDC_D2,
	MX50_PAD_EPDC_D3__EPDC_D3,
	MX50_PAD_EPDC_D4__EPDC_D4,
	MX50_PAD_EPDC_D5__EPDC_D5,
	MX50_PAD_EPDC_D6__EPDC_D6,
	MX50_PAD_EPDC_D7__EPDC_D7,
	MX50_PAD_EPDC_GDCLK__EPDC_GDCLK,
	MX50_PAD_EPDC_GDSP__EPDC_GDSP,
	MX50_PAD_EPDC_GDOE__EPDC_GDOE	,
	MX50_PAD_EPDC_GDRL__EPDC_GDRL,
	MX50_PAD_EPDC_SDCLK__EPDC_SDCLK,
	MX50_PAD_EPDC_SDOE__EPDC_SDOE,
	MX50_PAD_EPDC_SDLE__EPDC_SDLE,
	MX50_PAD_EPDC_SDSHR__EPDC_SDSHR,
	MX50_PAD_EPDC_BDR0__EPDC_BDR0,
	MX50_PAD_EPDC_SDCE0__EPDC_SDCE0,
	MX50_PAD_EPDC_SDCE1__EPDC_SDCE1,
	MX50_PAD_EPDC_SDCE2__EPDC_SDCE2,

	MX50_PAD_EPDC_PWRSTAT__GPIO_3_28,
	MX50_PAD_EPDC_VCOM0__GPIO_4_21,

	MX50_PAD_DISP_D8__DISP_D8,
	MX50_PAD_DISP_D9__DISP_D9,
	MX50_PAD_DISP_D10__DISP_D10,
	MX50_PAD_DISP_D11__DISP_D11,
	MX50_PAD_DISP_D12__DISP_D12,
	MX50_PAD_DISP_D13__DISP_D13,
	MX50_PAD_DISP_D14__DISP_D14,
	MX50_PAD_DISP_D15__DISP_D15,
	MX50_PAD_DISP_RS__ELCDIF_VSYNC,

	/* ELCDIF contrast */
	MX50_PAD_DISP_BUSY__GPIO_2_18,

	MX50_PAD_DISP_CS__ELCDIF_HSYNC,
	MX50_PAD_DISP_RD__ELCDIF_EN,
	MX50_PAD_DISP_WR__ELCDIF_PIXCLK,

	/* EPD PMIC WAKEUP */
	MX50_PAD_UART4_TXD__GPIO_6_16,

	/* EPD PMIC intr */
	MX50_PAD_UART4_RXD__GPIO_6_17,

	MX50_PAD_EPITO__USBH1_PWR,
	/* Need to comment below line if
	 * one needs to debug owire.
	 */
	MX50_PAD_OWIRE__USBH1_OC,
	MX50_PAD_PWM2__USBOTG_PWR,
	MX50_PAD_PWM1__USBOTG_OC,

	MX50_PAD_SSI_RXC__FEC_MDIO,
	MX50_PAD_SSI_RXC__FEC_MDIO,
	MX50_PAD_DISP_D0__FEC_TXCLK,
	MX50_PAD_DISP_D1__FEC_RX_ER,
	MX50_PAD_DISP_D2__FEC_RX_DV,
	MX50_PAD_DISP_D3__FEC_RXD1,
	MX50_PAD_DISP_D4__FEC_RXD0,
	MX50_PAD_DISP_D5__FEC_TX_EN,
	MX50_PAD_DISP_D6__FEC_TXD1,
	MX50_PAD_DISP_D7__FEC_TXD0,
	MX50_PAD_SSI_RXFS__FEC_MDC,

	MX50_PAD_CSPI_SS0__CSPI_SS0,
	MX50_PAD_ECSPI1_MOSI__CSPI_SS1,
	MX50_PAD_CSPI_MOSI__CSPI_MOSI,
	MX50_PAD_CSPI_MISO__CSPI_MISO,
};

static struct pad_desc  mx50_gpmi_nand[] = {
	MX50_PIN_EIM_DA8__NANDF_CLE,
	MX50_PIN_EIM_DA9__NANDF_ALE,
	MX50_PIN_EIM_DA10__NANDF_CE0,
	MX50_PIN_EIM_DA11__NANDF_CE1,
	MX50_PIN_EIM_DA12__NANDF_CE2,
	MX50_PIN_EIM_DA13__NANDF_CE3,
	MX50_PIN_EIM_DA14__NANDF_READY,
	MX50_PIN_EIM_DA15__NANDF_DQS,
	MX50_PIN_SD3_D4__NANDF_D0,
	MX50_PIN_SD3_D5__NANDF_D1,
	MX50_PIN_SD3_D6__NANDF_D2,
	MX50_PIN_SD3_D7__NANDF_D3,
	MX50_PIN_SD3_D0__NANDF_D4,
	MX50_PIN_SD3_D1__NANDF_D5,
	MX50_PIN_SD3_D2__NANDF_D6,
	MX50_PIN_SD3_D3__NANDF_D7,
	MX50_PIN_SD3_CLK__NANDF_RDN,
	MX50_PIN_SD3_CMD__NANDF_WRN,
	MX50_PIN_SD3_WP__NANDF_RESETN,
};

static struct mxc_dvfs_platform_data dvfs_core_data = {
	.reg_id = "SW1",
	.clk1_id = "cpu_clk",
	.clk2_id = "gpc_dvfs_clk",
	.gpc_cntr_offset = MXC_GPC_CNTR_OFFSET,
	.gpc_vcr_offset = MXC_GPC_VCR_OFFSET,
	.ccm_cdcr_offset = MXC_CCM_CDCR_OFFSET,
	.ccm_cacrr_offset = MXC_CCM_CACRR_OFFSET,
	.ccm_cdhipr_offset = MXC_CCM_CDHIPR_OFFSET,
	.prediv_mask = 0x1F800,
	.prediv_offset = 11,
	.prediv_val = 3,
	.div3ck_mask = 0xE0000000,
	.div3ck_offset = 29,
	.div3ck_val = 2,
	.emac_val = 0x08,
	.upthr_val = 25,
	.dnthr_val = 9,
	.pncthr_val = 33,
	.upcnt_val = 10,
	.dncnt_val = 10,
	.delay_time = 30,
	.num_wp = 2,
};

/* working point(wp): 0 - 800MHz; 1 - 166.25MHz; */
static struct cpu_wp cpu_wp_auto[] = {
	{
	 .pll_rate = 800000000,
	 .cpu_rate = 800000000,
	 .pdf = 0,
	 .mfi = 8,
	 .mfd = 2,
	 .mfn = 1,
	 .cpu_podf = 0,
	 .cpu_voltage = 1050000,},
	{
	 .pll_rate = 800000000,
	 .cpu_rate = 160000000,
	 .pdf = 4,
	 .mfi = 8,
	 .mfd = 2,
	 .mfn = 1,
	 .cpu_podf = 4,
	 .cpu_voltage = 850000,},
};

static struct cpu_wp *mx50_arm2_get_cpu_wp(int *wp)
{
	*wp = num_cpu_wp;
	return cpu_wp_auto;
}

static void mx50_arm2_set_num_cpu_wp(int num)
{
	num_cpu_wp = num;
	return;
}

static struct mxc_w1_config mxc_w1_data = {
	.search_rom_accelerator = 1,
};

static struct fec_platform_data fec_data = {
	.phy = PHY_INTERFACE_MODE_RMII,
	.phy_mask = ~1UL,
};

/* workaround for cspi chipselect pin may not keep correct level when idle */
static void mx50_arm2_gpio_spi_chipselect_active(int cspi_mode, int status,
					     int chipselect)
{
	switch (cspi_mode) {
	case 1:
		break;
	case 2:
		break;
	case 3:
		switch (chipselect) {
		case 0x1:
			{
			struct pad_desc cspi_ss0 = MX50_PAD_CSPI_SS0__CSPI_SS0;
			struct pad_desc cspi_cs1 = MX50_PAD_ECSPI1_MOSI__GPIO_4_13;

			/* pull up/down deassert it */
			mxc_iomux_v3_setup_pad(&cspi_ss0);
			mxc_iomux_v3_setup_pad(&cspi_cs1);

			gpio_request(CSPI_CS1, "cspi-cs1");
			gpio_direction_input(CSPI_CS1);
			}
			break;
		case 0x2:
			{
			struct pad_desc cspi_ss1 = MX50_PAD_ECSPI1_MOSI__CSPI_SS1;
			struct pad_desc cspi_ss0 = MX50_PAD_CSPI_SS0__GPIO_4_11;

			/*disable other ss */
			mxc_iomux_v3_setup_pad(&cspi_ss1);
			mxc_iomux_v3_setup_pad(&cspi_ss0);

			/* pull up/down deassert it */
			gpio_request(CSPI_CS2, "cspi-cs2");
			gpio_direction_input(CSPI_CS2);
			}
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

static void mx50_arm2_gpio_spi_chipselect_inactive(int cspi_mode, int status,
					       int chipselect)
{
	switch (cspi_mode) {
	case 1:
		break;
	case 2:
		break;
	case 3:
		switch (chipselect) {
		case 0x1:
			gpio_free(CSPI_CS1);
			break;
		case 0x2:
			gpio_free(CSPI_CS2);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

}

static struct mxc_spi_master mxcspi1_data = {
	.maxchipselect = 4,
	.spi_version = 23,
	.chipselect_active = mx50_arm2_gpio_spi_chipselect_active,
	.chipselect_inactive = mx50_arm2_gpio_spi_chipselect_inactive,
};

static struct mxc_spi_master mxcspi3_data = {
	.maxchipselect = 4,
	.spi_version = 7,
	.chipselect_active = mx50_arm2_gpio_spi_chipselect_active,
	.chipselect_inactive = mx50_arm2_gpio_spi_chipselect_inactive,
};

static struct mxc_i2c_platform_data mxci2c_data = {
	.i2c_clk = 100000,
};

static struct mxc_srtc_platform_data srtc_data = {
	.srtc_sec_mode_addr = OCOTP_CTRL_BASE_ADDR + 0x80,
};

static int z160_version = 1;

#define mV_to_uV(mV) (mV * 1000)
#define uV_to_mV(uV) (uV / 1000)
#define V_to_uV(V) (mV_to_uV(V * 1000))
#define uV_to_V(uV) (uV_to_mV(uV) / 1000)

static struct regulator_init_data max17135_init_data[] __initdata = {
	{
		.constraints = {
			.name = "DISPLAY",
		},
	}, {
		.constraints = {
			.name = "GVDD",
			.min_uV = V_to_uV(20),
			.max_uV = V_to_uV(20),
		},
	}, {
		.constraints = {
			.name = "GVEE",
			.min_uV = V_to_uV(-22),
			.max_uV = V_to_uV(-22),
		},
	}, {
		.constraints = {
			.name = "HVINN",
			.min_uV = V_to_uV(-22),
			.max_uV = V_to_uV(-22),
		},
	}, {
		.constraints = {
			.name = "HVINP",
			.min_uV = V_to_uV(20),
			.max_uV = V_to_uV(20),
		},
	}, {
		.constraints = {
			.name = "VCOM",
			.min_uV = mV_to_uV(-4325),
			.max_uV = mV_to_uV(-500),
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
		},
	}, {
		.constraints = {
			.name = "VNEG",
			.min_uV = V_to_uV(-15),
			.max_uV = V_to_uV(-15),
		},
	}, {
		.constraints = {
			.name = "VPOS",
			.min_uV = V_to_uV(15),
			.max_uV = V_to_uV(15),
		},
	},
};

static void epdc_get_pins(void)
{
	/* Claim GPIOs for EPDC pins - used during power up/down */
	gpio_request(EPDC_D0, "epdc_d0");
	gpio_request(EPDC_D1, "epdc_d1");
	gpio_request(EPDC_D2, "epdc_d2");
	gpio_request(EPDC_D3, "epdc_d3");
	gpio_request(EPDC_D4, "epdc_d4");
	gpio_request(EPDC_D5, "epdc_d5");
	gpio_request(EPDC_D6, "epdc_d6");
	gpio_request(EPDC_D7, "epdc_d7");
	gpio_request(EPDC_GDCLK, "epdc_gdclk");
	gpio_request(EPDC_GDSP, "epdc_gdsp");
	gpio_request(EPDC_GDOE, "epdc_gdoe");
	gpio_request(EPDC_GDRL, "epdc_gdrl");
	gpio_request(EPDC_SDCLK, "epdc_sdclk");
	gpio_request(EPDC_SDOE, "epdc_sdoe");
	gpio_request(EPDC_SDLE, "epdc_sdle");
	gpio_request(EPDC_SDSHR, "epdc_sdshr");
	gpio_request(EPDC_BDR0, "epdc_bdr0");
	gpio_request(EPDC_SDCE0, "epdc_sdce0");
	gpio_request(EPDC_SDCE1, "epdc_sdce1");
	gpio_request(EPDC_SDCE2, "epdc_sdce2");
}

static void epdc_put_pins(void)
{
	gpio_free(EPDC_D0);
	gpio_free(EPDC_D1);
	gpio_free(EPDC_D2);
	gpio_free(EPDC_D3);
	gpio_free(EPDC_D4);
	gpio_free(EPDC_D5);
	gpio_free(EPDC_D6);
	gpio_free(EPDC_D7);
	gpio_free(EPDC_GDCLK);
	gpio_free(EPDC_GDSP);
	gpio_free(EPDC_GDOE);
	gpio_free(EPDC_GDRL);
	gpio_free(EPDC_SDCLK);
	gpio_free(EPDC_SDOE);
	gpio_free(EPDC_SDLE);
	gpio_free(EPDC_SDSHR);
	gpio_free(EPDC_BDR0);
	gpio_free(EPDC_SDCE0);
	gpio_free(EPDC_SDCE1);
	gpio_free(EPDC_SDCE2);
}

static void epdc_enable_pins(void)
{
	struct pad_desc epdc_d0 = MX50_PAD_EPDC_D0__EPDC_D0;
	struct pad_desc epdc_d1 = MX50_PAD_EPDC_D1__EPDC_D1;
	struct pad_desc epdc_d2 = MX50_PAD_EPDC_D2__EPDC_D2;
	struct pad_desc epdc_d3 = MX50_PAD_EPDC_D3__EPDC_D3;
	struct pad_desc epdc_d4 = MX50_PAD_EPDC_D4__EPDC_D4;
	struct pad_desc epdc_d5 = MX50_PAD_EPDC_D5__EPDC_D5;
	struct pad_desc epdc_d6 = MX50_PAD_EPDC_D6__EPDC_D6;
	struct pad_desc epdc_d7 = MX50_PAD_EPDC_D7__EPDC_D7;
	struct pad_desc epdc_gdclk = MX50_PAD_EPDC_GDCLK__EPDC_GDCLK;
	struct pad_desc epdc_gdsp = MX50_PAD_EPDC_GDSP__EPDC_GDSP;
	struct pad_desc epdc_gdoe = MX50_PAD_EPDC_GDOE__EPDC_GDOE;
	struct pad_desc epdc_gdrl = MX50_PAD_EPDC_GDRL__EPDC_GDRL;
	struct pad_desc epdc_sdclk = MX50_PAD_EPDC_SDCLK__EPDC_SDCLK;
	struct pad_desc epdc_sdoe = MX50_PAD_EPDC_SDOE__EPDC_SDOE;
	struct pad_desc epdc_sdle = MX50_PAD_EPDC_SDLE__EPDC_SDLE;
	struct pad_desc epdc_sdshr = MX50_PAD_EPDC_SDSHR__EPDC_SDSHR;
	struct pad_desc epdc_bdr0 = MX50_PAD_EPDC_BDR0__EPDC_BDR0;
	struct pad_desc epdc_sdce0 = MX50_PAD_EPDC_SDCE0__EPDC_SDCE0;
	struct pad_desc epdc_sdce1 = MX50_PAD_EPDC_SDCE1__EPDC_SDCE1;
	struct pad_desc epdc_sdce2 = MX50_PAD_EPDC_SDCE2__EPDC_SDCE2;

	/* Configure MUX settings to enable EPDC use */
	mxc_iomux_v3_setup_pad(&epdc_d0);
	mxc_iomux_v3_setup_pad(&epdc_d1);
	mxc_iomux_v3_setup_pad(&epdc_d2);
	mxc_iomux_v3_setup_pad(&epdc_d3);
	mxc_iomux_v3_setup_pad(&epdc_d4);
	mxc_iomux_v3_setup_pad(&epdc_d5);
	mxc_iomux_v3_setup_pad(&epdc_d6);
	mxc_iomux_v3_setup_pad(&epdc_d7);
	mxc_iomux_v3_setup_pad(&epdc_gdclk);
	mxc_iomux_v3_setup_pad(&epdc_gdsp);
	mxc_iomux_v3_setup_pad(&epdc_gdoe);
	mxc_iomux_v3_setup_pad(&epdc_gdrl);
	mxc_iomux_v3_setup_pad(&epdc_sdclk);
	mxc_iomux_v3_setup_pad(&epdc_sdoe);
	mxc_iomux_v3_setup_pad(&epdc_sdle);
	mxc_iomux_v3_setup_pad(&epdc_sdshr);
	mxc_iomux_v3_setup_pad(&epdc_bdr0);
	mxc_iomux_v3_setup_pad(&epdc_sdce0);
	mxc_iomux_v3_setup_pad(&epdc_sdce1);
	mxc_iomux_v3_setup_pad(&epdc_sdce2);

	gpio_direction_input(EPDC_D0);
	gpio_direction_input(EPDC_D1);
	gpio_direction_input(EPDC_D2);
	gpio_direction_input(EPDC_D3);
	gpio_direction_input(EPDC_D4);
	gpio_direction_input(EPDC_D5);
	gpio_direction_input(EPDC_D6);
	gpio_direction_input(EPDC_D7);
	gpio_direction_input(EPDC_GDCLK);
	gpio_direction_input(EPDC_GDSP);
	gpio_direction_input(EPDC_GDOE);
	gpio_direction_input(EPDC_GDRL);
	gpio_direction_input(EPDC_SDCLK);
	gpio_direction_input(EPDC_SDOE);
	gpio_direction_input(EPDC_SDLE);
	gpio_direction_input(EPDC_SDSHR);
	gpio_direction_input(EPDC_BDR0);
	gpio_direction_input(EPDC_SDCE0);
	gpio_direction_input(EPDC_SDCE1);
	gpio_direction_input(EPDC_SDCE2);
}

static void epdc_disable_pins(void)
{
	struct pad_desc epdc_d0 = MX50_PAD_EPDC_D0__GPIO_3_0;
	struct pad_desc epdc_d1 = MX50_PAD_EPDC_D1__GPIO_3_1;
	struct pad_desc epdc_d2 = MX50_PAD_EPDC_D2__GPIO_3_2;
	struct pad_desc epdc_d3 = MX50_PAD_EPDC_D3__GPIO_3_3;
	struct pad_desc epdc_d4 = MX50_PAD_EPDC_D4__GPIO_3_4;
	struct pad_desc epdc_d5 = MX50_PAD_EPDC_D5__GPIO_3_5;
	struct pad_desc epdc_d6 = MX50_PAD_EPDC_D6__GPIO_3_6;
	struct pad_desc epdc_d7 = MX50_PAD_EPDC_D7__GPIO_3_7;
	struct pad_desc epdc_gdclk = MX50_PAD_EPDC_GDCLK__GPIO_3_16;
	struct pad_desc epdc_gdsp = MX50_PAD_EPDC_GDSP__GPIO_3_17;
	struct pad_desc epdc_gdoe = MX50_PAD_EPDC_GDOE__GPIO_3_18;
	struct pad_desc epdc_gdrl = MX50_PAD_EPDC_GDRL__GPIO_3_19;
	struct pad_desc epdc_sdclk = MX50_PAD_EPDC_SDCLK__GPIO_3_20;
	struct pad_desc epdc_sdoe = MX50_PAD_EPDC_SDOE__GPIO_3_23;
	struct pad_desc epdc_sdle = MX50_PAD_EPDC_SDLE__GPIO_3_24;
	struct pad_desc epdc_sdshr = MX50_PAD_EPDC_SDSHR__GPIO_3_26;
	struct pad_desc epdc_bdr0 = MX50_PAD_EPDC_BDR0__GPIO_4_23;
	struct pad_desc epdc_sdce0 = MX50_PAD_EPDC_SDCE0__GPIO_4_25;
	struct pad_desc epdc_sdce1 = MX50_PAD_EPDC_SDCE1__GPIO_4_26;
	struct pad_desc epdc_sdce2 = MX50_PAD_EPDC_SDCE2__GPIO_4_27;

	/* Configure MUX settings for EPDC pins to
	 * GPIO and drive to 0. */
	mxc_iomux_v3_setup_pad(&epdc_d0);
	mxc_iomux_v3_setup_pad(&epdc_d1);
	mxc_iomux_v3_setup_pad(&epdc_d2);
	mxc_iomux_v3_setup_pad(&epdc_d3);
	mxc_iomux_v3_setup_pad(&epdc_d4);
	mxc_iomux_v3_setup_pad(&epdc_d5);
	mxc_iomux_v3_setup_pad(&epdc_d6);
	mxc_iomux_v3_setup_pad(&epdc_d7);
	mxc_iomux_v3_setup_pad(&epdc_gdclk);
	mxc_iomux_v3_setup_pad(&epdc_gdsp);
	mxc_iomux_v3_setup_pad(&epdc_gdoe);
	mxc_iomux_v3_setup_pad(&epdc_gdrl);
	mxc_iomux_v3_setup_pad(&epdc_sdclk);
	mxc_iomux_v3_setup_pad(&epdc_sdoe);
	mxc_iomux_v3_setup_pad(&epdc_sdle);
	mxc_iomux_v3_setup_pad(&epdc_sdshr);
	mxc_iomux_v3_setup_pad(&epdc_bdr0);
	mxc_iomux_v3_setup_pad(&epdc_sdce0);
	mxc_iomux_v3_setup_pad(&epdc_sdce1);
	mxc_iomux_v3_setup_pad(&epdc_sdce2);

	gpio_direction_output(EPDC_D0, 0);
	gpio_direction_output(EPDC_D1, 0);
	gpio_direction_output(EPDC_D2, 0);
	gpio_direction_output(EPDC_D3, 0);
	gpio_direction_output(EPDC_D4, 0);
	gpio_direction_output(EPDC_D5, 0);
	gpio_direction_output(EPDC_D6, 0);
	gpio_direction_output(EPDC_D7, 0);
	gpio_direction_output(EPDC_GDCLK, 0);
	gpio_direction_output(EPDC_GDSP, 0);
	gpio_direction_output(EPDC_GDOE, 0);
	gpio_direction_output(EPDC_GDRL, 0);
	gpio_direction_output(EPDC_SDCLK, 0);
	gpio_direction_output(EPDC_SDOE, 0);
	gpio_direction_output(EPDC_SDLE, 0);
	gpio_direction_output(EPDC_SDSHR, 0);
	gpio_direction_output(EPDC_BDR0, 0);
	gpio_direction_output(EPDC_SDCE0, 0);
	gpio_direction_output(EPDC_SDCE1, 0);
	gpio_direction_output(EPDC_SDCE2, 0);
}

static struct fb_videomode e60_mode = {
	.name = "E60",
	.refresh = 50,
	.xres = 800,
	.yres = 600,
	.pixclock = 20000000,
	.left_margin = 10,
	.right_margin = 217,
	.upper_margin = 4,
	.lower_margin = 10,
	.hsync_len = 20,
	.vsync_len = 4,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

static struct fb_videomode e97_mode = {
	.name = "E97",
	.refresh = 50,
	.xres = 1200,
	.yres = 825,
	.pixclock = 32000000,
	.left_margin = 8,
	.right_margin = 125,
	.upper_margin = 4,
	.lower_margin = 17,
	.hsync_len = 20,
	.vsync_len = 4,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

static struct mxc_epdc_fb_mode panel_modes[] = {
	{
		&e60_mode,
		4, 10, 20, 10, 20, 480, 20, 0, 1, 1,
	},
	{
		&e97_mode,
		8, 10, 20, 10, 20, 580, 20, 0, 1, 3,
	},
};

static struct mxc_epdc_fb_platform_data epdc_data = {
	.epdc_mode = panel_modes,
	.num_modes = ARRAY_SIZE(panel_modes),
	.get_pins = epdc_get_pins,
	.put_pins = epdc_put_pins,
	.enable_pins = epdc_enable_pins,
	.disable_pins = epdc_disable_pins,
};


static struct max17135_platform_data max17135_pdata __initdata = {
	.vneg_pwrup = 1,
	.gvee_pwrup = 1,
	.vpos_pwrup = 2,
	.gvdd_pwrup = 1,
	.gvdd_pwrdn = 1,
	.vpos_pwrdn = 2,
	.gvee_pwrdn = 1,
	.vneg_pwrdn = 1,
	.gpio_pmic_pwrgood = EPDC_PWRSTAT,
	.gpio_pmic_vcom_ctrl = EPDC_VCOM,
	.gpio_pmic_wakeup = EPDC_PMIC_WAKE,
	.gpio_pmic_intr = EPDC_PMIC_INT,
	.regulator_init = max17135_init_data,
};

static int __initdata max17135_pass_num = { 1 };
static int __initdata max17135_vcom = { -1250000 };
/*
 * Parse user specified options (`max17135:')
 * example:
 * 	max17135:pass=2,vcom=-1250000
 */
static int __init max17135_setup(char *options)
{
	char *opt;
	while ((opt = strsep(&options, ",")) != NULL) {
		if (!*opt)
			continue;
		if (!strncmp(opt, "pass=", 5))
			max17135_pass_num =
				simple_strtoul(opt + 5, NULL, 0);
		if (!strncmp(opt, "vcom=", 5)) {
			int offs = 5;
			if (opt[5] == '-')
				offs = 6;
			max17135_vcom =
				simple_strtoul(opt + offs, NULL, 0);
			max17135_vcom = -max17135_vcom;
		}
	}
	return 1;
}

__setup("max17135:", max17135_setup);

static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
	{
	 .type = "sgtl5000-i2c",
	 .addr = 0x0a,
	 },
	{
	 .type = "backlight-i2c",
	 .addr = 0x2c,
	 },
	{
	 .type = "eeprom",
	 .addr = 0x50,
	 },
};

static struct i2c_board_info mxc_i2c2_board_info[] __initdata = {
	{
	 I2C_BOARD_INFO("max17135", 0x48),
	 .platform_data = &max17135_pdata,
	 },
};

static struct mtd_partition mxc_dataflash_partitions[] = {
	{
	 .name = "bootloader",
	 .offset = 0,
	 .size = 0x000100000,},
	{
	 .name = "kernel",
	 .offset = MTDPART_OFS_APPEND,
	 .size = MTDPART_SIZ_FULL,},
};

static struct flash_platform_data mxc_spi_flash_data[] = {
	{
	 .name = "mxc_dataflash",
	 .parts = mxc_dataflash_partitions,
	 .nr_parts = ARRAY_SIZE(mxc_dataflash_partitions),
	 .type = "at45db321d",}
};


static struct spi_board_info mxc_dataflash_device[] __initdata = {
	{
	 .modalias = "mxc_dataflash",
	 .max_speed_hz = 25000000,	/* max spi clock (SCK) speed in HZ */
	 .bus_num = 3,
	 .chip_select = 1,
	 .platform_data = &mxc_spi_flash_data[0],},
};

static int sdhc_write_protect(struct device *dev)
{
	unsigned short rc = 0;

	if (to_platform_device(dev)->id == 0)
		rc = gpio_get_value(SD1_WP);
	else if (to_platform_device(dev)->id == 1)
		rc = gpio_get_value(SD2_WP);
	else if (to_platform_device(dev)->id == 2)
		rc = gpio_get_value(SD3_WP);

	return rc;
}

static unsigned int sdhc_get_card_det_status(struct device *dev)
{
	int ret = 0;
	if (to_platform_device(dev)->id == 0)
		ret = gpio_get_value(SD1_CD);
	else if (to_platform_device(dev)->id == 1)
		ret = gpio_get_value(SD2_CD);
	else if (to_platform_device(dev)->id == 2)
		ret = gpio_get_value(SD3_CD);

	return ret;
}

static struct mxc_mmc_platform_data mmc1_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30
		| MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA,
	.min_clk = 400000,
	.max_clk = 50000000,
	.card_inserted_state = 0,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
	.power_mmc = NULL,
};

static struct mxc_mmc_platform_data mmc2_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30
		| MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
	.min_clk = 400000,
	.max_clk = 50000000,
	.card_inserted_state = 0,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
};

static struct mxc_mmc_platform_data mmc3_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30
		| MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA | MMC_CAP_DATA_DDR,
	.min_clk = 400000,
	.max_clk = 40000000,
	.dll_override_en = 1,
	.dll_delay_cells = 0xc,
	.card_inserted_state = 0,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
	.clk_always_on = 1,
};

static int mxc_sgtl5000_amp_enable(int enable)
{
/* TO DO */
	return 0;
}

static int headphone_det_status(void)
{
	return (gpio_get_value(HP_DETECT) != 0);
}

static struct mxc_audio_platform_data sgtl5000_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 3,
	.hp_irq = IOMUX_TO_IRQ_V3(HP_DETECT),
	.hp_status = headphone_det_status,
	.amp_enable = mxc_sgtl5000_amp_enable,
	.sysclk = 12288000,
};

static struct platform_device mxc_sgtl5000_device = {
	.name = "imx-3stack-sgtl5000",
};

static struct pad_desc armadillo2_wvga_pads[] = {
	MX50_PAD_DISP_D0__DISP_D0,
	MX50_PAD_DISP_D1__DISP_D1,
	MX50_PAD_DISP_D2__DISP_D2,
	MX50_PAD_DISP_D3__DISP_D3,
	MX50_PAD_DISP_D4__DISP_D4,
	MX50_PAD_DISP_D5__DISP_D5,
	MX50_PAD_DISP_D6__DISP_D6,
	MX50_PAD_DISP_D7__DISP_D7,
};

static void wvga_reset(void)
{
	mxc_iomux_v3_setup_multiple_pads(armadillo2_wvga_pads, \
				ARRAY_SIZE(armadillo2_wvga_pads));
	return;
}

static struct mxc_lcd_platform_data lcd_wvga_data = {
	.reset = wvga_reset,
};

static struct platform_device lcd_wvga_device = {
	.name = "lcd_claa",
	.dev = {
		.platform_data = &lcd_wvga_data,
		},
};

static struct fb_videomode video_modes[] = {
	{
	 /* 800x480 @ 57 Hz , pixel clk @ 27MHz */
	 "CLAA-WVGA", 57, 800, 480, 37037, 40, 60, 10, 10, 20, 10,
	 FB_SYNC_CLK_LAT_FALL,
	 FB_VMODE_NONINTERLACED,
	 0,},
};

static struct mxc_fb_platform_data fb_data[] = {
	{
	 .interface_pix_fmt = V4L2_PIX_FMT_RGB565,
	 .mode_str = "CLAA-WVGA",
	 .mode = video_modes,
	 .num_modes = ARRAY_SIZE(video_modes),
	 },
};

static int __initdata enable_w1 = { 0 };
static int __init w1_setup(char *__unused)
{
	enable_w1 = 1;
	return cpu_is_mx50();
}

__setup("w1", w1_setup);

int enable_gpmi_nand = { 0 };
static int __init gpmi_nand_setup(char *__unused)
{
	enable_gpmi_nand = 1;
	return 1;
}

__setup("gpmi:nand", gpmi_nand_setup);

static struct mxs_dma_plat_data dma_apbh_data = {
	.chan_base = MXS_DMA_CHANNEL_AHB_APBH,
	.chan_num = MXS_MAX_DMA_CHANNELS,
};

static int gpmi_nfc_platform_init(unsigned int max_chip_count)
{
	return !enable_gpmi_nand;
}

static void gpmi_nfc_platform_exit(unsigned int max_chip_count)
{
}

static const char *gpmi_nfc_partition_source_types[] = { "cmdlinepart", 0 };

static struct gpmi_nfc_platform_data  gpmi_nfc_platform_data = {
	.nfc_version             = 2,
	.boot_rom_version        = 1,
	.clock_name              = "gpmi-nfc",
	.platform_init           = gpmi_nfc_platform_init,
	.platform_exit           = gpmi_nfc_platform_exit,
	.min_prop_delay_in_ns    = 5,
	.max_prop_delay_in_ns    = 9,
	.max_chip_count          = 2,
	.boot_area_size_in_bytes = 20 * SZ_1M,
	.partition_source_types  = gpmi_nfc_partition_source_types,
	.partitions              = 0,
	.partition_count         = 0,
};

static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem_adsp",
	.start = 0,
	.size = SZ_4M,
	.no_allocator = 0,
	.cached = PMEM_NONCACHE_NORMAL,
};

static struct android_pmem_platform_data android_pmem_gpu_pdata = {
	.name = "pmem_gpu",
	.start = 0,
	.size = SZ_32M,
	.no_allocator = 0,
	.cached = PMEM_CACHE_ENABLE,
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id      = 0x0bb4,
	.product_id     = 0x0c01,
	.adb_product_id = 0x0c02,
	.version        = 0x0100,
	.product_name   = "Android Phone",
	.manufacturer_name = "Freescale",
	.nluns = 3,
};

/* OTP data */
/* Building up eight registers's names of a bank */
#define BANK(a, b, c, d, e, f, g, h)	\
	{\
	("HW_OCOTP_"#a), ("HW_OCOTP_"#b), ("HW_OCOTP_"#c), ("HW_OCOTP_"#d), \
	("HW_OCOTP_"#e), ("HW_OCOTP_"#f), ("HW_OCOTP_"#g), ("HW_OCOTP_"#h) \
	}

#define BANKS		(5)
#define BANK_ITEMS	(8)
static const char *bank_reg_desc[BANKS][BANK_ITEMS] = {
	BANK(LOCK, CFG0, CFG1, CFG2, CFG3, CFG4, CFG5, CFG6),
	BANK(MEM0, MEM1, MEM2, MEM3, MEM4, MEM5, GP0, GP1),
	BANK(SCC0, SCC1, SCC2, SCC3, SCC4, SCC5, SCC6, SCC7),
	BANK(SRK0, SRK1, SRK2, SRK3, SRK4, SRK5, SRK6, SRK7),
	BANK(SJC0, SJC1, MAC0, MAC1, HWCAP0, HWCAP1, HWCAP2, SWCAP),
};

static struct fsl_otp_data otp_data = {
	.fuse_name	= (char **)bank_reg_desc,
	.fuse_num	= BANKS * BANK_ITEMS,
};
#undef BANK
#undef BANKS
#undef BANK_ITEMS

/*!
 * Board specific fixup function. It is called by \b setup_arch() in
 * setup.c file very early on during kernel starts. It allows the user to
 * statically fill in the proper values for the passed-in parameters. None of
 * the parameters is used currently.
 *
 * @param  desc         pointer to \b struct \b machine_desc
 * @param  tags         pointer to \b struct \b tag
 * @param  cmdline      pointer to the command line
 * @param  mi           pointer to \b struct \b meminfo
 */
static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	struct tag *t;
	int size;

	mxc_set_cpu_type(MXC_CPU_MX50);

	get_cpu_wp = mx50_arm2_get_cpu_wp;
	set_num_cpu_wp = mx50_arm2_set_num_cpu_wp;

	for_each_tag(t, tags) {
		if (t->hdr.tag != ATAG_MEM)
			continue;
		size = t->u.mem.size;

		android_pmem_pdata.start =
				PHYS_OFFSET + size - android_pmem_pdata.size;
		android_pmem_gpu_pdata.start =
				android_pmem_pdata.start - android_pmem_gpu_pdata.size;
#if 0
		gpu_device.resource[5].start =
				android_pmem_gpu_pdata.start - SZ_16M;
		gpu_device.resource[5].end =
				gpu_device.resource[5].start + SZ_16M - 1;
#endif
		size -= android_pmem_pdata.size;
		size -= android_pmem_gpu_pdata.size;
		//size -= SZ_16M;
		t->u.mem.size = size;
	}
}

static void __init mx50_arm2_io_init(void)
{
	mxc_iomux_v3_setup_multiple_pads(mx50_armadillo2, \
			ARRAY_SIZE(mx50_armadillo2));

	gpio_request(SD1_WP, "sdhc1-wp");
	gpio_direction_input(SD1_WP);

	gpio_request(SD1_CD, "sdhc1-cd");
	gpio_direction_input(SD1_CD);

	gpio_request(SD2_WP, "sdhc2-wp");
	gpio_direction_input(SD2_WP);

	gpio_request(SD2_CD, "sdhc2-cd");
	gpio_direction_input(SD2_CD);

	gpio_request(SD3_WP, "sdhc3-wp");
	gpio_direction_input(SD3_WP);

	gpio_request(SD3_CD, "sdhc3-cd");
	gpio_direction_input(SD3_CD);

	gpio_request(HP_DETECT, "hp-det");
	gpio_direction_input(HP_DETECT);

	gpio_request(PWR_INT, "pwr-int");
	gpio_direction_input(PWR_INT);

	gpio_request(EPDC_PMIC_WAKE, "epdc-pmic-wake");
	gpio_direction_output(EPDC_PMIC_WAKE, 0);

	gpio_request(EPDC_VCOM, "epdc-vcom");
	gpio_direction_output(EPDC_VCOM, 0);

	gpio_request(EPDC_PMIC_INT, "epdc-pmic-int");
	gpio_direction_input(EPDC_PMIC_INT);

	gpio_request(EPDC_PWRSTAT, "epdc-pwrstat");
	gpio_direction_input(EPDC_PWRSTAT);

	/* ELCDIF backlight */
	gpio_request(EPDC_ELCDIF_BACKLIGHT, "elcdif-backlight");
	gpio_direction_output(EPDC_ELCDIF_BACKLIGHT, 1);

	if (enable_w1) {
		struct pad_desc one_wire = MX50_PAD_OWIRE__OWIRE;
		mxc_iomux_v3_setup_pad(&one_wire);
	}

	if (enable_gpmi_nand)
		mxc_iomux_v3_setup_multiple_pads(mx50_gpmi_nand, \
					ARRAY_SIZE(mx50_gpmi_nand));
}

/*!
 * Board specific initialization.
 */
static void __init mxc_board_init(void)
{
	/* SD card detect irqs */
	mxcsdhc1_device.resource[2].start = IOMUX_TO_IRQ_V3(SD1_CD);
	mxcsdhc1_device.resource[2].end = IOMUX_TO_IRQ_V3(SD1_CD);
	mxcsdhc2_device.resource[2].start = IOMUX_TO_IRQ_V3(SD2_CD);
	mxcsdhc2_device.resource[2].end = IOMUX_TO_IRQ_V3(SD2_CD);
	mxcsdhc3_device.resource[2].start = IOMUX_TO_IRQ_V3(SD3_CD);
	mxcsdhc3_device.resource[2].end = IOMUX_TO_IRQ_V3(SD3_CD);

	mxc_cpu_common_init();
	mxc_register_gpios();
	mx50_arm2_io_init();

	mxc_register_device(&mxc_dma_device, NULL);
	//mxc_register_device(&mxs_dma_apbh_device, &dma_apbh_data);
	mxc_register_device(&mxc_wdt_device, NULL);
	mxc_register_device(&mxcspi1_device, &mxcspi1_data);
	mxc_register_device(&mxcspi3_device, &mxcspi3_data);
	mxc_register_device(&mxci2c_devices[0], &mxci2c_data);
	mxc_register_device(&mxci2c_devices[1], &mxci2c_data);
	mxc_register_device(&mxci2c_devices[2], &mxci2c_data);

	mxc_register_device(&mxc_rtc_device, &srtc_data);
	mxc_register_device(&mxc_w1_master_device, &mxc_w1_data);
	mxc_register_device(&gpu_device, &z160_version);
	mxc_register_device(&mxc_pxp_device, NULL);
	mxc_register_device(&mxc_pxp_client_device, NULL);
	mxc_register_device(&mxc_pxp_v4l2, NULL);
	mxc_register_device(&mxc_dvfs_core_device, &dvfs_core_data);
	mxc_register_device(&busfreq_device, NULL);

	/*
	mxc_register_device(&mx53_lpmode_device, NULL);
	mxc_register_device(&mxc_dvfs_per_device, &dvfs_per_data);
	*/

/*	mxc_register_device(&mxc_keypad_device, &keypad_plat_data); */

	mxc_register_device(&mxcsdhc1_device, &mmc1_data);
	mxc_register_device(&mxcsdhc2_device, &mmc2_data);
	mxc_register_device(&mxcsdhc3_device, &mmc3_data);
	mxc_register_device(&mxc_ssi1_device, NULL);
	mxc_register_device(&mxc_ssi2_device, NULL);
	mxc_register_device(&mxc_fec_device, &fec_data);
	spi_register_board_info(mxc_dataflash_device,
				ARRAY_SIZE(mxc_dataflash_device));
	i2c_register_board_info(1, mxc_i2c1_board_info,
				ARRAY_SIZE(mxc_i2c1_board_info));
	max17135_pdata.pass_num = max17135_pass_num;
	max17135_pdata.vcom_uV = max17135_vcom;
	i2c_register_board_info(2, mxc_i2c2_board_info,
				ARRAY_SIZE(mxc_i2c2_board_info));

	mxc_register_device(&epdc_device, &epdc_data);
	mxc_register_device(&lcd_wvga_device, &lcd_wvga_data);
	mxc_register_device(&elcdif_device, &fb_data[0]);
	mxc_register_device(&mxs_viim, NULL);

	mxc_register_device(&mxc_android_pmem_device, &android_pmem_pdata);
	mxc_register_device(&mxc_android_pmem_gpu_device, &android_pmem_gpu_pdata);
	mxc_register_device(&android_usb_device, &android_usb_pdata);

	mx50_arm2_init_mc13892();
/*
	pm_power_off = mxc_power_off;
	*/
	mxc_register_device(&mxc_sgtl5000_device, &sgtl5000_data);
	mxc_register_device(&gpmi_nfc_device, &gpmi_nfc_platform_data);
	mx5_usb_dr_init();
	mx5_usbh1_init();

	mxc_register_device(&mxc_rngb_device, NULL);
	mxc_register_device(&dcp_device, NULL);
	mxc_register_device(&fsl_otp_device, &otp_data);
}

static void __init mx50_arm2_timer_init(void)
{
	struct clk *uart_clk;

	mx50_clocks_init(32768, 24000000, 22579200);

	uart_clk = clk_get(NULL, "uart_clk.0");
	early_console_setup(MX53_BASE_ADDR(UART1_BASE_ADDR), uart_clk);
}

static struct sys_timer mxc_timer = {
	.init	= mx50_arm2_timer_init,
};

/*
 * The following uses standard kernel macros define in arch.h in order to
 * initialize __mach_desc_MX50_ARM2 data structure.
 */
MACHINE_START(MX50_ARM2, "Freescale MX50 ARM2 Board")
	/* Maintainer: Freescale Semiconductor, Inc. */
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.init_machine = mxc_board_init,
	.timer = &mxc_timer,
MACHINE_END
