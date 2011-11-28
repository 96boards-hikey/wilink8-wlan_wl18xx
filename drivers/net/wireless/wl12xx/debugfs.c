/*
 * This file is part of wl1271
 *
 * Copyright (C) 2009 Nokia Corporation
 *
 * Contact: Luciano Coelho <luciano.coelho@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include "debugfs.h"

#include <linux/skbuff.h>
#include <linux/slab.h>

#include "wl12xx.h"
#include "acx.h"
#include "ps.h"
#include "io.h"
#include "tx.h"


#define DEBUGFS_SINGLE_PARAM_ADD(parent, name, base)					\
static ssize_t name ##_read(struct file *file, char __user *user_buf, 	\
                                  size_t count, loff_t *ppos)			\
{													\
        struct wl1271 *wl = file->private_data;		\
		printk("0x%02x\n", wl->conf.parent.name);	\
        return 0;									\
}													\
																\
static ssize_t name ##_write(struct file *file,					\
                                   const char __user *user_buf,	\
                                   size_t count, loff_t *ppos)	\
{																\
        struct wl1271 *wl = file->private_data;					\
        char buf[10];				\
        size_t len;					\
        unsigned long value;		\
        int ret;								\
												\
        len = min(count, sizeof(buf) - 1);		\
        if (copy_from_user(buf, user_buf, len))	\
                return -EFAULT;					\
        buf[len] = '\0';						\
												\
        ret = kstrtoul(buf, 0, &value);							\
        if (ret < 0) {											\
                wl1271_warning("illegal value for " #name );	\
                return -EINVAL;			\
        }								\
										\
        mutex_lock(&wl->mutex);			\
        wl->conf.parent.name = value;	\
        mutex_unlock(&wl->mutex);		\
						\
        return count;	\
}						\
						\
static const struct file_operations name ##_ops = {	\
        .read = name ##_read,				\
        .write = name ##_write,				\
        .open = wl1271_open_file_generic,	\
        .llseek = default_llseek,			\
};											\


#define DEBUGFS_ARRAY_PARAM_ADD(parent, name, base, arr_len)			\
static ssize_t name ##_read(struct file *file, char __user *user_buf, 	\
                                  size_t count, loff_t *ppos)			\
{															\
	struct wl1271 *wl = file->private_data;					\
	u16 i;													\
															\
	for (i=0 ; i<arr_len ; i++)								\
	{														\
		printk("[%d] = 0x%02x\n", i, wl->conf.parent.name[i]);	\
	}									\
										\
       return 0;						\
}										\
										\
static ssize_t name ##_write(struct file *file,					\
                                   const char __user *user_buf,	\
                                   size_t count, loff_t *ppos)	\
{												\
	struct wl1271 *wl = file->private_data;		\
	unsigned long value;	\
	int ret;				\
	u16 i;						\
	char buf[arr_len*3 + 1];	\
								\
	if (count != (arr_len * 3))	\
    	{						\
       	printk("Failed to configure " #name "!  str length should be %d! \n", (arr_len*3));	\
       	return -EINVAL;	\
    	}				\
						\
        if (copy_from_user(buf, user_buf, count))	\
                return -EFAULT;						\
        						\
        						\
	for (i=0 ; i<arr_len ; i++)	\
	{							\
		buf[(i*3 + 2)] = '\0';						\
		ret = kstrtoul(&buf[i*3], base, &value);	\
		mutex_lock(&wl->mutex);						\
		wl->conf.parent.name[i] = value;			\
		mutex_unlock(&wl->mutex);					\
	}	\
		\
		\
        return count;	\
}						\
						\
static const struct file_operations name ##_ops = {		\
        .read = name ##_read,							\
        .write = name ##_write,							\
        .open = wl1271_open_file_generic,				\
        .llseek = default_llseek,						\
};

/* ms */
#define WL1271_DEBUGFS_STATS_LIFETIME 1000

/* debugfs macros idea from mac80211 */
#define DEBUGFS_FORMAT_BUFFER_SIZE 100
static int wl1271_format_buffer(char __user *userbuf, size_t count,
				    loff_t *ppos, char *fmt, ...)
{
	va_list args;
	char buf[DEBUGFS_FORMAT_BUFFER_SIZE];
	int res;

	va_start(args, fmt);
	res = vscnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	return simple_read_from_buffer(userbuf, count, ppos, buf, res);
}

#define DEBUGFS_READONLY_FILE(name, fmt, value...)			\
static ssize_t name## _read(struct file *file, char __user *userbuf,	\
			    size_t count, loff_t *ppos)			\
{									\
	struct wl1271 *wl = file->private_data;				\
	return wl1271_format_buffer(userbuf, count, ppos,		\
				    fmt "\n", ##value);			\
}									\
									\
static const struct file_operations name## _ops = {			\
	.read = name## _read,						\
	.open = wl1271_open_file_generic,				\
	.llseek	= generic_file_llseek,					\
};

#define DEBUGFS_ADD(name, parent)			        		\
	entry = debugfs_create_file(#name, 0400, parent,		\
				    wl, &name## _ops);			\
	if (!entry || IS_ERR(entry))					\
		goto err;						\

#define DEBUGFS_ADD_PREFIX(prefix, name, parent)			\
	do {								\
		entry = debugfs_create_file(#name, 0400, parent,	\
				    wl, &prefix## _## name## _ops);	\
		if (!entry || IS_ERR(entry))				\
			goto err;					\
	} while (0);

#define DEBUGFS_FWSTATS_FILE(sub, name, fmt)				\
static ssize_t sub## _ ##name## _read(struct file *file,		\
				      char __user *userbuf,		\
				      size_t count, loff_t *ppos)	\
{									\
	struct wl1271 *wl = file->private_data;				\
									\
	wl1271_debugfs_update_stats(wl);				\
									\
	return wl1271_format_buffer(userbuf, count, ppos, fmt "\n",	\
				    wl->stats.fw_stats->sub.name);	\
}									\
									\
static const struct file_operations sub## _ ##name## _ops = {		\
	.read = sub## _ ##name## _read,					\
	.open = wl1271_open_file_generic,				\
	.llseek	= generic_file_llseek,					\
};

#define DEBUGFS_FWSTATS_ADD(sub, name)				\
	DEBUGFS_ADD(sub## _ ##name, stats)

static void wl1271_debugfs_update_stats(struct wl1271 *wl)
{
	int ret;

	mutex_lock(&wl->mutex);

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	if (wl->state == WL1271_STATE_ON &&
	    time_after(jiffies, wl->stats.fw_stats_update +
		       msecs_to_jiffies(WL1271_DEBUGFS_STATS_LIFETIME))) {
		wl1271_acx_statistics(wl, wl->stats.fw_stats);
		wl->stats.fw_stats_update = jiffies;
	}

	wl1271_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);
}

