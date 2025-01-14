/*-
 * Copyright (c) 2007-2015 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efsys.h"
#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_impl.h"

#if EFSYS_OPT_MON_NULL
#include "nullmon.h"
#endif

#if EFSYS_OPT_MON_LM87
#include "lm87.h"
#endif

#if EFSYS_OPT_MON_MAX6647
#include "max6647.h"
#endif

#if EFSYS_OPT_MON_MCDI
#include "mcdi_mon.h"
#endif

#if EFSYS_OPT_NAMES

static const char	*__efx_mon_name[] = {
	"",
	"nullmon",
	"lm87",
	"max6647",
	"sfx90x0",
	"sfx91x0"
};

		const char *
efx_mon_name(
	__in	efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	EFSYS_ASSERT(encp->enc_mon_type != EFX_MON_INVALID);
	EFSYS_ASSERT3U(encp->enc_mon_type, <, EFX_MON_NTYPES);
	return (__efx_mon_name[encp->enc_mon_type]);
}

#endif	/* EFSYS_OPT_NAMES */

#if EFSYS_OPT_MON_NULL
static efx_mon_ops_t	__efx_mon_null_ops = {
	nullmon_reset,			/* emo_reset */
	nullmon_reconfigure,		/* emo_reconfigure */
#if EFSYS_OPT_MON_STATS
	nullmon_stats_update		/* emo_stats_update */
#endif	/* EFSYS_OPT_MON_STATS */
};
#endif

#if EFSYS_OPT_MON_LM87
static efx_mon_ops_t	__efx_mon_lm87_ops = {
	lm87_reset,			/* emo_reset */
	lm87_reconfigure,		/* emo_reconfigure */
#if EFSYS_OPT_MON_STATS
	lm87_stats_update		/* emo_stats_update */
#endif	/* EFSYS_OPT_MON_STATS */
};
#endif

#if EFSYS_OPT_MON_MAX6647
static efx_mon_ops_t	__efx_mon_max6647_ops = {
	max6647_reset,			/* emo_reset */
	max6647_reconfigure,		/* emo_reconfigure */
#if EFSYS_OPT_MON_STATS
	max6647_stats_update		/* emo_stats_update */
#endif	/* EFSYS_OPT_MON_STATS */
};
#endif

#if EFSYS_OPT_MON_MCDI
static efx_mon_ops_t	__efx_mon_mcdi_ops = {
	NULL,				/* emo_reset */
	NULL,				/* emo_reconfigure */
#if EFSYS_OPT_MON_STATS
	mcdi_mon_stats_update		/* emo_stats_update */
#endif	/* EFSYS_OPT_MON_STATS */
};
#endif

static efx_mon_ops_t	*__efx_mon_ops[] = {
	NULL,
#if EFSYS_OPT_MON_NULL
	&__efx_mon_null_ops,
#else
	NULL,
#endif
#if EFSYS_OPT_MON_LM87
	&__efx_mon_lm87_ops,
#else
	NULL,
#endif
#if EFSYS_OPT_MON_MAX6647
	&__efx_mon_max6647_ops,
#else
	NULL,
#endif
#if EFSYS_OPT_MON_MCDI
	&__efx_mon_mcdi_ops,
#else
	NULL,
#endif
#if EFSYS_OPT_MON_MCDI
	&__efx_mon_mcdi_ops
#else
	NULL
#endif
};

	__checkReturn	int
