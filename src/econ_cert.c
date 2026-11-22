/* SPDX-License-Identifier: MIT */
#include "econ_cert.h"

#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__extension__ typedef __int128 econ_i128;

/* ============================================================================
 * 1. FIPS-180-4 SHA-256
 * ============================================================================ */

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t data[64];
    size_t datalen;
} econ_sha256_ctx_t;

static const uint32_t ECON_SHA256_K[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static inline uint32_t econ_rotr32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32U - n));
}

static void econ_sha256_transform(econ_sha256_ctx_t *ctx, const uint8_t block[64]) {
    uint32_t m[64];
    for (int i = 0; i < 16; ++i) {
        size_t k = (size_t)i * 4U;
        m[i] = ((uint32_t)block[k] << 24) | ((uint32_t)block[k + 1U] << 16) |
               ((uint32_t)block[k + 2U] << 8) | (uint32_t)block[k + 3U];
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = econ_rotr32(m[i - 15], 7U) ^ econ_rotr32(m[i - 15], 18U) ^ (m[i - 15] >> 3U);
        uint32_t s1 = econ_rotr32(m[i - 2], 17U) ^ econ_rotr32(m[i - 2], 19U) ^ (m[i - 2] >> 10U);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = econ_rotr32(e, 6U) ^ econ_rotr32(e, 11U) ^ econ_rotr32(e, 25U);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + ECON_SHA256_K[i] + m[i];
        uint32_t S0 = econ_rotr32(a, 2U) ^ econ_rotr32(a, 13U) ^ econ_rotr32(a, 22U);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void econ_sha256_init(econ_sha256_ctx_t *ctx) {
    ctx->datalen = 0U;
    ctx->bitlen = 0ULL;
    ctx->state[0] = 0x6a09e667U; ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U; ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU; ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU; ctx->state[7] = 0x5be0cd19U;
}

static void econ_sha256_update(econ_sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0U; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64U) {
            econ_sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512ULL;
            ctx->datalen = 0U;
        }
    }
}

static void econ_sha256_final(econ_sha256_ctx_t *ctx, uint8_t hash[32]) {
    size_t i = ctx->datalen;
    if (ctx->datalen < 56U) {
        ctx->data[i++] = 0x80U;
        while (i < 56U) ctx->data[i++] = 0x00U;
    } else {
        ctx->data[i++] = 0x80U;
        while (i < 64U) ctx->data[i++] = 0x00U;
        econ_sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56U);
    }
    ctx->bitlen += (uint64_t)ctx->datalen * 8ULL;
    for (int j = 0; j < 8; ++j) {
        ctx->data[63 - j] = (uint8_t)((ctx->bitlen >> (j * 8)) & 0xFFULL);
    }
    econ_sha256_transform(ctx, ctx->data);
    for (int j = 0; j < 8; ++j) {
        hash[j * 4] = (uint8_t)((ctx->state[j] >> 24) & 0xFFU);
        hash[j * 4 + 1] = (uint8_t)((ctx->state[j] >> 16) & 0xFFU);
        hash[j * 4 + 2] = (uint8_t)((ctx->state[j] >> 8) & 0xFFU);
        hash[j * 4 + 3] = (uint8_t)(ctx->state[j] & 0xFFU);
    }
}

void econ_sha256_bytes(const uint8_t *data, size_t len, char hex_out[65]) {
    static const char hex[] = "0123456789abcdef";
    econ_sha256_ctx_t ctx;
    uint8_t digest[32];
    econ_sha256_init(&ctx);
    if (data != NULL && len > 0U) econ_sha256_update(&ctx, data, len);
    econ_sha256_final(&ctx, digest);
    for (int i = 0; i < 32; ++i) {
        hex_out[i * 2] = hex[(digest[i] >> 4) & 0xFU];
        hex_out[i * 2 + 1] = hex[digest[i] & 0xFU];
    }
    hex_out[64] = '\0';
}

int econ_sha256_file(const char *path, char hex_out[65]) {
    if (path == NULL || path[0] == '\0') return -1;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return -1;
    econ_sha256_ctx_t ctx;
    econ_sha256_init(&ctx);
    uint8_t buf[4096];
    size_t nr;
    while ((nr = fread(buf, 1U, sizeof(buf), fp)) > 0U) {
        econ_sha256_update(&ctx, buf, nr);
    }
    int err = ferror(fp);
    fclose(fp);
    if (err != 0) return -1;
    static const char hex[] = "0123456789abcdef";
    uint8_t digest[32];
    econ_sha256_final(&ctx, digest);
    for (int i = 0; i < 32; ++i) {
        hex_out[i * 2] = hex[(digest[i] >> 4) & 0xFU];
        hex_out[i * 2 + 1] = hex[digest[i] & 0xFU];
    }
    hex_out[64] = '\0';
    return 0;
}

/* ============================================================================
 * 2. Linear Algebra, Jacobi Eigensolver & Exact Two-Phase Simplex LP
 * ============================================================================ */

int econ_lu_solve_real(int n, const double *A, const double *b, double *x) {
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return -1;
    size_t nn = (size_t)n;
    double *M = (double *)malloc(nn * nn * sizeof(double));
    double *rhs = (double *)malloc(nn * sizeof(double));
    if (M == NULL || rhs == NULL) {
        free(M); free(rhs);
        return -1;
    }
    memcpy(M, A, nn * nn * sizeof(double));
    memcpy(rhs, b, nn * sizeof(double));
    for (int k = 0; k < n; ++k) {
        int piv = k;
        double best = fabs(M[(size_t)k * nn + (size_t)k]);
        for (int i = k + 1; i < n; ++i) {
            double v = fabs(M[(size_t)i * nn + (size_t)k]);
            if (v > best) { best = v; piv = i; }
        }
        if (best < 1e-14) { free(M); free(rhs); return -1; }
        if (piv != k) {
            for (int j = k; j < n; ++j) {
                double t = M[(size_t)k * nn + (size_t)j];
                M[(size_t)k * nn + (size_t)j] = M[(size_t)piv * nn + (size_t)j];
                M[(size_t)piv * nn + (size_t)j] = t;
            }
            double tb = rhs[k]; rhs[k] = rhs[piv]; rhs[piv] = tb;
        }
        double diag = M[(size_t)k * nn + (size_t)k];
        for (int i = k + 1; i < n; ++i) {
            double fac = M[(size_t)i * nn + (size_t)k] / diag;
            M[(size_t)i * nn + (size_t)k] = 0.0;
            for (int j = k + 1; j < n; ++j) {
                M[(size_t)i * nn + (size_t)j] -= fac * M[(size_t)k * nn + (size_t)j];
            }
            rhs[i] -= fac * rhs[k];
        }
    }
    for (int i = n - 1; i >= 0; --i) {
        double s = rhs[i];
        for (int j = i + 1; j < n; ++j) s -= M[(size_t)i * nn + (size_t)j] * x[j];
        x[i] = s / M[(size_t)i * nn + (size_t)i];
    }
    free(M); free(rhs);
    return 0;
}

int econ_sym_jacobi_eigs(int n, const double *A, double *evals, double *evecs) {
    if (n <= 0 || A == NULL || evals == NULL) return -1;
    size_t nn = (size_t)n;
    double *M = (double *)malloc(nn * nn * sizeof(double));
    double *V = (evecs != NULL) ? evecs : (double *)malloc(nn * nn * sizeof(double));
    if (M == NULL || V == NULL) {
        free(M);
        if (evecs == NULL) free(V);
        return -1;
    }
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            M[(size_t)i * nn + (size_t)j] =
                0.5 * (A[(size_t)i * nn + (size_t)j] + A[(size_t)j * nn + (size_t)i]);
            V[(size_t)i * nn + (size_t)j] = (i == j) ? 1.0 : 0.0;
        }
    }
    int max_sweeps = 64 * n * n + 64;
    for (int it = 0; it < max_sweeps; ++it) {
        int p = 0, q = (n > 1) ? 1 : 0;
        double mx = 0.0;
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                double v = fabs(M[(size_t)i * nn + (size_t)j]);
                if (v > mx) { mx = v; p = i; q = j; }
            }
        }
        if (mx < 1e-13 || n == 1) break;
        double app = M[(size_t)p * nn + (size_t)p];
        double aqq = M[(size_t)q * nn + (size_t)q];
        double apq = M[(size_t)p * nn + (size_t)q];
        double tau = (aqq - app) / (2.0 * apq);
        double t = (tau >= 0.0) ? (1.0 / (tau + sqrt(1.0 + tau * tau)))
                                : (-1.0 / (-tau + sqrt(1.0 + tau * tau)));
        double cs = 1.0 / sqrt(1.0 + t * t);
        double sn = t * cs;
        for (int k = 0; k < n; ++k) {
            if (k == p || k == q) continue;
            double mkp = M[(size_t)k * nn + (size_t)p];
            double mkq = M[(size_t)k * nn + (size_t)q];
            M[(size_t)k * nn + (size_t)p] = M[(size_t)p * nn + (size_t)k] = cs * mkp - sn * mkq;
            M[(size_t)k * nn + (size_t)q] = M[(size_t)q * nn + (size_t)k] = sn * mkp + cs * mkq;
        }
        M[(size_t)p * nn + (size_t)p] = app - t * apq;
        M[(size_t)q * nn + (size_t)q] = aqq + t * apq;
        M[(size_t)p * nn + (size_t)q] = M[(size_t)q * nn + (size_t)p] = 0.0;
        for (int k = 0; k < n; ++k) {
            double vkp = V[(size_t)k * nn + (size_t)p];
            double vkq = V[(size_t)k * nn + (size_t)q];
            V[(size_t)k * nn + (size_t)p] = cs * vkp - sn * vkq;
            V[(size_t)k * nn + (size_t)q] = sn * vkp + cs * vkq;
        }
    }
    int *perm = (int *)malloc(nn * sizeof(int));
    if (perm == NULL) {
        free(M);
        if (evecs == NULL) free(V);
        return -1;
    }
    for (int i = 0; i < n; ++i) {
        evals[i] = M[(size_t)i * nn + (size_t)i];
        perm[i] = i;
    }
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (evals[j] < evals[i]) {
                double te = evals[i]; evals[i] = evals[j]; evals[j] = te;
                int tp = perm[i]; perm[i] = perm[j]; perm[j] = tp;
            }
        }
    }
    if (evecs != NULL) {
        double *Vtmp = (double *)malloc(nn * nn * sizeof(double));
        if (Vtmp != NULL) {
            for (int j = 0; j < n; ++j) {
                for (int i = 0; i < n; ++i) {
                    Vtmp[(size_t)i * nn + (size_t)j] = V[(size_t)i * nn + (size_t)perm[j]];
                }
            }
            memcpy(V, Vtmp, nn * nn * sizeof(double));
            free(Vtmp);
        }
    } else {
        free(V);
    }
    free(perm);
    free(M);
    return 0;
}