static int wl1271_open_file_generic(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

/* ring */
DEBUGFS_FWSTATS_FILE(ring, tx_procs, "%u");
DEBUGFS_FWSTATS_FILE(ring, prepared_descs, "%u");
DEBUGFS_FWSTATS_FILE(ring, tx_xfr, "%u");
DEBUGFS_FWSTATS_FILE(ring, tx_dma, "%u");
DEBUGFS_FWSTATS_FILE(ring, tx_cmplt, "%u");
DEBUGFS_FWSTATS_FILE(ring, rx_procs, "%u");
DEBUGFS_FWSTATS_FILE(ring, rx_data, "%u");

/* debug */
DEBUGFS_FWSTATS_FILE(dbg, debug1, "%u");
DEBUGFS_FWSTATS_FILE(dbg, debug2, "%u");
DEBUGFS_FWSTATS_FILE(dbg, debug3, "%u");
DEBUGFS_FWSTATS_FILE(dbg, debug4, "%u");
DEBUGFS_FWSTATS_FILE(dbg, debug5, "%u");
DEBUGFS_FWSTATS_FILE(dbg, debug6, "%u");

/* tx */
DEBUGFS_FWSTATS_FILE(tx, frag_called, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_mpdu_alloc_failed, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_init_called, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_in_process_called, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_tkip_called, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_key_not_found, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_need_fragmentation, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_bad_mem_blk_num, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_failed, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_cache_hit, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_cache_miss, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_1, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_2, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_3, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_4, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_5, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_6, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_7, "%u");
DEBUGFS_FWSTATS_FILE(tx, frag_8, "%u");
DEBUGFS_FWSTATS_FILE(tx, template_prepared, "%u");
DEBUGFS_FWSTATS_FILE(tx, data_prepared, "%u");
DEBUGFS_FWSTATS_FILE(tx, template_programmed, "%u");
DEBUGFS_FWSTATS_FILE(tx, data_programmed, "%u");
DEBUGFS_FWSTATS_FILE(tx, burst_programmed, "%u");
DEBUGFS_FWSTATS_FILE(tx, starts, "%u");
DEBUGFS_FWSTATS_FILE(tx, imm_resp, "%u");
DEBUGFS_FWSTATS_FILE(tx, start_tempaltes, "%u");
DEBUGFS_FWSTATS_FILE(tx, start_int_template, "%u");
DEBUGFS_FWSTATS_FILE(tx, start_fw_gen, "%u");
DEBUGFS_FWSTATS_FILE(tx, start_data, "%u");
DEBUGFS_FWSTATS_FILE(tx, start_null_frame, "%u");
DEBUGFS_FWSTATS_FILE(tx, exch, "%u");
DEBUGFS_FWSTATS_FILE(tx, retry_template, "%u");
DEBUGFS_FWSTATS_FILE(tx, retry_data, "%u");
DEBUGFS_FWSTATS_FILE(tx, exch_pending, "%u")
DEBUGFS_FWSTATS_FILE(tx, exch_mismatch, "%u")
DEBUGFS_FWSTATS_FILE(tx, done_template, "%u")
DEBUGFS_FWSTATS_FILE(tx, done_data, "%u")
DEBUGFS_FWSTATS_FILE(tx, done_intTemplate, "%u")
DEBUGFS_FWSTATS_FILE(tx, pre_xfr, "%u")
DEBUGFS_FWSTATS_FILE(tx, xfr, "%u")
DEBUGFS_FWSTATS_FILE(tx, xfr_out_of_mem, "%u")
DEBUGFS_FWSTATS_FILE(tx, dma_programmed, "%u");
DEBUGFS_FWSTATS_FILE(tx, dma_done, "%u");
DEBUGFS_FWSTATS_FILE(tx, checksum_req, "%u");
DEBUGFS_FWSTATS_FILE(tx, checksum_calc, "%u");

/* rx */
DEBUGFS_FWSTATS_FILE(rx, rx_out_of_mem, "%u");
DEBUGFS_FWSTATS_FILE(rx, rx_hdr_overflow, "%u");
DEBUGFS_FWSTATS_FILE(rx, rx_hw_stuck, "%u");
DEBUGFS_FWSTATS_FILE(rx, rx_dropped_frame, "%u");
DEBUGFS_FWSTATS_FILE(rx, rx_complete_dropped_frame, "%u");
DEBUGFS_FWSTATS_FILE(rx, rx_Alloc_frame, "%u");
DEBUGFS_FWSTATS_FILE(rx, rx_done_queue, "%u");
DEBUGFS_FWSTATS_FILE(rx, rx_done, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag_called, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag_init_Called, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag_in_Process_Called, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag_tkip_called, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag_need_defrag, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag_decrypt_failed, "%u");
DEBUGFS_FWSTATS_FILE(rx, decrypt_Key_not_found, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag_need_decr, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag1, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag2, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag3, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag4, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag5, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag6, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag7, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag8, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag, "%u");
DEBUGFS_FWSTATS_FILE(rx, defrag_end, "%u");
DEBUGFS_FWSTATS_FILE(rx, xfr, "%u");
DEBUGFS_FWSTATS_FILE(rx, xfr_end, "%u");
DEBUGFS_FWSTATS_FILE(rx, cmplt, "%u");
DEBUGFS_FWSTATS_FILE(rx, pre_cmplt, "%u");
DEBUGFS_FWSTATS_FILE(rx, cmplt_task, "%u");
DEBUGFS_FWSTATS_FILE(rx, phy_hdr, "%u");
DEBUGFS_FWSTATS_FILE(rx, timeout, "%u");
DEBUGFS_FWSTATS_FILE(rx, checksum_req, "%u");
DEBUGFS_FWSTATS_FILE(rx, checksum_calc, "%u");


/* dma */
DEBUGFS_FWSTATS_FILE(dma, rx_errors, "%u");
DEBUGFS_FWSTATS_FILE(dma, tx_errors, "%u");

/* irq */
DEBUGFS_FWSTATS_FILE(isr, irqs, "%u");


/* pwr */
DEBUGFS_FWSTATS_FILE(pwr, missing_bcns, "%u");
DEBUGFS_FWSTATS_FILE(pwr, rcvd_beacons, "%u");
DEBUGFS_FWSTATS_FILE(pwr, conn_out_of_sync, "%u");
DEBUGFS_FWSTATS_FILE(pwr, cont_missbcns_spread_1, "%u");
DEBUGFS_FWSTATS_FILE(pwr, cont_missbcns_spread_2, "%u");
DEBUGFS_FWSTATS_FILE(pwr, cont_missbcns_spread_3, "%u");
DEBUGFS_FWSTATS_FILE(pwr, cont_missbcns_spread_4, "%u");
DEBUGFS_FWSTATS_FILE(pwr, cont_missbcns_spread_5, "%u");
DEBUGFS_FWSTATS_FILE(pwr, cont_missbcns_spread_6, "%u");
DEBUGFS_FWSTATS_FILE(pwr, cont_missbcns_spread_7, "%u");
DEBUGFS_FWSTATS_FILE(pwr, cont_missbcns_spread_8, "%u");
DEBUGFS_FWSTATS_FILE(pwr, cont_missbcns_spread_9, "%u");
DEBUGFS_FWSTATS_FILE(pwr, cont_missbcns_spread_10_plus, "%u");
DEBUGFS_FWSTATS_FILE(pwr, rcvd_awake_beacons_cnt, "%u");


/* event */
DEBUGFS_FWSTATS_FILE(event, calibration, "%u");
DEBUGFS_FWSTATS_FILE(event, rx_mismatch, "%u");
DEBUGFS_FWSTATS_FILE(event, rx_mem_empty, "%u");


/* ps_poll_upsd */
DEBUGFS_FWSTATS_FILE(ps_poll_upsd, ps_poll_timeouts, "%u");
DEBUGFS_FWSTATS_FILE(ps_poll_upsd, upsd_timeouts, "%u");
DEBUGFS_FWSTATS_FILE(ps_poll_upsd, upsd_max_ap_turn, "%u");
DEBUGFS_FWSTATS_FILE(ps_poll_upsd, ps_poll_max_ap_turn, "%u");
DEBUGFS_FWSTATS_FILE(ps_poll_upsd, ps_poll_utilization, "%u");
DEBUGFS_FWSTATS_FILE(ps_poll_upsd, upsd_utilization, "%u");


/* rx_filter */
DEBUGFS_FWSTATS_FILE(rx_filter, beacon_filter, "%u");
DEBUGFS_FWSTATS_FILE(rx_filter, arp_filter, "%u");
DEBUGFS_FWSTATS_FILE(rx_filter, mc_filter, "%u");
DEBUGFS_FWSTATS_FILE(rx_filter, dup_filter, "%u");
DEBUGFS_FWSTATS_FILE(rx_filter, data_filter, "%u");
DEBUGFS_FWSTATS_FILE(rx_filter, ibss_filter, "%u");


/* calibration_fail */
DEBUGFS_FWSTATS_FILE(calibration_fail, init_cal_total, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, init_radio_bands_fail, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, init_set_params, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, init_tx_clpc_fail, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, init_rx_iq_mm_fail, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_cal_total, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_drpw_rtrim_fail, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_drpw_pd_buf_fail, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_drpw_tx_mix_freq_fail, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_drpw_ta_cal, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_drpw_rxIf2Gain, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_drpw_rx_dac, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_drpw_chan_tune, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_drpw_rx_tx_lpf, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_drpw_lna_tank, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_tx_lo_leak_fail, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_tx_iq_mm_fail, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_tx_pdet_fail, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_tx_ppa_fail, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_tx_clpc_fail, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_rx_ana_dc_fail, "%u");
#ifdef TNETW1283
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_rx_dig_dc_fail, "%u");
#endif
DEBUGFS_FWSTATS_FILE(calibration_fail, tune_rx_iq_mm_fail, "%u");
DEBUGFS_FWSTATS_FILE(calibration_fail, cal_state_fail, "%u");


