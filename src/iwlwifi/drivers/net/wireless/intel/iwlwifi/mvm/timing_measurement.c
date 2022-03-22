/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#include <linux/etherdevice.h>
#include <net/mac80211.h>
#include <net/netlink.h>
#include <linux/timekeeping.h>
#include <linux/timecounter.h>
#include <linux/clocksource.h>
#include <asm/tsc.h>

#include "mvm.h"
#include "fw-api.h"
#include "iwl-io.h"
#include "iwl-prph.h"

/* To Do: Find kernel defined value */
#define NS_PER_SEC      1000000000ULL

static u64 s_cumulative_gp2 = 0; /* in nsec units */
static u32 s_last_GP2 = 0; /* last GP2 register value in usec units; not adjusted for wraps */

/* #define __GETTIME(X) ktime_get_ts(X)  */
#define __GETTIME(X) __getnstimeofday(X)

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
#define CONFIRM_PAYLOAD_SIZE 31
#else
#define CONFIRM_PAYLOAD_SIZE 23
#endif
#define MIN_TM_MSMT_ACTION_PAYLOAD 38

struct iwl_mvm_find_sta_iter_data {
   u8 addr[ETH_ALEN];
   struct iwl_mvm_sta *mvmsta;
};

static void hexdump(const u8 *buf, size_t len)
{
	size_t i;
    u8 j;

    char out_buf[49];
	if (!(buf == NULL) && !(len == 0)) {
        printk(KERN_DEBUG "Dumping payload ... ");
        //printk(KERN_ERR "Dumping payload ... ");
		for (j = 0,i = 0; i < len; i++) {
            sprintf (&out_buf[j], "%02x ", buf[i]);
            j+=3;
            if (j == 48) {
               out_buf[j] = '\0';
               printk(KERN_ERR "%s\n", out_buf);
               j = 0;
            }
        }
        out_buf[j] = '\0';
        printk(KERN_ERR "%s\n", out_buf);
	}
}

/* convert from 32-bit timestamp values to a 64-bit value */
u64 get_64bit_ts (u32 hi, u32 lo) 
{
	u64 ret_val = 0;

	ret_val = (long) hi;
	ret_val <<= 32;
	ret_val += lo;
	return ret_val;
}

#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
/* time_stamp is in nsec units. g_last_system_time is */
/* in nsec units and g_last_gp2 is in nsec units      */
/* return ingress or egress system time in nsec units */
static u64 get_system_time_corresponding_to_ts(struct iwl_mvm *mvm, u64 time_stamp)
{
    u64 retval = 0;

    if (mvm->last_GP2_ns > time_stamp) { 
       /* GP2 has wrapped */
       /* u64 _t = time_stamp + 0x100000000L; */
       retval = mvm->last_system_time_ns + 
                (time_stamp + (0xFFFFFFFFFFFFFFFF - mvm->last_GP2_ns)) + mvm->error;
    } else
       retval = mvm->last_system_time_ns + (time_stamp - mvm->last_GP2_ns) + mvm->error;

#if 0
    printk(KERN_ERR "get_system_time_corresponding_to_ts: time_stamp(%llu), retval(%llu)\n",
			time_stamp, retval);
#endif
    return retval; 
}
#endif

static void iwl_mvm_find_sta_iter(void *_data,
                  struct ieee80211_sta *sta)
{
    struct iwl_mvm_find_sta_iter_data *data = _data;

    if (ether_addr_equal(sta->addr, data->addr))
       data->mvmsta = iwl_mvm_sta_from_mac80211(sta);
}

static struct ieee80211_vif
*iwl_mvm_timing_measurement_find_vif(struct iwl_mvm *mvm, u8 *addr)
{
    struct iwl_mvm_find_sta_iter_data data = {};

    ether_addr_copy(data.addr, addr);

    ieee80211_iterate_stations_atomic(mvm->hw,
                      iwl_mvm_find_sta_iter,
                      &data);
    if (!data.mvmsta) {
        IWL_DEBUG_STATS(mvm,
                "Timing measurement: unrecognized sending station\n");
        return NULL;
    }

    return data.mvmsta->vif;
}