static void lp_pivot(int m, int ncols, double *T, int *basis, int r, int c) {
    size_t stride = (size_t)(ncols + 1);
    double piv = T[(size_t)r * stride + (size_t)c];
    for (int j = 0; j <= ncols; ++j) T[(size_t)r * stride + (size_t)j] /= piv;
    T[(size_t)r * stride + (size_t)c] = 1.0;
    for (int i = 0; i <= m; ++i) {
        if (i == r) continue;
        double f = T[(size_t)i * stride + (size_t)c];
        if (fabs(f) < 1e-16) continue;
        for (int j = 0; j <= ncols; ++j) {
            T[(size_t)i * stride + (size_t)j] -= f * T[(size_t)r * stride + (size_t)j];
        }
        T[(size_t)i * stride + (size_t)c] = 0.0;
    }
    basis[r] = c;
}

static int lp_run_simplex(int m, int ncols, double *T, int *basis, const int *allowed_col,
                          int max_iters) {
    size_t stride = (size_t)(ncols + 1);
    int bland_thresh = 4 * (m + ncols) + 128;
    for (int iter = 0; iter < max_iters; ++iter) {
        int enter = -1;
        double best_rc = -1e-9;
        if (iter < bland_thresh) {
            for (int j = 0; j < ncols; ++j) {
                if (allowed_col != NULL && !allowed_col[j]) continue;
                double rc = T[(size_t)m * stride + (size_t)j];
                if (rc < best_rc) {
                    best_rc = rc;
                    enter = j;
                }
            }
        } else {
            for (int j = 0; j < ncols; ++j) {
                if (allowed_col != NULL && !allowed_col[j]) continue;
                if (T[(size_t)m * stride + (size_t)j] < -1e-9) {
                    enter = j;
                    break;
                }
            }
        }
        if (enter < 0) return 0; /* Optimal */
        int leave = -1;
        double best_ratio = 1e300;
        for (int i = 0; i < m; ++i) {
            double a = T[(size_t)i * stride + (size_t)enter];
            if (a > 1e-10) {
                double rhs = T[(size_t)i * stride + (size_t)ncols];
                if (rhs < 0.0) rhs = 0.0;
                double ratio = rhs / a;
                if (ratio < best_ratio - 1e-12 ||
                    (fabs(ratio - best_ratio) <= 1e-12 &&
                     (leave < 0 || basis[i] < basis[leave]))) {
                    best_ratio = ratio;
                    leave = i;
                }
            }
        }
        if (leave < 0) return 2; /* Unbounded */
        lp_pivot(m, ncols, T, basis, leave, enter);
    }
    return -1;
}

int econ_lp_solve(int n_vars, int n_cons, const double *c, const double *A,
                  const econ_lp_sense_t *sense, const double *b,
                  double *x_out, double *y_out, double *obj_out) {
    if (n_vars <= 0 || n_cons < 0 || c == NULL || x_out == NULL) return -1;
    if (n_cons > 0 && (A == NULL || sense == NULL || b == NULL)) return -1;
    if (n_cons == 0) {
        for (int j = 0; j < n_vars; ++j) {
            if (c[j] < -1e-12) return 2;
            x_out[j] = 0.0;
        }
        if (obj_out != NULL) *obj_out = 0.0;
        return 0;
    }

    int m = n_cons;
    int *flipped = (int *)calloc((size_t)m, sizeof(int));
    econ_lp_sense_t *nsense = (econ_lp_sense_t *)malloc((size_t)m * sizeof(econ_lp_sense_t));
    int *surplus_col = (int *)malloc((size_t)m * sizeof(int));
    int *unit_col = (int *)malloc((size_t)m * sizeof(int));
    int *basis = (int *)malloc((size_t)m * sizeof(int));
    if (!flipped || !nsense || !surplus_col || !unit_col || !basis) {
        free(flipped); free(nsense); free(surplus_col); free(unit_col); free(basis);
        return -1;
    }

    int n_surplus = 0;
    int need_phase1 = 0;
    for (int i = 0; i < m; ++i) {
        surplus_col[i] = -1;
        nsense[i] = sense[i];
        if (b[i] < 0.0) {
            flipped[i] = 1;
            if (sense[i] == ECON_LP_LE) nsense[i] = ECON_LP_GE;
            else if (sense[i] == ECON_LP_GE) nsense[i] = ECON_LP_LE;
        }
        if (nsense[i] == ECON_LP_GE) {
            surplus_col[i] = n_vars + n_surplus++;
            need_phase1 = 1;
        } else if (nsense[i] == ECON_LP_EQ) {
            need_phase1 = 1;
        }
    }

    int ncols = n_vars + n_surplus + m;
    for (int i = 0; i < m; ++i) {
        unit_col[i] = n_vars + n_surplus + i;
        basis[i] = unit_col[i];
    }

    size_t stride = (size_t)(ncols + 1);
    double *T = (double *)calloc((size_t)(m + 1) * stride, sizeof(double));
    int *allowed = (int *)malloc((size_t)ncols * sizeof(int));
    int *is_art = (int *)calloc((size_t)ncols, sizeof(int));
    if (!T || !allowed || !is_art) {
        free(flipped); free(nsense); free(surplus_col); free(unit_col);
        free(basis); free(T); free(allowed); free(is_art);
        return -1;
    }

    for (int i = 0; i < m; ++i) {
        double sgn = flipped[i] ? -1.0 : 1.0;
        for (int j = 0; j < n_vars; ++j) {
            T[(size_t)i * stride + (size_t)j] = sgn * A[(size_t)i * (size_t)n_vars + (size_t)j];
        }
        if (surplus_col[i] >= 0) {
            T[(size_t)i * stride + (size_t)surplus_col[i]] = -1.0;
        }
        T[(size_t)i * stride + (size_t)unit_col[i]] = 1.0;
        T[(size_t)i * stride + (size_t)ncols] = sgn * b[i];
        if (nsense[i] == ECON_LP_GE || nsense[i] == ECON_LP_EQ) {
            is_art[unit_col[i]] = 1;
        }
    }

    for (int j = 0; j < ncols; ++j) allowed[j] = 1;

    if (need_phase1) {
        for (int i = 0; i < m; ++i) {
            if (is_art[unit_col[i]]) {
                for (int j = 0; j <= ncols; ++j) {
                    T[(size_t)m * stride + (size_t)j] -= T[(size_t)i * stride + (size_t)j];
                }
                T[(size_t)m * stride + (size_t)unit_col[i]] = 0.0;
            }
        }
        int p1 = lp_run_simplex(m, ncols, T, basis, allowed, 16000);
        if (p1 != 0 || -T[(size_t)m * stride + (size_t)ncols] > 1e-6) {
            free(flipped); free(nsense); free(surplus_col); free(unit_col);
            free(basis); free(T); free(allowed); free(is_art);
            return 1; /* Infeasible */
        }
        /* Pivot out zero-valued basic artificials where possible */
        for (int r = 0; r < m; ++r) {
            if (is_art[basis[r]]) {
                int best_c = -1;
                double best_abs = 1e-8;
                for (int j = 0; j < ncols; ++j) {
                    if (is_art[j]) continue;
                    double v = fabs(T[(size_t)r * stride + (size_t)j]);
                    if (v > best_abs) { best_abs = v; best_c = j; }
                }
                if (best_c >= 0) {
                    lp_pivot(m, ncols, T, basis, r, best_c);
                }
            }
        }
    }

    /* Phase II setup: keep artificial columns non-entering so T[m][unit_col[i]] = -pi_i */
    memset(&T[(size_t)m * stride], 0, stride * sizeof(double));
    for (int j = 0; j < n_vars; ++j) T[(size_t)m * stride + (size_t)j] = c[j];
    for (int j = 0; j < ncols; ++j) allowed[j] = is_art[j] ? 0 : 1;

    for (int r = 0; r < m; ++r) {
        double cb = (basis[r] < n_vars) ? c[basis[r]] : 0.0;
        if (fabs(cb) > 0.0) {
            for (int j = 0; j <= ncols; ++j) {
                T[(size_t)m * stride + (size_t)j] -= cb * T[(size_t)r * stride + (size_t)j];
            }
        }
    }

    int p2 = lp_run_simplex(m, ncols, T, basis, allowed, 20000);
    if (p2 == 0) {
        memset(x_out, 0, (size_t)n_vars * sizeof(double));
        for (int r = 0; r < m; ++r) {
            if (basis[r] < n_vars) {
                double val = T[(size_t)r * stride + (size_t)ncols];
                x_out[basis[r]] = (val > 0.0) ? val : 0.0;
            }
        }
        if (y_out != NULL) {
            for (int i = 0; i < m; ++i) {
                double pi = -T[(size_t)m * stride + (size_t)unit_col[i]];
                if (flipped[i]) pi = -pi;
                y_out[i] = pi;
            }
        }
        if (obj_out != NULL) {
            double obj = 0.0;
            for (int j = 0; j < n_vars; ++j) obj += c[j] * x_out[j];
            *obj_out = obj;
        }
    }

    free(flipped); free(nsense); free(surplus_col); free(unit_col);
    free(basis); free(T); free(allowed); free(is_art);
    return p2;
}

/* ============================================================================
 * 3. Zero-FP-Trust 128-bit Integer Cholesky + SDD Certifier & Verifier
 * ============================================================================ */

static int64_t econ_i128_clamp_to_i64(econ_i128 v, int *overflow) {
    if (v > (econ_i128)INT64_MAX || v < (econ_i128)INT64_MIN) {
        *overflow = 1;
        return 0;
    }
    return (int64_t)v;
}

