/* SPDX-License-Identifier: MIT */
#ifndef ECON_WIRE_H
#define ECON_WIRE_H

#include "flow_wire_pack.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * ECON_WIRE — Freestanding Canonical Packed Binary Wire Codec ("ECN2")
 * ============================================================================
 * Decouples bulky JSON certificates (~770 B) from compact wire payloads (~70 B)
 * via FLOW/sdk/flow_wire_pack.h canonical ULEB128 varints, zigzag signed
 * bps/margins, 32-byte raw SHA-256 digest, and FNV-1a64 integrity trailer.
 *
 * Freestanding Pure-GPR C17 & C++20 clean: zero heap, zero libc/stdio/string,
 * zero division, zero FPU/SIMD, zero indirect calls, zero jump tables.
 * ============================================================================ */

#define ECON_WIRE_MAGIC        UINT32_C(0x324E4345) /* "ECN2" */
#define ECON_WIRE_VERSION      UINT8_C(1)
#define ECON_WIRE_HEADER_BYTES 5U
#define ECON_WIRE_DIGEST_BYTES 32U
#define ECON_WIRE_TAG_BYTES    8U
/* 5B hdr + 5*1B varu32 + 11*1B varu64 + 32B sha256 + 8B FNV64 = 61B min */
#define ECON_WIRE_MIN_BYTES    61U
/* 5B hdr + 5*5B varu32 + 11*10B varu64 + 32B sha256 + 8B FNV64 = 180B worst-case */
#define ECON_WIRE_MAX_BYTES    192U

/* Pillar IDs */
#define ECON_WIRE_PILLAR_INST     UINT32_C(0)
#define ECON_WIRE_PILLAR_FIN      UINT32_C(1)
#define ECON_WIRE_PILLAR_ACTUARY  UINT32_C(2)
#define ECON_WIRE_PILLAR_CLEARING UINT32_C(3)
#define ECON_WIRE_PILLAR_MECH     UINT32_C(4)
#define ECON_WIRE_PILLAR_AGENCY   UINT32_C(5)
#define ECON_WIRE_PILLAR_ADVERSE  UINT32_C(6)
#define ECON_WIRE_PILLAR_MARGIN   UINT32_C(7)

/* Certificate Kind IDs */
#define ECON_WIRE_KIND_INST       UINT32_C(0)
#define ECON_WIRE_KIND_ARB_SCAN   UINT32_C(1)
#define ECON_WIRE_KIND_MOT_SOS    UINT32_C(2)
#define ECON_WIRE_KIND_RISK_DRO   UINT32_C(3)
#define ECON_WIRE_KIND_RUIN_BOUND UINT32_C(4)
#define ECON_WIRE_KIND_CLEARING   UINT32_C(5)
#define ECON_WIRE_KIND_VCG        UINT32_C(6)
#define ECON_WIRE_KIND_MS_DEFICIT UINT32_C(7)
#define ECON_WIRE_KIND_FISHER     UINT32_C(8)
#define ECON_WIRE_KIND_AGENCY     UINT32_C(9)
#define ECON_WIRE_KIND_ADVERSE    UINT32_C(10)
#define ECON_WIRE_KIND_MARGIN     UINT32_C(11)
#define ECON_WIRE_KIND_SUPPLY     UINT32_C(12)

/* Verdict Status Codes */
#define ECON_WIRE_STATUS_VETOED      UINT32_C(0)
#define ECON_WIRE_STATUS_CERTIFIED   UINT32_C(1)
#define ECON_WIRE_STATUS_ARBITRAGE   UINT32_C(2)
#define ECON_WIRE_STATUS_OBSTRUCTION UINT32_C(3)

/* Weakest Institutional Margin Dimension */
#define ECON_WIRE_WEAK_BALANCED UINT32_C(0)
#define ECON_WIRE_WEAK_INC      UINT32_C(1)
#define ECON_WIRE_WEAK_DRO      UINT32_C(2)
#define ECON_WIRE_WEAK_SYS      UINT32_C(3)