/* agg_size */
DEBUGFS_FWSTATS_FILE(agg, size_1, "%u");
DEBUGFS_FWSTATS_FILE(agg, size_2, "%u");
DEBUGFS_FWSTATS_FILE(agg, size_3, "%u");
DEBUGFS_FWSTATS_FILE(agg, size_4, "%u");
DEBUGFS_FWSTATS_FILE(agg, size_5, "%u");
DEBUGFS_FWSTATS_FILE(agg, size_6, "%u");
DEBUGFS_FWSTATS_FILE(agg, size_7, "%u");
DEBUGFS_FWSTATS_FILE(agg, size_8, "%u");

/* new_pipe_line */
DEBUGFS_FWSTATS_FILE(new_pipe_line, hs_tx_stat_fifo_int, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, hs_rx_stat_fifo_int, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, tcp_tx_stat_fifo_int, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, tcp_rx_stat_fifo_int, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, enc_tx_stat_fifo_int, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, enc_rx_stat_fifo_int, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, rx_complete_stat_fifo_Int, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, pre_proc_swi, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, post_proc_swi, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, sec_frag_swi, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, pre_to_defrag_swi, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, defrag_to_csum_swi, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, csum_to_rx_xfer_swi, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, dec_packet_in_fifo_Full, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, dec_packet_out, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, cs_rx_packet_in, "%u");
DEBUGFS_FWSTATS_FILE(new_pipe_line, cs_rx_packet_out, "%u");

DEBUGFS_READONLY_FILE(retry_count, "%u", wl->stats.retry_count);
DEBUGFS_READONLY_FILE(excessive_retries, "%u",
		      wl->stats.excessive_retries);


DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, primary_clock_setting_time, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, secondary_clock_setting_time, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, external_pa_dc2dc, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, io_configuration, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, sdio_configuration, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, settings, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, enable_clpc, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, enable_tx_low_pwr_on_siso_rdl, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, rx_profile, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, pwr_limit_reference_11_abg, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, pwr_limit_reference_11p, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, xtal_itrim_val, 0)
DEBUGFS_ARRAY_PARAM_ADD(mac_and_phy_params, per_chan_pwr_limit_arr_11abg, 16, NUM_OF_CHANNELS_11_ABG)
DEBUGFS_ARRAY_PARAM_ADD(mac_and_phy_params, per_chan_pwr_limit_arr_11p, 16, NUM_OF_CHANNELS_11_P)
DEBUGFS_ARRAY_PARAM_ADD(mac_and_phy_params, per_sub_band_tx_trace_loss, 16, NUM_OF_SUB_BANDS)
DEBUGFS_ARRAY_PARAM_ADD(mac_and_phy_params, per_sub_band_rx_trace_loss, 16, NUM_OF_SUB_BANDS)


#ifdef DEBUGFS_TI
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, rdl, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, auto_detect, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, dedicated_fem, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, low_band_component, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, low_band_component_type, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, high_band_component, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, high_band_component_type, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, number_of_assembled_ant2_4, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, number_of_assembled_ant5, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, tcxo_ldo_voltage, 0)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, hw_board_type, 0)
DEBUGFS_ARRAY_PARAM_ADD(mac_and_phy_params, pin_muxing_platform_options, 16, PIN_MUXING_SIZE)
DEBUGFS_SINGLE_PARAM_ADD(mac_and_phy_params, srf_state, 0)
DEBUGFS_ARRAY_PARAM_ADD(mac_and_phy_params, srf1, 16, SRF_TABLE_LEN)
DEBUGFS_ARRAY_PARAM_ADD(mac_and_phy_params, srf2, 16, SRF_TABLE_LEN)
DEBUGFS_ARRAY_PARAM_ADD(mac_and_phy_params, srf3, 16, SRF_TABLE_LEN)
#endif


static ssize_t tx_queue_len_read(struct file *file, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	u32 queue_len;
	char buf[20];
	int res;

	queue_len = wl1271_tx_total_queue_count(wl);

	res = scnprintf(buf, sizeof(buf), "%u\n", queue_len);
	return simple_read_from_buffer(userbuf, count, ppos, buf, res);
}

static const struct file_operations tx_queue_len_ops = {
	.read = tx_queue_len_read,
	.open = wl1271_open_file_generic,
	.llseek = default_llseek,
};

static ssize_t gpio_power_read(struct file *file, char __user *user_buf,
			  size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	bool state = test_bit(WL1271_FLAG_GPIO_POWER, &wl->flags);

	int res;
	char buf[10];

	res = scnprintf(buf, sizeof(buf), "%d\n", state);

	return simple_read_from_buffer(user_buf, count, ppos, buf, res);
}

static ssize_t gpio_power_write(struct file *file,
			   const char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	char buf[10];
	size_t len;
	unsigned long value;
	int ret;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len)) {
		return -EFAULT;
	}
	buf[len] = '\0';

	ret = kstrtoul(buf, 0, &value);
	if (ret < 0) {
		wl1271_warning("illegal value in gpio_power");
		return -EINVAL;
	}

	mutex_lock(&wl->mutex);

	if (value)
		wl1271_power_on(wl);
	else
		wl1271_power_off(wl);

	mutex_unlock(&wl->mutex);
	return count;
}

static const struct file_operations gpio_power_ops = {
	.read = gpio_power_read,
	.write = gpio_power_write,
	.open = wl1271_open_file_generic,
	.llseek = default_llseek,
};

static ssize_t start_recovery_write(struct file *file,
				    const char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;

	mutex_lock(&wl->mutex);
	ieee80211_queue_work(wl->hw, &wl->recovery_work);
	mutex_unlock(&wl->mutex);

	return count;
}

static const struct file_operations start_recovery_ops = {
	.write = start_recovery_write,
	.open = wl1271_open_file_generic,
	.llseek = default_llseek,
};

static ssize_t dynamic_ps_timeout_write(struct file *file,
				    const char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	char buf[10];
	size_t len;
	unsigned long value;
	int ret;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len)) {
		return -EFAULT;
	}
	buf[len] = '\0';

	ret = kstrtoul(buf, 0, &value);
	if (ret < 0) {
		wl1271_warning("illegal value in dynamic_ps");
		return -EINVAL;
	}

	mutex_lock(&wl->mutex);
	ieee80211_set_dyn_ps_timeout(wl->vif, value);
	mutex_unlock(&wl->mutex);

	return count;
}

static const struct file_operations dynamic_ps_timeout_ops = {
	.write = dynamic_ps_timeout_write,
	.open = wl1271_open_file_generic,
	.llseek = default_llseek,
};

static ssize_t driver_state_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	int res = 0;
	char buf[1024];

	mutex_lock(&wl->mutex);

