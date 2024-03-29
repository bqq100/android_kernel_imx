/*
 * Copyright 2009-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @file bus_freq.c
 *
 * @brief A common API for the Freescale Semiconductor i.MXC CPUfreq module
 * and DVFS CORE module.
 *
 * The APIs are for setting bus frequency to low or high.
 *
 * @ingroup PM
 */
#include <asm/io.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/iram_alloc.h>
#include <linux/mutex.h>
#include <mach/hardware.h>
#include <mach/clock.h>
#include <mach/mxc_dvfs.h>
#include <mach/sdram_autogating.h>
#include <asm/mach/map.h>
#include <asm/cacheflush.h>
#include <asm/tlb.h>
#include "crm_regs.h"

#define LP_LOW_VOLTAGE		1050000
#define LP_NORMAL_VOLTAGE		1250000
#define LP_APM_CLK   			24000000
#define NAND_LP_APM_CLK			12000000
#define AXI_A_NORMAL_CLK		166250000
#define AXI_A_CLK_NORMAL_DIV		4
#define AXI_B_CLK_NORMAL_DIV		5
#define AHB_CLK_NORMAL_DIV		AXI_B_CLK_NORMAL_DIV
#define EMI_SLOW_CLK_NORMAL_DIV		AXI_B_CLK_NORMAL_DIV
#define NFC_CLK_NORMAL_DIV      	4
#define SPIN_DELAY	1000000 /* in nanoseconds */

DEFINE_SPINLOCK(ddr_freq_lock);

static unsigned long lp_normal_rate;
static unsigned long lp_med_rate;
static unsigned long ddr_normal_rate;
static unsigned long ddr_low_rate;

static struct clk *ddr_clk;
static struct clk *pll1_sw_clk;
static struct clk *pll1;
static struct clk *pll2;
static struct clk *pll3;
static struct clk *pll4;
static struct clk *main_bus_clk;
static struct clk *axi_a_clk;
static struct clk *axi_b_clk;
static struct clk *cpu_clk;
static struct clk *ddr_hf_clk;
static struct clk *ahb_clk;
static struct clk *ddr_clk;
static struct clk *periph_apm_clk;
static struct clk *lp_apm;
static struct clk *osc;
static struct clk *gpc_dvfs_clk;
static struct clk *emi_garb_clk;
static void __iomem *pll1_base;
static void __iomem *pll4_base;

struct regulator *lp_regulator;
int low_bus_freq_mode;
int high_bus_freq_mode;
int bus_freq_scaling_initialized;
char *gp_reg_id = "SW1";
char *lp_reg_id = "SW2";

static struct cpu_wp *cpu_wp_tbl;
static struct device *busfreq_dev;
static int busfreq_suspended;
static int cpu_podf;
/* True if bus_frequency is scaled not using DVFS-PER */
int bus_freq_scaling_is_active;

int cpu_wp_nr;
int lp_high_freq;
int lp_med_freq;

void enter_lpapm_mode_mx50(void);
void enter_lpapm_mode_mx51(void);
void exit_lpapm_mode_mx50(void);
void exit_lpapm_mode_mx51(void);
void *ddr_freq_change_iram_base;
void (*change_ddr_freq)(void *ccm_addr, void *databahn_addr, u32 freq) = NULL;

extern void mx50_ddr_freq_change(u32 ccm_base,
					u32 databahn_addr, u32 freq);
extern int dvfs_core_is_active;
extern struct cpu_wp *(*get_cpu_wp)(int *wp);
extern void propagate_rate(struct clk *tclk);
extern void __iomem *ccm_base;
extern void __iomem *databahn_base;

struct dvfs_wp dvfs_core_setpoint[] = {
						{33, 8, 33, 10, 10, 0x08},
						{26, 0, 33, 20, 10, 0x08},
						{28, 8, 33, 20, 30, 0x08},
						{29, 0, 33, 20, 10, 0x08},};