/* applies to both the legacy and new f/w APIs      */
/* handler for Datapath Group (0x05) Command (0xFD) */
/* and for Legacy Group (0x00) and Command (0x66)   */

void iwl_mvm_rx_timing_measurement_req_notif(struct iwl_mvm *mvm,
                        struct iwl_rx_cmd_buffer *rxb)
                        /* , struct iwl_device_cmd *cmd) */
{

    struct iwl_rx_packet *pkt = rxb_addr(rxb);
    struct iwl_time_measurement_req_notif *notif = (void *)pkt->data;
    struct sk_buff *skb;
    struct ieee80211_mgmt *mgmt;
    int len = offsetof(struct ieee80211_mgmt,
                   u.action.u.timing_measurement_req) +
                   sizeof(mgmt->u.action.u.timing_measurement_req);
    struct ieee80211_vif *vif;

    skb = dev_alloc_skb(len);
    if (!skb)
        return;

#if 0
    printk (KERN_DEBUG "**GV** iwl_mvm_rx_timing_measurement_req_notif "
                "(data %p, data_len %u)\n", notif, len);
#endif

    vif = iwl_mvm_timing_measurement_find_vif(mvm, notif->addr);
    if (!vif)
        return;

    mgmt = (struct ieee80211_mgmt *)skb_put(skb, len);
    memset(mgmt, 0, len);
    mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
                      IEEE80211_STYPE_ACTION);
    ether_addr_copy(mgmt->da, vif->addr);
    ether_addr_copy(mgmt->sa, notif->addr);
    ether_addr_copy(mgmt->bssid, vif->bss_conf.bssid);

    mgmt->u.action.category = WLAN_CATEGORY_WNM;
    mgmt->u.action.u.timing_measurement_req.action_code =
                    WLAN_WNM_ACTION_TIMING_MEASUREMENT_REQ;
    mgmt->u.action.u.timing_measurement_req.trigger =
                        le32_to_cpu(notif->trigger);

    ieee80211_rx(mvm->hw, skb);
}

