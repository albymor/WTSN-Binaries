// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 1999 - 2018 Intel Corporation. */

/* PTP 1588 Hardware Clock (PHC)
 * Derived from PTP Hardware Clock driver for Intel 82576 and 82580 (igb)
 * Copyright (C) 2011 Richard Cochran <richardcochran@gmail.com>
 * Derived from Intel 82576 and 82580 (igb) for Intel Wireless driver(s)
 */

#include "mvm.h"

#ifdef CPTCFG_IWLWIFI_TIMING_MEASUREMENT
#include <linux/clocksource.h>
#include <linux/ktime.h>
#include <asm/tsc.h>
#endif

/* this is not clear -- needs review */
/* this most likely will have to do with how the Wi-Fi logic */
/* keeps track of the ART value and probably has nothing to  */
/* with GP2                                                  */
/* 32 bit GP2 where each count is 100 nsec */
/* 2^32*100 nsec ~ 7.1583 minutes -- so, read GP2 every 7 minutes */
#define IWLMVM_GP2_OVERFLOW_PERIOD (HZ * 60 * 7)

#define MAX_FW_WAIT_COUNT (3)

/**
 * iwl_mvm_get_syncdevicetime - Callback given to timekeeping code reads system/device registers
 * @device: current device time
 * @system: system counter value read synchronously with device time
 * @ctx: context provided by timekeeping code
 *
 * Read device and system (ART) clock simultaneously and return the corrected
 * clock values in ns.
 **/
static int iwl_mvm_phc_get_syncdevicetime(ktime_t *device,
					 struct system_counterval_t *system,
					 void *ctx)
{
	int retval = -1;
	int sleep_cnt = MAX_FW_WAIT_COUNT;
	struct iwl_mvm *mvm = (struct iwl_mvm *)ctx;

	unsigned long flags;
	struct iwl_get_correlated_time_mode cmd = {
		.operation = cpu_to_le32(WNM_80211V_TIMING_OPERATION_READ_BOTH)
	};
	u32 cmd_id = iwl_cmd_id(WNM_PLATFORM_PTM_REQUEST_CMD, DATA_PATH_GROUP, 0);

	/* wait for any pending firmware operations to complete */
	while (mvm->firmware_operation_done != 0) {
		if (!sleep_cnt--) {
			printk(KERN_DEBUG "iwl_mvm_g_syncdevicetime(): Timed out"
				"Waiting for pending firmware operation to complete.\n");
			return -1;
		}
		msleep(1);
	}

	spin_lock_irqsave(&mvm->systim_lock, flags);
	sleep_cnt = MAX_FW_WAIT_COUNT;
	mvm->firmware_operation_done = 1; /* firmware operation ongoing */
	retval = iwl_mvm_send_cmd_pdu(mvm, cmd_id, 0, sizeof(cmd), &cmd);

	if (retval) {
		printk(KERN_DEBUG "iwl_mvm_get_syncdevicetime(): Firmware Error "
				"retval %d\n", retval);
	        mvm->firmware_operation_done = 0; /* ready for firmware operation */
	        spin_unlock_irqrestore(&mvm->systim_lock, flags);
		return retval;
	}

	/* wait for the firmware to complete the requested operation */
	while (mvm->firmware_operation_done != 2) {
		if (!sleep_cnt--) {
			printk(KERN_DEBUG "iwl_mvm_g_syncdevicetime(): Timed out"
				"Waiting for firmware to return ART and GP2 register\n");
	                mvm->firmware_operation_done = 0; /* ready for firmware operation */
	                spin_unlock_irqrestore(&mvm->systim_lock, flags);
			return -1;
		}
		msleep(1);
	}
	mvm->firmware_operation_done = 0; /* ready for firmware operation */
	spin_unlock_irqrestore(&mvm->systim_lock, flags);

	*device = mvm->GP2_ns;
	/* *device = ns_to_ktime(timecounter_cyc2time(&mvm->tc, mvm->GP2_ns)); */
	*system = convert_art_to_tsc(mvm->ART_ns);
	/* *system = convert_art__ns_to_tsc(mvm->ART_ns); */
	
#if 0
	printk(KERN_DEBUG "iwl_mvm_get_syncdevicetime(): returning 0, system->cycles <%llu>, cycles <%llu>, device <%llu>", system->cycles, get_cycles(), *device);

#endif
	return 0;
}

/**
 * iwl_mvm_get_crosstimestamp - Reads the current system/device cross timestamp
 * @ptp: ptp clock structure
 * @cts: structure containing timestamp
 *
 * Read device and system (ART) clock simultaneously and return the scaled
 * clock values in ns.
 **/
static int iwl_mvm_phc_get_crosstimestamp(struct ptp_clock_info *ptp,
				     struct system_device_crosststamp *xtstamp)
{
	u64 device_delta = 0, system_delta=0;
	s64 error = 0;
	int sleep_cnt = MAX_FW_WAIT_COUNT;
	struct iwl_mvm *mvm = container_of(ptp, struct iwl_mvm,
						     ptp_clock_info);
	int err = -1;