int set_low_bus_freq(void)
{
	u32 reg;
	struct timespec nstimeofday;
	struct timespec curtime;

	if (busfreq_suspended)
		return 0;

	if (bus_freq_scaling_initialized) {
		/* can not enter low bus freq, when cpu is in highest freq */
		if (clk_get_rate(cpu_clk) !=
				cpu_wp_tbl[cpu_wp_nr - 1].cpu_rate) {
			return 0;
		}

		stop_dvfs_per();

		stop_sdram_autogating();
		/* Set PLL3 to 133Mhz if no-one is using it. */
		if ((clk_get_usecount(pll3) == 0) && !cpu_is_mx53()) {
			u32 pll3_rate = clk_get_rate(pll3);

			clk_enable(pll3);
			clk_set_rate(pll3, clk_round_rate(pll3, 133000000));
			if (cpu_is_mx50())
				enter_lpapm_mode_mx50();
			else
				enter_lpapm_mode_mx51();

			/* Set PLL3 back to original rate. */
			clk_set_rate(pll3, clk_round_rate(pll3, pll3_rate));
			clk_disable(pll3);

		} else if (cpu_is_mx53()) {
			/*Change the DDR freq to 133Mhz. */
			clk_set_rate(ddr_hf_clk,
			     clk_round_rate(ddr_hf_clk, ddr_low_rate));

			/* move cpu clk to pll2, 400 / 3 = 133Mhz for cpu  */
			clk_set_parent(pll1_sw_clk, pll2);

			cpu_podf = __raw_readl(MXC_CCM_CACRR);
			reg = __raw_readl(MXC_CCM_CDHIPR);
			if ((reg & MXC_CCM_CDHIPR_ARM_PODF_BUSY) == 0)
				__raw_writel(0x2, MXC_CCM_CACRR);
			else
				printk(KERN_DEBUG "ARM_PODF still in busy!!!!\n");

			/* ahb = 400/8, axi_b = 400/8, axi_a = 133*/
			reg = __raw_readl(MXC_CCM_CBCDR);
			reg &= ~(MXC_CCM_CBCDR_AXI_A_PODF_MASK
				| MXC_CCM_CBCDR_AXI_B_PODF_MASK
				| MXC_CCM_CBCDR_AHB_PODF_MASK);
			reg |= (2 << MXC_CCM_CBCDR_AXI_A_PODF_OFFSET
				| 7 << MXC_CCM_CBCDR_AXI_B_PODF_OFFSET
				| 7 << MXC_CCM_CBCDR_AHB_PODF_OFFSET);
			__raw_writel(reg, MXC_CCM_CBCDR);

			getnstimeofday(&nstimeofday);
			while (__raw_readl(MXC_CCM_CDHIPR) &
					(MXC_CCM_CDHIPR_AXI_A_PODF_BUSY |
					 MXC_CCM_CDHIPR_AXI_B_PODF_BUSY |
					 MXC_CCM_CDHIPR_AHB_PODF_BUSY)) {
					getnstimeofday(&curtime);
				if (curtime.tv_nsec - nstimeofday.tv_nsec
					       > SPIN_DELAY)
					panic("low bus freq set rate error\n");
			}

			/* keep this infront of propagating */
			low_bus_freq_mode = 1;
			high_bus_freq_mode = 0;

			propagate_rate(main_bus_clk);
			propagate_rate(pll1_sw_clk);

			if (clk_get_usecount(pll1) == 0) {
				reg = __raw_readl(pll1_base + MXC_PLL_DP_CTL);
				reg &= ~MXC_PLL_DP_CTL_UPEN;
				__raw_writel(reg, pll1_base + MXC_PLL_DP_CTL);
			}
			if (clk_get_usecount(pll4) == 0) {
				reg = __raw_readl(pll4_base + MXC_PLL_DP_CTL);
				reg &= ~MXC_PLL_DP_CTL_UPEN;
				__raw_writel(reg, pll4_base + MXC_PLL_DP_CTL);
			}
		}
	}
	return 0;
}

