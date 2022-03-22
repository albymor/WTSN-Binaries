/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016        Intel Deutschland GmbH
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
 *  Intel Linux Wireless <linuxwifi@intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016        Intel Deutschland GmbH
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
#ifndef __iwl_fw_api_tm_h__
#define __iwl_fw_api_tm_h__

/* commands */
/* These apply to firmware versions <=23 */
enum {
	/* Time measurement */
	TIME_MEASUREMENT_REQ = 0x66,

    /**
    * struct iwl_time_measurement_notif  (f/w <= 23)
    */
	TIME_MEASUREMENT = 0x67,

    /**
    * struct iwl_time_measurement_confirm_notif  (f/w <= 23)
    */
	TIME_MEASUREMENT_CONFIRM = 0x68,
};

/* Time measurement */
#define PTP_CTX_MAX_DATA_SIZE   128

struct iwl_time_measurement_ptp_ctx {
	u8 element_id;
	u8 length;
	u8 data[PTP_CTX_MAX_DATA_SIZE];
} __packed /* PTP_CTX_VER_1 */;

/**
 * iwl_time_measurement_req_notif - timing measurement request notification
 *
 * @addr: sending station address
 * @reserved: for alignment
 * @trigger: 0 - stop masurement, 1 - start measurement
 */
struct iwl_time_measurement_req_notif {
	u8 addr[ETH_ALEN];
	__le16 reserved;
	__le32 trigger;
} __packed; /* WNM_80211V_TIMING_MEASUREMENT_REQUEST_NTFY_API_S_VER_1 */

/**
 * iwl_time_measurement_confirm_notif - used to indicate an ack on a timing
 *	measurement frame
 *
 * @addr: sending station address
 * @reserved: for alignment
 * @dialog_token: the dialog token from the sent frame
 * @t1: time of frame departure in units of microseconds
 * @t1_error: maximal @t1 error in units of microseconds
 * @t4: time of ack arrival in units of microseconds
 * @t4_error: maximal @t4 error in units of microseconds
 */
struct iwl_time_measurement_confirm_notif {
	u8 addr[ETH_ALEN];
	__le16 reserved;
	__le32 dialog_token;
	__le32 t1;
	__le32 t1_error;
	__le32 t4;
	__le32 t4_error;
} __packed; /* WNM_80211V_TIMING_MEASUREMENT_CONFIRM_NTFY_API_S_VER_1 */

/**
 * iwl_time_measurement_confirm_notif_v2 - used to indicate an ack on a timing
 *	measurement frame -- with firmware > 28
 *
 * @addr: sending station address
 * @reserved: for alignment
 * @dialog_token: the dialog token from the sent frame
 * @{hi|lo}_t1: time of frame departure in units of 10 nanoseconds
 * @t1_error: maximal @t1 error in units of 10 nanoseconds
 * @{hi|lo}_t4: time of ack arrival in units of 10 nanoseconds
 * @t4_error: maximal @t4 error in units of 10 nanoseconds
 */
struct iwl_time_measurement_confirm_notif_v2 {
	u8 addr[ETH_ALEN];
	__le16 reserved;
	__le32 dialog_token;
	__le32 hi_t1;
	__le32 lo_t1;
	__le32 t1_error;
	__le32 hi_t4;
	__le32 lo_t4;
	__le32 t4_error;
} __packed; /* WNM_80211V_TIMING_MEASUREMENT_CONFIRM_NTFY_API_S_VER_2 */

/**
 * iwl_time_measurement_notif - notification upon timing measurement frame
 *
 * @addr: sending station address
 * @reserved: for alignment
 * @dialog_token: the dialog token from the received frame
 * @followup_token: a folow up token in case of a second measurement frame
 * @t1: time of measurement frame departure (in follow up frames) - 10 ns units
 * @t1_error: maximal @t1 error - microsecond units
 * @t2: time of measurement frame arrival - microsecond units
 * @t2_error: maximal @t2 error - microsecond units
 * @t3: time of ack departure - microsecond units
 * @t3_error: maximal @t3 error - microsecond units
 * @t4: time of ack arrival (in follow up frames) - microsecond units
 * @t4_error: maximal @t4 error - microsecond units
 * @ptp: vendor specific information element
 */
struct iwl_time_measurement_notif {
	u8 addr[ETH_ALEN];
	__le16 reserved;
	__le32 dialog_token;
	__le32 followup_token;
	__le32 t1;
	__le32 t1_error;
	__le32 t4;
	__le32 t4_error;
	__le32 t2;
	__le32 t2_error;
	__le32 t3;
	__le32 t3_error;
	struct iwl_time_measurement_ptp_ctx ptp;
} __packed; /* WNM_80211V_TIMING_MEASUREMENT_NTFY_API_S_VER_1 */

/**
 * iwl_time_measurement_notif_v2 - notification upon timing measurement frame
 * for firmware versions > 28
 *
 * @addr: sending station address
 * @reserved: for alignment
 * @dialog_token: the dialog token from the received frame
 * @followup_token: a folow up token in case of a second measurement frame
 * @{hi|lo}_t1: time of measurement frame departure (in follow up frames) - 10 ns units
 * @t1_error: maximal @t1 error - 10 ns units
 * @{hi|lo}_t2: time of measurement frame arrival - 10 ns units
 * @t2_error: maximal @t2 error - 10 ns units
 * @{hi|lo}_t3: time of ack departure - 10 ns units
 * @t3_error: maximal @t3 error - 10 ns units
 * @{hi|lo}_t4: time of ack arrival (in follow up frames) - 10 ns units
 * @t4_error: maximal @t4 error - 10 ns units
 * @ptp: vendor specific information element
 */
struct iwl_time_measurement_notif_v2 {
	u8 addr[ETH_ALEN];
	__le16 reserved;
	__le32 dialog_token;
	__le32 followup_token;
	__le32 hi_t1;
	__le32 lo_t1;
	__le32 t1_error;
	__le32 hi_t4;
	__le32 lo_t4;
	__le32 t4_error;
	__le32 hi_t2;
	__le32 lo_t2;
	__le32 t2_error;
	__le32 hi_t3;
	__le32 lo_t3;
	__le32 t3_error;
	struct iwl_time_measurement_ptp_ctx ptp;
} __packed; /* WNM_80211V_TIMING_MEASUREMENT_NTFY_API_S_VER_2 */

typedef enum {
    WNM_80211V_TIMING_OPERATION_STATUS_RESPONSE = 0x0,
    WNM_80211V_TIMING_OPERATION_READ_PLATFORM_TIME = 0x1,
    WNM_80211V_TIMING_OPERATION_READ_FIRMWARE_TIME = 0x2,
    WNM_80211V_TIMING_OPERATION_READ_BOTH = 0x3,
    WNM_80211V_TIMING_OPERATION_ERROR_COMMAND = 0xF
} get_correlated_time_modes; /* WNM_80211V_GET_CORRELATED_TIME_MODE_API_S_VER_2 */

struct iwl_get_correlated_time_mode {
    __le32 operation; /* 0x0, 0x1, 0x2 or 0x3 */
} __packed; /* WNM_80211V_GET_CORRELATED_TIME_MODE_API_S_VER_2 */

struct iwl_get_correlated_time {
    __le32 operation_status;
    __le32 platform_timestamp_hi;
    __le32 platform_timestamp_lo;
    __le32 gp2_timestamp_hi;
    __le32 gp2_timestamp_lo;
} __packed; /* WNM_80211V_GET_CORRELATED_TIME_MODE_API_S_VER_2 */

#endif /* __iwl_fw_api_tm_h__ */