/* Wire Flags */
#define ECON_WIRE_FLAG_CERTIFIED  UINT32_C(1)
#define ECON_WIRE_FLAG_HAS_INST   UINT32_C(2)
#define ECON_WIRE_FLAG_HAS_SHA256 UINT32_C(4)

typedef struct {
    uint32_t pillar_id;
    uint32_t kind_id;
    uint32_t status_code;
    uint32_t weakest_dim;
    uint32_t flags;
    uint32_t pad;
    int64_t scale;
    int64_t m_inc;
    int64_t m_dro;
    int64_t m_sys;
    int64_t m_star;
    int64_t target_bps;
    int64_t repair_cost;
    int64_t pillar_metrics[4];
    uint8_t sha256_digest[ECON_WIRE_DIGEST_BYTES];
    uint64_t tag;
} econ_wire_cert_t;

static inline uint64_t econ_wire_zigzag_encode(int64_t v) {
    uint64_t uv = (uint64_t)v;
    uint64_t mask = (v < 0) ? ~UINT64_C(0) : UINT64_C(0);
    return (uv << 1U) ^ mask;
}

static inline int64_t econ_wire_zigzag_decode(uint64_t zz) {
    uint64_t mask = UINT64_C(0) - (zz & UINT64_C(1));
    return (int64_t)((zz >> 1U) ^ mask);
}

static inline int econ_wire__hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return (int)(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (int)(c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (int)(c - 'A');
    }
    return -1;
}

static inline int econ_wire_hex_to_digest32(const char *hex,
                                            uint8_t out[ECON_WIRE_DIGEST_BYTES]) {
    if (!hex || !out) {
        return -1;
    }
    for (size_t i = 0U; i < ECON_WIRE_DIGEST_BYTES; ++i) {
        int hi = econ_wire__hex_nibble(hex[i * 2U]);
        int lo = econ_wire__hex_nibble(hex[i * 2U + 1U]);
        if (hi < 0 || lo < 0) {
            return -1;
        }
        out[i] = (uint8_t)(((uint32_t)hi << 4U) | (uint32_t)lo);
    }
    return (hex[64] == '\0') ? 0 : -1;
}

