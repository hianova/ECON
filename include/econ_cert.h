/* SPDX-License-Identifier: MIT */
#ifndef ECON_CERT_H
#define ECON_CERT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ECON_VERSION_STRING "1.0.0"
#define ECON_CERT_MAX_DIM   32
#define ECON_CERT_MAX_GRID  16
#define ECON_CERT_MAX_BANKS 32
#define ECON_CERT_MAX_OUT   16
#define ECON_CERT_PATH_MAX  512

typedef enum {
    ECON_LP_LE = 0,
    ECON_LP_GE = 1,
    ECON_LP_EQ = 2
} econ_lp_sense_t;

typedef struct {
    int n_vars;
    int n_cons;
    int status; /* 0 = optimal, 1 = infeasible, 2 = unbounded, -1 = error */
    double obj_val;
    double *x;  /* length n_vars */
    double *y;  /* length n_cons (dual multipliers) */
} econ_lp_sol_t;

typedef struct {
    int dim;
    int64_t scale_q;
    int64_t mu_psd;
    int64_t delta_round;
    int64_t mu_cert;
    int ok;
    int64_t A[ECON_CERT_MAX_DIM * ECON_CERT_MAX_DIM];
    int64_t L[ECON_CERT_MAX_DIM * ECON_CERT_MAX_DIM];
} econ_psd_cert_t;

typedef struct {
    char pillar[16];     /* "fin", "actuary", "clearing", "mech" */
    char kind[32];       /* "arb_scan", "mot_sos", "risk_dro", "clearing", "vcg", "ms_deficit", "fisher" */
    char status[16];     /* "CERTIFIED", "ARBITRAGE", "OBSTRUCTION", "VETOED" */
    char input_path[ECON_CERT_PATH_MAX];
    char input_sha256[65];

    /* Optional Cholesky+SDD matrix witness (SOS-2 / Covariance / Moment PSD) */
    int has_psd;
    econ_psd_cert_t psd;

    /* Optional 1D integer interval / bracket witness */
    int has_bracket;
    int64_t bracket_scale;
    int64_t lb_int;
    int64_t ub_int;
    int64_t ref_int;
    int has_ref;
    int64_t limit_lo_int;
    int64_t limit_hi_int;
    double lb_val;
    double ub_val;

    /* Pillar 2A: Model-free Bid-Ask + Fee Arbitrage witness */
    int has_arb;
    int arb_mode; /* 0=NO_ARB, 1=CONV, 2=REV, 3=BUTTERFLY */
    int64_t arb_scale;
    int64_t k1;
    int64_t k2;
    int64_t k3;
    int64_t c_bid1;
    int64_t c_ask1;
    int64_t p_bid1;
    int64_t p_ask1;
    int64_t f_bid;
    int64_t f_ask;
    int64_t c_bid2;
    int64_t c_ask3;
    int64_t arb_cost_int;
    int64_t arb_edge_int;
    int64_t sniper_mid_res_int;

    /* Pillar 2B: 2-Period Martingale Optimal Transport Super/Sub-Hedge Dual */
    int has_mot;
    int mot_n1;
    int mot_n2;
    int64_t mot_scale;
    int64_t s1[ECON_CERT_MAX_GRID];
    int64_t s2[ECON_CERT_MAX_GRID];
    int64_t q1[ECON_CERT_MAX_GRID];
    int64_t q2[ECON_CERT_MAX_GRID];
    int64_t phi1_up[ECON_CERT_MAX_GRID];
    int64_t phi2_up[ECON_CERT_MAX_GRID];
    int64_t delta_up[ECON_CERT_MAX_GRID];
    int64_t phi1_lo[ECON_CERT_MAX_GRID];
    int64_t phi2_lo[ECON_CERT_MAX_GRID];
    int64_t delta_lo[ECON_CERT_MAX_GRID];
    int64_t payoff[ECON_CERT_MAX_GRID * ECON_CERT_MAX_GRID];
    int64_t mot_frechet_lb_int;
    int64_t mot_lb_int;
    int64_t mot_ub_int;
    int64_t mot_frechet_ub_int;
    int64_t mot_bs_int;

    /* Pillar 3: Copula-Free Actuarial Risk & Cantelli Ruin DRO */
    int has_dro;
    int64_t dro_scale;
    int64_t gauss_cvar_int;
    int64_t dro_cvar_int;
    int64_t amp_bps;
    int has_cantelli;
    int64_t cantelli_W;
    int64_t cantelli_delta;
    int64_t cantelli_S2;
    int64_t cantelli_ptail;

    /* Pillar 4: Eisenberg-Noe Tarski Lattice Bracket & Bailout Sufficiency */
    int has_clearing;
    int clear_n_banks;
    int64_t clear_scale;
    int64_t clear_p_bar[ECON_CERT_MAX_BANKS];
    int64_t clear_ext[ECON_CERT_MAX_BANKS];
    int64_t clear_p_lo[ECON_CERT_MAX_BANKS];
    int64_t clear_p_hi[ECON_CERT_MAX_BANKS];
    int64_t clear_z[ECON_CERT_MAX_BANKS];
    int64_t clear_pi_scaled[ECON_CERT_MAX_BANKS * ECON_CERT_MAX_BANKS];
    int clear_has_bailout;

    /* Pillar 1: Mechanism Design (1=VCG, 2=MS_DEFICIT, 3=FISHER) */
    int has_mech;
    int mech_mode;
    int mech_n_agents;
    int mech_n_outcomes;
    int mech_chosen;
    int64_t mech_scale;
    int64_t mech_vals[ECON_CERT_MAX_GRID * ECON_CERT_MAX_OUT];
    int64_t mech_pivots[ECON_CERT_MAX_GRID];
    int64_t mech_welfare_int;
    int64_t mech_revenue_int;
    int mech_ms_K;
    int64_t mech_ms_surplus_int;
    int64_t mech_fisher_p[ECON_CERT_MAX_GRID];
    int64_t mech_fisher_e[ECON_CERT_MAX_GRID];
    int64_t mech_fisher_w[ECON_CERT_MAX_GRID];
    int64_t mech_fisher_x[ECON_CERT_MAX_GRID * ECON_CERT_MAX_GRID];
    int64_t mech_fisher_tol;
} econ_cert_record_t;