	if (mvm->trans->cfg->integrated == true) { /* CNVi */
	   err = get_device_system_crosststamp(iwl_mvm_phc_get_syncdevicetime,
						mvm, NULL, xtstamp);
           if (err < 0) {
	      printk(KERN_DEBUG "get_device_system_crosststamp(): returned err <%d>\n", err);
              return err;
           }
	} else { /* discrete wi-fi */
           err = 0;

	   /* wait for any pending firmware operations to complete */
	   while (mvm->firmware_operation_done != 0) {
             if (!sleep_cnt--) {
                printk(KERN_DEBUG "iwl_mvm_g_syncdevicetime(): Timed out"
                         "Waiting for pending firmware operation to complete.\n");
                return -1;
             }
             msleep(1);
	   }

	   iwl_mvm_get_correlated_time(mvm);
	   xtstamp->device = mvm->GP2_ns; 
	   xtstamp->sys_realtime = mvm->sys_realtime_ns; 
	   xtstamp->sys_monoraw = mvm->sys_monoraw_ns; 
	}

#if 0
	printk(KERN_DEBUG "xtstamp.device: <%lld>\n", xtstamp->device);
	printk(KERN_DEBUG "xtstamp.sys_realtime: <%lld>\n", xtstamp->sys_realtime);
	printk(KERN_DEBUG "xtstamp.sys_monoraw: <%lld>\n", xtstamp->sys_monoraw);
#endif
	if ( mvm->last_GP2_ns != 0 ) {
		device_delta = xtstamp->device - mvm->last_GP2_ns ; /* in nsec units */
		system_delta = xtstamp->sys_realtime - mvm->last_system_time_ns; /* in  nsec units */
		error = system_delta - device_delta;
#if 0
		error = 0;
		/* error -= 47500; */
#endif

	}

	mvm->error = error;
	mvm->last_GP2_ns = xtstamp->device; /* in nsec units */
	mvm->last_system_time_ns = xtstamp->sys_realtime; /* in  nsec units */
	xtstamp->sys_realtime += error;
#if 1
	printk(KERN_DEBUG "--->>> iwl_mvm_phc_get_crosstimestamp(): "
			"g_last_gp2: %llu (ns), g_last_system_time: %llu (ns), device_time: %llu (ns), system_time: %llu (ns),  device_delta: %llu (ns), system_delta: %llu (ns),  error: %lld\n",
				mvm->last_GP2_ns, mvm->last_system_time_ns, xtstamp->device, xtstamp->sys_realtime, device_delta, system_delta, error);
#endif
	return err;
}

/**
 * iwl_mvm_cyclecounter_read - Reads the GP2 counter value (nsec) 
 * @cc: cyclecounter structure
 *
 * Read the GP2 register 
 **/
u64 iwl_mvm_cyclecounter_read(const struct cyclecounter *cc)
{
	int retval = -1;
	int sleep_cnt = MAX_FW_WAIT_COUNT;
	struct iwl_mvm *mvm = container_of(cc, struct iwl_mvm, cc);
	unsigned long flags;
	struct iwl_get_correlated_time_mode cmd = {
		.operation = cpu_to_le32(WNM_80211V_TIMING_OPERATION_READ_FIRMWARE_TIME)
	};
	u32 cmd_id = iwl_cmd_id(WNM_PLATFORM_PTM_REQUEST_CMD, DATA_PATH_GROUP, 0);

	/* wait for any pending firmware operations to complete */
	while (mvm->firmware_operation_done != 0) {
		if (!sleep_cnt--) {
			printk(KERN_DEBUG "iwl_mvm_g_syncdevicetime(): Timed out"
				"Waiting for pending firmware operation to complete.\n");
			return -1;
		}
		msleep(1);
	}

	spin_lock_irqsave(&mvm->systim_lock, flags);
	sleep_cnt = MAX_FW_WAIT_COUNT;
	mvm->firmware_operation_done = 1; /* firmware operation ongoing */
	retval = iwl_mvm_send_cmd_pdu(mvm, cmd_id, 0, sizeof(cmd), &cmd);

	if (retval) {
		printk(KERN_DEBUG "iwl_mvm_cyclecounter_read(): Firmware Error "
				"retval %d\n", retval);
	        mvm->firmware_operation_done = 0; /* ready for firmware operation */
	        spin_unlock_irqrestore(&mvm->systim_lock, flags);
		return retval;
	}

	/* wait for the firmware to complete the requested operation */
	while (mvm->firmware_operation_done != 2) {
		if (!sleep_cnt--) {
			printk(KERN_DEBUG "iwl_mvm_cyclecounter_read(): Timed out"
					"Waiting for firmware to return GP2 register\n");
	                mvm->firmware_operation_done = 0; /* ready for firmware operation */
	                spin_unlock_irqrestore(&mvm->systim_lock, flags);
			return -1;
		}
		msleep(1);
	}
	mvm->firmware_operation_done = 0; /* ready for firmware operation */
	spin_unlock_irqrestore(&mvm->systim_lock, flags);
#if 0
	printk(KERN_DEBUG "iwl_mvm_get_cyclecounter_read(): Device Time %llu\n", mvm->GP2_ns);
#endif
	return(mvm->GP2_ns);
}

