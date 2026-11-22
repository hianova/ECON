/* SPDX-License-Identifier: MIT */
#ifndef ECON_ADVERSE_H
#define ECON_ADVERSE_H

#include "econ_cert.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int n_types;
    double take_rate;
    double weights[ECON_CERT_MAX_TYPES];
    double values[ECON_CERT_MAX_TYPES];
    double costs[ECON_CERT_MAX_TYPES];
    int has_screening;
    double qH;
    double qL;
    double cH;
    double cL;
    double PH;
    double PL;
    double BH;
    double BL;
} econ_adverse_spec_t;

typedef struct {
    int n_types;
    int k_eq;
    int unravel_rounds;
    double take_rate;
    double critical_r_star;
    double safety_margin_r;
    double surviving_mass_frac;
    double eq_pool_price;
    double platform_revenue;
    double total_welfare;
    int full_collapse;
    int screening_valid;
    double screened_r_star;
} econ_adverse_res_t;

int econ_parse_adverse_spec(const char *path, econ_adverse_spec_t *out);

int econ_adverse_audit(const econ_adverse_spec_t *spec, double take_rate_override,
                       const char *input_path, econ_adverse_res_t *res,
                       econ_cert_record_t *cert_out);

/* Fast C API for Python ctypes take-rate death-spiral sweep */
int econ_fast_adverse_eval(const char *path, double take_rate, double *out6);

#ifdef __cplusplus
}
#endif

#endif /* ECON_ADVERSE_H */