static inline void econ_wire_digest32_to_hex(const uint8_t digest[ECON_WIRE_DIGEST_BYTES],
                                             char hex_out[65]) {
    static const char hex_tab[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    if (!digest || !hex_out) {
        return;
    }
    for (size_t i = 0U; i < ECON_WIRE_DIGEST_BYTES; ++i) {
        uint8_t b = digest[i];
        hex_out[i * 2U] = hex_tab[(b >> 4U) & 0x0FU];
        hex_out[i * 2U + 1U] = hex_tab[b & 0x0FU];
    }
    hex_out[64] = '\0';
}

static inline int econ_wire__validate_cert(const econ_wire_cert_t *cert) {
    if (!cert) {
        return -1;
    }
    if (cert->pillar_id > ECON_WIRE_PILLAR_MARGIN ||
        cert->kind_id > ECON_WIRE_KIND_SUPPLY ||
        cert->status_code > ECON_WIRE_STATUS_OBSTRUCTION ||
        cert->weakest_dim > ECON_WIRE_WEAK_SYS) {
        return -1;
    }
    bool flag_cert = (cert->flags & ECON_WIRE_FLAG_CERTIFIED) != UINT32_C(0);
    if (flag_cert != (cert->status_code == ECON_WIRE_STATUS_CERTIFIED)) {
        return -1;
    }
    if ((cert->flags & ECON_WIRE_FLAG_HAS_INST) != UINT32_C(0)) {
        if (cert->scale <= 0 || cert->repair_cost < 0) {
            return -1;
        }
        int64_t min2 = (cert->m_dro < cert->m_sys) ? cert->m_dro : cert->m_sys;
        int64_t calc_star = (cert->m_inc < min2) ? cert->m_inc : min2;
        if (cert->m_star != calc_star) {
            return -1;
        }
        if (cert->status_code == ECON_WIRE_STATUS_CERTIFIED && cert->m_star <= 0) {
            return -1;
        }
        if (cert->status_code == ECON_WIRE_STATUS_VETOED && cert->m_star > 0) {
            return -1;
        }
        if (cert->weakest_dim == ECON_WIRE_WEAK_INC && cert->m_inc != cert->m_star) {
            return -1;
        }
        if (cert->weakest_dim == ECON_WIRE_WEAK_DRO && cert->m_dro != cert->m_star) {
            return -1;
        }
        if (cert->weakest_dim == ECON_WIRE_WEAK_SYS && cert->m_sys != cert->m_star) {
            return -1;
        }
        if (cert->target_bps > 0 && cert->m_star >= cert->target_bps && cert->repair_cost != 0) {
            return -1;
        }
    }
    return 0;
}

static inline int econ_wire_encode_cert(const econ_wire_cert_t *cert, uint8_t *out, size_t out_cap,
                                        size_t *out_len) {
    if (!cert || !out || !out_len) {
        return -1;
    }
    if (econ_wire__validate_cert(cert) != 0) {
        return -1;
    }
    if (out_cap < (size_t)ECON_WIRE_MIN_BYTES) {
        return -1;
    }

    size_t body_cap = out_cap - (size_t)ECON_WIRE_TAG_BYTES;
    flow_ir_wire_write_le32(out + 0U, ECON_WIRE_MAGIC);
    out[4] = ECON_WIRE_VERSION;
    size_t pos = (size_t)ECON_WIRE_HEADER_BYTES;

    if (!flow_wire_put_varu32(out, body_cap, &pos, cert->pillar_id) ||
        !flow_wire_put_varu32(out, body_cap, &pos, cert->kind_id) ||
        !flow_wire_put_varu32(out, body_cap, &pos, cert->status_code) ||
        !flow_wire_put_varu32(out, body_cap, &pos, cert->weakest_dim) ||
        !flow_wire_put_varu32(out, body_cap, &pos, cert->flags)) {
        return -1;
    }

    if (!flow_wire_put_varu64(out, body_cap, &pos, econ_wire_zigzag_encode(cert->scale)) ||
        !flow_wire_put_varu64(out, body_cap, &pos, econ_wire_zigzag_encode(cert->m_inc)) ||
        !flow_wire_put_varu64(out, body_cap, &pos, econ_wire_zigzag_encode(cert->m_dro)) ||
        !flow_wire_put_varu64(out, body_cap, &pos, econ_wire_zigzag_encode(cert->m_sys)) ||
        !flow_wire_put_varu64(out, body_cap, &pos, econ_wire_zigzag_encode(cert->m_star)) ||
        !flow_wire_put_varu64(out, body_cap, &pos, econ_wire_zigzag_encode(cert->target_bps)) ||
        !flow_wire_put_varu64(out, body_cap, &pos, econ_wire_zigzag_encode(cert->repair_cost))) {
        return -1;
    }

    for (size_t i = 0U; i < 4U; ++i) {
        if (!flow_wire_put_varu64(out, body_cap, &pos,
                                  econ_wire_zigzag_encode(cert->pillar_metrics[i]))) {
            return -1;
        }
    }

    if (pos + (size_t)ECON_WIRE_DIGEST_BYTES > body_cap) {
        return -1;
    }
    for (size_t i = 0U; i < (size_t)ECON_WIRE_DIGEST_BYTES; ++i) {
        out[pos + i] = cert->sha256_digest[i];
    }
    pos += (size_t)ECON_WIRE_DIGEST_BYTES;

    if (pos + (size_t)ECON_WIRE_TAG_BYTES > (size_t)ECON_WIRE_MAX_BYTES) {
        return -1;
    }
    *out_len = flow_ir_wire_seal_trailer(out, pos);
    return 0;
}

static inline int econ_wire_decode_cert(const uint8_t *in, size_t in_len, econ_wire_cert_t *out) {
    if (!in || !out) {
        return -1;
    }
    if (in_len < (size_t)ECON_WIRE_MIN_BYTES || in_len > (size_t)ECON_WIRE_MAX_BYTES) {
        return -1;
    }
    if (flow_ir_wire_read_le32(in + 0U) != ECON_WIRE_MAGIC || in[4] != ECON_WIRE_VERSION) {
        return -1;
    }

    size_t body_len = in_len - (size_t)ECON_WIRE_TAG_BYTES;
    uint64_t expected_tag = flow_ir_wire_fnv1a64(in, body_len);
    uint64_t got_tag = flow_ir_wire_read_le64(in + body_len);
    if (got_tag != expected_tag) {
        return -1;
    }

    econ_wire_cert_t tmp;
    flow_ir_wire_zero(&tmp, sizeof(tmp));

    size_t pos = (size_t)ECON_WIRE_HEADER_BYTES;
    if (!flow_wire_get_varu32(in, body_len, &pos, &tmp.pillar_id) ||
        !flow_wire_get_varu32(in, body_len, &pos, &tmp.kind_id) ||
        !flow_wire_get_varu32(in, body_len, &pos, &tmp.status_code) ||
        !flow_wire_get_varu32(in, body_len, &pos, &tmp.weakest_dim) ||
        !flow_wire_get_varu32(in, body_len, &pos, &tmp.flags)) {
        return -1;
    }

    uint64_t zz = UINT64_C(0);
    if (!flow_wire_get_varu64(in, body_len, &pos, &zz)) return -1;
    tmp.scale = econ_wire_zigzag_decode(zz);
    if (!flow_wire_get_varu64(in, body_len, &pos, &zz)) return -1;
    tmp.m_inc = econ_wire_zigzag_decode(zz);
    if (!flow_wire_get_varu64(in, body_len, &pos, &zz)) return -1;
    tmp.m_dro = econ_wire_zigzag_decode(zz);
    if (!flow_wire_get_varu64(in, body_len, &pos, &zz)) return -1;
    tmp.m_sys = econ_wire_zigzag_decode(zz);
    if (!flow_wire_get_varu64(in, body_len, &pos, &zz)) return -1;
    tmp.m_star = econ_wire_zigzag_decode(zz);
    if (!flow_wire_get_varu64(in, body_len, &pos, &zz)) return -1;
    tmp.target_bps = econ_wire_zigzag_decode(zz);
    if (!flow_wire_get_varu64(in, body_len, &pos, &zz)) return -1;
    tmp.repair_cost = econ_wire_zigzag_decode(zz);

    for (size_t i = 0U; i < 4U; ++i) {
        if (!flow_wire_get_varu64(in, body_len, &pos, &zz)) {
            return -1;
        }
        tmp.pillar_metrics[i] = econ_wire_zigzag_decode(zz);
    }

    if (pos + (size_t)ECON_WIRE_DIGEST_BYTES != body_len) {
        return -1;
    }
    for (size_t i = 0U; i < (size_t)ECON_WIRE_DIGEST_BYTES; ++i) {
        tmp.sha256_digest[i] = in[pos + i];
    }
    tmp.tag = got_tag;

    if (econ_wire__validate_cert(&tmp) != 0) {
        return -1;
    }

    *out = tmp;
    return 0;
}

static inline uint64_t econ_wire_compute_tag(const econ_wire_cert_t *cert) {
    uint8_t tmp[ECON_WIRE_MAX_BYTES];
    size_t out_len = 0U;
    if (econ_wire_encode_cert(cert, tmp, sizeof(tmp), &out_len) != 0 ||
        out_len < (size_t)ECON_WIRE_TAG_BYTES) {
        return UINT64_C(0);
    }
    return flow_ir_wire_read_le64(tmp + (out_len - (size_t)ECON_WIRE_TAG_BYTES));
}

#ifdef __cplusplus
}
#endif

#endif /* ECON_WIRE_H */