void enter_lpapm_mode_mx50()
{
	u32 reg;
	unsigned long flags;

	spin_lock_irqsave(&ddr_freq_lock, flags);

	/* Set the parent of main_bus_clk to be PLL3 */
	clk_set_parent(main_bus_clk, pll3);

	/* Set the AHB dividers to be 1. */
	/* Set the dividers to be  1, so the clock rates
	 * are at 133MHz.
	 */
	reg = __raw_readl(MXC_CCM_CBCDR);
	reg &= ~(MXC_CCM_CBCDR_AXI_A_PODF_MASK
			| MXC_CCM_CBCDR_AXI_B_PODF_MASK
			| MXC_CCM_CBCDR_AHB_PODF_MASK
			| MX50_CCM_CBCDR_WEIM_PODF_MASK);
	reg |= (0 << MXC_CCM_CBCDR_AXI_A_PODF_OFFSET
			| 0 << MXC_CCM_CBCDR_AXI_B_PODF_OFFSET
			| 0 << MXC_CCM_CBCDR_AHB_PODF_OFFSET
			| 0 << MX50_CCM_CBCDR_WEIM_PODF_OFFSET);
	__raw_writel(reg, MXC_CCM_CBCDR);
	while (__raw_readl(MXC_CCM_CDHIPR) & 0x0F)
		udelay(10);
	low_bus_freq_mode = 1;
	high_bus_freq_mode = 0;

	/* Set the source of main_bus_clk to be lp-apm. */
	clk_set_parent(main_bus_clk, lp_apm);

	/* Set SYS_CLK to 24MHz. sourced from XTAL*/
	/* Turn on the XTAL_CLK_GATE. */
	reg = __raw_readl(MXC_CCM_CLK_SYS);
	reg |= 3 << MXC_CCM_CLK_SYS_SYS_XTAL_CLKGATE_OFFSET;
	__raw_writel(reg, MXC_CCM_CLK_SYS);

	/* Set the divider. */
	reg = __raw_readl(MXC_CCM_CLK_SYS);
	reg &= ~MXC_CCM_CLK_SYS_DIV_XTAL_MASK;
	reg |= 1 << MXC_CCM_CLK_SYS_DIV_XTAL_OFFSET;
	__raw_writel(reg, MXC_CCM_CLK_SYS);
	while (__raw_readl(MXC_CCM_CSR2) & 0x1)
		udelay(10);

	/* Set the source to be XTAL. */
	reg = __raw_readl(MXC_CCM_CLKSEQ_BYPASS);
	reg &= ~0x1;
	__raw_writel(reg, MXC_CCM_CLKSEQ_BYPASS);
	while (!(__raw_readl(MXC_CCM_CSR2) & 0x400))
		udelay(10);

	/* Turn OFF the PLL_CLK_GATE. */
	reg = __raw_readl(MXC_CCM_CLK_SYS);
	reg &= ~MXC_CCM_CLK_SYS_SYS_PLL_CLKGATE_MASK;
	__raw_writel(reg, MXC_CCM_CLK_SYS);
	spin_unlock_irqrestore(&ddr_freq_lock, flags);

}

void enter_lpapm_mode_mx51()
{
	u32 reg;

	/*Change the DDR freq to 133Mhz. */
	clk_set_rate(ddr_hf_clk,
	     clk_round_rate(ddr_hf_clk, ddr_low_rate));

	/* Set the parent of Periph_apm_clk to be PLL3 */
	clk_set_parent(periph_apm_clk, pll3);
	clk_set_parent(main_bus_clk, periph_apm_clk);

	/* Set the dividers to be  1, so the clock rates
	  * are at 133MHz.
	  */
	reg = __raw_readl(MXC_CCM_CBCDR);
	reg &= ~(MXC_CCM_CBCDR_AXI_A_PODF_MASK
			| MXC_CCM_CBCDR_AXI_B_PODF_MASK
			| MXC_CCM_CBCDR_AHB_PODF_MASK
			| MXC_CCM_CBCDR_EMI_PODF_MASK
			| MXC_CCM_CBCDR_NFC_PODF_OFFSET);
	reg |= (0 << MXC_CCM_CBCDR_AXI_A_PODF_OFFSET
			| 0 << MXC_CCM_CBCDR_AXI_B_PODF_OFFSET
			| 0 << MXC_CCM_CBCDR_AHB_PODF_OFFSET
			| 0 << MXC_CCM_CBCDR_EMI_PODF_OFFSET
			| 3 << MXC_CCM_CBCDR_NFC_PODF_OFFSET);
	__raw_writel(reg, MXC_CCM_CBCDR);

	clk_enable(emi_garb_clk);
	while (__raw_readl(MXC_CCM_CDHIPR) & 0x1F)
		udelay(10);
	clk_disable(emi_garb_clk);

	/* Set the source of Periph_APM_Clock to be lp-apm. */
	clk_set_parent(periph_apm_clk, lp_apm);
}

