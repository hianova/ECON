/* SPDX-License-Identifier: MIT */
#ifndef ECON_AGENCY_H
#define ECON_AGENCY_H

#include "econ_cert.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int n_states;
    double alpha;      /* upside profit share */
    double beta;       /* downside clawback / escrow rate */
    double strike_K;   /* KPI bonus threshold K */
    double clawback_D; /* downside penalty trigger D */
    double c_safe;     /* effort / governance cost of safe strategy */
    double c_risk;     /* effort / cost of risky tail-seeking strategy */
    double u_reserve;  /* system capital reserve threshold for ruin check */
    double returns[ECON_CERT_MAX_STATES];
    double p_safe[ECON_CERT_MAX_STATES];
    double p_risk[ECON_CERT_MAX_STATES];
} econ_agency_spec_t;

typedef struct {
    int n_states;
    double alpha;
    double beta;
    double u_safe;
    double u_risk;
    double ic_margin;
    double sys_ev_safe;
    double sys_ev_risk;
    double sys_ev_gap;
    double beta_star;
    int ic_satisfied;
    double cantelli_ruin_safe;
    double cantelli_ruin_risk;
    double cantelli_amp_ratio;
    double mu_loss_safe;
    double std_loss_safe;
    double mu_loss_risk;
    double std_loss_risk;
} econ_agency_res_t;

int econ_parse_agency_spec(const char *path, econ_agency_spec_t *out);

int econ_agency_audit(const econ_agency_spec_t *spec, double beta_override,
                      const char *input_path, econ_agency_res_t *res,
                      econ_cert_record_t *cert_out);

/* Fast C API for Python ctypes 2D phase sweep */
int econ_fast_agency_eval(const char *path, double alpha, double beta, double *out6);

#ifdef __cplusplus
}
#endif

#endif /* ECON_AGENCY_H */