#define DRIVER_STATE_PRINT(x, fmt)   \
	(res += scnprintf(buf + res, sizeof(buf) - res,\
			  #x " = " fmt "\n", wl->x))

#define DRIVER_STATE_PRINT_LONG(x) DRIVER_STATE_PRINT(x, "%ld")
#define DRIVER_STATE_PRINT_INT(x)  DRIVER_STATE_PRINT(x, "%d")
#define DRIVER_STATE_PRINT_STR(x)  DRIVER_STATE_PRINT(x, "%s")
#define DRIVER_STATE_PRINT_LHEX(x) DRIVER_STATE_PRINT(x, "0x%lx")
#define DRIVER_STATE_PRINT_HEX(x)  DRIVER_STATE_PRINT(x, "0x%x")

	DRIVER_STATE_PRINT_INT(tx_blocks_available);
	DRIVER_STATE_PRINT_INT(tx_allocated_blocks);
	DRIVER_STATE_PRINT_INT(tx_allocated_pkts[0]);
	DRIVER_STATE_PRINT_INT(tx_allocated_pkts[1]);
	DRIVER_STATE_PRINT_INT(tx_allocated_pkts[2]);
	DRIVER_STATE_PRINT_INT(tx_allocated_pkts[3]);
	DRIVER_STATE_PRINT_INT(tx_frames_cnt);
	DRIVER_STATE_PRINT_LHEX(tx_frames_map[0]);
	DRIVER_STATE_PRINT_INT(tx_queue_count[0]);
	DRIVER_STATE_PRINT_INT(tx_queue_count[1]);
	DRIVER_STATE_PRINT_INT(tx_queue_count[2]);
	DRIVER_STATE_PRINT_INT(tx_queue_count[3]);
	DRIVER_STATE_PRINT_INT(tx_packets_count);
	DRIVER_STATE_PRINT_INT(tx_results_count);
	DRIVER_STATE_PRINT_LHEX(flags);
	DRIVER_STATE_PRINT_INT(tx_blocks_freed);
	DRIVER_STATE_PRINT_INT(tx_security_last_seq_lsb);
	DRIVER_STATE_PRINT_INT(rx_counter);
	DRIVER_STATE_PRINT_INT(session_counter);
	DRIVER_STATE_PRINT_INT(state);
	DRIVER_STATE_PRINT_INT(bss_type);
	DRIVER_STATE_PRINT_INT(channel);
	DRIVER_STATE_PRINT_HEX(rate_set);
	DRIVER_STATE_PRINT_HEX(basic_rate_set);
	DRIVER_STATE_PRINT_HEX(basic_rate);
	DRIVER_STATE_PRINT_INT(band);
	DRIVER_STATE_PRINT_INT(beacon_int);
	DRIVER_STATE_PRINT_INT(psm_entry_retry);
	DRIVER_STATE_PRINT_INT(ps_poll_failures);
	DRIVER_STATE_PRINT_HEX(filters);
	DRIVER_STATE_PRINT_HEX(rx_config);
	DRIVER_STATE_PRINT_HEX(rx_filter);
	DRIVER_STATE_PRINT_INT(power_level);
	DRIVER_STATE_PRINT_INT(rssi_thold);
	DRIVER_STATE_PRINT_INT(last_rssi_event);
	DRIVER_STATE_PRINT_INT(sg_enabled);
	DRIVER_STATE_PRINT_INT(enable_11a);
	DRIVER_STATE_PRINT_INT(noise);
	DRIVER_STATE_PRINT_LHEX(ap_hlid_map[0]);
	DRIVER_STATE_PRINT_INT(last_tx_hlid);
	DRIVER_STATE_PRINT_INT(ba_support);
	DRIVER_STATE_PRINT_HEX(ba_rx_bitmap);
	DRIVER_STATE_PRINT_HEX(ap_fw_ps_map);
	DRIVER_STATE_PRINT_LHEX(ap_ps_map);
	DRIVER_STATE_PRINT_HEX(quirks);
	DRIVER_STATE_PRINT_HEX(irq);
	DRIVER_STATE_PRINT_HEX(ref_clock);
	DRIVER_STATE_PRINT_HEX(tcxo_clock);
	DRIVER_STATE_PRINT_HEX(hw_pg_ver);
	DRIVER_STATE_PRINT_HEX(platform_quirks);
	DRIVER_STATE_PRINT_HEX(chip.id);
	DRIVER_STATE_PRINT_STR(chip.fw_ver_str);
	DRIVER_STATE_PRINT_STR(chip.phy_fw_ver_str);
	DRIVER_STATE_PRINT_INT(sched_scanning);

#undef DRIVER_STATE_PRINT_INT
#undef DRIVER_STATE_PRINT_LONG
#undef DRIVER_STATE_PRINT_HEX
#undef DRIVER_STATE_PRINT_LHEX
#undef DRIVER_STATE_PRINT_STR
#undef DRIVER_STATE_PRINT

	mutex_unlock(&wl->mutex);

	return simple_read_from_buffer(user_buf, count, ppos, buf, res);
}

static const struct file_operations driver_state_ops = {
	.read = driver_state_read,
	.open = wl1271_open_file_generic,
	.llseek = default_llseek,
};

static ssize_t dtim_interval_read(struct file *file, char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	u8 value;

	if (wl->conf.conn.wake_up_event == CONF_WAKE_UP_EVENT_DTIM ||
	    wl->conf.conn.wake_up_event == CONF_WAKE_UP_EVENT_N_DTIM)
		value = wl->conf.conn.listen_interval;
	else
		value = 0;

	return wl1271_format_buffer(user_buf, count, ppos, "%d\n", value);
}

static ssize_t dtim_interval_write(struct file *file,
				   const char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	char buf[10];
	size_t len;
	unsigned long value;
	int ret;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';

	ret = kstrtoul(buf, 0, &value);
	if (ret < 0) {
		wl1271_warning("illegal value for dtim_interval");
		return -EINVAL;
	}

	if (value < 1 || value > 10) {
		wl1271_warning("dtim value is not in valid range");
		return -ERANGE;
	}

	mutex_lock(&wl->mutex);

	wl->conf.conn.listen_interval = value;
	/* for some reason there are different event types for 1 and >1 */
	if (value == 1)
		wl->conf.conn.wake_up_event = CONF_WAKE_UP_EVENT_DTIM;
	else
		wl->conf.conn.wake_up_event = CONF_WAKE_UP_EVENT_N_DTIM;

	/*
	 * we don't reconfigure ACX_WAKE_UP_CONDITIONS now, so it will only
	 * take effect on the next time we enter psm.
	 */
	mutex_unlock(&wl->mutex);
	return count;
}

static const struct file_operations dtim_interval_ops = {
	.read = dtim_interval_read,
	.write = dtim_interval_write,
	.open = wl1271_open_file_generic,
	.llseek = default_llseek,
};

static ssize_t beacon_interval_read(struct file *file, char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	u8 value;

	if (wl->conf.conn.wake_up_event == CONF_WAKE_UP_EVENT_BEACON ||
	    wl->conf.conn.wake_up_event == CONF_WAKE_UP_EVENT_N_BEACONS)
		value = wl->conf.conn.listen_interval;
	else
		value = 0;

	return wl1271_format_buffer(user_buf, count, ppos, "%d\n", value);
}

static ssize_t beacon_interval_write(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	char buf[10];
	size_t len;
	unsigned long value;
	int ret;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';

	ret = kstrtoul(buf, 0, &value);
	if (ret < 0) {
		wl1271_warning("illegal value for beacon_interval");
		return -EINVAL;
	}

	if (value < 1 || value > 255) {
		wl1271_warning("beacon interval value is not in valid range");
		return -ERANGE;
	}

	mutex_lock(&wl->mutex);

	wl->conf.conn.listen_interval = value;
	/* for some reason there are different event types for 1 and >1 */
	if (value == 1)
		wl->conf.conn.wake_up_event = CONF_WAKE_UP_EVENT_BEACON;
	else
		wl->conf.conn.wake_up_event = CONF_WAKE_UP_EVENT_N_BEACONS;

	/*
	 * we don't reconfigure ACX_WAKE_UP_CONDITIONS now, so it will only
	 * take effect on the next time we enter psm.
	 */
	mutex_unlock(&wl->mutex);
	return count;
}

static const struct file_operations beacon_interval_ops = {
	.read = beacon_interval_read,
	.write = beacon_interval_write,
	.open = wl1271_open_file_generic,
	.llseek = default_llseek,
};

static ssize_t rx_streaming_interval_write(struct file *file,
			   const char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	char buf[10];
	size_t len;
	unsigned long value;
	int ret;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';

	ret = kstrtoul(buf, 0, &value);
	if (ret < 0) {
		wl1271_warning("illegal value in rx_streaming_interval!");
		return -EINVAL;
	}

	/* valid values: 0, 10-100 */
	if (value && (value < 10 || value > 100)) {
		wl1271_warning("value is not in range!");
		return -ERANGE;
	}

	mutex_lock(&wl->mutex);

	wl->conf.rx_streaming.interval = value;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	wl1271_recalc_rx_streaming(wl);

	wl1271_ps_elp_sleep(wl);
out:
	mutex_unlock(&wl->mutex);
	return count;
}

static ssize_t rx_streaming_interval_read(struct file *file,
			    char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	return wl1271_format_buffer(userbuf, count, ppos,
				    "%d\n", wl->conf.rx_streaming.interval);
}

static const struct file_operations rx_streaming_interval_ops = {
	.read = rx_streaming_interval_read,
	.write = rx_streaming_interval_write,
	.open = wl1271_open_file_generic,
	.llseek = default_llseek,
};

static ssize_t rx_streaming_always_write(struct file *file,
			   const char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	char buf[10];
	size_t len;
	unsigned long value;
	int ret;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';

	ret = kstrtoul(buf, 0, &value);
	if (ret < 0) {
		wl1271_warning("illegal value in rx_streaming_write!");
		return -EINVAL;
	}

	/* valid values: 0, 10-100 */
	if (!(value == 0 || value == 1)) {
		wl1271_warning("value is not in valid!");
		return -EINVAL;
	}

	mutex_lock(&wl->mutex);

	wl->conf.rx_streaming.always = value;

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	wl1271_recalc_rx_streaming(wl);

	wl1271_ps_elp_sleep(wl);
out:
	mutex_unlock(&wl->mutex);
	return count;
}

static ssize_t rx_streaming_always_read(struct file *file,
			    char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	return wl1271_format_buffer(userbuf, count, ppos,
				    "%d\n", wl->conf.rx_streaming.always);
}

static const struct file_operations rx_streaming_always_ops = {
	.read = rx_streaming_always_read,
	.write = rx_streaming_always_write,
	.open = wl1271_open_file_generic,
	.llseek = default_llseek,
};

static ssize_t beacon_filtering_write(struct file *file,
				      const char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	char buf[10];
	size_t len;
	unsigned long value;
	int ret;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';

	ret = kstrtoul(buf, 0, &value);
	if (ret < 0) {
		wl1271_warning("illegal value for beacon_filtering!");
		return -EINVAL;
	}

	mutex_lock(&wl->mutex);

	ret = wl1271_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl1271_acx_beacon_filter_opt(wl, !!value);

	wl1271_ps_elp_sleep(wl);
out:
	mutex_unlock(&wl->mutex);
	return count;
}

static const struct file_operations beacon_filtering_ops = {
	.write = beacon_filtering_write,
	.open = wl1271_open_file_generic,
	.llseek = default_llseek,
};


static ssize_t fwlog_enable_read(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	u8 value;

	if (wl->conf.fwlog.output == WL12XX_FWLOG_OUTPUT_DBG_PINS)
		value = 0;
	else
		value = 1;

	return wl1271_format_buffer(user_buf, count, ppos, "%d\n", value);
}

static ssize_t fwlog_enable_write(struct file *file,
		const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct wl1271 *wl = file->private_data;
	char buf[16];
	size_t len;
	unsigned long value;
	int ret;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';

	ret = kstrtoul(buf, 0, &value);
	if (ret < 0) {
		wl1271_warning("illegal value for fwlog_enable");
		return -EINVAL;
	}

	if (value > 1) {
		wl1271_warning("fwlog_enable value is not in valid range");
		return -ERANGE;
	}

	wl1271_warning("FW log options will take effect after the FW reboots");

	mutex_lock(&wl->mutex);

	if (value)
		wl->conf.fwlog.output = WL12XX_FWLOG_OUTPUT_HOST;
	else
		wl->conf.fwlog.output = WL12XX_FWLOG_OUTPUT_DBG_PINS;

	mutex_unlock(&wl->mutex);

	return count;
}

static const struct file_operations fwlog_enable_ops = {
	.read = fwlog_enable_read,
	.write = fwlog_enable_write,
	.open = wl1271_open_file_generic,
	.llseek = default_llseek,
};

static ssize_t tx_frag_thld_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.tx.frag_threshold);
}

