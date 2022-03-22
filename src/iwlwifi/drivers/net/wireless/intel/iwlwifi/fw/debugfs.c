/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 - 2017 Intel Deutschland GmbH
 * Copyright(c) 2018 - 2019 Intel Corporation
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
 * Copyright(c) 2016 - 2017 Intel Deutschland GmbH
 * Copyright(c) 2018 - 2019 Intel Corporation
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
#include "api/commands.h"
#include "debugfs.h"
#include "dbg.h"
#ifdef CPTCFG_IWLWIFI_DEBUG_HOST_CMD_ENABLED
#include "api/dhc.h"
#endif
#include "api/rs.h"

#define FWRT_DEBUGFS_OPEN_WRAPPER(name, buflen, argtype)		\
struct dbgfs_##name##_data {						\
	argtype *arg;							\
	bool read_done;							\
	ssize_t rlen;							\
	char rbuf[buflen];						\
};									\
static int _iwl_dbgfs_##name##_open(struct inode *inode,		\
				    struct file *file)			\
{									\
	struct dbgfs_##name##_data *data;				\
									\
	data = kzalloc(sizeof(*data), GFP_KERNEL);			\
	if (!data)							\
		return -ENOMEM;						\
									\
	data->read_done = false;					\
	data->arg = inode->i_private;					\
	file->private_data = data;					\
									\
	return 0;							\
}

#define FWRT_DEBUGFS_READ_WRAPPER(name)					\
static ssize_t _iwl_dbgfs_##name##_read(struct file *file,		\
					char __user *user_buf,		\
					size_t count, loff_t *ppos)	\
{									\
	struct dbgfs_##name##_data *data = file->private_data;		\
									\
	if (!data->read_done) {						\
		data->read_done = true;					\
		data->rlen = iwl_dbgfs_##name##_read(data->arg,		\
						     sizeof(data->rbuf),\
						     data->rbuf);	\
	}								\
									\
	if (data->rlen < 0)						\
		return data->rlen;					\
	return simple_read_from_buffer(user_buf, count, ppos,		\
				       data->rbuf, data->rlen);		\
}

static int _iwl_dbgfs_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);

	return 0;
}

#define _FWRT_DEBUGFS_READ_FILE_OPS(name, buflen, argtype)		\
FWRT_DEBUGFS_OPEN_WRAPPER(name, buflen, argtype)			\
FWRT_DEBUGFS_READ_WRAPPER(name)						\
static const struct file_operations iwl_dbgfs_##name##_ops = {		\
	.read = _iwl_dbgfs_##name##_read,				\
	.open = _iwl_dbgfs_##name##_open,				\
	.llseek = generic_file_llseek,					\
	.release = _iwl_dbgfs_release,					\
}

#define FWRT_DEBUGFS_WRITE_WRAPPER(name, buflen, argtype)		\
static ssize_t _iwl_dbgfs_##name##_write(struct file *file,		\
					 const char __user *user_buf,	\
					 size_t count, loff_t *ppos)	\
{									\
	argtype *arg =							\
		((struct dbgfs_##name##_data *)file->private_data)->arg;\
	char buf[buflen] = {};						\
	size_t buf_size = min(count, sizeof(buf) -  1);			\
									\
	if (copy_from_user(buf, user_buf, buf_size))			\
		return -EFAULT;						\
									\
	return iwl_dbgfs_##name##_write(arg, buf, buf_size);		\
}

#define _FWRT_DEBUGFS_READ_WRITE_FILE_OPS(name, buflen, argtype)	\
FWRT_DEBUGFS_OPEN_WRAPPER(name, buflen, argtype)			\
FWRT_DEBUGFS_WRITE_WRAPPER(name, buflen, argtype)			\
FWRT_DEBUGFS_READ_WRAPPER(name)						\
static const struct file_operations iwl_dbgfs_##name##_ops = {		\
	.write = _iwl_dbgfs_##name##_write,				\
	.read = _iwl_dbgfs_##name##_read,				\
	.open = _iwl_dbgfs_##name##_open,				\
	.llseek = generic_file_llseek,					\
	.release = _iwl_dbgfs_release,					\
}

#define _FWRT_DEBUGFS_WRITE_FILE_OPS(name, buflen, argtype)		\
FWRT_DEBUGFS_OPEN_WRAPPER(name, buflen, argtype)			\
FWRT_DEBUGFS_WRITE_WRAPPER(name, buflen, argtype)			\
static const struct file_operations iwl_dbgfs_##name##_ops = {		\
	.write = _iwl_dbgfs_##name##_write,				\
	.open = _iwl_dbgfs_##name##_open,				\
	.llseek = generic_file_llseek,					\
	.release = _iwl_dbgfs_release,					\
}

