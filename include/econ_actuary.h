/* SPDX-License-Identifier: MIT */
#ifndef ECON_ACTUARY_H
#define ECON_ACTUARY_H

#include "econ_cert.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECON_RISK_MAX_ASSETS 12
#define ECON_RISK_MAX_BINS   64

typedef struct {
    int n_assets;
    int n_bins;
    double alpha; /* e.g. 0.99 */
    double pareto_tail;
    double corr[ECON_RISK_MAX_ASSETS * ECON_RISK_MAX_ASSETS];
    double samples[ECON_RISK_MAX_ASSETS * ECON_RISK_MAX_BINS]; /* sorted ascending marginal samples */
    double ruin_threshold;
} econ_risk_table_t;

typedef struct {
    int n_assets;
    double alpha;
    double gauss_var;
    double gauss_cvar;
    double ra_var_ub;
    double comonotonic_cvar_ub;
    double moment_dro_cvar_ub;
    double amp_copula_vs_gauss;
    double amp_moment_vs_gauss;
    int corr_psd_ok;
} econ_dro_res_t;

typedef struct {
    double mu_loss;
    double sigma_loss;
    double threshold_u;
    double gauss_ruin_prob;
    double cantelli_ruin_ub;
    double ruin_ratio_ub_over_gauss;
    double min_capital_gauss_995;
    double min_capital_cantelli_995;
} econ_ruin_res_t;

int econ_parse_risk_table(const char *path, econ_risk_table_t *out);

int econ_risk_dro(const econ_risk_table_t *tbl, const char *input_path,
                  econ_dro_res_t *res, econ_cert_record_t *cert_out);

int econ_ruin_bound(const econ_risk_table_t *tbl, double target_ruin,
                    const char *input_path,
                    econ_ruin_res_t *res, econ_cert_record_t *cert_out);

#ifdef __cplusplus
}
#endif

#endif /* ECON_ACTUARY_H */
