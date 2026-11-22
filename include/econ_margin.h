/* SPDX-License-Identifier: MIT */
#ifndef ECON_MARGIN_H
#define ECON_MARGIN_H

#include "econ_clearing.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int n_accts;
    double P0;
    double Pmin;
    double alpha_impact;
    double theta_mm;
    double shock_pct;
    double equity_target;
    double h[ECON_CERT_MAX_ACCTS];
    double d[ECON_CERT_MAX_ACCTS];
    double c[ECON_CERT_MAX_ACCTS];
} econ_margin_mkt_t;

typedef struct {
    int n_accts;
    double theta_mm;
    double alpha_impact;
    double P_start;
    double P_eq;
    double total_debt;
    double liquidated_units;
    int liquidated_count;
    int cascade_rounds;
    double bad_debt;
    double micro_buffer_pct;
    double macro_safety_margin;
    double theta_opt;
    double opt_safety_margin;
    int paradox_detected;
    int triggered[ECON_CERT_MAX_ACCTS];
} econ_margin_res_t;

typedef struct {
    int n_firms;
    int core_idx;
    double tau_horizon;
    double alpha_fire;
    double beta_default;
    double shock_budget;
    double ext_assets[ECON_CERT_MAX_BANKS];
    double L_base[ECON_CERT_MAX_BANKS * ECON_CERT_MAX_BANKS];
} econ_supply_chain_t;

typedef struct {
    int n_firms;
    double tau_horizon;
    double total_payables;
    double core_wc_apparent_gain;
    double shock_horizon_margin;
    double uniform_shortfall;
    double searu_shortfall;
    double searu_amp;
    double min_bailout_total;
    int defaults_searu;
    double systemic_safety_margin;
} econ_supply_res_t;

int econ_parse_margin_market(const char *path, econ_margin_mkt_t *out);

int econ_margin_audit(const econ_margin_mkt_t *mkt, double theta_override,
                      double alpha_override, const char *input_path,
                      econ_margin_res_t *res, econ_cert_record_t *cert_out);

int econ_parse_supply_chain(const char *path, econ_supply_chain_t *out);

int econ_supply_chain_audit(const econ_supply_chain_t *sc, double tau_override,
                            int run_searu, const char *input_path,
                            econ_supply_res_t *res, econ_cert_record_t *cert_out);

/* Fast C APIs for Python ctypes sweeps (exp7) */
int econ_fast_margin_eval(const char *path, double theta_mm, double alpha_impact,
                          double shock_pct, double *out6);
int econ_fast_supply_eval(const char *path, double tau, double *out6);

#ifdef __cplusplus
}
#endif

#endif /* ECON_MARGIN_H */