efx_mon_init(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mon_t *emp = &(enp->en_mon);
	efx_mon_ops_t *emop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	if (enp->en_mod_flags & EFX_MOD_MON) {
		rc = EINVAL;
		goto fail1;
	}

	enp->en_mod_flags |= EFX_MOD_MON;

	emp->em_type = encp->enc_mon_type;

	EFSYS_ASSERT(encp->enc_mon_type != EFX_MON_INVALID);
	EFSYS_ASSERT3U(emp->em_type, <, EFX_MON_NTYPES);
	if ((emop = (efx_mon_ops_t *)__efx_mon_ops[emp->em_type]) == NULL) {
		rc = ENOTSUP;
		goto fail2;
	}

	if (emop->emo_reset != NULL) {
		if ((rc = emop->emo_reset(enp)) != 0)
			goto fail3;
	}

	if (emop->emo_reconfigure != NULL) {
		if ((rc = emop->emo_reconfigure(enp)) != 0)
			goto fail4;
	}

	emp->em_emop = emop;
	return (0);

fail4:
	EFSYS_PROBE(fail5);

	if (emop->emo_reset != NULL)
		(void) emop->emo_reset(enp);

fail3:
	EFSYS_PROBE(fail4);
fail2:
	EFSYS_PROBE(fail3);

	emp->em_type = EFX_MON_INVALID;

	enp->en_mod_flags &= ~EFX_MOD_MON;

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_MON_STATS

#if EFSYS_OPT_NAMES

/* START MKCONFIG GENERATED MonitorStatNamesBlock b9328f15438c4d01 */
static const char 	*__mon_stat_name[] = {
	"value_2_5v",
	"value_vccp1",
	"value_vcc",
	"value_5v",
	"value_12v",
	"value_vccp2",
	"value_ext_temp",
	"value_int_temp",
	"value_ain1",
	"value_ain2",
	"controller_cooling",
	"ext_cooling",
	"1v",
	"1_2v",
	"1_8v",
	"3_3v",
	"1_2va",
	"vref",
	"vaoe",
	"aoe_temperature",
	"psu_aoe_temperature",
	"psu_temperature",
	"fan0",
	"fan1",
	"fan2",
	"fan3",
	"fan4",
	"vaoe_in",
	"iaoe",
	"iaoe_in",
	"nic_power",
	"0_9v",
	"i0_9v",
	"i1_2v",
	"0_9v_adc",
	"controller_temperature2",
	"vreg_temperature",
	"vreg_0_9v_temperature",
	"vreg_1_2v_temperature",
	"int_vptat",
	"controller_internal_adc_temperature",
	"ext_vptat",
	"controller_external_adc_temperature",
	"ambient_temperature",
	"airflow",
	"vdd08d_vss08d_csr",
	"vdd08d_vss08d_csr_extadc",
	"hotpoint_temperature",
	"phy_power_switch_port0",
	"phy_power_switch_port1",
	"mum_vcc",
	"0v9_a",
	"i0v9_a",
	"0v9_a_temp",
	"0v9_b",
	"i0v9_b",
	"0v9_b_temp",
	"ccom_avreg_1v2_supply",
	"ccom_avreg_1v2_supply_ext_adc",
	"ccom_avreg_1v8_supply",
	"ccom_avreg_1v8_supply_ext_adc",
	"controller_master_vptat",
	"controller_master_internal_temp",
	"controller_master_vptat_ext_adc",
	"controller_master_internal_temp_ext_adc",
	"controller_slave_vptat",
	"controller_slave_internal_temp",
	"controller_slave_vptat_ext_adc",
	"controller_slave_internal_temp_ext_adc",
};

/* END MKCONFIG GENERATED MonitorStatNamesBlock */

extern					const char *
efx_mon_stat_name(
	__in				efx_nic_t *enp,
	__in				efx_mon_stat_t id)
{
	_NOTE(ARGUNUSED(enp))
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	EFSYS_ASSERT3U(id, <, EFX_MON_NSTATS);
	return (__mon_stat_name[id]);
}

#endif	/* EFSYS_OPT_NAMES */

	__checkReturn			int
efx_mon_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_MON_NSTATS)	efx_mon_stat_value_t *values)
{
	efx_mon_t *emp = &(enp->en_mon);
	efx_mon_ops_t *emop = emp->em_emop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MON);

	return (emop->emo_stats_update(enp, esmp, values));
}

#endif	/* EFSYS_OPT_MON_STATS */

		void
efx_mon_fini(
	__in	efx_nic_t *enp)
{
	efx_mon_t *emp = &(enp->en_mon);
	efx_mon_ops_t *emop = emp->em_emop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MON);

	emp->em_emop = NULL;

	if (emop->emo_reset != NULL) {
		rc = emop->emo_reset(enp);
		if (rc != 0)
			EFSYS_PROBE1(fail1, int, rc);
	}

	emp->em_type = EFX_MON_INVALID;

	enp->en_mod_flags &= ~EFX_MOD_MON;
}