int set_high_bus_freq(int high_bus_freq)
{
	u32 reg;
	struct timespec nstimeofday;
	struct timespec curtime;

	if (bus_freq_scaling_initialized) {

		stop_sdram_autogating();

		if (low_bus_freq_mode) {
			/* Relock PLL3 to 133MHz */
			if ((clk_get_usecount(pll3) == 0) && !cpu_is_mx53()) {
				u32 pll3_rate = clk_get_rate(pll3);

				clk_enable(pll3);
				clk_set_rate(pll3,
					clk_round_rate(pll3, 133000000));
				if (cpu_is_mx50())
					exit_lpapm_mode_mx50();
				else
					exit_lpapm_mode_mx51();

				/* Relock PLL3 to its original rate */
				clk_set_rate(pll3,
					clk_round_rate(pll3, pll3_rate));
				clk_disable(pll3);
			} else if (cpu_is_mx53()) {
				/* move cpu clk to pll1 */
				reg = __raw_readl(MXC_CCM_CDHIPR);
				if ((reg & MXC_CCM_CDHIPR_ARM_PODF_BUSY) == 0)
					__raw_writel(cpu_podf & 0x7,
							MXC_CCM_CACRR);
				else
					printk(KERN_DEBUG
						"ARM_PODF still in busy!!!!\n");

				clk_set_parent(pll1_sw_clk, pll1);

				/* ahb = 400/3, axi_b = 400/3, axi_a = 400*/
				reg = __raw_readl(MXC_CCM_CBCDR);
				reg &= ~(MXC_CCM_CBCDR_AXI_A_PODF_MASK
					| MXC_CCM_CBCDR_AXI_B_PODF_MASK
					| MXC_CCM_CBCDR_AHB_PODF_MASK);
				reg |= (0 << MXC_CCM_CBCDR_AXI_A_PODF_OFFSET
					| 2 << MXC_CCM_CBCDR_AXI_B_PODF_OFFSET
					| 2 << MXC_CCM_CBCDR_AHB_PODF_OFFSET);
				__raw_writel(reg, MXC_CCM_CBCDR);

				getnstimeofday(&nstimeofday);
				while (__raw_readl(MXC_CCM_CDHIPR) &
					(MXC_CCM_CDHIPR_AXI_A_PODF_BUSY |
					 MXC_CCM_CDHIPR_AXI_B_PODF_BUSY |
					 MXC_CCM_CDHIPR_AHB_PODF_BUSY)) {
						getnstimeofday(&curtime);
					if (curtime.tv_nsec
						- nstimeofday.tv_nsec
						> SPIN_DELAY)
						panic("bus freq error\n");
				}

				/* keep this infront of propagating */
				low_bus_freq_mode = 1;
				high_bus_freq_mode = 0;

				propagate_rate(main_bus_clk);
				propagate_rate(pll1_sw_clk);
				/*Change the DDR freq to mormal_rate*/
				clk_set_rate(ddr_hf_clk,
					clk_round_rate(ddr_hf_clk, ddr_normal_rate));
			}
			start_dvfs_per();
		}
		if (bus_freq_scaling_is_active) {
			/*
			 * If the CPU freq is 800MHz, set the bus to the high
			 * setpoint (133MHz) and DDR to 200MHz.
			 */
			if (clk_get_rate(cpu_clk) !=
					cpu_wp_tbl[cpu_wp_nr - 1].cpu_rate)
				high_bus_freq = 1;

			if (((clk_get_rate(ahb_clk) == lp_med_rate)
					&& lp_high_freq) || high_bus_freq) {
				/* Set to the high setpoint. */
				high_bus_freq_mode = 1;

				clk_set_rate(ahb_clk,
				clk_round_rate(ahb_clk, lp_normal_rate));

				clk_set_rate(ddr_hf_clk,
				clk_round_rate(ddr_hf_clk, ddr_normal_rate));
			}

			if (!lp_high_freq && !high_bus_freq) {
				/* Set to the medium setpoint. */
				high_bus_freq_mode = 0;
				low_bus_freq_mode = 0;

				clk_set_rate(ddr_hf_clk,
				clk_round_rate(ddr_hf_clk, ddr_low_rate));

				clk_set_rate(ahb_clk,
					clk_round_rate(ahb_clk, lp_med_rate));
			}
		}
		start_sdram_autogating();
	}
	return 0;
}