/* this is invoked when a higher layer (e.g. wpa_supplicant) requires */
/* a snapshot of the device (Wi-Fi device) time/clock and the         */
/* corresponding system clock                                         */
/* this API needs to be revisited -- not functional                   */
int iwl_mvm_get_correlated_time (struct iwl_mvm *mvm)
{
    int retval = 0;
    int sleep_cnt = 5;
    u32 GP2; /* raw value read off of the GP2 register in usec */
    u64 u64_GP2; /* adjusted for wrap and converted to nsec */
    s64 delta = 0x7EADBEEFDEADBEEF;
    s64 error_in_change = 0; /* diff between system time difference and GP2 difference */
    u8 num_try_again = 16;
    s64 min_error_in_change = 0x7EADBEEFDEADBEEF;
    u32 good_GP2 = 0; /* raw GP2 (usec) corresponding to minimal error */
    u64 good_GP2_ns = 0; /* GP2 (nsec) corresponding to minimal error */
    u64 good_sys_realtime_ns = 0; /* system time corresponding to minimal error */

    bool ps_disabled = mvm->ps_disabled;
    struct timespec time_of_day_ts, _time_of_day_ts;
    struct timespec monotonic_ts;

    printk (KERN_DEBUG "iwl_mvm_get_correlated_time ENTRY");

    while (mvm->firmware_operation_done != 0) {
       /* wait for the pending firmware operation to complete */
       printk (KERN_DEBUG "iwl_mvm_get_correlated_time() -- "
                          "Waiting for a pending get_correlation_time to "
                          "complete\n");
       if (!sleep_cnt--) {
          printk (KERN_DEBUG "iwl_mvm_get_correlated_time() -- "
                          "Timed out Waiting for a pending get_correlation_time"
                          " to complete\n");
          return -1;
       }
       msleep(1);
    }

TRY_AGAIN:
    /* invoke firmware operation and read GP2                         */
    /*        read system time                                        */ 
    /*        read monotonic raw time                                 */

    if (!ps_disabled) {
       mvm->ps_disabled = true;
       mutex_lock(&mvm->mutex);
       iwl_mvm_power_update_device(mvm);
       mutex_unlock(&mvm->mutex);
    }

    mutex_lock(&mvm->mutex);
    mvm->firmware_operation_done = 1; /* firmware operation ongoing */

    getnstimeofday(&time_of_day_ts);
    GP2 = iwl_read_prph(mvm->trans, DEVICE_SYSTEM_TIME_REG); 
    getnstimeofday(&_time_of_day_ts);
    getrawmonotonic(&monotonic_ts);
    mutex_unlock(&mvm->mutex);
    mvm->firmware_operation_done = 2; /* firmware operation done */



    if (!ps_disabled) {
       mvm->ps_disabled = false;
       mutex_lock(&mvm->mutex);
       iwl_mvm_power_update_device(mvm);
       mutex_unlock(&mvm->mutex);
    }

    if (s_last_GP2 > GP2) /* GP2 has wrapped */
       u64_GP2 = ((u64)(GP2) + (u64)0x100000000) * 1000L;
    else
       u64_GP2 = (u64)(GP2) * 1000L;

    mvm->sys_realtime_ns = (time_of_day_ts.tv_sec * NS_PER_SEC + time_of_day_ts.tv_nsec) + 
                            (((_time_of_day_ts.tv_sec * NS_PER_SEC + _time_of_day_ts.tv_nsec) -
    (time_of_day_ts.tv_sec * NS_PER_SEC + time_of_day_ts.tv_nsec))*12)/16; 

    if (s_cumulative_gp2 && s_last_GP2) 
       delta = u64_GP2 - (u64)s_last_GP2*1000L;
    else
       //s_cumulative_gp2 = u64_GP2;
	s_cumulative_gp2 = mvm->sys_realtime_ns;

    mvm->GP2_ns = u64_GP2;
    /*mvm->sys_realtime_ns = (time_of_day_ts.tv_sec * NS_PER_SEC + time_of_day_ts.tv_nsec) + 
                            (((_time_of_day_ts.tv_sec * NS_PER_SEC + _time_of_day_ts.tv_nsec) -
    (time_of_day_ts.tv_sec * NS_PER_SEC + time_of_day_ts.tv_nsec))*12)/16; */
    mvm->sys_monoraw_ns = monotonic_ts.tv_sec * NS_PER_SEC + monotonic_ts.tv_nsec;

    if (delta != 0x7EADBEEFDEADBEEF) {
       /* What does it means for error_in_change to be negative                   */
       /* if the absolute error is less than 1000, it is OK as GP2 counts in usec */
       /* either system time advanced slowly or GP2 advanced fast                 */
       error_in_change = (mvm->sys_realtime_ns - mvm->last_system_time_ns) - delta; 

       /* case 1: error is due to GP2 counting in usecs */
       if ((error_in_change < 0) && (error_in_change > -1024)) {
#if 0
          printk(KERN_DEBUG "DONE error: %lld, sys_realtime_ns: %llu, last_system_time_ns: %llu", 
				  error_in_change, mvm->sys_realtime_ns, mvm->last_system_time_ns);
#endif
       } else  if ((error_in_change < 0) && (error_in_change <= -1024)) {
          /* case 2: error is negative and is more than 1024 nsecs */
#if 0
          printk(KERN_DEBUG "SYSTEM TIME advanced slower than GP2!!! sys_realtime_ns: %llu, last_system_time_ns: %llu"
			  " GP2: %llu, last_GP2: %llu", 
			  mvm->sys_realtime_ns, mvm->last_system_time_ns, u64_GP2, (u64)s_last_GP2*1000L);
#endif
       } else {
          /* case 3: error is postive and is more than 1024, retry for a smaller error */
          if (error_in_change < min_error_in_change) {
	       min_error_in_change = error_in_change;
	       good_GP2_ns = u64_GP2;
	       good_GP2 = GP2;
	       good_sys_realtime_ns = mvm->sys_realtime_ns;
	  }

	  if (min_error_in_change >= 1024) {
             if (--num_try_again) {
#if 0
                printk (KERN_DEBUG "ITERATING for a smaller delta:%llu, "
		     "error: %lld, device: %llu, system: %llu",
		     delta, min_error_in_change, mvm->GP2_ns, mvm->sys_realtime_ns);
#endif
	        goto TRY_AGAIN;
	     } else {
                error_in_change = min_error_in_change;
                mvm->sys_realtime_ns = good_sys_realtime_ns;
                mvm->GP2_ns = good_GP2_ns;
#if 0
                printk(KERN_DEBUG "Bailing error: %lld, sys_realtime_ns: %llu, last_system_time_ns: %llu", 
				  min_error_in_change, mvm->sys_realtime_ns, mvm->last_system_time_ns);
#endif
	     }
	  }
       }
    }

    mvm->firmware_operation_done = 0; /* ready for firmware operation */
    s_cumulative_gp2 += good_GP2_ns - s_last_GP2;
    s_last_GP2 = good_GP2 ? good_GP2 : GP2;
#if 1
    printk (KERN_DEBUG "iwl_mvm_get_correlated_time returning: delta:%llu, error: %lld, "
		     "device: %llu, system: %llu",
		     delta, error_in_change, mvm->GP2_ns, mvm->sys_realtime_ns);
    printk (KERN_DEBUG "iwl_mvm_get_correlated_time returning: SYS_TIME_DELTA: %lld",
                            (_time_of_day_ts.tv_sec * NS_PER_SEC + _time_of_day_ts.tv_nsec) -
    (time_of_day_ts.tv_sec * NS_PER_SEC + time_of_day_ts.tv_nsec)); 
			
#endif

    return retval;
}