static ssize_t tx_frag_thld_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for frag_threshold");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);

        wl->conf.tx.frag_threshold = value;

		ret = wl1271_ps_elp_wakeup(wl);
		if (ret < 0)
			goto out;

		/* Send fragmentation threshold to FW */
		ret = wl1271_acx_frag_threshold(wl, wl->conf.tx.frag_threshold);
		if (ret < 0)
			wl1271_error("Failed to Send fragmentation threshold %d to FW", wl->conf.tx.frag_threshold);
 
		wl1271_ps_elp_sleep(wl);

out:
		mutex_unlock(&wl->mutex);

        return count;
}

static const struct file_operations tx_frag_thld_ops = {
        .read = tx_frag_thld_read,
        .write = tx_frag_thld_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};

static ssize_t tx_ba_win_size_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.ht.tx_ba_win_size);
}

static ssize_t tx_ba_win_size_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for tx_ba_win_size");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);
        wl->conf.ht.tx_ba_win_size = value;
		mutex_unlock(&wl->mutex);

        return count;
}

static const struct file_operations tx_ba_win_size_ops = {
        .read = tx_ba_win_size_read,
        .write = tx_ba_win_size_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};

static ssize_t hw_tx_extra_mem_blk_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.hw_tx_extra_mem_blk);
}

static ssize_t hw_tx_extra_mem_blk_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for hw_tx_extra_mem_blk");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);
        wl->conf.hw_tx_extra_mem_blk = value;
		mutex_unlock(&wl->mutex);

        return count;
}

static const struct file_operations hw_tx_extra_mem_blk_ops = {
        .read = hw_tx_extra_mem_blk_read,
        .write = hw_tx_extra_mem_blk_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};


static ssize_t tx_compl_timeout_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.tx.tx_compl_timeout);
}

static ssize_t tx_compl_timeout_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for tx_compl_timeout");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);

        wl->conf.tx.tx_compl_timeout = value;

	mutex_unlock(&wl->mutex);

        return count;
}

static const struct file_operations tx_compl_timeout_ops = {
        .read = tx_compl_timeout_read,
        .write = tx_compl_timeout_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};

static ssize_t tx_compl_threshold_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.tx.tx_compl_threshold);
}

static ssize_t tx_compl_threshold_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for tx_compl_threshold");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);

        wl->conf.tx.tx_compl_threshold = value;

	mutex_unlock(&wl->mutex);

        return count;
}

static const struct file_operations tx_compl_threshold_ops = {
        .read = tx_compl_threshold_read,
        .write = tx_compl_threshold_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};



static ssize_t irq_blk_threshold_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.rx.irq_blk_threshold);
}

static ssize_t irq_blk_threshold_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for irq_blk_threshold");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);

        wl->conf.rx.irq_blk_threshold = value;

	mutex_unlock(&wl->mutex);

        return count;
}

static const struct file_operations irq_blk_threshold_ops = {
        .read = irq_blk_threshold_read,
        .write = irq_blk_threshold_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};