void exit_lpapm_mode_mx50()
{
	u32 reg, ret;
	unsigned long flags;

	spin_lock_irqsave(&ddr_freq_lock, flags);

	/* Set SYS_CLK to source from PLL1 */
	/* Set sys_clk back to 200MHz. */
	/* Set the divider to 4. */
	reg = __raw_readl(MXC_CCM_CLK_SYS);
	reg &= ~MXC_CCM_CLK_SYS_DIV_PLL_MASK;
	reg |= 0x4 << MXC_CCM_CLK_SYS_DIV_PLL_OFFSET;
	__raw_writel(reg, MXC_CCM_CLK_SYS);
	udelay(100);

	/* Turn ON the PLL CLK_GATE. */
	reg = __raw_readl(MXC_CCM_CLK_SYS);
	reg |= 3 << MXC_CCM_CLK_SYS_SYS_PLL_CLKGATE_OFFSET;
	__raw_writel(reg, MXC_CCM_CLK_SYS);

	/* Source the SYS_CLK from PLL */
	reg = __raw_readl(MXC_CCM_CLKSEQ_BYPASS);
	reg |= 0x3;
	__raw_writel(reg, MXC_CCM_CLKSEQ_BYPASS);
	while (__raw_readl(MXC_CCM_CSR2) & 0x400)
		udelay(10);

	/* Turn OFF the XTAL_CLK_GATE. */
	reg = __raw_readl(MXC_CCM_CLK_SYS);
	reg &= ~MXC_CCM_CLK_SYS_SYS_XTAL_CLKGATE_MASK;
	__raw_writel(reg, MXC_CCM_CLK_SYS);

	clk_set_parent(main_bus_clk, pll3);

	/* Set the dividers to the default dividers */
	reg = __raw_readl(MXC_CCM_CBCDR);
	reg &= ~(MXC_CCM_CBCDR_AXI_A_PODF_MASK
		| MXC_CCM_CBCDR_AXI_B_PODF_MASK
		| MXC_CCM_CBCDR_AHB_PODF_MASK
		| MX50_CCM_CBCDR_WEIM_PODF_MASK);
	reg |= (0 << MXC_CCM_CBCDR_AXI_A_PODF_OFFSET
		|1 << MXC_CCM_CBCDR_AXI_B_PODF_OFFSET
		|2 << MXC_CCM_CBCDR_AHB_PODF_OFFSET
		|0 << MX50_CCM_CBCDR_WEIM_PODF_OFFSET);
	__raw_writel(reg, MXC_CCM_CBCDR);

	while (__raw_readl(MXC_CCM_CDHIPR) & 0xF)
		udelay(10);

	low_bus_freq_mode = 0;
	high_bus_freq_mode = 1;

	/*Set the main_bus_clk parent to be PLL2. */
	clk_set_parent(main_bus_clk, pll2);
	spin_unlock_irqrestore(&ddr_freq_lock, flags);

	udelay(100);
}