/* the firmware calls this function when get_correlated_time operation  */
/* is completed -- in response to the invokation of the vendor specific */
/* command, IWL_MVM_VENDOR_CMD_TIMING_OPERATION,                        */
/* handler for Datapath Group (0x05) Command (0x03)                     */
/* firmware returns GP2 values in 10ns units                            */
void iwl_mvm_get_correlated_time_notif(struct iwl_mvm *mvm,
			      struct iwl_rx_cmd_buffer *rxb)
{
   struct iwl_rx_packet *pkt = rxb_addr(rxb);
   struct iwl_get_correlated_time *gct = (void *)pkt->data;

   /* expect operation_status == 3 WNM_80211V_TIMING_OPERATION_READ_BOTH or */
   /* operation_status == 2 WNM_80211V_TIMING_OPERATION_READ_FIRMARE_TIME or */
   if ((gct->operation_status != 3) && (gct->operation_status != 2)) {
      printk (KERN_DEBUG "iwl_mvm_get_correlated_time_notif() -- "
               "firmware returned error %u in get_correlated_time_response\n",
               gct->operation_status);
      mvm->GP2_ns = 0;
      /* Note that this is NOT system time but ART */
      mvm->ART_ns = 0;
      mvm->firmware_operation_done = 2;
      return;
   }

   /* 64-bit GP2 value is returned to the caller */
   mvm->GP2_ns = get_64bit_ts(le32_to_cpu(gct->gp2_timestamp_hi), 
                     le32_to_cpu(gct->gp2_timestamp_lo)) * 10; 

   if (gct->operation_status == WNM_80211V_TIMING_OPERATION_READ_BOTH){
      mvm->ART_ns = get_64bit_ts(le32_to_cpu(gct->platform_timestamp_hi), 
                               le32_to_cpu(gct->platform_timestamp_lo));
   }

   mvm->firmware_operation_done = 2;
}

/* The firmware generates two callbacks into the driver when it receives a */
/* timing measurement action frame. This function invokation is the second */
/* callback from the firmware and delivers t2, max_t2_err, t3, max_t3_err  */
/* firmware delivers t2 and t3 in 10nsec units                             */
/* Handler for Legacy Group (0x00) Command 0x67                            */