static ssize_t irq_pkt_threshold_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.rx.irq_pkt_threshold);
}

static ssize_t irq_pkt_threshold_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for irq_pkt_threshold");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);

        wl->conf.rx.irq_pkt_threshold = value;

	mutex_unlock(&wl->mutex);

        return count;
}

static const struct file_operations irq_pkt_threshold_ops = {
        .read = irq_pkt_threshold_read,
        .write = irq_pkt_threshold_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};



static ssize_t irq_timeout_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.rx.irq_timeout);
}

static ssize_t irq_timeout_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for irq_timeout");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);

        wl->conf.rx.irq_timeout = value;

	mutex_unlock(&wl->mutex);

        return count;
}

static const struct file_operations irq_timeout_ops = {
        .read = irq_timeout_read,
        .write = irq_timeout_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};


static ssize_t dynamic_memory_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.mem_wl18xx.dynamic_memory);
}

static ssize_t dynamic_memory_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for dynamic_memory");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);

        wl->conf.mem_wl18xx.dynamic_memory = value;

	mutex_unlock(&wl->mutex);

        return count;
}

static const struct file_operations dynamic_memory_ops = {
        .read = dynamic_memory_read,
        .write = dynamic_memory_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};


static ssize_t min_req_rx_blocks_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.mem_wl18xx.min_req_rx_blocks);
}

static ssize_t min_req_rx_blocks_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for min_req_rx_blocks");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);

        wl->conf.mem_wl18xx.min_req_rx_blocks = value;

	mutex_unlock(&wl->mutex);

        return count;
}

static const struct file_operations min_req_rx_blocks_ops = {
        .read = min_req_rx_blocks_read,
        .write = min_req_rx_blocks_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};


static ssize_t hw_checksum_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.hw_checksum.state);
}

static ssize_t hw_checksum_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for hw_checksum");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);

        wl->conf.hw_checksum.state = value;

	mutex_unlock(&wl->mutex);

        return count;
}

static const struct file_operations hw_checksum_ops = {
        .read = hw_checksum_read,
        .write = hw_checksum_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};


static ssize_t tx_ba_tid_bitmap_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.ht.tx_ba_tid_bitmap);
}

static ssize_t tx_ba_tid_bitmap_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for tx_ba_tid_bitmap");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);

        wl->conf.ht.tx_ba_tid_bitmap = value;

	mutex_unlock(&wl->mutex);

        return count;
}

static const struct file_operations tx_ba_tid_bitmap_ops = {
        .read = tx_ba_tid_bitmap_read,
        .write = tx_ba_tid_bitmap_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};


static ssize_t sleep_auth_read(struct file *file, char __user *user_buf,
                                  size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;

        return wl1271_format_buffer(user_buf, count, ppos, "%d\n",
                                    wl->conf.sleep_auth);
}

static ssize_t sleep_auth_write(struct file *file,
                                   const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
        struct wl1271 *wl = file->private_data;
        char buf[10];
        size_t len;
        unsigned long value;
        int ret;

        len = min(count, sizeof(buf) - 1);
        if (copy_from_user(buf, user_buf, len))
                return -EFAULT;
        buf[len] = '\0';

        ret = kstrtoul(buf, 0, &value);
        if (ret < 0) {
                wl1271_warning("illegal value for sleep auth ");
                return -EINVAL;
        }

        mutex_lock(&wl->mutex);
        wl->conf.sleep_auth = value;

        ret = wl1271_ps_elp_wakeup(wl);
        if (ret < 0)
        	goto out;

        /* Configure for ELP power saving */
    	ret = wl1271_acx_sleep_auth(wl, wl->conf.sleep_auth);

    	if (ret < 0)
    		printk("Error!, FAILED to set sleep_auth_write to wl->conf.sleep_auth ****\n");

    	wl1271_ps_elp_sleep(wl);

out:
		mutex_unlock(&wl->mutex);
        return count;
}

static const struct file_operations sleep_auth_ops = {
        .read = sleep_auth_read,
        .write = sleep_auth_write,
        .open = wl1271_open_file_generic,
        .llseek = default_llseek,
};

/**********************************************************
***********************************************************/

static int wl1271_debugfs_add_files(struct wl1271 *wl,
				     struct dentry *rootdir)
{
	int ret = 0;
	struct dentry *entry, *stats, *streaming, *phy_mac_params;

	stats = debugfs_create_dir("fw-statistics", rootdir);
	if (!stats || IS_ERR(stats)) {
		entry = stats;
		goto err;
	}

	/* ring */
	DEBUGFS_FWSTATS_ADD(ring, prepared_descs);
	DEBUGFS_FWSTATS_ADD(ring, tx_xfr);
	DEBUGFS_FWSTATS_ADD(ring, tx_dma);
	DEBUGFS_FWSTATS_ADD(ring, tx_cmplt);
	DEBUGFS_FWSTATS_ADD(ring, rx_procs);
	DEBUGFS_FWSTATS_ADD(ring, rx_data);

	/* debug */
	DEBUGFS_FWSTATS_ADD(dbg, debug1);
	DEBUGFS_FWSTATS_ADD(dbg, debug2);
	DEBUGFS_FWSTATS_ADD(dbg, debug3);
	DEBUGFS_FWSTATS_ADD(dbg, debug4);
	DEBUGFS_FWSTATS_ADD(dbg, debug5);
	DEBUGFS_FWSTATS_ADD(dbg, debug6);

	/* tx */
	DEBUGFS_FWSTATS_ADD(tx, frag_called);
	DEBUGFS_FWSTATS_ADD(tx, frag_mpdu_alloc_failed);
	DEBUGFS_FWSTATS_ADD(tx, frag_init_called);
	DEBUGFS_FWSTATS_ADD(tx, frag_in_process_called);
	DEBUGFS_FWSTATS_ADD(tx, frag_tkip_called);
	DEBUGFS_FWSTATS_ADD(tx, frag_key_not_found);
	DEBUGFS_FWSTATS_ADD(tx, frag_need_fragmentation);
	DEBUGFS_FWSTATS_ADD(tx, frag_bad_mem_blk_num);
	DEBUGFS_FWSTATS_ADD(tx, frag_failed);
	DEBUGFS_FWSTATS_ADD(tx, frag_cache_hit);
	DEBUGFS_FWSTATS_ADD(tx, frag_cache_miss);
	DEBUGFS_FWSTATS_ADD(tx, frag_1);
	DEBUGFS_FWSTATS_ADD(tx, frag_2);
	DEBUGFS_FWSTATS_ADD(tx, frag_3);
	DEBUGFS_FWSTATS_ADD(tx, frag_4);
	DEBUGFS_FWSTATS_ADD(tx, frag_5);
	DEBUGFS_FWSTATS_ADD(tx, frag_6);
	DEBUGFS_FWSTATS_ADD(tx, frag_7);
	DEBUGFS_FWSTATS_ADD(tx, frag_8);
	DEBUGFS_FWSTATS_ADD(tx, template_prepared);
	DEBUGFS_FWSTATS_ADD(tx, data_prepared);
	DEBUGFS_FWSTATS_ADD(tx, template_programmed);
	DEBUGFS_FWSTATS_ADD(tx, data_programmed);
	DEBUGFS_FWSTATS_ADD(tx, burst_programmed);
	DEBUGFS_FWSTATS_ADD(tx, starts);
	DEBUGFS_FWSTATS_ADD(tx, imm_resp);
	DEBUGFS_FWSTATS_ADD(tx, start_tempaltes);
	DEBUGFS_FWSTATS_ADD(tx, start_int_template);
	DEBUGFS_FWSTATS_ADD(tx, start_fw_gen);
	DEBUGFS_FWSTATS_ADD(tx, start_data);
	DEBUGFS_FWSTATS_ADD(tx, start_null_frame);
	DEBUGFS_FWSTATS_ADD(tx, exch);
	DEBUGFS_FWSTATS_ADD(tx, retry_template);
	DEBUGFS_FWSTATS_ADD(tx, retry_data);
	DEBUGFS_FWSTATS_ADD(tx, exch_pending)
	DEBUGFS_FWSTATS_ADD(tx, exch_mismatch)
	DEBUGFS_FWSTATS_ADD(tx, done_template)
	DEBUGFS_FWSTATS_ADD(tx, done_data)
	DEBUGFS_FWSTATS_ADD(tx, done_intTemplate)
	DEBUGFS_FWSTATS_ADD(tx, pre_xfr)
	DEBUGFS_FWSTATS_ADD(tx, xfr)
	DEBUGFS_FWSTATS_ADD(tx, xfr_out_of_mem)
	DEBUGFS_FWSTATS_ADD(tx, dma_programmed);
	DEBUGFS_FWSTATS_ADD(tx, dma_done);
	DEBUGFS_FWSTATS_ADD(tx, checksum_req);
	DEBUGFS_FWSTATS_ADD(tx, checksum_calc);

