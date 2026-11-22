/* SPDX-License-Identifier: MIT */
#ifndef ECON_INST_H
#define ECON_INST_H

#include "econ_cert.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char institution_name[64];
    char institution_type[32]; /* "fund_agency", "platform_adverse", "exchange_margin", "supply_chain" */
    double target_margin_bps;
    double c_inc;
    double c_dro;
    double c_sys;

    /* Component refs or parameters */
    char inc_ref_path[ECON_CERT_PATH_MAX];
    char dro_ref_path[ECON_CERT_PATH_MAX];
    char sys_ref_path[ECON_CERT_PATH_MAX];

    /* Optional inline overrides for DRC parameter tweaks / repairs */
    double beta_override;     /* >=0 overrides agency clawback beta */
    double take_override;     /* >=0 overrides platform take rate r */
    double dro_reserve_mult;  /* >0 scales tail reserve / capital */
    double theta_override;    /* >=0 overrides exchange theta_mm */
    double tau_override;      /* >0 overrides supply-chain tau */
    double bailout_buffer;    /* >=0 adds direct capital cushion across systemic nodes */
    double chain_fee_bps;     /* for exchange taker fee headroom or supply discount */
} econ_inst_spec_t;

typedef struct {
    char institution_name[64];
    char institution_type[32];
    double m_inc_bps;
    double m_dro_bps;
    double m_sys_bps;
    double master_margin_bps;
    double target_margin_bps;
    double d_inc_bps;
    double d_dro_bps;
    double d_sys_bps;
    double min_repair_cost;
    int certified;
    char verdict[16];     /* "CERTIFIED" or "VETOED" */
    char weakest_dim[16]; /* "INC", "DRO", "SYS", or "BALANCED" */
    char exploit_note[160];
} econ_inst_res_t;

int econ_parse_inst_spec(const char *path, econ_inst_spec_t *out);

int econ_audit_institution(const econ_inst_spec_t *spec, const char *input_path,
                           econ_inst_res_t *res, econ_cert_record_t *cert_out);

#ifdef __cplusplus
}
#endif

#endif /* ECON_INST_H */