/* WFA_SPECIFIC -- incoming payload has two vendor specific elements       */
/* the vendor specific elements are bundled in the PTP_CTX data which is   */
/* 0xdd, Length, OUI[3], dialog_token, system_time, GP2 (22 bytes) plus    */
/* 802.1AS blob which is dd, Length, 80 bytes                              */
void iwl_mvm_rx_legacy_timing_measurement_notif(struct iwl_mvm *mvm,
					    struct iwl_rx_cmd_buffer *rxb)
{
   struct iwl_rx_packet *pkt = rxb_addr(rxb);
   struct iwl_time_measurement_notif_v2 *notif = (void *)pkt->data;
   struct sk_buff *skb;
   struct ieee80211_mgmt *mgmt;
   int hdr_len;
   struct ieee80211_vif *vif;
   u8 *pos;

#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
   u64 ingress_time; /* system time corresponding to t2, in 10 nsec units */
   u64 _ingress_time;
   u64 t2;
#endif

   int pkt_len = le32_to_cpu(pkt->len_n_flags) & FH_RSCSR_FRAME_SIZE_MSK;

   /* firmware returns corrupted payload at times               */
   /* if the payload is deemed corrupt, drop it                 */
   /* WFA_SPECIFIC                                              */
   /* notif->ptp.element_id = 0xdd; notif->ptp.length = 20;     */
   /*      OUI[3], dialog_token, master_system_time, master_GP2 */
   /* followed by                                               */
   /* end of WFA_SPECIFIC                                       */
   /* notif->ptp.element_id = 0xdd; notif->ptp.length = 80;     */
   /*      802.1AS TLV                                          */
#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
   u8 *wfa_spec = &notif->ptp.element_id; 
   u8 *ptp_pos = &(notif->ptp.element_id) + sizeof(u8) + sizeof(u8) + 
	           3*sizeof(u8) + sizeof(u8) + sizeof (u64) + sizeof (u64);
#else
   u8 *ptp_pos = &(notif->ptp.element_id);
#endif

   hexdump(pkt->data, pkt_len);

   if (*ptp_pos != 0) { /* vendor-specific data is present; */
                        /* not the first frame from master */
      if ((*ptp_pos != 0xdd) || (*(ptp_pos + 1) > 80)) {
         printk(KERN_ERR "[%x]: PTP_CTX data out-of-bounds <%d>\n", 
                      *ptp_pos, *(ptp_pos + 1));

         /* hexdump the frame */
         hexdump(pkt->data, 136);
         return;
      }
   }

#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
   /* Time Sync Verification */
   t2 = get_64bit_ts (le32_to_cpu(notif->hi_t2), le32_to_cpu(notif->lo_t2)); 
   _ingress_time = get_system_time_corresponding_to_ts(mvm, t2*10L);
   ingress_time = _ingress_time/10 + (((_ingress_time % 10) > 5) ? 1 : 0);
#endif

#if 1
    printk (KERN_DEBUG "iwl_mvm_rx_LEGACY_timing_measurement_notif -- "
            "TIMING MEASUREMENT NOTIFICATION RECEIVED "
            "notif->dialog_token <%u> notif->followup_token <%u>\n"
            "notif->hi_t1 <%u> notif->lo_t1 <%u> notif->t1_err <%u>\n"
            "notif->hi_t4 <%u> notif->lo_t4 <%u> notif->t4_err <%u>\n"
            "notif->hi_t2 <%u> notif->lo_t2 <%u> notif->t2_err <%u>\n"
            "notif->hi_t3 <%u> notif->lo_t3 <%u> notif->t3_err <%u>\n"
#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
            "notif->ptp.element_id <%u> notif->ptp.length <%u>\n"
            "t2 <%llu>>, ingress_time <%llu>\n",
#else
            "notif->ptp.element_id <%u> notif->ptp.length <%u>\n",
#endif
            le32_to_cpu(notif->dialog_token), 
	    le32_to_cpu(notif->followup_token),
            le32_to_cpu(notif->hi_t1), le32_to_cpu(notif->lo_t1), 
	    le32_to_cpu(notif->t1_error),
            le32_to_cpu(notif->hi_t4), le32_to_cpu(notif->lo_t4), 
	    le32_to_cpu(notif->t4_error),
            le32_to_cpu(notif->hi_t2), le32_to_cpu(notif->lo_t2), 
	    le32_to_cpu(notif->t2_error),
            le32_to_cpu(notif->hi_t3), le32_to_cpu(notif->lo_t3),
#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
	    le32_to_cpu(notif->t3_error), *ptp_pos, *(ptp_pos + 1),
	    t2, ingress_time);
#else
	    le32_to_cpu(notif->t3_error), *ptp_pos, *(ptp_pos + 1));
