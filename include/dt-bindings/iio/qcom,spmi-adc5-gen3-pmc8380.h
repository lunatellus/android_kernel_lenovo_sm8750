/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DT_BINDINGS_QCOM_SPMI_VADC_PMC8380_H
#define _DT_BINDINGS_QCOM_SPMI_VADC_PMC8380_H

#include <dt-bindings/iio/qcom,spmi-vadc.h>

/* ADC channels for PMC8380_ADC for PMIC5 Gen3 */
#define PMC8380_ADC5_GEN3_OFFSET_REF(sid)		((sid) << 8 | ADC5_GEN3_OFFSET_REF)
#define PMC8380_ADC5_GEN3_1P25VREF(sid)		((sid) << 8 | ADC5_GEN3_1P25VREF)
#define PMC8380_ADC5_GEN3_VREF_VADC(sid)		((sid) << 8 | ADC5_GEN3_VREF_VADC)
#define PMC8380_ADC5_GEN3_DIE_TEMP(sid)		((sid) << 8 | ADC5_GEN3_DIE_TEMP)

#endif /* _DT_BINDINGS_QCOM_SPMI_VADC_PMC8380_H */
