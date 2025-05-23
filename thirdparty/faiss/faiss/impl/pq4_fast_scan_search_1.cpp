/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <faiss/impl/pq4_fast_scan.h>

#include <faiss/impl/FaissAssert.h>
#include <faiss/impl/LookupTableScaler.h>
#include <faiss/impl/simd_result_handlers.h>

namespace faiss {

using namespace simd_result_handlers;

/***************************************************************
 * accumulation functions
 ***************************************************************/

namespace {

/*
 * The computation kernel
 * It accumulates results for NQ queries and BB * 32 database elements
 * writes results in a ResultHandler
 */

template <int NQ, int BB, class ResultHandler, class Scaler>
void kernel_accumulate_block(
        int nsq,
        const uint8_t* codes,
        const uint8_t* LUT,
        ResultHandler& res,
        const Scaler& scaler) {
    // distance accumulators
    simd16uint16 accu[NQ][BB][4];

    for (int q = 0; q < NQ; q++) {
        for (int b = 0; b < BB; b++) {
            accu[q][b][0].clear();
            accu[q][b][1].clear();
            accu[q][b][2].clear();
            accu[q][b][3].clear();
        }
    }

    for (int sq = 0; sq < nsq - scaler.nscale; sq += 2) {
        simd32uint8 lut_cache[NQ];
        for (int q = 0; q < NQ; q++) {
            lut_cache[q] = simd32uint8(LUT);
            LUT += 32;
        }

        for (int b = 0; b < BB; b++) {
            simd32uint8 c = simd32uint8(codes);
            codes += 32;
            simd32uint8 mask(15);
            simd32uint8 chi = simd32uint8(simd16uint16(c) >> 4) & mask;
            simd32uint8 clo = c & mask;

            for (int q = 0; q < NQ; q++) {
                simd32uint8 lut = lut_cache[q];
                simd32uint8 res0 = lut.lookup_2_lanes(clo);
                simd32uint8 res1 = lut.lookup_2_lanes(chi);

                accu[q][b][0] += simd16uint16(res0);
                accu[q][b][1] += simd16uint16(res0) >> 8;

                accu[q][b][2] += simd16uint16(res1);
                accu[q][b][3] += simd16uint16(res1) >> 8;
            }
        }
    }

    for (int sq = 0; sq < scaler.nscale; sq += 2) {
        simd32uint8 lut_cache[NQ];
        for (int q = 0; q < NQ; q++) {
            lut_cache[q] = simd32uint8(LUT);
            LUT += 32;
        }

        for (int b = 0; b < BB; b++) {
            simd32uint8 c = simd32uint8(codes);
            codes += 32;
            simd32uint8 mask(15);
            simd32uint8 chi = simd32uint8(simd16uint16(c) >> 4) & mask;
            simd32uint8 clo = c & mask;

            for (int q = 0; q < NQ; q++) {
                simd32uint8 lut = lut_cache[q];

                simd32uint8 res0 = scaler.lookup(lut, clo);
                accu[q][b][0] += scaler.scale_lo(res0); // handle vectors 0..7
                accu[q][b][1] += scaler.scale_hi(res0); // handle vectors 8..15

                simd32uint8 res1 = scaler.lookup(lut, chi);
                accu[q][b][2] += scaler.scale_lo(res1); // handle vectors 16..23
                accu[q][b][3] +=
                        scaler.scale_hi(res1); //  handle vectors 24..31
            }
        }
    }

    for (int q = 0; q < NQ; q++) {
        for (int b = 0; b < BB; b++) {
            accu[q][b][0] -= accu[q][b][1] << 8;
            simd16uint16 dis0 = combine2x2(accu[q][b][0], accu[q][b][1]);

            accu[q][b][2] -= accu[q][b][3] << 8;
            simd16uint16 dis1 = combine2x2(accu[q][b][2], accu[q][b][3]);

            res.handle(q, b, dis0, dis1);
        }
    }
}

template <int NQ, int BB, class ResultHandler, class Scaler>
void accumulate_fixed_blocks(
        size_t nb,
        int nsq,
        const uint8_t* codes,
        const uint8_t* LUT,
        ResultHandler& res,
        const Scaler& scaler) {
    constexpr int bbs = 32 * BB;
    for (size_t j0 = 0; j0 < nb; j0 += bbs, codes += bbs * nsq / 2) {
        res.set_block_origin(0, j0);
        // skip computing distances if all vectors inside a block are filtered out
        if constexpr(has_sel_member_v<ResultHandler>) {
            if (res.sel != nullptr) {  // we have filter here
                bool skip_flag = true;
                for (size_t jj = 0; jj < std::min<size_t>(bbs, res.ntotal - j0);
                    jj++) {
                    auto real_idx = res.adjust_id(0, jj);
                    if (res.sel->is_member(real_idx)) {  // id is not filtered out, can not skip computing
                        skip_flag = false;
                        break;
                    }
                }

                if (skip_flag) {
                    continue;
                }
            }
        }
        FixedStorageHandler<NQ, 2 * BB> res2;
        kernel_accumulate_block<NQ, BB>(nsq, codes, LUT, res2, scaler);
        res2.to_other_handler(res);
    }
}

template <class ResultHandler, class Scaler>
void pq4_accumulate_loop_fixed_scaler(
        int nq,
        size_t nb,
        int bbs,
        int nsq,
        const uint8_t* codes,
        const uint8_t* LUT,
        ResultHandler& res,
        const Scaler& scaler) {
    FAISS_THROW_IF_NOT(is_aligned_pointer(codes));
    FAISS_THROW_IF_NOT(is_aligned_pointer(LUT));
    FAISS_THROW_IF_NOT(bbs % 32 == 0);
    FAISS_THROW_IF_NOT(nb % bbs == 0);

#define DISPATCH(NQ, BB)                                                   \
    case NQ * 1000 + BB:                                                   \
        accumulate_fixed_blocks<NQ, BB>(nb, nsq, codes, LUT, res, scaler); \
        break

    switch (nq * 1000 + bbs / 32) {
        DISPATCH(1, 1);
        DISPATCH(1, 2);
        DISPATCH(1, 3);
        DISPATCH(1, 4);
        DISPATCH(1, 5);
        DISPATCH(2, 1);
        DISPATCH(2, 2);
        DISPATCH(3, 1);
        DISPATCH(4, 1);
        default:
            FAISS_THROW_FMT("nq=%d bbs=%d not instantiated", nq, bbs);
    }
#undef DISPATCH
}

template <class ResultHandler>
void pq4_accumulate_loop_fixed_handler(
        int nq,
        size_t nb,
        int bbs,
        int nsq,
        const uint8_t* codes,
        const uint8_t* LUT,
        ResultHandler& res,
        const NormTableScaler* scaler) {
    if (scaler) {
        pq4_accumulate_loop_fixed_scaler(
                nq, nb, bbs, nsq, codes, LUT, res, *scaler);
    } else {
        DummyScaler dscaler;
        pq4_accumulate_loop_fixed_scaler(
                nq, nb, bbs, nsq, codes, LUT, res, dscaler);
    }
}

struct Run_pq4_accumulate_loop {
    template <class ResultHandler>
    void f(ResultHandler& res,
           int nq,
           size_t nb,
           int bbs,
           int nsq,
           const uint8_t* codes,
           const uint8_t* LUT,
           const NormTableScaler* scaler) {
        pq4_accumulate_loop_fixed_handler(
                nq, nb, bbs, nsq, codes, LUT, res, scaler);
    }
};

} // anonymous namespace

void pq4_accumulate_loop(
        int nq,
        size_t nb,
        int bbs,
        int nsq,
        const uint8_t* codes,
        const uint8_t* LUT,
        SIMDResultHandler& res,
        const NormTableScaler* scaler) {
    Run_pq4_accumulate_loop consumer;
    dispatch_SIMDResultHanlder(
            res, consumer, nq, nb, bbs, nsq, codes, LUT, scaler);
}

} // namespace faiss