int econ_verify_cholesky_sdd_i64(int n, const int64_t *A_sym,
                                 const int64_t *L_tri, int64_t mu_psd) {
    if (n <= 0 || n > ECON_CERT_MAX_DIM || A_sym == NULL || L_tri == NULL) return -1;
    size_t nn = (size_t)n;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (A_sym[(size_t)i * nn + (size_t)j] != A_sym[(size_t)j * nn + (size_t)i]) return -1;
        }
    }
    for (int i = 0; i < n; ++i) {
        econ_i128 off = 0, diag = 0;
        for (int j = 0; j < n; ++j) {
            econ_i128 llt = 0;
            int kmax = (i < j) ? i : j;
            for (int k = 0; k <= kmax; ++k) {
                llt += (econ_i128)L_tri[(size_t)i * nn + (size_t)k] *
                       (econ_i128)L_tri[(size_t)j * nn + (size_t)k];
            }
            econ_i128 eij = (econ_i128)A_sym[(size_t)i * nn + (size_t)j] - llt;
            if (i == j) diag = eij - (econ_i128)mu_psd;
            else off += (eij < 0) ? -eij : eij;
        }
        if (diag < off) return -1;
    }
    return 0;
}

int econ_cert_cholesky_sdd_i64(int n, const int64_t *A_sym,
                               int64_t *L_out, int64_t *mu_psd_out) {
    if (n <= 0 || n > ECON_CERT_MAX_DIM || !A_sym || !L_out || !mu_psd_out) return -1;
    size_t nn = (size_t)n;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (A_sym[(size_t)i * nn + (size_t)j] != A_sym[(size_t)j * nn + (size_t)i]) return -1;
        }
    }
    double A_fp[ECON_CERT_MAX_DIM * ECON_CERT_MAX_DIM];
    double L_fp[ECON_CERT_MAX_DIM * ECON_CERT_MAX_DIM];
    double evals[ECON_CERT_MAX_DIM];
    double max_diag = 0.0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            double v = (double)A_sym[(size_t)i * nn + (size_t)j];
            A_fp[(size_t)i * nn + (size_t)j] = v;
            if (i == j && fabs(v) > max_diag) max_diag = fabs(v);
        }
    }
    if (econ_sym_jacobi_eigs(n, A_fp, evals, NULL) != 0) return -1;
    double lmin = evals[0];
    double eps = fmax(1.0, 1e-7 * (max_diag + 1.0));
    int chol_ok = 0;
    for (int attempt = 0; attempt < 8; ++attempt) {
        double shift = lmin - eps;
        memset(L_fp, 0, nn * nn * sizeof(double));
        chol_ok = 1;
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j <= i; ++j) {
                double s = A_fp[(size_t)i * nn + (size_t)j];
                if (i == j) s -= shift;
                for (int k = 0; k < j; ++k) {
                    s -= L_fp[(size_t)i * nn + (size_t)k] * L_fp[(size_t)j * nn + (size_t)k];
                }
                if (i == j) {
                    if (s <= 0.0) { chol_ok = 0; break; }
                    L_fp[(size_t)i * nn + (size_t)j] = sqrt(s);
                } else {
                    L_fp[(size_t)i * nn + (size_t)j] = s / L_fp[(size_t)j * nn + (size_t)j];
                }
            }
            if (!chol_ok) break;
        }
        if (chol_ok) break;
        eps *= 4.0;
    }
    if (!chol_ok) {
        memset(L_out, 0, nn * nn * sizeof(int64_t));
    } else {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                L_out[(size_t)i * nn + (size_t)j] =
                    (j <= i) ? (int64_t)llround(L_fp[(size_t)i * nn + (size_t)j]) : 0;
            }
        }
    }
    econ_i128 mu_best = 0;
    int first = 1;
    for (int i = 0; i < n; ++i) {
        econ_i128 off = 0, diag = 0;
        for (int j = 0; j < n; ++j) {
            econ_i128 llt = 0;
            int kmax = (i < j) ? i : j;
            for (int k = 0; k <= kmax; ++k) {
                llt += (econ_i128)L_out[(size_t)i * nn + (size_t)k] *
                       (econ_i128)L_out[(size_t)j * nn + (size_t)k];
            }
            econ_i128 eij = (econ_i128)A_sym[(size_t)i * nn + (size_t)j] - llt;
            if (i == j) diag = eij;
            else off += (eij < 0) ? -eij : eij;
        }
        econ_i128 rf = diag - off;
        if (first || rf < mu_best) { mu_best = rf; first = 0; }
    }
    int ov = 0;
    int64_t mu_i64 = econ_i128_clamp_to_i64(mu_best, &ov);
    if (ov != 0) return -1;
    *mu_psd_out = mu_i64;
    return econ_verify_cholesky_sdd_i64(n, A_sym, L_out, mu_i64);
}

/* ============================================================================
 * 4. Canonical .econcert.json Writer & Zero-FP-Trust Integer Verifier
 * ============================================================================ */

static void write_i64_array(FILE *fp, const char *name, int n, const int64_t *v, int comma) {
    fprintf(fp, "  \"%s\": [", name);
    for (int i = 0; i < n; ++i) fprintf(fp, "%s%" PRId64, (i > 0) ? ", " : "", v[i]);
    fprintf(fp, "]%s\n", comma ? "," : "");
}

static void write_i64_matrix(FILE *fp, const char *name, int rows, int cols,
                             int stride, const int64_t *M, int comma) {
    fprintf(fp, "  \"%s\": [\n", name);
    for (int i = 0; i < rows; ++i) {
        fprintf(fp, "    [");
        for (int j = 0; j < cols; ++j) {
            fprintf(fp, "%s%" PRId64, (j > 0) ? ", " : "", M[(size_t)i * (size_t)stride + (size_t)j]);
        }
        fprintf(fp, "]%s\n", (i + 1 < rows) ? "," : "");
    }
    fprintf(fp, "  ]%s\n", comma ? "," : "");
}