/* FIPS-180-4 SHA-256 */
void econ_sha256_bytes(const uint8_t *data, size_t len, char hex_out[65]);
int econ_sha256_file(const char *path, char hex_out[65]);

/* Portable dense linear algebra & Two-Phase Simplex LP solver */
int econ_lu_solve_real(int n, const double *A, const double *b, double *x);
int econ_sym_jacobi_eigs(int n, const double *A, double *evals, double *evecs);
int econ_lp_solve(int n_vars, int n_cons, const double *c, const double *A,
                  const econ_lp_sense_t *sense, const double *b,
                  double *x_out, double *y_out, double *obj_out);

/* Zero-FP-Trust 128-bit integer Cholesky + SDD certifier & independent verifier */
int econ_cert_cholesky_sdd_i64(int n, const int64_t *A_sym,
                               int64_t *L_out, int64_t *mu_psd_out);
int econ_verify_cholesky_sdd_i64(int n, const int64_t *A_sym,
                                 const int64_t *L_tri, int64_t mu_psd);

/* Canonical .econcert.json writer, reader & independent zero-FP-trust integer verifier */
int econ_cert_write_json(const char *out_path, const econ_cert_record_t *rec);
int econ_cert_verify_file(const char *cert_path, int verify_input_hash);

/* Standalone Lean 4 module emitter (import EconCert; 0 sorry, 0 native_decide) */
int econ_cert_emit_lean(const char *out_path, const char *mod_name,
                        const econ_cert_record_t *rec);

#ifdef __cplusplus
}
#endif

#endif /* ECON_CERT_H */