void exit_lpapm_mode_mx51()
{
	u32 reg;

	clk_set_parent(periph_apm_clk, pll3);

	/* Set the dividers to the default dividers */
	reg = __raw_readl(MXC_CCM_CBCDR);
	reg &= ~(MXC_CCM_CBCDR_AXI_A_PODF_MASK
		| MXC_CCM_CBCDR_AXI_B_PODF_MASK
		| MXC_CCM_CBCDR_AHB_PODF_MASK
		| MXC_CCM_CBCDR_EMI_PODF_MASK
		| MXC_CCM_CBCDR_NFC_PODF_OFFSET);
	reg |= (3 << MXC_CCM_CBCDR_AXI_A_PODF_OFFSET
		| 4 << MXC_CCM_CBCDR_AXI_B_PODF_OFFSET
		| 4 << MXC_CCM_CBCDR_AHB_PODF_OFFSET
		| 4 << MXC_CCM_CBCDR_EMI_PODF_OFFSET
		| 3 << MXC_CCM_CBCDR_NFC_PODF_OFFSET);
	__raw_writel(reg, MXC_CCM_CBCDR);

	clk_enable(emi_garb_clk);
	while (__raw_readl(MXC_CCM_CDHIPR) & 0x1F)
		udelay(10);

	low_bus_freq_mode = 0;
	high_bus_freq_mode = 1;
	clk_disable(emi_garb_clk);

	/*Set the main_bus_clk parent to be PLL2. */
	clk_set_parent(main_bus_clk, pll2);

	/*Change the DDR freq to 200MHz*/
	clk_set_rate(ddr_hf_clk,
	    clk_round_rate(ddr_hf_clk, ddr_normal_rate));
}

int low_freq_bus_used(void)
{
	if ((lp_high_freq == 0)
	    && (lp_med_freq == 0))
		return 1;
	else
		return 0;
}

void setup_pll(void)
{
}

static ssize_t bus_freq_scaling_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (bus_freq_scaling_is_active)
		return sprintf(buf, "Bus frequency scaling is enabled\n");
	else
		return sprintf(buf, "Bus frequency scaling is disabled\n");
}

static ssize_t bus_freq_scaling_enable_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	u32 reg;

	if (strncmp(buf, "1", 1) == 0) {
		if (dvfs_per_active()) {
			printk(KERN_INFO "bus frequency scaling cannot be\
				 enabled when DVFS-PER is active\n");
			return size;
		}

		/* Initialize DVFS-PODF to 0. */
		reg = __raw_readl(MXC_CCM_CDCR);
		reg &= ~MXC_CCM_CDCR_PERIPH_CLK_DVFS_PODF_MASK;
		__raw_writel(reg, MXC_CCM_CDCR);
		clk_set_parent(main_bus_clk, pll2);

		bus_freq_scaling_is_active = 1;
		set_high_bus_freq(0);
	} else if (strncmp(buf, "0", 1) == 0) {
		if (bus_freq_scaling_is_active)
			set_high_bus_freq(1);
		bus_freq_scaling_is_active = 0;
	}

	return size;
}

static int busfreq_suspend(struct platform_device *pdev, pm_message_t message)
{
	if (low_bus_freq_mode)
		set_high_bus_freq(1);
	busfreq_suspended = 1;
	return 0;
}

static int busfreq_resume(struct platform_device *pdev)
{
	busfreq_suspended = 0;
	return  0;
}

static DEVICE_ATTR(enable, 0644, bus_freq_scaling_enable_show,
			bus_freq_scaling_enable_store);

/*!
 * This is the probe routine for the bus frequency driver.
 *
 * @param   pdev   The platform device structure
 *
 * @return         The function returns 0 on success
 *
 */
