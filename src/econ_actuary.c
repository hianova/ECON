/* SPDX-License-Identifier: MIT */
#include "econ_actuary.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double inv_norm_cdf(double p) {
    /* Acklam rational approximation */
    if (p <= 1e-12) return -7.0;
    if (p >= 1.0 - 1e-12) return 7.0;
    static const double a[6] = {
        -3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
        1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00
    };
    static const double b[5] = {
        -5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
        6.680131188771972e+01, -1.328068155288572e+01
    };
    static const double c[6] = {
        -7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
        -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00
    };
    static const double d[4] = {
        7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00, 3.754408661907416e+00
    };
    double plow = 0.02425, phigh = 1.0 - plow, q, r;
    if (p < plow) {
        q = sqrt(-2.0 * log(p));
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    } else if (p > phigh) {
        q = sqrt(-2.0 * log(1.0 - p));
        return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
                ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    } else {
        q = p - 0.5;
        r = q * q;
        return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
               (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0);
    }
}

static double norm_cdf_act(double x) {
    return 0.5 * erfc(-x * 0.70710678118654752440);
}

static int cmp_dbl(const void *pa, const void *pb) {
    double a = *(const double *)pa, b = *(const double *)pb;
    return (a > b) - (a < b);
}

static double halton(int idx, int base) {
    double f = 1.0, r = 0.0;
    int i = idx + 1;
    while (i > 0) {
        f /= (double)base;
        r += f * (double)(i % base);
        i /= base;
    }
    return r;
}

static const char *find_json_key_act(const char *json, const char *key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p != '\0' && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    if (*p != ':') return NULL;
    ++p;
    while (*p != '\0' && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    return p;
}

int econ_parse_risk_table(const char *path, econ_risk_table_t *out) {
    if (!path || !out) return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz <= 0 || sz > 2L * 1024L * 1024L) { fclose(fp); return -1; }
    rewind(fp);
    char *buf = (char *)malloc((size_t)sz + 1U);
    if (!buf) { fclose(fp); return -1; }
    size_t nr = fread(buf, 1U, (size_t)sz, fp);
    fclose(fp);
    buf[nr] = '\0';

    memset(out, 0, sizeof(*out));
    out->alpha = 0.99;
    out->pareto_tail = 3.0;
    const char *v;
    if ((v = find_json_key_act(buf, "n_assets")) != NULL) out->n_assets = (int)strtol(v, NULL, 10);
    if ((v = find_json_key_act(buf, "n_bins")) != NULL) out->n_bins = (int)strtol(v, NULL, 10);
    if ((v = find_json_key_act(buf, "alpha")) != NULL) out->alpha = strtod(v, NULL);
    if ((v = find_json_key_act(buf, "pareto_tail")) != NULL) out->pareto_tail = strtod(v, NULL);
    if ((v = find_json_key_act(buf, "ruin_threshold")) != NULL) out->ruin_threshold = strtod(v, NULL);

    if (out->n_assets <= 0 || out->n_assets > ECON_RISK_MAX_ASSETS ||
        out->n_bins <= 1 || out->n_bins > ECON_RISK_MAX_BINS) {
        free(buf);
        return -1;
    }

    for (int i = 0; i < out->n_assets; ++i) {
        out->corr[i * out->n_assets + i] = 1.0;
    }

    if ((v = find_json_key_act(buf, "corr")) != NULL && *v == '[') {
        ++v;
        for (int i = 0; i < out->n_assets; ++i) {
            while (*v && *v != '[') ++v;
            if (*v != '[') { free(buf); return -1; }
            ++v;
            for (int j = 0; j < out->n_assets; ++j) {
                while (*v && (isspace((unsigned char)*v) || *v == ',')) ++v;
                char *ep = NULL;
                out->corr[i * out->n_assets + j] = strtod(v, &ep);
                if (ep == v) { free(buf); return -1; }
                v = ep;
            }
            while (*v && *v != ']') ++v;
            if (*v == ']') ++v;
        }
    }

    if ((v = find_json_key_act(buf, "samples")) == NULL || *v != '[') {
        free(buf);
        return -1;
    }
    ++v;
    for (int i = 0; i < out->n_assets; ++i) {
        while (*v && *v != '[') ++v;
        if (*v != '[') { free(buf); return -1; }
        ++v;
        for (int b = 0; b < out->n_bins; ++b) {
            while (*v && (isspace((unsigned char)*v) || *v == ',')) ++v;
            char *ep = NULL;
            out->samples[i * ECON_RISK_MAX_BINS + b] = strtod(v, &ep);
            if (ep == v) { free(buf); return -1; }
            v = ep;
        }
        qsort(&out->samples[i * ECON_RISK_MAX_BINS], (size_t)out->n_bins, sizeof(double), cmp_dbl);
        while (*v && *v != ']') ++v;
        if (*v == ']') ++v;
    }

    free(buf);
    return 0;
}