/**
 * iwl_mvm_gettime - Reads the current time from the hardware clock
 * @ptp: ptp clock structure
 * @ts: timespec structure to hold the current time value
 *
 * Read the timecounter and return the correct value in ns after converting
 * it into a struct timespec.
 **/
static int iwl_mvm_phc_gettime(struct ptp_clock_info *ptp, struct timespec64 *ts)
{
	struct iwl_mvm *mvm = container_of(ptp, struct iwl_mvm,
						     ptp_clock_info);
	u64 ns;

        if(mvm==NULL){
            return -1;
        }

        if(&mvm->tc == NULL){
            return -1;
        }

	ns = timecounter_read(&mvm->tc);

	*ts = ns_to_timespec64(ns);
#if 0
	printk(KERN_DEBUG "iwl_mvm_phc_get_gettime(): System Time (from ART): %llu\n", ns);
#endif
	return 0;
}

static void iwl_mvm_systim_overflow_work(struct work_struct *work)
{
	struct iwl_mvm *mvm = container_of(work, struct iwl_mvm,
						     systim_overflow_work.work);
	struct timespec64 ts;

	mvm->ptp_clock_info.gettime64(&mvm->ptp_clock_info, &ts);

	printk(KERN_DEBUG "SYSTIM overflow check at %lld.%09lu\n",
	      (long long) ts.tv_sec, ts.tv_nsec);

	schedule_delayed_work(&mvm->systim_overflow_work,
			      IWLMVM_GP2_OVERFLOW_PERIOD);
}

static const struct ptp_clock_info iwlmvm_ptp_clock_info = {
	.owner		= THIS_MODULE,
	.n_alarm	= 0,
	.n_ext_ts	= 0,
	.n_per_out	= 0,
	.n_pins		= 0,
	.pps		= 0,
	.adjfine	= 0,
	.adjfreq	= 0,
	.adjtime	= 0,
	.gettime64	= iwl_mvm_phc_gettime, 
	.getcrosststamp	= iwl_mvm_phc_get_crosstimestamp, /* return both ART and GP2 values */
	.settime64	= 0, /* iwl_mvm_phc_settime, */
	.enable		= 0,
	.verify		= 0,
};

/**
 * iwlmvm_ptp_init - initialize PTP for devices which support it
 * @adapter: board private structure
 *
 * This function performs the required steps for enabling PTP support.
 * If PTP support has already been loaded it simply calls the cyclecounter
 * init routine and exits.
 **/
void iwl_mvm_ptp_init(struct iwl_mvm *mvm)
{
	/* if the interface already has a ptp_clock defined just return */
	if (mvm->ptp_clock)
		return;

	mvm->ptp_clock = NULL;

	if (mvm->trans->cfg->integrated == true) { /* CNVi */
           /* Need ART support if the NIC is integrated */
	   if (!boot_cpu_has(X86_FEATURE_ART)) 
		return;

#if 1 /* deals with GP2 wraparound issues */
	   INIT_DELAYED_WORK(&mvm->systim_overflow_work,
			  iwl_mvm_systim_overflow_work);

	   schedule_delayed_work(&mvm->systim_overflow_work,
			      IWLMVM_GP2_OVERFLOW_PERIOD);
#endif
	}

        mvm->ptp_clock_info = iwlmvm_ptp_clock_info;

        /* name is a 16-byte array */
        snprintf(mvm->ptp_clock_info.name,
                   sizeof(mvm->ptp_clock_info.name), "%s",
                   "Wi-Fi-PTP");

	mvm->ptp_clock_info.max_adj = 0; 

	mvm->ptp_clock = ptp_clock_register(&mvm->ptp_clock_info,
						mvm->dev);
	if (IS_ERR(mvm->ptp_clock)) {
		mvm->ptp_clock = NULL;
		printk(KERN_DEBUG "ptp_clock_register failed\n");
	} else if (mvm->ptp_clock) {
		printk(KERN_DEBUG "registered PHC clock(%s%d)\n",
				mvm->ptp_clock_info.name, 
				ptp_clock_index(mvm->ptp_clock));
	}

}

/**
 * iwl_mvm_ptp_remove - disable PTP device and stop the overflow check
 * @adapter: board private structure
 *
 * Stop the PTP support, and cancel the delayed work.
 **/
void iwl_mvm_ptp_remove(struct iwl_mvm *mvm)
{
#if 0 /* hard code to always supported */
	if (!(adapter->flags & FLAG_HAS_HW_TIMESTAMP))
		return;
#endif

	if (mvm->trans->cfg->integrated == true)  /* CNVi */
	   cancel_delayed_work_sync(&mvm->systim_overflow_work);

	if (mvm->ptp_clock) {
		printk(KERN_DEBUG "unregistering PHC clock(%s%d)\n",
				mvm->ptp_clock_info.name, 
				ptp_clock_index(mvm->ptp_clock));
		ptp_clock_unregister(mvm->ptp_clock);
		mvm->ptp_clock = NULL;
		printk(KERN_DEBUG "removed PHC\n");
	}
}