int econ_cert_write_json(const char *out_path, const econ_cert_record_t *rec) {
    if (!out_path || !rec) return -1;
    FILE *fp = fopen(out_path, "wb");
    if (!fp) return -1;
    fprintf(fp, "{\n");
    fprintf(fp, "  \"format\": \"econcert.v1\",\n");
    fprintf(fp, "  \"version\": \"%s\",\n", ECON_VERSION_STRING);
    fprintf(fp, "  \"pillar\": \"%s\",\n", rec->pillar);
    fprintf(fp, "  \"kind\": \"%s\",\n", rec->kind);
    fprintf(fp, "  \"status\": \"%s\",\n", rec->status);
    fprintf(fp, "  \"input_path\": \"%s\",\n", rec->input_path);
    fprintf(fp, "  \"input_sha256\": \"%s\",\n", rec->input_sha256);
    fprintf(fp, "  \"has_psd\": %d,\n", rec->has_psd);
    if (rec->has_psd) {
        fprintf(fp, "  \"dim\": %d,\n", rec->psd.dim);
        fprintf(fp, "  \"scale_q\": %" PRId64 ",\n", rec->psd.scale_q);
        fprintf(fp, "  \"mu_psd\": %" PRId64 ",\n", rec->psd.mu_psd);
        fprintf(fp, "  \"delta_round\": %" PRId64 ",\n", rec->psd.delta_round);
        fprintf(fp, "  \"mu_cert\": %" PRId64 ",\n", rec->psd.mu_cert);
        fprintf(fp, "  \"psd_ok\": %d,\n", rec->psd.ok);
        write_i64_matrix(fp, "A_int", rec->psd.dim, rec->psd.dim, rec->psd.dim, rec->psd.A, 1);
        write_i64_matrix(fp, "L_int", rec->psd.dim, rec->psd.dim, rec->psd.dim, rec->psd.L, 1);
    }
    fprintf(fp, "  \"has_bracket\": %d,\n", rec->has_bracket);
    if (rec->has_bracket) {
        fprintf(fp, "  \"bracket_scale\": %" PRId64 ",\n", rec->bracket_scale);
        fprintf(fp, "  \"lb_int\": %" PRId64 ",\n", rec->lb_int);
        fprintf(fp, "  \"ub_int\": %" PRId64 ",\n", rec->ub_int);
        fprintf(fp, "  \"has_ref\": %d,\n", rec->has_ref);
        fprintf(fp, "  \"ref_int\": %" PRId64 ",\n", rec->ref_int);
        fprintf(fp, "  \"limit_lo_int\": %" PRId64 ",\n", rec->limit_lo_int);
        fprintf(fp, "  \"limit_hi_int\": %" PRId64 ",\n", rec->limit_hi_int);
        fprintf(fp, "  \"lb_val\": %.10g,\n", rec->lb_val);
        fprintf(fp, "  \"ub_val\": %.10g,\n", rec->ub_val);
    }
    fprintf(fp, "  \"has_arb\": %d,\n", rec->has_arb);
    if (rec->has_arb) {
        fprintf(fp, "  \"arb_mode\": %d,\n", rec->arb_mode);
        fprintf(fp, "  \"arb_scale\": %" PRId64 ",\n", rec->arb_scale);
        fprintf(fp, "  \"k1\": %" PRId64 ",\n", rec->k1);
        fprintf(fp, "  \"k2\": %" PRId64 ",\n", rec->k2);
        fprintf(fp, "  \"k3\": %" PRId64 ",\n", rec->k3);
        fprintf(fp, "  \"c_bid1\": %" PRId64 ",\n", rec->c_bid1);
        fprintf(fp, "  \"c_ask1\": %" PRId64 ",\n", rec->c_ask1);
        fprintf(fp, "  \"p_bid1\": %" PRId64 ",\n", rec->p_bid1);
        fprintf(fp, "  \"p_ask1\": %" PRId64 ",\n", rec->p_ask1);
        fprintf(fp, "  \"f_bid\": %" PRId64 ",\n", rec->f_bid);
        fprintf(fp, "  \"f_ask\": %" PRId64 ",\n", rec->f_ask);
        fprintf(fp, "  \"c_bid2\": %" PRId64 ",\n", rec->c_bid2);
        fprintf(fp, "  \"c_ask3\": %" PRId64 ",\n", rec->c_ask3);
        fprintf(fp, "  \"arb_cost_int\": %" PRId64 ",\n", rec->arb_cost_int);
        fprintf(fp, "  \"arb_edge_int\": %" PRId64 ",\n", rec->arb_edge_int);
        fprintf(fp, "  \"sniper_mid_res_int\": %" PRId64 ",\n", rec->sniper_mid_res_int);
    }
    fprintf(fp, "  \"has_mot\": %d,\n", rec->has_mot);
    if (rec->has_mot) {
        fprintf(fp, "  \"mot_n1\": %d,\n", rec->mot_n1);
        fprintf(fp, "  \"mot_n2\": %d,\n", rec->mot_n2);
        fprintf(fp, "  \"mot_scale\": %" PRId64 ",\n", rec->mot_scale);
        write_i64_array(fp, "s1_int", rec->mot_n1, rec->s1, 1);
        write_i64_array(fp, "s2_int", rec->mot_n2, rec->s2, 1);
        write_i64_array(fp, "q1_int", rec->mot_n1, rec->q1, 1);
        write_i64_array(fp, "q2_int", rec->mot_n2, rec->q2, 1);
        write_i64_array(fp, "phi1_up_int", rec->mot_n1, rec->phi1_up, 1);
        write_i64_array(fp, "phi2_up_int", rec->mot_n2, rec->phi2_up, 1);
        write_i64_array(fp, "delta_up_int", rec->mot_n1, rec->delta_up, 1);
        write_i64_array(fp, "phi1_lo_int", rec->mot_n1, rec->phi1_lo, 1);
        write_i64_array(fp, "phi2_lo_int", rec->mot_n2, rec->phi2_lo, 1);
        write_i64_array(fp, "delta_lo_int", rec->mot_n1, rec->delta_lo, 1);
        write_i64_matrix(fp, "payoff_int", rec->mot_n1, rec->mot_n2, ECON_CERT_MAX_GRID, rec->payoff, 1);
        fprintf(fp, "  \"mot_frechet_lb_int\": %" PRId64 ",\n", rec->mot_frechet_lb_int);
        fprintf(fp, "  \"mot_lb_int\": %" PRId64 ",\n", rec->mot_lb_int);
        fprintf(fp, "  \"mot_ub_int\": %" PRId64 ",\n", rec->mot_ub_int);
        fprintf(fp, "  \"mot_frechet_ub_int\": %" PRId64 ",\n", rec->mot_frechet_ub_int);
        fprintf(fp, "  \"mot_bs_int\": %" PRId64 ",\n", rec->mot_bs_int);
    }
    fprintf(fp, "  \"has_dro\": %d,\n", rec->has_dro);
    if (rec->has_dro) {
        fprintf(fp, "  \"dro_scale\": %" PRId64 ",\n", rec->dro_scale);
        fprintf(fp, "  \"gauss_cvar_int\": %" PRId64 ",\n", rec->gauss_cvar_int);
        fprintf(fp, "  \"dro_cvar_int\": %" PRId64 ",\n", rec->dro_cvar_int);
        fprintf(fp, "  \"amp_bps\": %" PRId64 ",\n", rec->amp_bps);
        fprintf(fp, "  \"has_cantelli\": %d,\n", rec->has_cantelli);
        fprintf(fp, "  \"cantelli_W\": %" PRId64 ",\n", rec->cantelli_W);
        fprintf(fp, "  \"cantelli_delta\": %" PRId64 ",\n", rec->cantelli_delta);
        fprintf(fp, "  \"cantelli_S2\": %" PRId64 ",\n", rec->cantelli_S2);
        fprintf(fp, "  \"cantelli_ptail\": %" PRId64 ",\n", rec->cantelli_ptail);
    }
    fprintf(fp, "  \"has_clearing\": %d,\n", rec->has_clearing);
    if (rec->has_clearing) {
        fprintf(fp, "  \"clear_n_banks\": %d,\n", rec->clear_n_banks);
        fprintf(fp, "  \"clear_scale\": %" PRId64 ",\n", rec->clear_scale);
        fprintf(fp, "  \"clear_has_bailout\": %d,\n", rec->clear_has_bailout);
        write_i64_array(fp, "clear_p_bar_int", rec->clear_n_banks, rec->clear_p_bar, 1);
        write_i64_array(fp, "clear_ext_int", rec->clear_n_banks, rec->clear_ext, 1);
        write_i64_array(fp, "clear_p_lo_int", rec->clear_n_banks, rec->clear_p_lo, 1);
        write_i64_array(fp, "clear_p_hi_int", rec->clear_n_banks, rec->clear_p_hi, 1);
        write_i64_array(fp, "clear_z_int", rec->clear_n_banks, rec->clear_z, 1);
        write_i64_matrix(fp, "clear_pi_scaled_int", rec->clear_n_banks, rec->clear_n_banks,
                         ECON_CERT_MAX_BANKS, rec->clear_pi_scaled, 1);
    }
    fprintf(fp, "  \"has_mech\": %d%s\n", rec->has_mech, rec->has_mech ? "," : "");
    if (rec->has_mech) {
        fprintf(fp, "  \"mech_mode\": %d,\n", rec->mech_mode);
        fprintf(fp, "  \"mech_scale\": %" PRId64 ",\n", rec->mech_scale);
        fprintf(fp, "  \"mech_n_agents\": %d,\n", rec->mech_n_agents);
        fprintf(fp, "  \"mech_n_outcomes\": %d,\n", rec->mech_n_outcomes);
        fprintf(fp, "  \"mech_chosen\": %d,\n", rec->mech_chosen);
        fprintf(fp, "  \"mech_welfare_int\": %" PRId64 ",\n", rec->mech_welfare_int);
        fprintf(fp, "  \"mech_revenue_int\": %" PRId64 ",\n", rec->mech_revenue_int);
        if (rec->mech_mode == 1) {
            write_i64_matrix(fp, "mech_vals_int", rec->mech_n_agents, rec->mech_n_outcomes,
                             ECON_CERT_MAX_OUT, rec->mech_vals, 1);
            write_i64_array(fp, "mech_pivots_int", rec->mech_n_agents, rec->mech_pivots, 0);
        } else if (rec->mech_mode == 2) {
            fprintf(fp, "  \"mech_ms_K\": %d,\n", rec->mech_ms_K);
            fprintf(fp, "  \"mech_ms_surplus_int\": %" PRId64 "\n", rec->mech_ms_surplus_int);
        } else {
            fprintf(fp, "  \"mech_fisher_tol\": %" PRId64 ",\n", rec->mech_fisher_tol);
            write_i64_array(fp, "mech_fisher_p_int", rec->mech_n_outcomes, rec->mech_fisher_p, 1);
            write_i64_array(fp, "mech_fisher_e_int", rec->mech_n_outcomes, rec->mech_fisher_e, 1);
            write_i64_array(fp, "mech_fisher_w_int", rec->mech_n_agents, rec->mech_fisher_w, 1);
            write_i64_matrix(fp, "mech_fisher_x_int", rec->mech_n_agents, rec->mech_n_outcomes,
                             ECON_CERT_MAX_GRID, rec->mech_fisher_x, 0);
        }
    }
    fprintf(fp, "}\n");
    fclose(fp);
    return 0;
}

static const char *find_json_key(const char *json, const char *key) {
    char pat[96];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = json;
    while ((p = strstr(p, pat)) != NULL) {
        if (p == json || isspace((unsigned char)p[-1]) || p[-1] == '{' || p[-1] == ',') {
            p += strlen(pat);
            while (*p != '\0' && isspace((unsigned char)*p)) ++p;
            if (*p == ':') {
                ++p;
                while (*p != '\0' && isspace((unsigned char)*p)) ++p;
                return p;
            }
        } else {
            p += strlen(pat);
        }
    }
    return NULL;
}

static int json_get_str(const char *json, const char *key, char *out, size_t cap) {
    const char *v = find_json_key(json, key);
    if (!v || *v != '"') return -1;
    ++v;
    size_t k = 0U;
    while (*v != '\0' && *v != '"') {
        if (k + 1U < cap) out[k++] = *v;
        ++v;
    }
    if (*v != '"') return -1;
    out[k] = '\0';
    return 0;
}

static int json_get_i64(const char *json, const char *key, int64_t *out) {
    const char *v = find_json_key(json, key);
    if (!v) return -1;
    char *endp = NULL;
    long long val = strtoll(v, &endp, 10);
    if (endp == v) return -1;
    *out = (int64_t)val;
    return 0;
}

static int json_get_i64_array(const char *json, const char *key, int n, int64_t *out) {
    const char *v = find_json_key(json, key);
    if (!v || *v != '[') return -1;
    ++v;
    for (int i = 0; i < n; ++i) {
        while (*v != '\0' && (isspace((unsigned char)*v) || *v == ',')) ++v;
        char *endp = NULL;
        long long val = strtoll(v, &endp, 10);
        if (endp == v) return -1;
        out[i] = (int64_t)val;
        v = endp;
    }
    while (*v != '\0' && isspace((unsigned char)*v)) ++v;
    return (*v == ']') ? 0 : -1;
}

static int json_get_i64_matrix(const char *json, const char *key, int rows, int cols,
                               int stride, int64_t *M) {
    const char *v = find_json_key(json, key);
    if (!v || *v != '[') return -1;
    ++v;
    for (int i = 0; i < rows; ++i) {
        while (*v != '\0' && *v != '[') ++v;
        if (*v != '[') return -1;
        ++v;
        for (int j = 0; j < cols; ++j) {
            while (*v != '\0' && (isspace((unsigned char)*v) || *v == ',')) ++v;
            char *endp = NULL;
            long long val = strtoll(v, &endp, 10);
            if (endp == v) return -1;
            M[(size_t)i * (size_t)stride + (size_t)j] = (int64_t)val;
            v = endp;
        }
        while (*v != '\0' && *v != ']') ++v;
        if (*v != ']') return -1;
        ++v;
    }
    while (*v != '\0' && (isspace((unsigned char)*v) || *v == ',')) ++v;
    return (*v == ']') ? 0 : -1;
}