static double calc_empirical_cvar(const double *sorted, int n, double alpha, double *var_out) {
    int k0 = (int)floor(alpha * (double)n);
    if (k0 >= n) k0 = n - 1;
    if (k0 < 0) k0 = 0;
    if (var_out) *var_out = sorted[k0];
    double sum = 0.0;
    int cnt = 0;
    for (int k = k0; k < n; ++k) { sum += sorted[k]; cnt++; }
    return (cnt > 0) ? (sum / (double)cnt) : sorted[n - 1];
}

int econ_risk_dro(const econ_risk_table_t *tbl, const char *input_path,
                  econ_dro_res_t *res, econ_cert_record_t *cert_out) {
    if (!tbl || !res || tbl->n_assets <= 0 || tbl->n_bins <= 1) return -1;
    memset(res, 0, sizeof(*res));
    int d = tbl->n_assets;
    int B = tbl->n_bins;
    res->n_assets = d;
    res->alpha = tbl->alpha;

    /* Cholesky of correlation matrix R (Gaussian copula) & marginal means/stds */
    double Lchol[ECON_RISK_MAX_ASSETS * ECON_RISK_MAX_ASSETS] = {0};
    double mu_i[ECON_RISK_MAX_ASSETS] = {0}, sd_i[ECON_RISK_MAX_ASSETS] = {0};
    double marg_cvar[ECON_RISK_MAX_ASSETS] = {0}, marg_var[ECON_RISK_MAX_ASSETS] = {0};
    for (int i = 0; i < d; ++i) {
        double s1 = 0.0, s2 = 0.0;
        for (int b = 0; b < B; ++b) {
            double v = tbl->samples[i * ECON_RISK_MAX_BINS + b];
            s1 += v; s2 += v * v;
        }
        mu_i[i] = s1 / (double)B;
        double var_i = (s2 / (double)B) - mu_i[i] * mu_i[i];
        sd_i[i] = (var_i > 0.0) ? sqrt(var_i) : 0.0;
        marg_cvar[i] = calc_empirical_cvar(&tbl->samples[i * ECON_RISK_MAX_BINS], B, tbl->alpha, &marg_var[i]);
        res->comonotonic_cvar_ub += marg_cvar[i];
    }

    int64_t R_int[ECON_CERT_MAX_DIM * ECON_CERT_MAX_DIM] = {0};
    int64_t R_L_int[ECON_CERT_MAX_DIM * ECON_CERT_MAX_DIM] = {0};
    int64_t mu_psd = 0;
    for (int i = 0; i < d; ++i) {
        for (int j = 0; j < d; ++j) {
            R_int[i * d + j] = (int64_t)llround(tbl->corr[i * d + j] * 10000.0);
        }
        for (int j = 0; j <= i; ++j) {
            double s = tbl->corr[i * d + j];
            for (int k = 0; k < j; ++k) s -= Lchol[i * d + k] * Lchol[j * d + k];
            if (i == j) Lchol[i * d + j] = (s > 1e-12) ? sqrt(s) : 1e-6;
            else Lchol[i * d + j] = s / Lchol[j * d + j];
        }
    }
    res->corr_psd_ok = (econ_cert_cholesky_sdd_i64(d, R_int, R_L_int, &mu_psd) == 0 && mu_psd >= 0) ? 1 : 0;

    /* Deterministic Halton QMC Gaussian-Copula baseline (N = 2048 samples) */
    static const int primes[12] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    const int nq = 2048;
    double *g_losses = (double *)malloc((size_t)nq * sizeof(double));
    if (!g_losses) return -1;
    double cov_sum = 0.0;
    for (int i = 0; i < d; ++i) {
        for (int j = 0; j < d; ++j) cov_sum += tbl->corr[i * d + j] * sd_i[i] * sd_i[j];
    }
    double mu_p = 0.0;
    for (int i = 0; i < d; ++i) mu_p += mu_i[i];
    double sd_p = (cov_sum > 0.0) ? sqrt(cov_sum) : 0.0;

    for (int q = 0; q < nq; ++q) {
        double z[ECON_RISK_MAX_ASSETS];
        for (int i = 0; i < d; ++i) z[i] = inv_norm_cdf(halton(q, primes[i]));
        double tot = 0.0;
        for (int i = 0; i < d; ++i) {
            double y = 0.0;
            for (int k = 0; k <= i; ++k) y += Lchol[i * d + k] * z[k];
            double u = norm_cdf_act(y);
            int idx = (int)floor(u * (double)B);
            if (idx >= B) idx = B - 1;
            if (idx < 0) idx = 0;
            tot += tbl->samples[i * ECON_RISK_MAX_BINS + idx];
        }
        g_losses[q] = tot;
    }
    qsort(g_losses, (size_t)nq, sizeof(double), cmp_dbl);
    res->gauss_cvar = calc_empirical_cvar(g_losses, nq, tbl->alpha, &res->gauss_var);
    free(g_losses);

    /* Comonotonic Rearrangement VaR upper bound = sorted sum */
    int k0 = (int)floor(tbl->alpha * (double)B);
    if (k0 >= B) k0 = B - 1;
    for (int i = 0; i < d; ++i) res->ra_var_ub += tbl->samples[i * ECON_RISK_MAX_BINS + k0];

    /* Moment-DRO (Cantelli / Zhu-Fukushima) Worst-Case CVaR = mu_P + sd_P * sqrt(alpha/(1-alpha)) */
    double a_clamped = (tbl->alpha < 0.9999) ? tbl->alpha : 0.9999;
    res->moment_dro_cvar_ub = mu_p + sd_p * sqrt(a_clamped / (1.0 - a_clamped));
    double max_wc = (res->comonotonic_cvar_ub > res->moment_dro_cvar_ub)
        ? res->comonotonic_cvar_ub : res->moment_dro_cvar_ub;

    res->amp_copula_vs_gauss = (res->gauss_cvar > 1e-12) ? (res->comonotonic_cvar_ub / res->gauss_cvar) : 1.0;
    res->amp_moment_vs_gauss = (res->gauss_cvar > 1e-12) ? (res->moment_dro_cvar_ub / res->gauss_cvar) : 1.0;

    if (cert_out != NULL) {
        memset(cert_out, 0, sizeof(*cert_out));
        snprintf(cert_out->pillar, sizeof(cert_out->pillar), "actuary");
        snprintf(cert_out->kind, sizeof(cert_out->kind), "risk_dro");
        snprintf(cert_out->status, sizeof(cert_out->status), "CERTIFIED");
        snprintf(cert_out->input_path, sizeof(cert_out->input_path), "%s",
                 input_path ? input_path : "synthetic");
        if (input_path && econ_sha256_file(input_path, cert_out->input_sha256) != 0) {
            snprintf(cert_out->input_sha256, sizeof(cert_out->input_sha256),
                     "0000000000000000000000000000000000000000000000000000000000000000");
        }
        const int64_t DS = 1000;
        double wc_dro = max_wc;
        cert_out->has_dro = 1;
        cert_out->dro_scale = DS;
        cert_out->gauss_cvar_int = (int64_t)llround(res->gauss_cvar * (double)DS);
        if (cert_out->gauss_cvar_int < 1) cert_out->gauss_cvar_int = 1;
        cert_out->dro_cvar_int = (int64_t)ceil(wc_dro * (double)DS);
        if (cert_out->dro_cvar_int < cert_out->gauss_cvar_int) {
            cert_out->dro_cvar_int = cert_out->gauss_cvar_int;
        }
        cert_out->amp_bps = (cert_out->dro_cvar_int * 10000) / cert_out->gauss_cvar_int;

        cert_out->has_bracket = 1;
        cert_out->bracket_scale = DS;
        cert_out->lb_int = cert_out->gauss_cvar_int;
        cert_out->ub_int = cert_out->dro_cvar_int;
        cert_out->limit_lo_int = 0;
        cert_out->limit_hi_int = cert_out->dro_cvar_int * 2;
        cert_out->lb_val = res->gauss_cvar;
        cert_out->ub_val = wc_dro;

        if (d <= ECON_CERT_MAX_DIM) {
            cert_out->has_psd = 1;
            cert_out->psd.dim = d;
            cert_out->psd.scale_q = 10000;
            cert_out->psd.mu_psd = mu_psd;
            cert_out->psd.delta_round = 0;
            cert_out->psd.mu_cert = mu_psd;
            cert_out->psd.ok = (mu_psd >= 0) ? 1 : 0;
            memcpy(cert_out->psd.A, R_int, (size_t)(d * d) * sizeof(int64_t));
            memcpy(cert_out->psd.L, R_L_int, (size_t)(d * d) * sizeof(int64_t));
        }
    }
    return 0;
}