#endif
#endif

    vif = iwl_mvm_timing_measurement_find_vif(mvm, notif->addr);
    if (!vif) {
       printk (KERN_DEBUG "iwl_mvm_rx_LEGACY_timing_measurement_notif -- " 
                       "iwl_mvm_timing_measurement_find_vif returned NULL.\n"
                       "Could not find a matching network inferface\n");

       goto BAIL;
    }

    hdr_len = offsetof(struct ieee80211_mgmt, u.action.u.tm_msmt_ind) +
                                    sizeof(mgmt->u.action.u.tm_msmt_ind);

#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
    /* 0xdd, Length, OUI[3], dialog_token, system_time, GP2 (22 bytes) */
    skb = dev_alloc_skb(hdr_len + 22 + *(ptp_pos + 1) + 2);
#else
    skb = dev_alloc_skb(hdr_len + *(ptp_pos + 1) + 2);
#endif
    if (!skb) {
       printk (KERN_DEBUG "iwl_mvm_rx_LEGACY_timing_measurement_notif -- " 
               "Failed to allocate memory for skb.\n");
       goto BAIL;
    }

    mgmt = (struct ieee80211_mgmt *)skb_put(skb, hdr_len);
    memset(mgmt, 0, hdr_len);
    mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
                                IEEE80211_STYPE_ACTION);
    ether_addr_copy(mgmt->da, vif->addr);
    ether_addr_copy(mgmt->sa, notif->addr);
    ether_addr_copy(mgmt->bssid, vif->bss_conf.bssid);

    mgmt->u.action.category = WLAN_CATEGORY_UNPROTECTED_WNM;
    mgmt->u.action.u.tm_msmt_ind.action_code =
        WLAN_UNPROTECTED_WNM_ACTION_TIMING_MEASUREMENT;

    mgmt->u.action.u.tm_msmt_ind.dialog_token =
        le32_to_cpu(notif->dialog_token);
    mgmt->u.action.u.tm_msmt_ind.followup_dialog_token =
        le32_to_cpu(notif->followup_token);

    /* t1, t4 are 32-bit values from the received payload       */
    /* t2, t3 are 64-bit values from the firmware               */
    /* return all timestamps as 64-bit values to the supplicant */
    /* t2, max_t2_err, t3 and max_t3_err are sent via rx_status */
    /* copying them over here is just to make up the payload    */
    mgmt->u.action.u.tm_msmt_ind.t1 = 
        get_64bit_ts (le32_to_cpu(notif->hi_t1), le32_to_cpu(notif->lo_t1)); 
    /* stale -- used when tested with Master sending timestamps in usec units */
    /* mgmt->u.action.u.tm_msmt_ind.t1 *= 100L; */
    mgmt->u.action.u.tm_msmt_ind.max_t1_err = le32_to_cpu(notif->t1_error);
    mgmt->u.action.u.tm_msmt_ind.t4 = 
        get_64bit_ts (le32_to_cpu(notif->hi_t4), le32_to_cpu(notif->lo_t4)); 
    /* stale -- used when tested with Master sending timestamps in usec units */
    /* mgmt->u.action.u.tm_msmt_ind.t4 *= 100L; */
    mgmt->u.action.u.tm_msmt_ind.max_t4_err = le32_to_cpu(notif->t4_error);
    mgmt->u.action.u.tm_msmt_ind.t2 = 
        get_64bit_ts (le32_to_cpu(notif->hi_t2), le32_to_cpu(notif->lo_t2)); 
    mgmt->u.action.u.tm_msmt_ind.max_t2_err = le32_to_cpu(notif->t2_error);
    mgmt->u.action.u.tm_msmt_ind.t3 = 
        get_64bit_ts (le32_to_cpu(notif->hi_t3), le32_to_cpu(notif->lo_t3)); 
    mgmt->u.action.u.tm_msmt_ind.max_t3_err = le32_to_cpu(notif->t3_error);