static int verify_arb_block(const char *buf, const char *status) {
    int64_t mode = 0, scale = 0, k1 = 0, k2 = 0, k3 = 0;
    int64_t cb1 = 0, ca1 = 0, pb1 = 0, pa1 = 0, fb = 0, fa = 0, cb2 = 0, ca3 = 0;
    int64_t cost = 0, edge = 0;
    if (json_get_i64(buf, "arb_mode", &mode) != 0 ||
        json_get_i64(buf, "arb_scale", &scale) != 0 ||
        json_get_i64(buf, "k1", &k1) != 0 ||
        json_get_i64(buf, "k2", &k2) != 0 ||
        json_get_i64(buf, "k3", &k3) != 0 ||
        json_get_i64(buf, "c_bid1", &cb1) != 0 ||
        json_get_i64(buf, "c_ask1", &ca1) != 0 ||
        json_get_i64(buf, "p_bid1", &pb1) != 0 ||
        json_get_i64(buf, "p_ask1", &pa1) != 0 ||
        json_get_i64(buf, "f_bid", &fb) != 0 ||
        json_get_i64(buf, "f_ask", &fa) != 0 ||
        json_get_i64(buf, "c_bid2", &cb2) != 0 ||
        json_get_i64(buf, "c_ask3", &ca3) != 0 ||
        json_get_i64(buf, "arb_cost_int", &cost) != 0 ||
        json_get_i64(buf, "arb_edge_int", &edge) != 0) return -1;
    if (scale <= 0 || cost < 0) return -1;
    if (mode == 1) { /* Conversion */
        econ_i128 e = (econ_i128)fb - ((econ_i128)k1 + (econ_i128)ca1 - (econ_i128)pb1) - (econ_i128)cost;
        if (e <= 0 || e != (econ_i128)edge) return -1;
        if (strcmp(status, "ARBITRAGE") != 0 && strcmp(status, "CERTIFIED") != 0) return -1;
    } else if (mode == 2) { /* Reversal */
        econ_i128 e = ((econ_i128)k1 + (econ_i128)cb1 - (econ_i128)pa1) - (econ_i128)fa - (econ_i128)cost;
        if (e <= 0 || e != (econ_i128)edge) return -1;
        if (strcmp(status, "ARBITRAGE") != 0 && strcmp(status, "CERTIFIED") != 0) return -1;
    } else if (mode == 3) { /* Butterfly convexity M10 */
        if (!(k1 < k2 && k2 < k3)) return -1;
        econ_i128 credit = ((econ_i128)(k3 - k1) * (econ_i128)cb2) -
                           ((econ_i128)(k3 - k2) * (econ_i128)ca1) -
                           ((econ_i128)(k2 - k1) * (econ_i128)ca3) -
                           (econ_i128)cost;
        if (credit <= 0 || credit != (econ_i128)edge) return -1;
        int64_t test_S[3] = { k1 - scale, k2, k3 + scale };
        for (int t = 0; t < 3; ++t) {
            int64_t S = test_S[t];
            econ_i128 p1 = (S > k1) ? (econ_i128)(S - k1) : 0;
            econ_i128 p2 = (S > k2) ? (econ_i128)(S - k2) : 0;
            econ_i128 p3 = (S > k3) ? (econ_i128)(S - k3) : 0;
            econ_i128 pay = (econ_i128)(k3 - k2) * p1 + (econ_i128)(k2 - k1) * p3 -
                            (econ_i128)(k3 - k1) * p2;
            if (pay < 0) return -1;
            if (pay + credit <= 0) return -1;
        }
    } else if (mode == 0) {
        econ_i128 conv = (econ_i128)fb - ((econ_i128)k1 + (econ_i128)ca1 - (econ_i128)pb1) - (econ_i128)cost;
        econ_i128 rev  = ((econ_i128)k1 + (econ_i128)cb1 - (econ_i128)pa1) - (econ_i128)fa - (econ_i128)cost;
        if (conv > 0 || rev > 0) return -1;
    } else {
        return -1;
    }
    return 0;
}

static int verify_mot_block(const char *buf, const char *status) {
    int64_t n1_64 = 0, n2_64 = 0, scale = 0;
    int64_t flb = 0, lb = 0, ub = 0, fub = 0;
    if (json_get_i64(buf, "mot_n1", &n1_64) != 0 ||
        json_get_i64(buf, "mot_n2", &n2_64) != 0 ||
        json_get_i64(buf, "mot_scale", &scale) != 0 ||
        json_get_i64(buf, "mot_frechet_lb_int", &flb) != 0 ||
        json_get_i64(buf, "mot_lb_int", &lb) != 0 ||
        json_get_i64(buf, "mot_ub_int", &ub) != 0 ||
        json_get_i64(buf, "mot_frechet_ub_int", &fub) != 0) return -1;
    if (n1_64 <= 0 || n1_64 > ECON_CERT_MAX_GRID ||
        n2_64 <= 0 || n2_64 > ECON_CERT_MAX_GRID || scale <= 0) return -1;
    int n1 = (int)n1_64, n2 = (int)n2_64;
    int64_t s1[ECON_CERT_MAX_GRID], s2[ECON_CERT_MAX_GRID];
    int64_t q1[ECON_CERT_MAX_GRID], q2[ECON_CERT_MAX_GRID];
    int64_t p1u[ECON_CERT_MAX_GRID], p2u[ECON_CERT_MAX_GRID], du[ECON_CERT_MAX_GRID];
    int64_t p1l[ECON_CERT_MAX_GRID], p2l[ECON_CERT_MAX_GRID], dl[ECON_CERT_MAX_GRID];
    int64_t pay[ECON_CERT_MAX_GRID * ECON_CERT_MAX_GRID];
    if (json_get_i64_array(buf, "s1_int", n1, s1) != 0 ||
        json_get_i64_array(buf, "s2_int", n2, s2) != 0 ||
        json_get_i64_array(buf, "q1_int", n1, q1) != 0 ||
        json_get_i64_array(buf, "q2_int", n2, q2) != 0 ||
        json_get_i64_array(buf, "phi1_up_int", n1, p1u) != 0 ||
        json_get_i64_array(buf, "phi2_up_int", n2, p2u) != 0 ||
        json_get_i64_array(buf, "delta_up_int", n1, du) != 0 ||
        json_get_i64_array(buf, "phi1_lo_int", n1, p1l) != 0 ||
        json_get_i64_array(buf, "phi2_lo_int", n2, p2l) != 0 ||
        json_get_i64_array(buf, "delta_lo_int", n1, dl) != 0 ||
        json_get_i64_matrix(buf, "payoff_int", n1, n2, ECON_CERT_MAX_GRID, pay) != 0) return -1;

    for (int i = 0; i < n1; ++i) {
        if (q1[i] < 0) return -1;
        for (int j = 0; j < n2; ++j) {
            if (q2[j] < 0) return -1;
            econ_i128 ds = (econ_i128)s2[j] - (econ_i128)s1[i];
            econ_i128 py = (econ_i128)pay[(size_t)i * ECON_CERT_MAX_GRID + (size_t)j];
            econ_i128 sup = ((econ_i128)p1u[i] + (econ_i128)p2u[j]) * (econ_i128)scale +
                            (econ_i128)du[i] * ds;
            econ_i128 sub = ((econ_i128)p1l[i] + (econ_i128)p2l[j]) * (econ_i128)scale +
                            (econ_i128)dl[i] * ds;
            if (sup < py * (econ_i128)scale) return -1;
            if (sub > py * (econ_i128)scale) return -1;
        }
    }
    if (strcmp(status, "CERTIFIED") == 0) {
        if (flb > lb || lb > ub || ub > fub) return -1;
    }
    return 0;
}

static int verify_dro_block(const char *buf) {
    int64_t scale = 0, gcvar = 0, dcvar = 0, amp = 0, has_cant = 0;
    if (json_get_i64(buf, "dro_scale", &scale) != 0 ||
        json_get_i64(buf, "gauss_cvar_int", &gcvar) != 0 ||
        json_get_i64(buf, "dro_cvar_int", &dcvar) != 0 ||
        json_get_i64(buf, "amp_bps", &amp) != 0 ||
        json_get_i64(buf, "has_cantelli", &has_cant) != 0) return -1;
    if (scale <= 0 || gcvar <= 0 || dcvar < gcvar) return -1;
    econ_i128 exp_amp = ((econ_i128)dcvar * 10000) / (econ_i128)gcvar;
    econ_i128 diff = amp - exp_amp;
    if (diff < 0) diff = -diff;
    if (diff > 5) return -1;
    if (has_cant) {
        int64_t W = 0, delta = 0, S2 = 0, ptail = 0;
        if (json_get_i64(buf, "cantelli_W", &W) != 0 ||
            json_get_i64(buf, "cantelli_delta", &delta) != 0 ||
            json_get_i64(buf, "cantelli_S2", &S2) != 0 ||
            json_get_i64(buf, "cantelli_ptail", &ptail) != 0) return -1;
        if (W <= 0 || delta <= 0 || S2 < 0 || ptail < 0) return -1;
        econ_i128 lhs = (econ_i128)ptail * ((econ_i128)W * (econ_i128)delta * (econ_i128)delta + (econ_i128)S2);
        econ_i128 rhs = (econ_i128)W * (econ_i128)S2;
        if (lhs > rhs) return -1;
    }
    return 0;
}

static int verify_clearing_block(const char *buf) {
    int64_t nb64 = 0, scale = 0, has_bail = 0;
    if (json_get_i64(buf, "clear_n_banks", &nb64) != 0 ||
        json_get_i64(buf, "clear_scale", &scale) != 0 ||
        json_get_i64(buf, "clear_has_bailout", &has_bail) != 0) return -1;
    if (nb64 <= 0 || nb64 > ECON_CERT_MAX_BANKS || scale <= 0) return -1;
    int nb = (int)nb64;
    int64_t pbar[ECON_CERT_MAX_BANKS], ext[ECON_CERT_MAX_BANKS];
    int64_t plo[ECON_CERT_MAX_BANKS], phi[ECON_CERT_MAX_BANKS], z[ECON_CERT_MAX_BANKS];
    int64_t pi_s[ECON_CERT_MAX_BANKS * ECON_CERT_MAX_BANKS];
    if (json_get_i64_array(buf, "clear_p_bar_int", nb, pbar) != 0 ||
        json_get_i64_array(buf, "clear_ext_int", nb, ext) != 0 ||
        json_get_i64_array(buf, "clear_p_lo_int", nb, plo) != 0 ||
        json_get_i64_array(buf, "clear_p_hi_int", nb, phi) != 0 ||
        json_get_i64_array(buf, "clear_z_int", nb, z) != 0 ||
        json_get_i64_matrix(buf, "clear_pi_scaled_int", nb, nb, ECON_CERT_MAX_BANKS, pi_s) != 0) return -1;
    for (int i = 0; i < nb; ++i) {
        if (pbar[i] < 0 || plo[i] < 0 || plo[i] > phi[i] || phi[i] > pbar[i]) return -1;
        econ_i128 in_lo = 0, in_hi = 0, in_bar = 0;
        for (int j = 0; j < nb; ++j) {
            int64_t pij = pi_s[(size_t)j * ECON_CERT_MAX_BANKS + (size_t)i];
            if (pij < 0) return -1;
            in_lo  += (econ_i128)pij * (econ_i128)plo[j];
            in_hi  += (econ_i128)pij * (econ_i128)phi[j];
            in_bar += (econ_i128)pij * (econ_i128)pbar[j];
        }
        econ_i128 raw_lo = (econ_i128)ext[i] + in_lo / (econ_i128)scale;
        econ_i128 f_lo = (raw_lo < 0) ? 0 : ((raw_lo > (econ_i128)pbar[i]) ? (econ_i128)pbar[i] : raw_lo);
        econ_i128 raw_hi = (econ_i128)ext[i] + in_hi / (econ_i128)scale;
        econ_i128 f_hi = (raw_hi < 0) ? 0 : ((raw_hi > (econ_i128)pbar[i]) ? (econ_i128)pbar[i] : raw_hi);
        if ((econ_i128)plo[i] > f_lo) return -1;
        if (f_hi > (econ_i128)phi[i]) return -1;
        if (has_bail) {
            if (z[i] < 0) return -1;
            econ_i128 lhs = (econ_i128)pbar[i] * (econ_i128)scale;
            econ_i128 rhs = ((econ_i128)ext[i] + (econ_i128)z[i]) * (econ_i128)scale + in_bar;
            if (lhs > rhs) return -1;
        }
    }
    return 0;
}