	/* rx */
	DEBUGFS_FWSTATS_ADD(rx, rx_out_of_mem);
	DEBUGFS_FWSTATS_ADD(rx, rx_hdr_overflow);
	DEBUGFS_FWSTATS_ADD(rx, rx_hw_stuck);
	DEBUGFS_FWSTATS_ADD(rx, rx_dropped_frame);
	DEBUGFS_FWSTATS_ADD(rx, rx_complete_dropped_frame);
	DEBUGFS_FWSTATS_ADD(rx, rx_Alloc_frame);
	DEBUGFS_FWSTATS_ADD(rx, rx_done_queue);
	DEBUGFS_FWSTATS_ADD(rx, rx_done);
	DEBUGFS_FWSTATS_ADD(rx, defrag_called);
	DEBUGFS_FWSTATS_ADD(rx, defrag_init_Called);
	DEBUGFS_FWSTATS_ADD(rx, defrag_in_Process_Called);
	DEBUGFS_FWSTATS_ADD(rx, defrag_tkip_called);
	DEBUGFS_FWSTATS_ADD(rx, defrag_need_defrag);
	DEBUGFS_FWSTATS_ADD(rx, defrag_decrypt_failed);
	DEBUGFS_FWSTATS_ADD(rx, decrypt_Key_not_found);
	DEBUGFS_FWSTATS_ADD(rx, defrag_need_decr);
	DEBUGFS_FWSTATS_ADD(rx, defrag1);
	DEBUGFS_FWSTATS_ADD(rx, defrag2);
	DEBUGFS_FWSTATS_ADD(rx, defrag3);
	DEBUGFS_FWSTATS_ADD(rx, defrag4);
	DEBUGFS_FWSTATS_ADD(rx, defrag5);
	DEBUGFS_FWSTATS_ADD(rx, defrag6);
	DEBUGFS_FWSTATS_ADD(rx, defrag7);
	DEBUGFS_FWSTATS_ADD(rx, defrag8);
	DEBUGFS_FWSTATS_ADD(rx, defrag);
	DEBUGFS_FWSTATS_ADD(rx, defrag_end);
	DEBUGFS_FWSTATS_ADD(rx, xfr);
	DEBUGFS_FWSTATS_ADD(rx, xfr_end);
	DEBUGFS_FWSTATS_ADD(rx, cmplt);
	DEBUGFS_FWSTATS_ADD(rx, pre_cmplt);
	DEBUGFS_FWSTATS_ADD(rx, cmplt_task);
	DEBUGFS_FWSTATS_ADD(rx, phy_hdr);
	DEBUGFS_FWSTATS_ADD(rx, timeout);
	DEBUGFS_FWSTATS_ADD(rx, checksum_req);
	DEBUGFS_FWSTATS_ADD(rx, checksum_calc);


	/* dma */
	DEBUGFS_FWSTATS_ADD(dma, rx_errors);
	DEBUGFS_FWSTATS_ADD(dma, tx_errors);

	/* isr */
	DEBUGFS_FWSTATS_ADD(isr, irqs);


	/* pwr */
	DEBUGFS_FWSTATS_ADD(pwr, missing_bcns);
	DEBUGFS_FWSTATS_ADD(pwr, rcvd_beacons);
	DEBUGFS_FWSTATS_ADD(pwr, conn_out_of_sync);
	DEBUGFS_FWSTATS_ADD(pwr, cont_missbcns_spread_1);
	DEBUGFS_FWSTATS_ADD(pwr, cont_missbcns_spread_2);
	DEBUGFS_FWSTATS_ADD(pwr, cont_missbcns_spread_3);
	DEBUGFS_FWSTATS_ADD(pwr, cont_missbcns_spread_4);
	DEBUGFS_FWSTATS_ADD(pwr, cont_missbcns_spread_5);
	DEBUGFS_FWSTATS_ADD(pwr, cont_missbcns_spread_6);
	DEBUGFS_FWSTATS_ADD(pwr, cont_missbcns_spread_7);
	DEBUGFS_FWSTATS_ADD(pwr, cont_missbcns_spread_8);
	DEBUGFS_FWSTATS_ADD(pwr, cont_missbcns_spread_9);
	DEBUGFS_FWSTATS_ADD(pwr, cont_missbcns_spread_10_plus);
	DEBUGFS_FWSTATS_ADD(pwr, rcvd_awake_beacons_cnt);


	/* event */
	DEBUGFS_FWSTATS_ADD(event, calibration);
	DEBUGFS_FWSTATS_ADD(event, rx_mismatch);
	DEBUGFS_FWSTATS_ADD(event, rx_mem_empty);


	/* ps_poll_upsd */
	DEBUGFS_FWSTATS_ADD(ps_poll_upsd, ps_poll_timeouts);
	DEBUGFS_FWSTATS_ADD(ps_poll_upsd, upsd_timeouts);
	DEBUGFS_FWSTATS_ADD(ps_poll_upsd, upsd_max_ap_turn);
	DEBUGFS_FWSTATS_ADD(ps_poll_upsd, ps_poll_max_ap_turn);
	DEBUGFS_FWSTATS_ADD(ps_poll_upsd, ps_poll_utilization);
	DEBUGFS_FWSTATS_ADD(ps_poll_upsd, upsd_utilization);


	/* rx_filter */
	DEBUGFS_FWSTATS_ADD(rx_filter, beacon_filter);
	DEBUGFS_FWSTATS_ADD(rx_filter, arp_filter);
	DEBUGFS_FWSTATS_ADD(rx_filter, mc_filter);
	DEBUGFS_FWSTATS_ADD(rx_filter, dup_filter);
	DEBUGFS_FWSTATS_ADD(rx_filter, data_filter);
	DEBUGFS_FWSTATS_ADD(rx_filter, ibss_filter);


