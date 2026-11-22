/* SPDX-License-Identifier: MIT */
#ifndef ECON_FIN_H
#define ECON_FIN_H

#include "econ_cert.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECON_MAX_STRIKES 64

typedef enum {
    ECON_PAYOFF_FWD_START = 0,
    ECON_PAYOFF_VAR_SWAP  = 1,
    ECON_PAYOFF_LOOKBACK  = 2
} econ_payoff_t;

typedef struct {
    double strike;
    double c_bid;
    double c_ask;
    double p_bid;
    double p_ask;
    double fut_bid;
    double fut_ask;
} econ_option_quote_t;

typedef struct {
    int n_strikes;
    double spot;
    double r_rate;
    double ttm;
    double tax_div;
    double fee;
    double fut_bid;
    double fut_ask;
    econ_option_quote_t quotes[ECON_MAX_STRIKES];
} econ_chain_t;

typedef struct {
    int n_strikes;
    int arb_found;
    int best_mode; /* 0=NO_ARB, 1=CONV, 2=REV, 3=BUTTERFLY */
    double best_edge;
    double best_k1;
    double best_k2;
    double best_k3;
    int sniper_flags;
    int sniper_false_pos;
    int sniper_false_neg;
    int econ_conv_count;
    int econ_rev_count;
    int econ_vertical_count;
    int econ_butterfly_count;
    double max_sniper_mid_abs;
} econ_arb_res_t;

typedef struct {
    int n1;
    int n2;
    double s0;
    double fwd;
    double t1;
    double t2;
    double iv_atm;
    double s1[ECON_CERT_MAX_GRID];
    double q1[ECON_CERT_MAX_GRID];
    double s2[ECON_CERT_MAX_GRID];
    double q2[ECON_CERT_MAX_GRID];
} econ_mot_market_t;

typedef struct {
    double frechet_lb;
    double mot_lb;
    double sos_lb;
    double bs_price;
    double sos_ub;
    double mot_ub;
    double frechet_ub;
    double spread_mot;
    double spread_frechet;
    double tightening_ratio;
    int certified;
} econ_mot_res_t;

int econ_parse_option_chain(const char *path, econ_chain_t *out);
int econ_parse_mot_market(const char *path, econ_mot_market_t *out);

int econ_arb_scan(const econ_chain_t *ch, const char *input_path,
                  econ_arb_res_t *res, econ_cert_record_t *cert_out);

int econ_mot_bound(const econ_mot_market_t *mkt, econ_payoff_t payoff_kind,
                   double param, const char *input_path,
                   econ_mot_res_t *res, econ_cert_record_t *cert_out);

#ifdef __cplusplus
}
#endif

#endif /* ECON_FIN_H */