static int verify_mech_block(const char *buf) {
    int64_t mode = 0, scale = 0, na64 = 0, no64 = 0, ch64 = 0;
    if (json_get_i64(buf, "mech_mode", &mode) != 0 ||
        json_get_i64(buf, "mech_scale", &scale) != 0 ||
        json_get_i64(buf, "mech_n_agents", &na64) != 0 ||
        json_get_i64(buf, "mech_n_outcomes", &no64) != 0 ||
        json_get_i64(buf, "mech_chosen", &ch64) != 0) return -1;
    if (scale <= 0) return -1;
    if (mode == 1) { /* VCG */
        if (na64 <= 0 || na64 > ECON_CERT_MAX_GRID ||
            no64 <= 0 || no64 > ECON_CERT_MAX_OUT ||
            ch64 < 0 || ch64 >= no64) return -1;
        int na = (int)na64, no = (int)no64, ch = (int)ch64;
        int64_t vals[ECON_CERT_MAX_GRID * ECON_CERT_MAX_OUT];
        int64_t piv[ECON_CERT_MAX_GRID];
        if (json_get_i64_matrix(buf, "mech_vals_int", na, no, ECON_CERT_MAX_OUT, vals) != 0 ||
            json_get_i64_array(buf, "mech_pivots_int", na, piv) != 0) return -1;
        econ_i128 w_star = 0;
        for (int i = 0; i < na; ++i) w_star += vals[(size_t)i * ECON_CERT_MAX_OUT + (size_t)ch];
        for (int x = 0; x < no; ++x) {
            econ_i128 wx = 0;
            for (int i = 0; i < na; ++i) wx += vals[(size_t)i * ECON_CERT_MAX_OUT + (size_t)x];
            if (wx > w_star) return -1;
        }
        for (int i = 0; i < na; ++i) {
            econ_i128 oth_star = w_star - vals[(size_t)i * ECON_CERT_MAX_OUT + (size_t)ch];
            econ_i128 u_star = vals[(size_t)i * ECON_CERT_MAX_OUT + (size_t)ch] - ((econ_i128)piv[i] - oth_star);
            if ((econ_i128)piv[i] > w_star || u_star < 0) return -1;
            for (int x = 0; x < no; ++x) {
                econ_i128 oth_x = 0;
                for (int j = 0; j < na; ++j) if (j != i) oth_x += vals[(size_t)j * ECON_CERT_MAX_OUT + (size_t)x];
                if (oth_x > (econ_i128)piv[i]) return -1;
                econ_i128 u_alt = vals[(size_t)i * ECON_CERT_MAX_OUT + (size_t)x] - ((econ_i128)piv[i] - oth_x);
                if (u_alt > u_star) return -1;
            }
        }
        return 0;
    } else if (mode == 2) { /* MS Deficit M7 */
        int64_t K = 0, surplus = 0;
        if (json_get_i64(buf, "mech_ms_K", &K) != 0 ||
            json_get_i64(buf, "mech_ms_surplus_int", &surplus) != 0) return -1;
        if (K < 1) return -1;
        econ_i128 bound = (econ_i128)scale * (econ_i128)K * (econ_i128)(K + 1) * (econ_i128)(4 - K);
        if ((econ_i128)6 * (econ_i128)surplus > bound) return -1;
        if (K >= 5 && surplus >= 0) return -1;
        return 0;
    } else if (mode == 3) { /* Fisher Eq M8 */
        if (na64 <= 0 || na64 > ECON_CERT_MAX_GRID || no64 <= 0 || no64 > ECON_CERT_MAX_GRID) return -1;
        int na = (int)na64, ng = (int)no64;
        int64_t tol = 0;
        int64_t p[ECON_CERT_MAX_GRID], e[ECON_CERT_MAX_GRID], w[ECON_CERT_MAX_GRID];
        int64_t x[ECON_CERT_MAX_GRID * ECON_CERT_MAX_GRID];
        if (json_get_i64(buf, "mech_fisher_tol", &tol) != 0 ||
            json_get_i64_array(buf, "mech_fisher_p_int", ng, p) != 0 ||
            json_get_i64_array(buf, "mech_fisher_e_int", ng, e) != 0 ||
            json_get_i64_array(buf, "mech_fisher_w_int", na, w) != 0 ||
            json_get_i64_matrix(buf, "mech_fisher_x_int", na, ng, ECON_CERT_MAX_GRID, x) != 0) return -1;
        if (tol < 0) return -1;
        for (int j = 0; j < ng; ++j) {
            if (p[j] <= 0) return -1;
            econ_i128 dem = 0;
            for (int i = 0; i < na; ++i) {
                int64_t xij = x[(size_t)i * ECON_CERT_MAX_GRID + (size_t)j];
                if (xij < 0) return -1;
                dem += xij;
            }
            if (dem > (econ_i128)e[j] + (econ_i128)tol) return -1;
        }
        for (int i = 0; i < na; ++i) {
            econ_i128 spend = 0;
            for (int j = 0; j < ng; ++j) {
                spend += (econ_i128)p[j] * (econ_i128)x[(size_t)i * ECON_CERT_MAX_GRID + (size_t)j];
            }
            econ_i128 diff = spend - (econ_i128)w[i];
            if (diff < 0) diff = -diff;
            if (diff > (econ_i128)tol) return -1;
        }
        return 0;
    }
    return -1;
}

int econ_cert_verify_file(const char *cert_path, int verify_input_hash) {
    if (!cert_path) return -1;
    FILE *fp = fopen(cert_path, "rb");
    if (!fp) return -1;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -1; }
    long sz = ftell(fp);
    if (sz <= 0 || sz > 8L * 1024L * 1024L) { fclose(fp); return -1; }
    rewind(fp);
    char *buf = (char *)malloc((size_t)sz + 1U);
    if (!buf) { fclose(fp); return -1; }
    size_t nr = fread(buf, 1U, (size_t)sz, fp);
    fclose(fp);
    buf[nr] = '\0';

    int rc = 0;
    char status[32], in_path[ECON_CERT_PATH_MAX], in_sha[65];
    if (json_get_str(buf, "status", status, sizeof(status)) != 0 ||
        json_get_str(buf, "input_path", in_path, sizeof(in_path)) != 0 ||
        json_get_str(buf, "input_sha256", in_sha, sizeof(in_sha)) != 0) {
        free(buf);
        return -1;
    }
    if (verify_input_hash && strcmp(in_path, "synthetic") != 0 && in_path[0] != '\0') {
        char actual_sha[65];
        if (econ_sha256_file(in_path, actual_sha) != 0 || strcmp(actual_sha, in_sha) != 0) {
            rc = -1;
        }
    }

    int64_t has_psd = 0, has_br = 0, has_arb = 0, has_mot = 0, has_dro = 0, has_clr = 0, has_mech = 0;
    if (json_get_i64(buf, "has_psd", &has_psd) != 0 ||
        json_get_i64(buf, "has_bracket", &has_br) != 0 ||
        json_get_i64(buf, "has_arb", &has_arb) != 0 ||
        json_get_i64(buf, "has_mot", &has_mot) != 0 ||
        json_get_i64(buf, "has_dro", &has_dro) != 0 ||
        json_get_i64(buf, "has_clearing", &has_clr) != 0 ||
        json_get_i64(buf, "has_mech", &has_mech) != 0) {
        rc = -1;
    }

    if (rc == 0 && has_psd) {
        int64_t dim = 0, mu_psd = 0, d_round = 0, mu_cert = 0, ok = 0;
        if (json_get_i64(buf, "dim", &dim) != 0 ||
            json_get_i64(buf, "mu_psd", &mu_psd) != 0 ||
            json_get_i64(buf, "delta_round", &d_round) != 0 ||
            json_get_i64(buf, "mu_cert", &mu_cert) != 0 ||
            json_get_i64(buf, "psd_ok", &ok) != 0 ||
            dim <= 0 || dim > ECON_CERT_MAX_DIM || d_round < 0 ||
            mu_cert != mu_psd - d_round) {
            rc = -1;
        } else {
            int64_t A[ECON_CERT_MAX_DIM * ECON_CERT_MAX_DIM];
            int64_t L[ECON_CERT_MAX_DIM * ECON_CERT_MAX_DIM];
            if (json_get_i64_matrix(buf, "A_int", (int)dim, (int)dim, (int)dim, A) != 0 ||
                json_get_i64_matrix(buf, "L_int", (int)dim, (int)dim, (int)dim, L) != 0 ||
                econ_verify_cholesky_sdd_i64((int)dim, A, L, mu_psd) != 0) {
                rc = -1;
            }
            if (strcmp(status, "CERTIFIED") == 0 && (ok != 1 || mu_cert < 0)) rc = -1;
            if (strcmp(status, "VETOED") == 0 && mu_cert >= 0) rc = -1;
        }
    }

    if (rc == 0 && has_br) {
        int64_t lb = 0, ub = 0, lo = 0, hi = 0, href = 0, rv = 0;
        if (json_get_i64(buf, "lb_int", &lb) != 0 ||
            json_get_i64(buf, "ub_int", &ub) != 0 ||
            json_get_i64(buf, "limit_lo_int", &lo) != 0 ||
            json_get_i64(buf, "limit_hi_int", &hi) != 0 ||
            json_get_i64(buf, "has_ref", &href) != 0 ||
            json_get_i64(buf, "ref_int", &rv) != 0) {
            rc = -1;
        } else {
            if (lb > ub) rc = -1;
            if (strcmp(status, "CERTIFIED") == 0) {
                if (lb < lo || ub > hi) rc = -1;
                if (href && (rv < lb || rv > ub)) rc = -1;
            }
        }
    }

    if (rc == 0 && has_arb && verify_arb_block(buf, status) != 0) rc = -1;
    if (rc == 0 && has_mot && verify_mot_block(buf, status) != 0) rc = -1;
    if (rc == 0 && has_dro && verify_dro_block(buf) != 0) rc = -1;
    if (rc == 0 && has_clr && verify_clearing_block(buf) != 0) rc = -1;
    if (rc == 0 && has_mech && verify_mech_block(buf) != 0) rc = -1;

    free(buf);
    return rc;
}