static int __devinit busfreq_probe(struct platform_device *pdev)
{
	int err = 0;
	unsigned long pll2_rate, pll1_rate;
	unsigned long iram_paddr;

	pll1_base = ioremap(MX53_BASE_ADDR(PLL1_BASE_ADDR), SZ_4K);
	if (cpu_is_mx53())
		pll4_base = ioremap(MX53_BASE_ADDR(PLL4_BASE_ADDR), SZ_4K);

	busfreq_dev = &pdev->dev;

	main_bus_clk = clk_get(NULL, "main_bus_clk");
	if (IS_ERR(main_bus_clk)) {
		printk(KERN_DEBUG "%s: failed to get main_bus_clk\n",
		       __func__);
		return PTR_ERR(main_bus_clk);
	}

	pll1_sw_clk = clk_get(NULL, "pll1_sw_clk");
	if (IS_ERR(pll1_sw_clk)) {
		printk(KERN_DEBUG "%s: failed to get pll1_sw_clk\n", __func__);
		return PTR_ERR(pll1_sw_clk);
	}

	pll1 = clk_get(NULL, "pll1_main_clk");
	if (IS_ERR(pll1)) {
		printk(KERN_DEBUG "%s: failed to get pll1\n", __func__);
		return PTR_ERR(pll1);
	}

	pll2 = clk_get(NULL, "pll2");
	if (IS_ERR(pll2)) {
		printk(KERN_DEBUG "%s: failed to get pll2\n", __func__);
		return PTR_ERR(pll2);
	}

	pll3 = clk_get(NULL, "pll3");
	if (IS_ERR(pll3)) {
		printk(KERN_DEBUG "%s: failed to get pll3\n", __func__);
		return PTR_ERR(pll3);
	}

	if (cpu_is_mx53()) {
		pll4 = clk_get(NULL, "pll4");
		if (IS_ERR(pll4)) {
			printk(KERN_DEBUG "%s: failed to get pll4\n", __func__);
			return PTR_ERR(pll4);
		}
	}

	axi_a_clk = clk_get(NULL, "axi_a_clk");
	if (IS_ERR(axi_a_clk)) {
		printk(KERN_DEBUG "%s: failed to get axi_a_clk\n",
		       __func__);
		return PTR_ERR(axi_a_clk);
	}

	axi_b_clk = clk_get(NULL, "axi_b_clk");
	if (IS_ERR(axi_b_clk)) {
		printk(KERN_DEBUG "%s: failed to get axi_b_clk\n",
		       __func__);
		return PTR_ERR(axi_b_clk);
	}

	ddr_clk = clk_get(NULL, "ddr_clk");
	if (IS_ERR(ddr_clk)) {
		printk(KERN_DEBUG "%s: failed to get ddr_clk\n",
		       __func__);
		return PTR_ERR(ddr_clk);
	}

	ddr_hf_clk = clk_get_parent(ddr_clk);

	if (IS_ERR(ddr_hf_clk)) {
		printk(KERN_DEBUG "%s: failed to get ddr_hf_clk\n",
		       __func__);
		return PTR_ERR(ddr_hf_clk);
	}

	ahb_clk = clk_get(NULL, "ahb_clk");
	if (IS_ERR(ahb_clk)) {
		printk(KERN_DEBUG "%s: failed to get ahb_clk\n",
		       __func__);
		return PTR_ERR(ahb_clk);
	}

	cpu_clk = clk_get(NULL, "cpu_clk");
	if (IS_ERR(cpu_clk)) {
		printk(KERN_DEBUG "%s: failed to get cpu_clk\n",
		       __func__);
		return PTR_ERR(cpu_clk);
	}

	if (cpu_is_mx51())
		emi_garb_clk = clk_get(NULL, "emi_garb_clk");
	else if (cpu_is_mx53())
		emi_garb_clk = clk_get(NULL, "emi_intr_clk.1");
	else
		emi_garb_clk = clk_get(NULL, "ocram_clk");
	if (IS_ERR(emi_garb_clk)) {
		printk(KERN_DEBUG "%s: failed to get emi_garb_clk\n",
		       __func__);
		return PTR_ERR(emi_garb_clk);
	}

	if (cpu_is_mx51()  || cpu_is_mx53()) {
		periph_apm_clk = clk_get(NULL, "periph_apm_clk");
		if (IS_ERR(periph_apm_clk)) {
			printk(KERN_DEBUG "%s: failed to get periph_apm_clk\n",
			       __func__);
			return PTR_ERR(periph_apm_clk);
		}
	}

	lp_apm = clk_get(NULL, "lp_apm");
	if (IS_ERR(lp_apm)) {
		printk(KERN_DEBUG "%s: failed to get lp_apm\n",
		       __func__);
		return PTR_ERR(lp_apm);
	}

	osc = clk_get(NULL, "osc");
	if (IS_ERR(osc)) {
		printk(KERN_DEBUG "%s: failed to get osc\n", __func__);
		return PTR_ERR(osc);
	}

	gpc_dvfs_clk = clk_get(NULL, "gpc_dvfs_clk");
	if (IS_ERR(gpc_dvfs_clk)) {
		printk(KERN_DEBUG "%s: failed to get gpc_dvfs_clk\n", __func__);
		return PTR_ERR(gpc_dvfs_clk);
	}

	err = sysfs_create_file(&busfreq_dev->kobj, &dev_attr_enable.attr);
	if (err) {
		printk(KERN_ERR
		       "Unable to register sysdev entry for BUSFREQ");
		return err;
	}

	pll1_rate = clk_get_rate(pll1_sw_clk);
	pll2_rate = clk_get_rate(pll2);

	if (pll2_rate == 665000000) {
		/* for mx51 */
		lp_normal_rate = pll2_rate / 5;
		lp_med_rate = pll2_rate / 8;
		ddr_normal_rate = pll1_rate / 4; /* 200M */
		ddr_low_rate = pll1_rate / 6; /* 133M */
	} else if (pll2_rate == 600000000) {
		/* for mx53 evk rev.A */
		lp_normal_rate = pll2_rate / 5;
		lp_med_rate = pll2_rate / 8;
		ddr_normal_rate = pll2_rate / 2;
		ddr_low_rate = pll2_rate / 2;
	} else if (pll2_rate == 400000000) {
		/* for mx53 evk rev.B */
		lp_normal_rate = pll2_rate / 3;
		lp_med_rate = pll2_rate / 5;
		if (cpu_is_mx53()) {
			ddr_normal_rate = pll2_rate / 1;
			ddr_low_rate = pll2_rate / 3;
		} else if (cpu_is_mx50()) {
			ddr_normal_rate = clk_get_rate(ddr_clk);
			ddr_low_rate = LP_APM_CLK;
		}
	}
	if (cpu_is_mx50()) {
		iram_alloc(SZ_8K, &iram_paddr);
		/* Need to remap the area here since we want the memory region
			 to be executable. */
		ddr_freq_change_iram_base = __arm_ioremap(iram_paddr,
							SZ_8K, MT_HIGH_VECTORS);
		memcpy(ddr_freq_change_iram_base, mx50_ddr_freq_change, SZ_8K);
		change_ddr_freq = (void *)ddr_freq_change_iram_base;

		lp_regulator = regulator_get(NULL, "SW2");
		if (IS_ERR(lp_regulator)) {
			printk(KERN_DEBUG
			"%s: failed to get lp regulator\n", __func__);
			return PTR_ERR(lp_regulator);
		}
	}
	cpu_wp_tbl = get_cpu_wp(&cpu_wp_nr);
	low_bus_freq_mode = 0;
	high_bus_freq_mode = 1;
	bus_freq_scaling_is_active = 0;
	bus_freq_scaling_initialized = 1;

	return 0;
}

static struct platform_driver busfreq_driver = {
	.driver = {
		   .name = "busfreq",
		},
	.probe = busfreq_probe,
	.suspend = busfreq_suspend,
	.resume = busfreq_resume,
};

/*!
 * Initialise the busfreq_driver.
 *
 * @return  The function always returns 0.
 */

static int __init busfreq_init(void)
{
	if (platform_driver_register(&busfreq_driver) != 0) {
		printk(KERN_ERR "busfreq_driver register failed\n");
		return -ENODEV;
	}

	printk(KERN_INFO "Bus freq driver module loaded\n");
	return 0;
}

static void __exit busfreq_cleanup(void)
{
	sysfs_remove_file(&busfreq_dev->kobj, &dev_attr_enable.attr);

	/* Unregister the device structure */
	platform_driver_unregister(&busfreq_driver);
	bus_freq_scaling_initialized = 0;
}

module_init(busfreq_init);
module_exit(busfreq_cleanup);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("BusFreq driver");
MODULE_LICENSE("GPL");