#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
    mgmt->u.action.u.tm_msmt_ind.ingress_time = ingress_time;
#endif

    if (*(ptp_pos + 1) > 80) {
       printk(KERN_ERR "[%x]: PTP_CTX data out-of-bounds <%d>\n", 
                       *ptp_pos, *(ptp_pos + 1));
       *(ptp_pos + 1) = 80; /* IEEE802.1AS PTP_CTX payload is 80 bytes max */
    }
#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
    /* move the tail of the skb past WFA Vendor-specific and 802.1 Vendor-specific */
    /* *(ptp_pos + 1) + 2 is the total size of the 802.1 Vendor-specific element   */
    /* 22 bytes is the total size of the WFA Vendoe-specific element               */
    pos = skb_put(skb, *(ptp_pos + 1) ? *(ptp_pos + 1) + 2 + 22: 2);
#else
    /* move the tail of the skb past 802.1 Vendor-specific */
    pos = skb_put(skb, *(ptp_pos + 1) ? *(ptp_pos + 1) + 2 : 2);
#endif
    /* pos points to the first byte of Vendor-specific element(s) */

    /* WFA_SPECIFIC */
#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
    /* 0xdd, Length, OUI[3], dialog_token, system_time, GP2 (22 bytes) */
    *pos++ = *wfa_spec++; /* vendor specific element id */
    *pos++ = *wfa_spec++; /* length */
    *pos++ = *wfa_spec++; /* OUI[0] */
    *pos++ = *wfa_spec++; /* OUI[1] */
    *pos++ = *wfa_spec++; /* OUI[2] */
    *pos++ = *wfa_spec++; /* dialog_token */
    *((u64 *)pos) = *((u64 *)wfa_spec); /* system_time */
    pos += sizeof(u64); wfa_spec += sizeof(u64);
    *((u64 *)pos) = *((u64 *)wfa_spec); /* device_time */
    pos += sizeof(u64); wfa_spec += sizeof(u64);
    /* WFA_SPECIFIC ends */
#endif

    *pos++ = *(ptp_pos + 1) ? *ptp_pos : 0;
    *pos++ = *(ptp_pos + 1);
    if (*(ptp_pos + 1)) 
        memcpy(pos, ptp_pos+2, *(ptp_pos + 1));
#if 0
    if (skb->len > 164) {
        /* out-of-bounds */
        printk(KERN_DEBUG "rx indication out-of-bounds %d\n", skb->len);
        dev_kfree_skb(skb);
	    return;
    }
#endif

    ieee80211_rx(mvm->hw, skb);

BAIL: return;
}

/* firmware delivers t1 and t4 in 10 nsec units */
/* Handler for Legacy Group (0x00) Command 0x68 */
void iwl_mvm_rx_legacy_timing_measurement_confirm_notif(struct iwl_mvm *mvm,
						struct iwl_rx_cmd_buffer *rxb)
{
   struct iwl_rx_packet *pkt = rxb_addr(rxb);
   struct iwl_time_measurement_confirm_notif_v2 *notif = (void *)pkt->data;
   struct sk_buff *skb;
   int len;
   struct ieee80211_hdr_3addr *hdr;
   struct ieee80211_vif *vif;
   u8 *pos;

#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
   u64 t1;
   u64 egress_time; /* system time corresponding to t1, in 10 nsec units */
   u64 _egress_time; 
#endif
	
#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
   /* Time Sync Verification */
   t1 = get_64bit_ts (le32_to_cpu(notif->hi_t1), le32_to_cpu(notif->lo_t1)); 
   _egress_time = get_system_time_corresponding_to_ts(mvm, t1*10L);
   egress_time = _egress_time/10 + (((_egress_time % 10) > 5) ? 1 : 0);
#endif