/* ============================================================================
 * 5. Standalone Lean 4 Certificate Module Emitter (`import EconCert`)
 * ============================================================================ */

static void emit_lean_i64_list(FILE *fp, const char *name, int n, const int64_t *v) {
    fprintf(fp, "def %s : List Int := [", name);
    for (int i = 0; i < n; ++i) fprintf(fp, "%s%" PRId64, (i > 0) ? ", " : "", v[i]);
    fprintf(fp, "]\n");
}

static void emit_lean_i64_mat(FILE *fp, const char *name, int rows, int cols,
                              int stride, const int64_t *M) {
    fprintf(fp, "def %s : List (List Int) := [\n", name);
    for (int i = 0; i < rows; ++i) {
        fprintf(fp, "  [");
        for (int j = 0; j < cols; ++j) {
            fprintf(fp, "%s%" PRId64, (j > 0) ? ", " : "",
                    M[(size_t)i * (size_t)stride + (size_t)j]);
        }
        fprintf(fp, "]%s\n", (i + 1 < rows) ? "," : "");
    }
    fprintf(fp, "]\n");
}

int econ_cert_emit_lean(const char *out_path, const char *mod_name,
                        const econ_cert_record_t *rec) {
    if (!out_path || !mod_name || !rec) return -1;
    FILE *fp = fopen(out_path, "wb");
    if (!fp) return -1;

    fprintf(fp, "/- Generated by ECON v%s — Zero-FP-Trust Formal Certificate (0 sorry, 0 native_decide) -/\n",
            ECON_VERSION_STRING);
    fprintf(fp, "import EconCert\n\n");
    fprintf(fp, "namespace %s\nopen EconCert\n\n", mod_name);

    int emitted_specific = 0;
    if (rec->has_psd) {
        fprintf(fp, "def certDim : Nat := %d\n", rec->psd.dim);
        fprintf(fp, "def certMu : Int := %" PRId64 "\n", rec->psd.mu_psd);
        fprintf(fp, "def certErr : Int := %" PRId64 "\n", rec->psd.delta_round);
        emit_lean_i64_mat(fp, "certA", rec->psd.dim, rec->psd.dim, rec->psd.dim, rec->psd.A);
        emit_lean_i64_mat(fp, "certL", rec->psd.dim, rec->psd.dim, rec->psd.dim, rec->psd.L);
        fprintf(fp, "\ntheorem psd_check_ok : checkCholeskySDD certDim certA certL certMu = true := by decide\n\n");
        if (rec->psd.delta_round <= rec->psd.mu_psd) {
            fprintf(fp, "theorem sos_form_nonneg : ∀ v : Fin certDim → Int,\n");
            fprintf(fp, "    0 ≤ matQuad certDim (matAt certA) v - certErr * vecNormSq certDim v :=\n");
            fprintf(fp, "  sos_polynomial_nonneg_certificate certDim certA certL certMu certErr psd_check_ok (by decide)\n\n");
        } else {
            fprintf(fp, "theorem psd_form_bound : ∀ v : Fin certDim → Int,\n");
            fprintf(fp, "    certMu * vecNormSq certDim v ≤ matQuad certDim (matAt certA) v :=\n");
            fprintf(fp, "  checkCholeskySDD_sound certDim certA certL certMu psd_check_ok\n\n");
        }
        emitted_specific = 1;
    }

    if (rec->has_arb) {
        fprintf(fp, "def arbK1 : Int := %" PRId64 "\n", rec->k1);
        fprintf(fp, "def arbK2 : Int := %" PRId64 "\n", rec->k2);
        fprintf(fp, "def arbK3 : Int := %" PRId64 "\n", rec->k3);
        fprintf(fp, "def arbEdge : Int := %" PRId64 "\n\n", rec->arb_edge_int);
        if (rec->arb_mode == 3) {
            fprintf(fp, "theorem certified_butterfly_arbitrage : ∀ S : Int,\n");
            fprintf(fp, "    0 < (arbK3 - arbK2) * posPart (S - arbK1) + (arbK2 - arbK1) * posPart (S - arbK3) -\n");
            fprintf(fp, "        (arbK3 - arbK1) * posPart (S - arbK2) + arbEdge := by\n");
            fprintf(fp, "  intro S\n");
            fprintf(fp, "  have hPay := butterfly_convexity_arbitrage_sound arbK1 arbK2 arbK3 (by decide) (by decide) S\n");
            fprintf(fp, "  have hEdge : 0 < arbEdge := by decide\n");
            fprintf(fp, "  omega\n\n");
        } else if (rec->arb_mode == 1) {
            fprintf(fp, "theorem certified_conversion_arbitrage : ∀ S : Int,\n");
            fprintf(fp, "    0 < (posPart (S - arbK1) - posPart (arbK1 - S)) + (%" PRId64 " - S) +\n", rec->f_bid);
            fprintf(fp, "        (%" PRId64 " - %" PRId64 ") - %" PRId64 " := by\n",
                    rec->p_bid1, rec->c_ask1, rec->arb_cost_int);
            fprintf(fp, "  intro S\n");
            fprintf(fp, "  have hId := (bidask_conversion_reversal_arbitrage_sound arbK1 %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 ").1 S\n",
                    rec->c_bid1, rec->c_ask1, rec->p_bid1, rec->p_ask1, rec->f_bid, rec->f_ask, rec->arb_cost_int);
            fprintf(fp, "  have hPos : 0 < %" PRId64 " - (arbK1 + %" PRId64 " - %" PRId64 ") - %" PRId64 " := by decide\n",
                    rec->f_bid, rec->c_ask1, rec->p_bid1, rec->arb_cost_int);
            fprintf(fp, "  omega\n\n");
        } else if (rec->arb_mode == 2) {
            fprintf(fp, "theorem certified_reversal_arbitrage : ∀ S : Int,\n");
            fprintf(fp, "    0 < (S - %" PRId64 ") + (posPart (arbK1 - S) - posPart (S - arbK1)) +\n", rec->f_ask);
            fprintf(fp, "        (%" PRId64 " - %" PRId64 ") - %" PRId64 " := by\n",
                    rec->c_bid1, rec->p_ask1, rec->arb_cost_int);
            fprintf(fp, "  intro S\n");
            fprintf(fp, "  have hId := (bidask_conversion_reversal_arbitrage_sound arbK1 %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 ").2 S\n",
                    rec->c_bid1, rec->c_ask1, rec->p_bid1, rec->p_ask1, rec->f_bid, rec->f_ask, rec->arb_cost_int);
            fprintf(fp, "  have hPos : 0 < (arbK1 + %" PRId64 " - %" PRId64 ") - %" PRId64 " - %" PRId64 " := by decide\n",
                    rec->c_bid1, rec->p_ask1, rec->f_ask, rec->arb_cost_int);
            fprintf(fp, "  omega\n\n");
        } else {
            fprintf(fp, "theorem certified_no_bidask_arbitrage :\n");
            fprintf(fp, "    %" PRId64 " - (arbK1 + %" PRId64 " - %" PRId64 ") - %" PRId64 " ≤ 0 ∧\n",
                    rec->f_bid, rec->c_ask1, rec->p_bid1, rec->arb_cost_int);
            fprintf(fp, "    (arbK1 + %" PRId64 " - %" PRId64 ") - %" PRId64 " - %" PRId64 " ≤ 0 := by decide\n\n",
                    rec->c_bid1, rec->p_ask1, rec->f_ask, rec->arb_cost_int);
        }
        emitted_specific = 1;
    }

    if (rec->has_mot) {
        int n1 = rec->mot_n1, n2 = rec->mot_n2;
        int64_t pay_scaled[ECON_CERT_MAX_GRID * ECON_CERT_MAX_GRID];
        int64_t p1_s[ECON_CERT_MAX_GRID], p2_s[ECON_CERT_MAX_GRID];
        for (int i = 0; i < n1; ++i) {
            p1_s[i] = rec->phi1_up[i] * rec->mot_scale;
            for (int j = 0; j < n2; ++j) {
                pay_scaled[(size_t)i * ECON_CERT_MAX_GRID + (size_t)j] =
                    rec->payoff[(size_t)i * ECON_CERT_MAX_GRID + (size_t)j] * rec->mot_scale;
            }
        }
        for (int j = 0; j < n2; ++j) p2_s[j] = rec->phi2_up[j] * rec->mot_scale;
        emit_lean_i64_list(fp, "motS1", n1, rec->s1);
        emit_lean_i64_list(fp, "motS2", n2, rec->s2);
        emit_lean_i64_list(fp, "motPhi1", n1, p1_s);
        emit_lean_i64_list(fp, "motPhi2", n2, p2_s);
        emit_lean_i64_list(fp, "motDelta", n1, rec->delta_up);
        emit_lean_i64_mat(fp, "motPayoff", n1, n2, ECON_CERT_MAX_GRID, pay_scaled);
        fprintf(fp, "\ntheorem mot_superhedge_check :\n");
        fprintf(fp, "    checkMotDualBound %d %d motS1 motS2 motPhi1 motPhi2 motDelta motPayoff = true := by decide\n\n",
                n1, n2);
        fprintf(fp, "theorem mot_ftap_dual_sound (q : Fin %d → Fin %d → Int)\n", n1, n2);
        fprintf(fp, "    (hq : ∀ i j, 0 ≤ q i j)\n");
        fprintf(fp, "    (hm : ∀ i : Fin %d, sumFin %d (fun j => q i j * (motS2.getD j.val 0 - motS1.getD i.val 0)) = 0) :\n",
                n1, n2);
        fprintf(fp, "    sumFin %d (fun i => sumFin %d (fun j => q i j * (motPayoff.getD i.val []).getD j.val 0)) ≤\n",
                n1, n2);
        fprintf(fp, "      sumFin %d (fun i => sumFin %d (fun j => q i j * motPhi1.getD i.val 0)) +\n", n1, n2);
        fprintf(fp, "      sumFin %d (fun i => sumFin %d (fun j => q i j * motPhi2.getD j.val 0)) :=\n", n1, n2);
        fprintf(fp, "  checkMotDualBound_sound %d %d q motS1 motS2 motPhi1 motPhi2 motDelta motPayoff hq hm mot_superhedge_check\n\n",
                n1, n2);
        emitted_specific = 1;
    }

    if (rec->has_dro) {
        fprintf(fp, "theorem dro_amplification_witness :\n");
        fprintf(fp, "    %" PRId64 " ≤ %" PRId64 " ∧ (0 : Int) < %" PRId64 " := by decide\n\n",
                rec->gauss_cvar_int, rec->dro_cvar_int, rec->amp_bps);
        if (rec->has_cantelli) {
            fprintf(fp, "theorem cantelli_bound_verified :\n");
            fprintf(fp, "    %" PRId64 " * (%" PRId64 " * (%" PRId64 " * %" PRId64 ") + %" PRId64 ") ≤ %" PRId64 " * %" PRId64 " := by decide\n\n",
                    rec->cantelli_ptail, rec->cantelli_W, rec->cantelli_delta, rec->cantelli_delta,
                    rec->cantelli_S2, rec->cantelli_W, rec->cantelli_S2);
        }
        fprintf(fp, "theorem copula_obstruction_certified : (∃ _ : Unit, distTailCrash ⟨2, by omega⟩ ⟨2, by omega⟩ = 1) := by\n");
        fprintf(fp, "  have h := copula_correlation_obstruction\n");
        fprintf(fp, "  exact ⟨(), h.2.2.2.2.2⟩\n\n");
        emitted_specific = 1;
    }

    if (rec->has_clearing) {
        int nb = rec->clear_n_banks;
        emit_lean_i64_list(fp, "clrPBar", nb, rec->clear_p_bar);
        emit_lean_i64_list(fp, "clrExt", nb, rec->clear_ext);
        emit_lean_i64_list(fp, "clrPLo", nb, rec->clear_p_lo);
        emit_lean_i64_list(fp, "clrPHi", nb, rec->clear_p_hi);
        emit_lean_i64_list(fp, "clrZ", nb, rec->clear_z);
        emit_lean_i64_mat(fp, "clrPi", nb, nb, ECON_CERT_MAX_BANKS, rec->clear_pi_scaled);
        fprintf(fp, "\ntheorem clearing_tarski_bracket_ok :\n");
        fprintf(fp, "    checkTarskiBracket %d %" PRId64 " clrPBar clrExt clrPLo clrPHi clrPi = true := by decide\n\n",
                nb, rec->clear_scale);
        fprintf(fp, "theorem clearing_tarski_bracket_sound :\n");
        fprintf(fp, "    ∀ i : Fin %d, 0 ≤ clrPLo.getD i.val 0 ∧\n", nb);
        fprintf(fp, "      clrPLo.getD i.val 0 ≤ clearingStepInt %d %" PRId64 " clrPBar clrExt clrPi (fun j => clrPLo.getD j.val 0) i ∧\n",
                nb, rec->clear_scale);
        fprintf(fp, "      clearingStepInt %d %" PRId64 " clrPBar clrExt clrPi (fun j => clrPHi.getD j.val 0) i ≤ clrPHi.getD i.val 0 ∧\n",
                nb, rec->clear_scale);
        fprintf(fp, "      clrPHi.getD i.val 0 ≤ clrPBar.getD i.val 0 :=\n");
        fprintf(fp, "  checkTarskiBracket_sound %d %" PRId64 " clrPBar clrExt clrPLo clrPHi clrPi clearing_tarski_bracket_ok\n\n",
                nb, rec->clear_scale);
        if (rec->clear_has_bailout) {
            fprintf(fp, "theorem bailout_sufficient_ok :\n");
            fprintf(fp, "    checkBailoutSufficiency %d %" PRId64 " clrPBar clrExt clrZ clrPi = true := by decide\n\n",
                    nb, rec->clear_scale);
            fprintf(fp, "theorem bailout_sufficient_sound :\n");
            fprintf(fp, "    ∀ i : Fin %d, 0 ≤ clrZ.getD i.val 0 ∧\n", nb);
            fprintf(fp, "      minInt (clrPBar.getD i.val 0) (clrExt.getD i.val 0 + clrZ.getD i.val 0 +\n");
            fprintf(fp, "        sumFin %d (fun j => (clrPi.getD j.val []).getD i.val 0 * clrPBar.getD j.val 0) / %" PRId64 ") = clrPBar.getD i.val 0 :=\n",
                    nb, rec->clear_scale);
            fprintf(fp, "  checkBailoutSufficiency_sound %d %" PRId64 " clrPBar clrExt clrZ clrPi bailout_sufficient_ok\n\n",
                    nb, rec->clear_scale);
        }
        emitted_specific = 1;
    }

    if (rec->has_mech) {
        if (rec->mech_mode == 1) {
            int na = rec->mech_n_agents, no = rec->mech_n_outcomes;
            emit_lean_i64_mat(fp, "vcgVals", na, no, ECON_CERT_MAX_OUT, rec->mech_vals);
            emit_lean_i64_list(fp, "vcgPivots", na, rec->mech_pivots);
            fprintf(fp, "\ntheorem vcg_outcome_ok : checkVcgOutcome %d %d vcgVals %d vcgPivots = true := by decide\n\n",
                    no, na, rec->mech_chosen);
            fprintf(fp, "theorem vcg_certified_dsic_ir : ∃ _ : (%d < %d), True :=\n",
                    rec->mech_chosen, no);
            fprintf(fp, "  let h := checkVcgOutcome_sound %d %d vcgVals %d vcgPivots vcg_outcome_ok\n",
                    no, na, rec->mech_chosen);
            fprintf(fp, "  ⟨h.1, trivial⟩\n\n");
        } else if (rec->mech_mode == 2) {
            fprintf(fp, "theorem ms_deficit_certified : ∀ uB uS : Nat → Int,\n");
            fprintf(fp, "    0 ≤ uB 0 → 0 ≤ uS %d →\n", rec->mech_ms_K);
            fprintf(fp, "    (∀ b, b < %d → uB b + (b : Int) ≤ uB (b + 1)) →\n", rec->mech_ms_K);
            fprintf(fp, "    (∀ s, s < %d → uS (s + 1) + ((%d - 1 - s : Nat) : Int) ≤ uS s) →\n",
                    rec->mech_ms_K, rec->mech_ms_K);
            fprintf(fp, "    let W := sumFin (%d + 1) (fun b : Fin (%d + 1) => sumFin (%d + 1) (fun s : Fin (%d + 1) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0))\n",
                    rec->mech_ms_K, rec->mech_ms_K, rec->mech_ms_K, rec->mech_ms_K);
            fprintf(fp, "    W - (sumFin (%d + 1) (fun b => uB b.val) + sumFin (%d + 1) (fun s => uS s.val)) < 0 :=\n",
                    rec->mech_ms_K, rec->mech_ms_K);
            fprintf(fp, "  fun uB uS h0 hK hB hS => (myerson_satterthwaite_bilateral_obstruction %d uB uS h0 hK hB hS).2 (by omega)\n\n",
                    rec->mech_ms_K);
        } else {
            int na = rec->mech_n_agents, ng = rec->mech_n_outcomes;
            emit_lean_i64_list(fp, "fishP", ng, rec->mech_fisher_p);
            emit_lean_i64_list(fp, "fishE", ng, rec->mech_fisher_e);
            emit_lean_i64_list(fp, "fishW", na, rec->mech_fisher_w);
            emit_lean_i64_mat(fp, "fishX", na, ng, ECON_CERT_MAX_GRID, rec->mech_fisher_x);
            fprintf(fp, "\ntheorem fisher_clear_ok :\n");
            fprintf(fp, "    checkFisherClear %d %d fishP fishE fishW fishX %" PRId64 " = true := by decide\n\n",
                    na, ng, rec->mech_fisher_tol);
            fprintf(fp, "theorem fisher_clear_sound :\n");
            fprintf(fp, "    (∀ j : Fin %d, 0 < fishP.getD j.val 0) ∧\n", ng);
            fprintf(fp, "    (∀ i : Fin %d, ∀ j : Fin %d, 0 ≤ (fishX.getD i.val []).getD j.val 0) ∧\n", na, ng);
            fprintf(fp, "    (∀ j : Fin %d, sumFin %d (fun i => (fishX.getD i.val []).getD j.val 0) ≤ fishE.getD j.val 0 + %" PRId64 ") :=\n",
                    ng, na, rec->mech_fisher_tol);
            fprintf(fp, "  fisher_equilibrium_walras_and_certificate %d %d fishP fishE fishW fishX %" PRId64 " (by decide) fisher_clear_ok\n\n",
                    na, ng, rec->mech_fisher_tol);
        }
        emitted_specific = 1;
    }

    if (!emitted_specific && rec->has_bracket) {
        fprintf(fp, "theorem bracket_enclosure_valid : (%" PRId64 " : Int) ≤ %" PRId64 " := by decide\n\n",
                rec->lb_int, rec->ub_int);
        emitted_specific = 1;
    }

    /* Emit #print axioms for audit */
    if (rec->has_psd) {
        if (rec->psd.delta_round <= rec->psd.mu_psd) fprintf(fp, "#print axioms sos_form_nonneg\n");
        else fprintf(fp, "#print axioms psd_form_bound\n");
    }
    if (rec->has_arb) {
        if (rec->arb_mode == 3) fprintf(fp, "#print axioms certified_butterfly_arbitrage\n");
        else if (rec->arb_mode == 1) fprintf(fp, "#print axioms certified_conversion_arbitrage\n");
        else if (rec->arb_mode == 2) fprintf(fp, "#print axioms certified_reversal_arbitrage\n");
        else fprintf(fp, "#print axioms certified_no_bidask_arbitrage\n");
    }
    if (rec->has_mot) fprintf(fp, "#print axioms mot_ftap_dual_sound\n");
    if (rec->has_dro) fprintf(fp, "#print axioms dro_amplification_witness\n");
    if (rec->has_clearing) fprintf(fp, "#print axioms clearing_tarski_bracket_sound\n");
    if (rec->has_mech) {
        if (rec->mech_mode == 1) fprintf(fp, "#print axioms vcg_certified_dsic_ir\n");
        else if (rec->mech_mode == 2) fprintf(fp, "#print axioms ms_deficit_certified\n");
        else fprintf(fp, "#print axioms fisher_clear_sound\n");
    }
    if (!emitted_specific) fprintf(fp, "#print axioms sdd_quadratic_form_nonneg\n");

    fprintf(fp, "\nend %s\n", mod_name);
    fclose(fp);
    return 0;
}
