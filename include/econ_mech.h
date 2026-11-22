/* SPDX-License-Identifier: MIT */
#ifndef ECON_MECH_H
#define ECON_MECH_H

#include "econ_cert.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int is_combinatorial;
    int n_agents;
    int n_items;
    int n_outcomes;
    double reserve_price;
    /* If is_combinatorial: vals[i * 16 + bundle_mask]; else: vals[i * ECON_CERT_MAX_OUT + outcome] */
    double vals[ECON_CERT_MAX_GRID * ECON_CERT_MAX_OUT];
} econ_auction_t;

typedef struct {
    int n_agents;
    int n_outcomes;
    int chosen_outcome;
    int dsic_verified;
    int ir_verified;
    double total_welfare;
    double vcg_revenue;
    double myerson_reserve_rev;
    double payments[ECON_CERT_MAX_GRID];
    double utilities[ECON_CERT_MAX_GRID];
} econ_vcg_res_t;

typedef struct {
    int K;
    double efficient_welfare;
    double min_rent_required;
    double max_budget_surplus;
    double norm_surplus_ratio;  /* max_budget_surplus / (K*(K+1)^2) -> -1/6 */
    double theoretical_formula; /* (4 - K) / (6*(K+1)) */
    double second_best_welfare;
    double second_best_eff_pct; /* -> 27/32 = 84.375% as K -> inf */
    int impossibility_holds;
} econ_ms_res_t;

typedef struct {
    int n_agents;
    int n_goods;
    double budgets[ECON_CERT_MAX_GRID];
    double endowments[ECON_CERT_MAX_GRID];
    double utils[ECON_CERT_MAX_GRID * ECON_CERT_MAX_GRID];
} econ_fisher_mkt_t;

typedef struct {
    int n_agents;
    int n_goods;
    double prices[ECON_CERT_MAX_GRID];
    double allocs[ECON_CERT_MAX_GRID * ECON_CERT_MAX_GRID];
    double max_clearing_err;
    double walras_residual;
    double max_kkt_gap;
    double social_welfare_eg;
    int certified;
} econ_fisher_res_t;

int econ_parse_auction(const char *path, econ_auction_t *out);
int econ_parse_fisher(const char *path, econ_fisher_mkt_t *out);

int econ_mech_verify(const econ_auction_t *auc, const char *input_path,
                     econ_vcg_res_t *res, econ_cert_record_t *cert_out);

int econ_bilateral_ms(int K, int solve_second_best, const char *input_path,
                      econ_ms_res_t *res, econ_cert_record_t *cert_out);

int econ_fisher_eq(const econ_fisher_mkt_t *mkt, const char *input_path,
                   econ_fisher_res_t *res, econ_cert_record_t *cert_out);

int econ_fast_ms_eval(int K, int solve_second_best, double *out_metrics5);

#ifdef __cplusplus
}
#endif

#endif /* ECON_MECH_H */
