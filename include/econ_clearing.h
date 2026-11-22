/* SPDX-License-Identifier: MIT */
#ifndef ECON_CLEARING_H
#define ECON_CLEARING_H

#include "econ_cert.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int n_banks;
    double alpha_fire;
    double beta_default;
    double shock_budget;
    double ext_assets[ECON_CERT_MAX_BANKS];
    double L[ECON_CERT_MAX_BANKS * ECON_CERT_MAX_BANKS]; /* L[i*N + j] = bank i owes j */
} econ_clearing_net_t;

typedef struct {
    int n_banks;
    double total_obligations;
    double total_paid_upper;
    double total_paid_lower;
    double multi_eq_gap;
    int defaults_upper;
    int defaults_lower;
    int cascade_rounds;
    double systemic_shortfall;
    double min_bailout_total;
    double adv_shock_shortfall;
    double adv_shock_amp;
    double p_upper[ECON_CERT_MAX_BANKS];
    double p_lower[ECON_CERT_MAX_BANKS];
    double bailout_z[ECON_CERT_MAX_BANKS];
    double adv_shock[ECON_CERT_MAX_BANKS];
} econ_clearing_res_t;

int econ_parse_clearing_net(const char *path, econ_clearing_net_t *out);

int econ_clear_net(const econ_clearing_net_t *net, double fire_override,
                   int run_searu_attack, const char *input_path,
                   econ_clearing_res_t *res, econ_cert_record_t *cert_out);

/* Persistent in-memory ctypes session API for zero-spawn fast parameter sweeps */
void *econ_clear_session_create(const char *net_json_path);
void  econ_clear_session_destroy(void *sess);
int   econ_clear_session_sweep(void *sess, double alpha_fire, double beta_def,
                               double shock_scale, double *out_metrics6);

#ifdef __cplusplus
}
#endif

#endif /* ECON_CLEARING_H */