   vif = iwl_mvm_timing_measurement_find_vif(mvm, notif->addr);
   if (!vif)
      return;

	/* Building a vendor specific (Intel) action frame    */
	/* length is action, oui[3], dialog_token, t2 +       */
    /* t2_err + t3 + t3_err + egress_time                 */
	/* length is 1 + 3 + 1 + 8 + 1 + 8 + 1 + 8 = 31 bytes */
    len = sizeof(struct ieee80211_hdr_3addr) + 1 + 1 + CONFIRM_PAYLOAD_SIZE;
	skb = dev_alloc_skb(len);
	if (!skb)
		return;

#if 0
    printk (KERN_DEBUG "iwl_mvm_rx_LEGACY_timing_measurement_confirm_notif -- "
#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
                    "DT:%u(t4:%llu t4_err:%u t1:%llu t1_err:%u) egress_time <%llu>\n", 
#else
                    "DT:%u(t4:%llu t4_err:%u t1:%llu t1_err:%u)\n", 
#endif
                    notif->dialog_token, 
                    get_64bit_ts(le32_to_cpu(notif->hi_t4), 
                                le32_to_cpu(notif->lo_t4)), 
                    notif->t4_error, 
                    get_64bit_ts(le32_to_cpu(notif->hi_t1), 
                                le32_to_cpu(notif->lo_t1)), 
#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
                    notif->t1_error, 
                    egress_time);
#else
                    notif->t1_error);
#endif
#endif
    if (((notif->lo_t1 == 0) && (notif->hi_t1 == 0))|| 
        ((notif->lo_t4 == 0) && (notif->hi_t4 == 0))) {
       printk (KERN_DEBUG 
               "iwl_mvm_rx_LEGACY_timing_measurement_confirm_notif -- "
               "CONFIRM NOTIFICATION BAD TIMESTAMP(S) "
               "t4:%u t4_error:%u t1:%u t1_error:%u\n", 
               notif->lo_t4, notif->t4_error, notif->lo_t1, notif->t1_error);
       }

       hdr = (struct ieee80211_hdr_3addr *)
              skb_put(skb, sizeof(struct ieee80211_hdr_3addr));
       memset(hdr, 0, len);
       hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
                                        IEEE80211_STYPE_ACTION);
       ether_addr_copy(hdr->addr1, vif->addr);
       ether_addr_copy(hdr->addr2, notif->addr);
       ether_addr_copy(hdr->addr3, vif->bss_conf.bssid);

       /* Building a vendor specific (Intel) action frame */
       pos = skb_put(skb, CONFIRM_PAYLOAD_SIZE);
       *pos++ = 0x7f; /* Vendor specific (unprotected) */
       *pos++ = 0x00; /* OUI for Intel */
       *pos++ = 0x03;
       *pos++ = 0x47;
       *pos++ = (u8)(le32_to_cpu(notif->dialog_token));
       *((u64 *)(pos)) = (u64)(get_64bit_ts (le32_to_cpu(notif->hi_t1), 
                               le32_to_cpu(notif->lo_t1))); 
       pos += 8;
       *pos++ = (u8)(le32_to_cpu(notif->t1_error));
       *((u64 *)(pos)) = (u64)(get_64bit_ts (le32_to_cpu(notif->hi_t4), 
                               le32_to_cpu(notif->lo_t4))); 
       pos += 8;
       *pos++ = (u8)(le32_to_cpu(notif->t4_error));
#ifdef CPTCFG_IWLWIFI_WFA_SPECIFIC
       *((u64 *)(pos)) = egress_time;	
#endif

       ieee80211_rx(mvm->hw, skb);
}