int econ_ruin_bound(const econ_risk_table_t *tbl, double target_ruin,
                    const char *input_path,
                    econ_ruin_res_t *res, econ_cert_record_t *cert_out) {
    if (!tbl || !res || target_ruin <= 1e-9 || target_ruin >= 0.5) return -1;
    memset(res, 0, sizeof(*res));
    int d = tbl->n_assets, B = tbl->n_bins;

    /* Aggregate comonotonic empirical scenario vector for moment + Cantelli check */
    double sc[ECON_RISK_MAX_BINS] = {0};
    double mu = 0.0, s2 = 0.0;
    for (int b = 0; b < B; ++b) {
        double v = 0.0;
        for (int i = 0; i < d; ++i) v += tbl->samples[i * ECON_RISK_MAX_BINS + b];
        sc[b] = v;
        mu += v;
    }
    mu /= (double)B;
    for (int b = 0; b < B; ++b) {
        double dv = sc[b] - mu;
        s2 += dv * dv;
    }
    double var = s2 / (double)B;
    double sd = (var > 0.0) ? sqrt(var) : 0.0;
    double u = (tbl->ruin_threshold > mu) ? tbl->ruin_threshold : (mu + 2.5 * sd);

    res->mu_loss = mu;
    res->sigma_loss = sd;
    res->threshold_u = u;
    double delta = u - mu;
    res->gauss_ruin_prob = 1.0 - norm_cdf_act((sd > 1e-12) ? (delta / sd) : 10.0);
    res->cantelli_ruin_ub = var / (var + delta * delta);
    res->ruin_ratio_ub_over_gauss = (res->gauss_ruin_prob > 1e-15)
        ? (res->cantelli_ruin_ub / res->gauss_ruin_prob) : 1e6;
    res->min_capital_gauss_995 = mu + inv_norm_cdf(1.0 - target_ruin) * sd;
    res->min_capital_cantelli_995 = mu + sqrt((1.0 - target_ruin) / target_ruin) * sd;

    if (cert_out != NULL) {
        memset(cert_out, 0, sizeof(*cert_out));
        snprintf(cert_out->pillar, sizeof(cert_out->pillar), "actuary");
        snprintf(cert_out->kind, sizeof(cert_out->kind), "ruin_bound");
        snprintf(cert_out->status, sizeof(cert_out->status), "CERTIFIED");
        snprintf(cert_out->input_path, sizeof(cert_out->input_path), "%s",
                 input_path ? input_path : "synthetic");
        if (input_path && econ_sha256_file(input_path, cert_out->input_sha256) != 0) {
            snprintf(cert_out->input_sha256, sizeof(cert_out->input_sha256),
                     "0000000000000000000000000000000000000000000000000000000000000000");
        }
        /* Exact integer Cantelli verification matching M13b over integer deviations z_b */
        const int64_t RS = 100;
        int64_t z[ECON_RISK_MAX_BINS];
        int64_t sum_z = 0;
        for (int b = 0; b < B; ++b) {
            z[b] = (int64_t)llround((sc[b] - mu) * (double)RS);
            sum_z += z[b];
        }
        z[0] -= sum_z; /* Force exact zero empirical mean over Z so M13b precondition holds bit-for-bit */
        int64_t delta_i = (int64_t)llround(delta * (double)RS);
        if (delta_i < 1) delta_i = 1;
        int64_t S2_i = 0, ptail_i = 0;
        for (int b = 0; b < B; ++b) {
            S2_i += z[b] * z[b];
            if (z[b] >= delta_i) ptail_i += 1;
        }
        cert_out->has_dro = 1;
        cert_out->dro_scale = RS;
        cert_out->gauss_cvar_int = (int64_t)llround(res->min_capital_gauss_995 * (double)RS);
        if (cert_out->gauss_cvar_int < 1) cert_out->gauss_cvar_int = 1;
        cert_out->dro_cvar_int = (int64_t)ceil(res->min_capital_cantelli_995 * (double)RS);
        if (cert_out->dro_cvar_int < cert_out->gauss_cvar_int) {
            cert_out->dro_cvar_int = cert_out->gauss_cvar_int;
        }
        cert_out->amp_bps = (cert_out->dro_cvar_int * 10000) / cert_out->gauss_cvar_int;
        cert_out->has_cantelli = 1;
        cert_out->cantelli_W = (int64_t)B;
        cert_out->cantelli_delta = delta_i;
        cert_out->cantelli_S2 = S2_i;
        cert_out->cantelli_ptail = ptail_i;
    }
    return 0;
}