#define FWRT_DEBUGFS_READ_FILE_OPS(name, bufsz)				\
	_FWRT_DEBUGFS_READ_FILE_OPS(name, bufsz, struct iwl_fw_runtime)

#define FWRT_DEBUGFS_WRITE_FILE_OPS(name, bufsz)			\
	_FWRT_DEBUGFS_WRITE_FILE_OPS(name, bufsz, struct iwl_fw_runtime)

#define FWRT_DEBUGFS_READ_WRITE_FILE_OPS(name, bufsz)			\
	_FWRT_DEBUGFS_READ_WRITE_FILE_OPS(name, bufsz, struct iwl_fw_runtime)

#define FWRT_DEBUGFS_ADD_FILE_ALIAS(alias, name, parent, mode) do {	\
	debugfs_create_file(alias, mode, parent, fwrt,			\
			    &iwl_dbgfs_##name##_ops);			\
	} while (0)
#define FWRT_DEBUGFS_ADD_FILE(name, parent, mode) \
	FWRT_DEBUGFS_ADD_FILE_ALIAS(#name, name, parent, mode)

static int iwl_fw_send_timestamp_marker_cmd(struct iwl_fw_runtime *fwrt)
{
	struct iwl_mvm_marker marker = {
		.dw_len = sizeof(struct iwl_mvm_marker) / 4,
		.marker_id = MARKER_ID_SYNC_CLOCK,

		/* the real timestamp is taken from the ftrace clock
		 * this is for finding the match between fw and kernel logs
		 */
		.timestamp = cpu_to_le64(fwrt->timestamp.seq++),
	};

	struct iwl_host_cmd hcmd = {
		.id = MARKER_CMD,
		.flags = CMD_ASYNC,
		.data[0] = &marker,
		.len[0] = sizeof(marker),
	};

	return iwl_trans_send_cmd(fwrt->trans, &hcmd);
}

static void iwl_fw_timestamp_marker_wk(struct work_struct *work)
{
	int ret;
	struct iwl_fw_runtime *fwrt =
		container_of(work, struct iwl_fw_runtime, timestamp.wk.work);
	unsigned long delay = fwrt->timestamp.delay;

	ret = iwl_fw_send_timestamp_marker_cmd(fwrt);
	if (!ret && delay)
		schedule_delayed_work(&fwrt->timestamp.wk,
				      round_jiffies_relative(delay));
	else
		IWL_INFO(fwrt,
			 "stopping timestamp_marker, ret: %d, delay: %u\n",
			 ret, jiffies_to_msecs(delay) / 1000);
}

void iwl_fw_trigger_timestamp(struct iwl_fw_runtime *fwrt, u32 delay)
{
	IWL_INFO(fwrt,
		 "starting timestamp_marker trigger with delay: %us\n",
		 delay);

	iwl_fw_cancel_timestamp(fwrt);

	fwrt->timestamp.delay = msecs_to_jiffies(delay * 1000);

	schedule_delayed_work(&fwrt->timestamp.wk,
			      round_jiffies_relative(fwrt->timestamp.delay));
}

static ssize_t iwl_dbgfs_timestamp_marker_write(struct iwl_fw_runtime *fwrt,
						char *buf, size_t count)
{
	int ret;
	u32 delay;

	ret = kstrtou32(buf, 10, &delay);
	if (ret < 0)
		return ret;

	iwl_fw_trigger_timestamp(fwrt, delay);

	return count;
}

static ssize_t iwl_dbgfs_timestamp_marker_read(struct iwl_fw_runtime *fwrt,
					       size_t size, char *buf)
{
	u32 delay_secs = jiffies_to_msecs(fwrt->timestamp.delay) / 1000;

	return scnprintf(buf, size, "%d\n", delay_secs);
}

FWRT_DEBUGFS_READ_WRITE_FILE_OPS(timestamp_marker, 16);

struct hcmd_write_data {
	__be32 cmd_id;
	__be32 flags;
	__be16 length;
	u8 data[0];
} __packed;

static ssize_t iwl_dbgfs_send_hcmd_write(struct iwl_fw_runtime *fwrt, char *buf,
					 size_t count)
{
	size_t header_size = (sizeof(u32) * 2 + sizeof(u16)) * 2;
	size_t data_size = (count - 1) / 2;
	int ret;
	struct hcmd_write_data *data;
	struct iwl_host_cmd hcmd = {
		.len = { 0, },
		.data = { NULL, },
	};

	if (!iwl_trans_fw_running(fwrt->trans))
		return -EIO;

	if (count < header_size + 1 || count > 1024 * 4)
		return -EINVAL;

	data = kmalloc(data_size, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ret = hex2bin((u8 *)data, buf, data_size);
	if (ret)
		goto out;

	hcmd.id = be32_to_cpu(data->cmd_id);
	hcmd.flags = be32_to_cpu(data->flags);
	hcmd.len[0] = be16_to_cpu(data->length);
	hcmd.data[0] = data->data;

	if (count != header_size + hcmd.len[0] * 2 + 1) {
		IWL_ERR(fwrt,
			"host command data size does not match header length\n");
		ret = -EINVAL;
		goto out;
	}

	if (fwrt->ops && fwrt->ops->send_hcmd)
		ret = fwrt->ops->send_hcmd(fwrt->ops_ctx, &hcmd);
	else
		ret = -EPERM;

	if (ret < 0)
		goto out;

	if (hcmd.flags & CMD_WANT_SKB)
		iwl_free_resp(&hcmd);
out:
	kfree(data);
	return ret ?: count;
}

FWRT_DEBUGFS_WRITE_FILE_OPS(send_hcmd, 512);

#ifdef CPTCFG_IWLWIFI_DEBUG_HOST_CMD_ENABLED
struct iwl_dhc_write_data {
	__be32 length;
	__be32 index_and_mask;
	__be32 data[0];
} __packed;

static ssize_t iwl_dbgfs_send_dhc_write(struct iwl_fw_runtime *fwrt,
					char *buf, size_t count)
{
	int ret, i;
	struct iwl_dhc_write_data *data;
	u32 length;
	size_t header_size = sizeof(u32) * 2 * 2;
	size_t data_size = (count - 1) / 2, cmd_size;
	struct iwl_dhc_cmd *dhc_cmd = NULL;
	struct iwl_host_cmd hcmd = {
		.id = iwl_cmd_id(DEBUG_HOST_COMMAND, LEGACY_GROUP, 0),
		.flags = CMD_ASYNC,
		.len = { 0, },
		.data = { NULL, },
	};

	if (!iwl_trans_fw_running(fwrt->trans))
		return -EIO;

	if (count < header_size + 1 || count > 1024 * 4)
		return -EINVAL;

	data = kmalloc(data_size, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ret = hex2bin((u8 *)data, buf, data_size);
	if (ret)
		goto out;

	length = be32_to_cpu(data->length);

	if (count != header_size + sizeof(u32) * length * 2 + 1) {
		IWL_ERR(fwrt, "DHC data size does not match length header\n");
		ret = -EINVAL;
		goto out;
	}

	cmd_size = sizeof(*dhc_cmd) + length * sizeof(u32);
	dhc_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!dhc_cmd) {
		ret = -ENOMEM;
		goto out;
	}

	dhc_cmd->length = cpu_to_le32(length);
	dhc_cmd->index_and_mask =
		cpu_to_le32(be32_to_cpu(data->index_and_mask));
	for (i = 0; i < length; i++)
		dhc_cmd->data[i] =
			cpu_to_le32(be32_to_cpu(data->data[i]));

	hcmd.len[0] = cmd_size;
	hcmd.data[0] = dhc_cmd;

	if (fwrt->ops && fwrt->ops->send_hcmd)
		ret = fwrt->ops->send_hcmd(fwrt->ops_ctx, &hcmd);
	else
		ret = -EPERM;
out:
	kfree(dhc_cmd);
	kfree(data);
	return ret ?: count;
}

FWRT_DEBUGFS_WRITE_FILE_OPS(send_dhc, 512);

struct iwl_dhc_tlc_whole_cmd {
	struct iwl_dhc_cmd dhc;
	struct iwl_dhc_tlc_cmd tlc_data;
} __packed;

static void iwl_fw_build_dhc_tlc_cmd(struct iwl_dhc_tlc_whole_cmd *cmd,
				     enum iwl_tlc_debug_types type, u32 data)
{
	cmd->dhc.length = cpu_to_le32(sizeof(cmd->tlc_data) >> 2);
	cmd->dhc.index_and_mask = cpu_to_le32(DHC_TABLE_INTEGRATION |
					  DHC_TARGET_UMAC |
					  DHC_INTEGRATION_TLC_DEBUG_CONFIG);

	cmd->tlc_data.type = cpu_to_le32(type);
	cmd->tlc_data.data[0] = cpu_to_le32(data);
}

static ssize_t iwl_dbgfs_tpc_enable_write(struct iwl_fw_runtime *fwrt,
					  char *buf, size_t count)
{
	struct iwl_dhc_tlc_whole_cmd dhc_cmd = { {0} };
	struct iwl_host_cmd hcmd = {
		.id = iwl_cmd_id(DEBUG_HOST_COMMAND, IWL_ALWAYS_LONG_GROUP, 0),
		.data[0] = &dhc_cmd,
		.len[0] = sizeof(dhc_cmd),
	};
	bool enabled;
	int ret;

	ret = kstrtobool(buf, &enabled);
	iwl_fw_build_dhc_tlc_cmd(&dhc_cmd, IWL_TLC_DEBUG_TPC_ENABLED,
				 enabled);

	ret = iwl_trans_send_cmd(fwrt->trans, &hcmd);
	if (ret) {
		IWL_ERR(fwrt, "Failed to send TLC Debug command: %d\n", ret);
		return ret;
	}

	fwrt->tpc_enabled = enabled;

	return count;
}

static ssize_t iwl_dbgfs_tpc_enable_read(struct iwl_fw_runtime *fwrt,
					 size_t size, char *buf)
{
	return scnprintf(buf, size, "tpc is currently %s\n",
			 fwrt->tpc_enabled ? "enabled" : "disabled");
}

FWRT_DEBUGFS_READ_WRITE_FILE_OPS(tpc_enable, 30);

static ssize_t iwl_dbgfs_tpc_stats_read(struct iwl_fw_runtime *fwrt,
					size_t size, char *buf)
{
	struct iwl_dhc_tlc_whole_cmd dhc_cmd = { {0} };
	struct iwl_host_cmd hcmd = {
		.id = iwl_cmd_id(DEBUG_HOST_COMMAND, IWL_ALWAYS_LONG_GROUP, 0),
		.flags = CMD_WANT_SKB,
		.data[0] = &dhc_cmd,
		.len[0] = sizeof(dhc_cmd),
	};
	struct iwl_dhc_cmd_resp *resp;
	struct iwl_tpc_stats *stats;
	int ret = 0;

	iwl_fw_build_dhc_tlc_cmd(&dhc_cmd, IWL_TLC_DEBUG_TPC_STATS, 0);

	ret = iwl_trans_send_cmd(fwrt->trans, &hcmd);
	if (ret) {
		IWL_ERR(fwrt, "Failed to send TLC Debug command: %d\n", ret);
		goto err;
	}

	if (!hcmd.resp_pkt) {
		IWL_ERR(fwrt,
			"Response expected\n");
		goto err;
	}

	if (iwl_rx_packet_payload_len(hcmd.resp_pkt) !=
	    sizeof(*resp) + sizeof(*stats)) {
		IWL_ERR(fwrt,
			"Invalid size for TPC stats request response (%u instead of %lu)\n",
			iwl_rx_packet_payload_len(hcmd.resp_pkt),
			sizeof(*resp) + sizeof(*stats));
		ret = -EINVAL;
		goto err;
	}

	resp = (struct iwl_dhc_cmd_resp *)hcmd.resp_pkt->data;
	if (le32_to_cpu(resp->status) != 1) {
		IWL_ERR(fwrt, "response status is not success: %d\n",
			resp->status);
		ret = -EINVAL;
		goto err;
	}

	stats = (struct iwl_tpc_stats *)resp->data;

	return scnprintf(buf, size,
			 "tpc stats: no-tpc %u, step1 %u, step2 %u, step3 %u, step4 %u, step5 %u\n",
			 le32_to_cpu(stats->no_tpc),
			 le32_to_cpu(stats->step[0]),
			 le32_to_cpu(stats->step[1]),
			 le32_to_cpu(stats->step[2]),
			 le32_to_cpu(stats->step[3]),
			 le32_to_cpu(stats->step[4]));

err:
	return ret ?: -EIO;
}

FWRT_DEBUGFS_READ_FILE_OPS(tpc_stats, 150);

static ssize_t iwl_dbgfs_ps_report_read(struct iwl_fw_runtime *fwrt,
					size_t size, char *buf, int mac_mask)
{
	__le32 cmd_data;

	struct iwl_dhc_cmd cmd = {
		.length = cpu_to_le32(1),
		.index_and_mask = cpu_to_le32(DHC_TABLE_AUTOMATION |
			mac_mask |
			DHC_AUTO_UMAC_REPORT_POWER_STATISTICS),
	};

	struct iwl_host_cmd hcmd = {
		.id = iwl_cmd_id(DEBUG_HOST_COMMAND, LEGACY_GROUP, 0),
		.flags = CMD_WANT_SKB,
		.data = { &cmd, &cmd_data},
		.len = { sizeof(cmd), sizeof(cmd_data) },
	};
	struct iwl_dhc_cmd_resp *resp;
	int ret = 0;

	static const char * const entries[] = {
		"total_sleep_counter",
		"total_sleep_duration",
		"report_duration",
		"total_missed_beacon_counter",
		"missed_3_consecutive_beacon_count",
		"ps_flags",
		"phy_inactive_duration",
		"mac_ctdp_sum",
		"ppm_offset_vs_ap_sum",
		"deep_sleep_duration",
		"received_beacon_counter",
		"bcon_in_lprx_counter",
		"bcon_abort_counter",
		"multicast_indication_tim_counter",
		"missed_multicast_counter",
		"misbehave_counter",
		"reserved1",
		"reserved2",
		"reserved3",
		"reserved4"
	};

	u32 report_size;
	u32 total_size_written;
	u32 i;

	ret = iwl_trans_send_cmd(fwrt->trans, &hcmd);
	if (ret) {
		IWL_ERR(fwrt,
			"Failed to send power-save report command: %d\n", ret);
		goto err;
	}

	if (!hcmd.resp_pkt) {
		IWL_ERR(fwrt,
			"Response expected\n");
		goto err;
	}

	resp = (struct iwl_dhc_cmd_resp *)hcmd.resp_pkt->data;
	if (le32_to_cpu(resp->status) != 1) {
		IWL_ERR(fwrt, "response status is not success: %d\n",
			resp->status);
		ret = -EINVAL;
		goto err;
	}

	report_size = (iwl_rx_packet_payload_len(hcmd.resp_pkt) -
		offsetof(struct iwl_dhc_cmd_resp, data)) / sizeof(*resp->data);
	total_size_written = scnprintf(buf, size,
				       "power-save report (expected: %lu received: %d):\n",
				       ARRAY_SIZE(entries), report_size);

	for (i = 0; i < report_size; i++)
		total_size_written += scnprintf(buf + total_size_written,
						size - total_size_written,
						"%s %u\n",
						i < ARRAY_SIZE(entries) ?
						entries[i] : "Unknown",
						le32_to_cpu(resp->data[i]));

	return total_size_written;

err:
	return ret ?: -EIO;
}

static ssize_t iwl_dbgfs_ps_report_umac_read
				(struct iwl_fw_runtime *fwrt,
				size_t size,
				char *buf)
{
	return iwl_dbgfs_ps_report_read(fwrt, size, buf, DHC_TARGET_UMAC);
}

FWRT_DEBUGFS_READ_FILE_OPS(ps_report_umac, 721);

static ssize_t iwl_dbgfs_ps_report_lmac_read
				(struct iwl_fw_runtime *fwrt,
				size_t size,
				char *buf)
{
	// LMAC value is 0 for backwards compatibility
	return iwl_dbgfs_ps_report_read(fwrt, size, buf, 0);
}

FWRT_DEBUGFS_READ_FILE_OPS(ps_report_lmac, 268);

#endif

static ssize_t iwl_dbgfs_fw_dbg_domain_write(struct iwl_fw_runtime *fwrt,
					     char *buf, size_t count)
{
	u32 new_domain;
	long val;
	int ret;

	if (!iwl_trans_fw_running(fwrt->trans))
		return -EIO;

	ret = kstrtol(buf, 0, &val);
	if (ret)
		return ret;

	new_domain = (u32)val;
	if (new_domain != fwrt->trans->dbg.domains_bitmap) {
		ret = iwl_dbg_tlv_gen_active_trigs(fwrt, new_domain);
		if (ret)
			return ret;

		iwl_dbg_tlv_time_point(fwrt, IWL_FW_INI_TIME_POINT_PERIODIC,
				       NULL);
	}

	return count;
}

static ssize_t iwl_dbgfs_fw_dbg_domain_read(struct iwl_fw_runtime *fwrt,
					    size_t size, char *buf)
{
	return scnprintf(buf, size, "0x%08x\n",
			 fwrt->trans->dbg.domains_bitmap);
}

FWRT_DEBUGFS_READ_WRITE_FILE_OPS(fw_dbg_domain, 20);

void iwl_fwrt_dbgfs_register(struct iwl_fw_runtime *fwrt,
			    struct dentry *dbgfs_dir)
{
	INIT_DELAYED_WORK(&fwrt->timestamp.wk, iwl_fw_timestamp_marker_wk);
	FWRT_DEBUGFS_ADD_FILE(timestamp_marker, dbgfs_dir, 0200);
	FWRT_DEBUGFS_ADD_FILE(send_hcmd, dbgfs_dir, 0200);
	FWRT_DEBUGFS_ADD_FILE(fw_dbg_domain, dbgfs_dir, 0600);
#ifdef CPTCFG_IWLWIFI_DEBUG_HOST_CMD_ENABLED
	if (fw_has_capa(&fwrt->fw->ucode_capa,
			IWL_UCODE_TLV_CAPA_TLC_OFFLOAD)) {
		FWRT_DEBUGFS_ADD_FILE(tpc_enable, dbgfs_dir, 0600);
		FWRT_DEBUGFS_ADD_FILE(tpc_stats, dbgfs_dir, 0400);
	}
	FWRT_DEBUGFS_ADD_FILE(ps_report_umac, dbgfs_dir, 0400);
	FWRT_DEBUGFS_ADD_FILE(ps_report_lmac, dbgfs_dir, 0400);
	FWRT_DEBUGFS_ADD_FILE(send_dhc, dbgfs_dir, 0200);
#endif
}