	/* calibration_fail */
	DEBUGFS_FWSTATS_ADD(calibration_fail, init_cal_total);
	DEBUGFS_FWSTATS_ADD(calibration_fail, init_radio_bands_fail);
	DEBUGFS_FWSTATS_ADD(calibration_fail, init_set_params);
	DEBUGFS_FWSTATS_ADD(calibration_fail, init_tx_clpc_fail);
	DEBUGFS_FWSTATS_ADD(calibration_fail, init_rx_iq_mm_fail);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_cal_total);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_drpw_rtrim_fail);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_drpw_pd_buf_fail);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_drpw_tx_mix_freq_fail);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_drpw_ta_cal);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_drpw_rxIf2Gain);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_drpw_rx_dac);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_drpw_chan_tune);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_drpw_rx_tx_lpf);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_drpw_lna_tank);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_tx_lo_leak_fail);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_tx_iq_mm_fail);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_tx_pdet_fail);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_tx_ppa_fail);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_tx_clpc_fail);
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_rx_ana_dc_fail);
#ifdef TNETW1283
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_rx_dig_dc_fail);
#endif
	DEBUGFS_FWSTATS_ADD(calibration_fail, tune_rx_iq_mm_fail);
	DEBUGFS_FWSTATS_ADD(calibration_fail, cal_state_fail);


	/* agg_size */
	DEBUGFS_FWSTATS_ADD(agg, size_1);
	DEBUGFS_FWSTATS_ADD(agg, size_2);
	DEBUGFS_FWSTATS_ADD(agg, size_3);
	DEBUGFS_FWSTATS_ADD(agg, size_4);
	DEBUGFS_FWSTATS_ADD(agg, size_5);
	DEBUGFS_FWSTATS_ADD(agg, size_6);
	DEBUGFS_FWSTATS_ADD(agg, size_7);
	DEBUGFS_FWSTATS_ADD(agg, size_8);

	/* new_pipe_line */
	DEBUGFS_FWSTATS_ADD(new_pipe_line, hs_tx_stat_fifo_int);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, hs_rx_stat_fifo_int);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, tcp_tx_stat_fifo_int);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, tcp_rx_stat_fifo_int);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, enc_tx_stat_fifo_int);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, enc_rx_stat_fifo_int);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, rx_complete_stat_fifo_Int);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, pre_proc_swi);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, post_proc_swi);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, sec_frag_swi);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, pre_to_defrag_swi);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, defrag_to_csum_swi);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, csum_to_rx_xfer_swi);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, dec_packet_in_fifo_Full);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, dec_packet_out);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, cs_rx_packet_in);
	DEBUGFS_FWSTATS_ADD(new_pipe_line, cs_rx_packet_out);


	DEBUGFS_ADD(tx_queue_len, rootdir);
	DEBUGFS_ADD(retry_count, rootdir);
	DEBUGFS_ADD(excessive_retries, rootdir);
	DEBUGFS_ADD(gpio_power, rootdir);
	DEBUGFS_ADD(start_recovery, rootdir);
	DEBUGFS_ADD(dynamic_ps_timeout, rootdir);
	DEBUGFS_ADD(driver_state, rootdir);
	DEBUGFS_ADD(dtim_interval, rootdir);
	DEBUGFS_ADD(beacon_interval, rootdir);
	DEBUGFS_ADD(fwlog_enable, rootdir);
	DEBUGFS_ADD(beacon_filtering, rootdir);
	DEBUGFS_ADD(tx_frag_thld, rootdir);
	DEBUGFS_ADD(hw_checksum, rootdir);
	DEBUGFS_ADD(min_req_rx_blocks, rootdir);
	DEBUGFS_ADD(dynamic_memory, rootdir);
	DEBUGFS_ADD(irq_timeout, rootdir);
	DEBUGFS_ADD(irq_pkt_threshold, rootdir);
	DEBUGFS_ADD(irq_blk_threshold, rootdir);
	DEBUGFS_ADD(tx_compl_threshold, rootdir);
	DEBUGFS_ADD(tx_compl_timeout, rootdir);
	DEBUGFS_ADD(sleep_auth, rootdir);

	streaming = debugfs_create_dir("rx_streaming", rootdir);
	if (!streaming || IS_ERR(streaming))
		goto err;

	DEBUGFS_ADD_PREFIX(rx_streaming, interval, streaming);
	DEBUGFS_ADD_PREFIX(rx_streaming, always, streaming);


	phy_mac_params = debugfs_create_dir("phy-mac-ini-params", rootdir);
	if (!phy_mac_params || IS_ERR(phy_mac_params)) {
		entry = stats;
		goto err;
	}


#ifdef DEBUGFS_TI
	DEBUGFS_ADD(rdl, phy_mac_params)
	DEBUGFS_ADD(auto_detect, phy_mac_params)
	DEBUGFS_ADD(dedicated_fem, phy_mac_params)
	DEBUGFS_ADD(low_band_component, phy_mac_params)
	DEBUGFS_ADD(low_band_component_type, phy_mac_params)
	DEBUGFS_ADD(high_band_component, phy_mac_params)
	DEBUGFS_ADD(high_band_component_type, phy_mac_params)
	DEBUGFS_ADD(number_of_assembled_ant2_4, phy_mac_params)
	DEBUGFS_ADD(number_of_assembled_ant5, phy_mac_params)
	DEBUGFS_ADD(external_pa_dc2dc, phy_mac_params)
	DEBUGFS_ADD(tcxo_ldo_voltage, phy_mac_params)
	DEBUGFS_ADD(pin_muxing_platform_options, phy_mac_params)
	DEBUGFS_ADD(srf_state, phy_mac_params)
	DEBUGFS_ADD(srf1, phy_mac_params)
	DEBUGFS_ADD(srf2, phy_mac_params)
	DEBUGFS_ADD(srf3, phy_mac_params)
	DEBUGFS_ADD(hw_board_type, phy_mac_params)
	DEBUGFS_ADD(hw_tx_extra_mem_blk, phy_mac_params)
	DEBUGFS_ADD(tx_ba_win_size, rootdir);
	DEBUGFS_ADD(tx_ba_tid_bitmap, rootdir);
#endif


	DEBUGFS_ADD(primary_clock_setting_time, phy_mac_params)
	DEBUGFS_ADD(secondary_clock_setting_time, phy_mac_params)
	DEBUGFS_ADD(xtal_itrim_val, phy_mac_params)
	DEBUGFS_ADD(io_configuration, phy_mac_params)
	DEBUGFS_ADD(sdio_configuration, phy_mac_params)
	DEBUGFS_ADD(settings, phy_mac_params)
	DEBUGFS_ADD(enable_clpc, phy_mac_params)
	DEBUGFS_ADD(enable_tx_low_pwr_on_siso_rdl, phy_mac_params)
	DEBUGFS_ADD(rx_profile, phy_mac_params)
	DEBUGFS_ADD(pwr_limit_reference_11_abg, phy_mac_params)
	DEBUGFS_ADD(pwr_limit_reference_11p, phy_mac_params)
	DEBUGFS_ADD(per_chan_pwr_limit_arr_11abg, phy_mac_params)
	DEBUGFS_ADD(per_chan_pwr_limit_arr_11p, phy_mac_params)
	DEBUGFS_ADD(per_sub_band_tx_trace_loss, phy_mac_params)
	DEBUGFS_ADD(per_sub_band_rx_trace_loss, phy_mac_params)

	return 0;

err:
	if (IS_ERR(entry))
		ret = PTR_ERR(entry);
	else
		ret = -ENOMEM;

	return ret;
}

void wl1271_debugfs_reset(struct wl1271 *wl)
{
	if (!wl->stats.fw_stats)
		return;

	memset(wl->stats.fw_stats, 0, sizeof(*wl->stats.fw_stats));
	wl->stats.retry_count = 0;
	wl->stats.excessive_retries = 0;
}

int wl1271_debugfs_init(struct wl1271 *wl)
{
	int ret;
	struct dentry *rootdir;

	rootdir = debugfs_create_dir(KBUILD_MODNAME,
				     wl->hw->wiphy->debugfsdir);

	if (IS_ERR(rootdir)) {
		ret = PTR_ERR(rootdir);
		goto err;
	}

	wl->stats.fw_stats = kzalloc(sizeof(*wl->stats.fw_stats),
				      GFP_KERNEL);

	if (!wl->stats.fw_stats) {
		ret = -ENOMEM;
		goto err_fw;
	}

	wl->stats.fw_stats_update = jiffies;

	ret = wl1271_debugfs_add_files(wl, rootdir);

	if (ret < 0)
		goto err_file;

	return 0;

err_file:
	kfree(wl->stats.fw_stats);
	wl->stats.fw_stats = NULL;

err_fw:
	debugfs_remove_recursive(rootdir);

err:
	return ret;
}

void wl1271_debugfs_exit(struct wl1271 *wl)
{
	kfree(wl->stats.fw_stats);
	wl->stats.fw_stats = NULL;
}
