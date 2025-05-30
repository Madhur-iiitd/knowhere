/* Copyright (C) 2019-2023 Zilliz. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain a
 * copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 */

#if defined(__powerpc64__)

#include "distances_powerpc.h"

#include <altivec.h> /* Required for the Power GCC built-ins  */

#include <cmath>

#define FLOAT_VEC_SIZE 4
#define INT32_VEC_SIZE 4
#define INT8_VEC_SIZE 16

namespace faiss {

// float
// fvec_L2sqr_ppc(const float* x, const float* y, size_t d) {
//     /* Vector implmentaion uses vector size of FLOAT_VEC_SIZE.  If the input
//        array size is not a power of FLOAT_VEC_SIZE, do the remaining elements
//        in scalar mode.  */
//     float res = 0;
//     size_t base;

//     vector float *vx, *vy;
//     vector float vtmp = {0, 0, 0, 0};
//     vector float vres = {0, 0, 0, 0};

//     base = (d / FLOAT_VEC_SIZE) * FLOAT_VEC_SIZE;

//     for (size_t i = 0; i < base; i += FLOAT_VEC_SIZE) {
//         vx = (vector float*)(&x[i]);
//         vy = (vector float*)(&y[i]);

//         vtmp = vx[0] - vy[0];
//         vres += vtmp * vtmp;
//     }

//     /* Handle any remaining data elements */
//     for (size_t i = base; i < d; i++) {
//         const float tmp = x[i] - y[i];
//         res += tmp * tmp;
//     }

//     return res + vres[0] + vres[1] + vres[2] + vres[3];
// }
float 
fvec_L2sqr_ppc(const float* x, const float* y, size_t d) {
    float res = 0;

    // For small d, no vector unrolling is beneficial.
    if (d < 32) {
        size_t base;

        vector float *vx, *vy;
        vector float vtmp = {0, 0, 0, 0};
        vector float vres = {0, 0, 0, 0};

        base = (d / FLOAT_VEC_SIZE) * FLOAT_VEC_SIZE;

        for (size_t i = 0; i < base; i += FLOAT_VEC_SIZE) {
            vx = (vector float*)(&x[i]);
            vy = (vector float*)(&y[i]);

            vtmp = vx[0] - vy[0];
            vres += vtmp * vtmp;
        }

        /* Handle any remaining data elements */
        for (size_t i = base; i < d; i++) {
            const float tmp = x[i] - y[i];
            res += tmp * tmp;
        }

        return res + vres[0] + vres[1] + vres[2] + vres[3];
    }
    // For moderate d: use unroll factor 4 for d between 32 and 64.
    else if (d < 64 || d == 48) {
        size_t factor = FLOAT_VEC_SIZE * 4;
        size_t base = (d / factor) * factor;
        vector float *vx0, *vy0, *vx1, *vy1, *vx2, *vy2, *vx3, *vy3;
        vector float vres0 = {0, 0, 0, 0};
        vector float vres1 = {0, 0, 0, 0};
        vector float vres2 = {0, 0, 0, 0};
        vector float vres3 = {0, 0, 0, 0};
        vector float vtmp0, vtmp1, vtmp2, vtmp3;

        for (size_t i = 0; i < base; i += factor) {
            vx0 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 0]);
            vy0 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 0]);
            vx1 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 1]);
            vy1 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 1]);
            vx2 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 2]);
            vy2 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 2]);
            vx3 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 3]);
            vy3 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 3]);

            vtmp0 = vx0[0] - vy0[0];
            vtmp1 = vx1[0] - vy1[0];
            vtmp2 = vx2[0] - vy2[0];
            vtmp3 = vx3[0] - vy3[0];

            vres0 += vtmp0 * vtmp0;
            vres1 += vtmp1 * vtmp1;
            vres2 += vtmp2 * vtmp2;
            vres3 += vtmp3 * vtmp3;
        }

        // Process leftover elements
        for (size_t i = base; i < d; i++) {
            float tmp = x[i] - y[i];
            res += tmp * tmp;
        }

        // Pairwise reduction of the vector accumulators
        vres0 += vres1;
        vres2 += vres3;
        vres0 += vres2;
        res += vres0[0] + vres0[1] + vres0[2] + vres0[3];

        return res;
    }
    // For larger sizes, use unroll factor 8 when d is between 64 and 128.
    else if (d < 128) {
        size_t factor = FLOAT_VEC_SIZE * 8;
        size_t base = (d / factor) * factor;
        vector float *vx0, *vy0, *vx1, *vy1, *vx2, *vy2, *vx3, *vy3,
                     *vx4, *vy4, *vx5, *vy5, *vx6, *vy6, *vx7, *vy7;
        vector float vres0 = {0, 0, 0, 0};
        vector float vres1 = {0, 0, 0, 0};
        vector float vres2 = {0, 0, 0, 0};
        vector float vres3 = {0, 0, 0, 0};
        vector float vres4 = {0, 0, 0, 0};
        vector float vres5 = {0, 0, 0, 0};
        vector float vres6 = {0, 0, 0, 0};
        vector float vres7 = {0, 0, 0, 0};
        vector float vtmp0, vtmp1, vtmp2, vtmp3, vtmp4, vtmp5, vtmp6, vtmp7;

        for (size_t i = 0; i < base; i += factor) {
            vx0 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 0]);
            vy0 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 0]);
            vx1 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 1]);
            vy1 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 1]);
            vx2 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 2]);
            vy2 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 2]);
            vx3 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 3]);
            vy3 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 3]);
            vx4 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 4]);
            vy4 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 4]);
            vx5 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 5]);
            vy5 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 5]);
            vx6 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 6]);
            vy6 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 6]);
            vx7 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 7]);
            vy7 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 7]);

            vtmp0 = vx0[0] - vy0[0];
            vtmp1 = vx1[0] - vy1[0];
            vtmp2 = vx2[0] - vy2[0];
            vtmp3 = vx3[0] - vy3[0];
            vtmp4 = vx4[0] - vy4[0];
            vtmp5 = vx5[0] - vy5[0];
            vtmp6 = vx6[0] - vy6[0];
            vtmp7 = vx7[0] - vy7[0];

            vres0 += vtmp0 * vtmp0;
            vres1 += vtmp1 * vtmp1;
            vres2 += vtmp2 * vtmp2;
            vres3 += vtmp3 * vtmp3;
            vres4 += vtmp4 * vtmp4;
            vres5 += vtmp5 * vtmp5;
            vres6 += vtmp6 * vtmp6;
            vres7 += vtmp7 * vtmp7;
        }

        // Process remainder
        for (size_t i = base; i < d; i++) {
            float tmp = x[i] - y[i];
            res += tmp * tmp;
        }

        // Reduction
        vres0 += vres1;
        vres2 += vres3;
        vres4 += vres5;
        vres6 += vres7;
        vres0 += vres2;
        vres4 += vres6;
        vres0 += vres4;
        res += vres0[0] + vres0[1] + vres0[2] + vres0[3];

        return res;
    }
    // For very large d (>= 128), we use unroll factor 16.
    else {
        size_t factor = FLOAT_VEC_SIZE * 16;
        size_t base = (d / factor) * factor;
        vector float *vx0,  *vy0,  *vx1,  *vy1,  *vx2,  *vy2,  *vx3,  *vy3,
                     *vx4,  *vy4,  *vx5,  *vy5,  *vx6,  *vy6,  *vx7,  *vy7,
                     *vx8,  *vy8,  *vx9,  *vy9,  *vx10, *vy10, *vx11, *vy11,
                     *vx12, *vy12, *vx13, *vy13, *vx14, *vy14, *vx15, *vy15;
        vector float vres0 = {0, 0, 0, 0};
        vector float vres1 = {0, 0, 0, 0};
        vector float vres2 = {0, 0, 0, 0};
        vector float vres3 = {0, 0, 0, 0};
        vector float vres4 = {0, 0, 0, 0};
        vector float vres5 = {0, 0, 0, 0};
        vector float vres6 = {0, 0, 0, 0};
        vector float vres7 = {0, 0, 0, 0};
        vector float vres8 = {0, 0, 0, 0};
        vector float vres9 = {0, 0, 0, 0};
        vector float vres10 = {0, 0, 0, 0};
        vector float vres11 = {0, 0, 0, 0};
        vector float vres12 = {0, 0, 0, 0};
        vector float vres13 = {0, 0, 0, 0};
        vector float vres14 = {0, 0, 0, 0};
        vector float vres15 = {0, 0, 0, 0};
        vector float vtmp0,  vtmp1,  vtmp2,  vtmp3,  vtmp4,  vtmp5,  vtmp6,  vtmp7,
                     vtmp8,  vtmp9,  vtmp10, vtmp11, vtmp12, vtmp13, vtmp14, vtmp15;

        for (size_t i = 0; i < base; i += factor) {
            vx0  = (vector float *)(&x[i + FLOAT_VEC_SIZE * 0]);
            vy0  = (vector float *)(&y[i + FLOAT_VEC_SIZE * 0]);
            vx1  = (vector float *)(&x[i + FLOAT_VEC_SIZE * 1]);
            vy1  = (vector float *)(&y[i + FLOAT_VEC_SIZE * 1]);
            vx2  = (vector float *)(&x[i + FLOAT_VEC_SIZE * 2]);
            vy2  = (vector float *)(&y[i + FLOAT_VEC_SIZE * 2]);
            vx3  = (vector float *)(&x[i + FLOAT_VEC_SIZE * 3]);
            vy3  = (vector float *)(&y[i + FLOAT_VEC_SIZE * 3]);
            vx4  = (vector float *)(&x[i + FLOAT_VEC_SIZE * 4]);
            vy4  = (vector float *)(&y[i + FLOAT_VEC_SIZE * 4]);
            vx5  = (vector float *)(&x[i + FLOAT_VEC_SIZE * 5]);
            vy5  = (vector float *)(&y[i + FLOAT_VEC_SIZE * 5]);
            vx6  = (vector float *)(&x[i + FLOAT_VEC_SIZE * 6]);
            vy6  = (vector float *)(&y[i + FLOAT_VEC_SIZE * 6]);
            vx7  = (vector float *)(&x[i + FLOAT_VEC_SIZE * 7]);
            vy7  = (vector float *)(&y[i + FLOAT_VEC_SIZE * 7]);
            vx8  = (vector float *)(&x[i + FLOAT_VEC_SIZE * 8]);
            vy8  = (vector float *)(&y[i + FLOAT_VEC_SIZE * 8]);
            vx9  = (vector float *)(&x[i + FLOAT_VEC_SIZE * 9]);
            vy9  = (vector float *)(&y[i + FLOAT_VEC_SIZE * 9]);
            vx10 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 10]);
            vy10 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 10]);
            vx11 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 11]);
            vy11 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 11]);
            vx12 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 12]);
            vy12 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 12]);
            vx13 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 13]);
            vy13 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 13]);
            vx14 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 14]);
            vy14 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 14]);
            vx15 = (vector float *)(&x[i + FLOAT_VEC_SIZE * 15]);
            vy15 = (vector float *)(&y[i + FLOAT_VEC_SIZE * 15]);

            vtmp0  = vx0[0]  - vy0[0];
            vtmp1  = vx1[0]  - vy1[0];
            vtmp2  = vx2[0]  - vy2[0];
            vtmp3  = vx3[0]  - vy3[0];
            vtmp4  = vx4[0]  - vy4[0];
            vtmp5  = vx5[0]  - vy5[0];
            vtmp6  = vx6[0]  - vy6[0];
            vtmp7  = vx7[0]  - vy7[0];
            vtmp8  = vx8[0]  - vy8[0];
            vtmp9  = vx9[0]  - vy9[0];
            vtmp10 = vx10[0] - vy10[0];
            vtmp11 = vx11[0] - vy11[0];
            vtmp12 = vx12[0] - vy12[0];
            vtmp13 = vx13[0] - vy13[0];
            vtmp14 = vx14[0] - vy14[0];
            vtmp15 = vx15[0] - vy15[0];

            vres0  += vtmp0  * vtmp0;
            vres1  += vtmp1  * vtmp1;
            vres2  += vtmp2  * vtmp2;
            vres3  += vtmp3  * vtmp3;
            vres4  += vtmp4  * vtmp4;
            vres5  += vtmp5  * vtmp5;
            vres6  += vtmp6  * vtmp6;
            vres7  += vtmp7  * vtmp7;
            vres8  += vtmp8  * vtmp8;
            vres9  += vtmp9  * vtmp9;
            vres10 += vtmp10 * vtmp10;
            vres11 += vtmp11 * vtmp11;
            vres12 += vtmp12 * vtmp12;
            vres13 += vtmp13 * vtmp13;
            vres14 += vtmp14 * vtmp14;
            vres15 += vtmp15 * vtmp15;
        }

        // Process remaining elements
        for (size_t i = base; i < d; i++) {
            float tmp = x[i] - y[i];
            res += tmp * tmp;
        }

        // Reduction tree: reduce the 16 vector accumulators into one.
        vres0  += vres1;   vres2  += vres3;   vres4  += vres5;   vres6  += vres7;
        vres8  += vres9;   vres10 += vres11;  vres12 += vres13;  vres14 += vres15;
        vres0  += vres2;   vres4  += vres6;   vres8  += vres10;  vres12 += vres14;
        vres0  += vres4;   vres8  += vres12;
        vres0  += vres8;

        res += vres0[0] + vres0[1] + vres0[2] + vres0[3];
        return res;
    }
}

float
fvec_L1_ppc(const float* x, const float* y, size_t d) {
    /* Vector implmentaion uses vector size of FLOAT_VEC_SIZE.  If the input
       array size is not a power of FLOAT_VEC_SIZE, do the remaining elements
       in scalar mode.  */
    float res = 0;
    size_t base;

    vector float vx, vy;
    vector float vtmp = {0, 0, 0, 0};
    vector float vres = {0, 0, 0, 0};

    base = (d / FLOAT_VEC_SIZE) * FLOAT_VEC_SIZE;

    for (size_t i = 0; i < base; i += FLOAT_VEC_SIZE) {
        vx = vec_xl(i * sizeof(float), x);
        vy = vec_xl(i * sizeof(float), y);

        vtmp = vec_sub(vx, vy);
        vtmp = vec_abs(vtmp);
        vres = vec_add(vtmp, vres);
    }

    /* Handle any remaining data elements */
    for (size_t i = base; i < d; i++) {
        const float tmp = x[i] - y[i];
        res += std::fabs(tmp);
    }

    return res + vres[0] + vres[1] + vres[2] + vres[3];
}

float
fvec_Linf_ppc(const float* x, const float* y, size_t d) {
    /* Vector implmentaion uses vector size of FLOAT_VEC_SIZE.  If the input
       array size is not a power of FLOAT_VEC_SIZE, do the remaining elements
       in scalar mode.  */
    float res = 0;
    size_t base;

    vector float vx, vy;
    vector float vtmp = {0, 0, 0, 0};
    vector float vres = {0, 0, 0, 0};

    base = (d / FLOAT_VEC_SIZE) * FLOAT_VEC_SIZE;

    for (size_t i = 0; i < base; i += FLOAT_VEC_SIZE) {
        vx = vec_xl(i * sizeof(float), x);
        vy = vec_xl(i * sizeof(float), y);

        vtmp = vec_sub(vx, vy);
        vtmp = vec_abs(vtmp);
        res = std::fmax(res, vtmp[0]);
        res = std::fmax(res, vtmp[1]);
        res = std::fmax(res, vtmp[2]);
        res = std::fmax(res, vtmp[3]);
    }

    /* Handle any remaining data elements */
    for (size_t i = base; i < d; i++) {
        res = std::fmax(res, std::fabs(x[i] - y[i]));
    }

    return res;
}

float
fvec_inner_product_ppc(const float *x, const float *y, size_t d)
{
    size_t i, base;
    float res = 0.0f;

    if (d < 32)
    {
        size_t nvec = d / FLOAT_VEC_SIZE;
        vector float *vx = (vector float *)x;
        vector float *vy = (vector float *)y;
        vector float vres = {0, 0, 0, 0};

        for (i = 0; i < nvec; ++i)
            vres += vx[i] * vy[i];

        for (i = nvec * FLOAT_VEC_SIZE; i < d; ++i)
            res += x[i] * y[i];

        return res + vres[0] + vres[1] + vres[2] + vres[3];
    }
    else if (d < 64)
    {
        base = (d / (FLOAT_VEC_SIZE * 4)) * (FLOAT_VEC_SIZE * 4);
        vector float vres0 = {0, 0, 0, 0}, vres1 = {0, 0, 0, 0},
                        vres2 = {0, 0, 0, 0}, vres3 = {0, 0, 0, 0};

        for (i = 0; i < base; i += FLOAT_VEC_SIZE * 4)
        {
            vector float *vx = (vector float *)&x[i];
            vector float *vy = (vector float *)&y[i];
            vres0 += vx[0] * vy[0];
            vres1 += vx[1] * vy[1];
            vres2 += vx[2] * vy[2];
            vres3 += vx[3] * vy[3];
        }

        for (i = base; i < d; ++i)
            res += x[i] * y[i];

        vres0 += vres1;
        vres2 += vres3;
        vres0 += vres2;
        return res + (vres0[0] + vres0[1] + vres0[2] + vres0[3]);
    }
    else if (d < 128)
    {
        base = (d / (FLOAT_VEC_SIZE * 8)) * (FLOAT_VEC_SIZE * 8);
        vector float vres0 = {0, 0, 0, 0}, vres1 = {0, 0, 0, 0},
                        vres2 = {0, 0, 0, 0}, vres3 = {0, 0, 0, 0},
                        vres4 = {0, 0, 0, 0}, vres5 = {0, 0, 0, 0},
                        vres6 = {0, 0, 0, 0}, vres7 = {0, 0, 0, 0};

        for (i = 0; i < base; i += FLOAT_VEC_SIZE * 8)
        {
            vector float *vx = (vector float *)&x[i];
            vector float *vy = (vector float *)&y[i];
            vres0 += vx[0] * vy[0];
            vres1 += vx[1] * vy[1];
            vres2 += vx[2] * vy[2];
            vres3 += vx[3] * vy[3];
            vres4 += vx[4] * vy[4];
            vres5 += vx[5] * vy[5];
            vres6 += vx[6] * vy[6];
            vres7 += vx[7] * vy[7];
        }

        for (i = base; i < d; ++i)
            res += x[i] * y[i];

        vres0 += vres1;
        vres0 += vres2;
        vres0 += vres3;
        vres0 += vres4;
        vres0 += vres5;
        vres0 += vres6;
        vres0 += vres7;
        return res + (vres0[0] + vres0[1] + vres0[2] + vres0[3]);
    }
    else
    {
        base = (d / (FLOAT_VEC_SIZE * 16)) * (FLOAT_VEC_SIZE * 16);
        vector float vres0 = {0, 0, 0, 0}, vres1 = {0, 0, 0, 0},
                        vres2 = {0, 0, 0, 0}, vres3 = {0, 0, 0, 0},
                        vres4 = {0, 0, 0, 0}, vres5 = {0, 0, 0, 0},
                        vres6 = {0, 0, 0, 0}, vres7 = {0, 0, 0, 0},
                        vres8 = {0, 0, 0, 0}, vres9 = {0, 0, 0, 0},
                        vres10 = {0, 0, 0, 0}, vres11 = {0, 0, 0, 0},
                        vres12 = {0, 0, 0, 0}, vres13 = {0, 0, 0, 0},
                        vres14 = {0, 0, 0, 0}, vres15 = {0, 0, 0, 0};

        for (i = 0; i < base; i += FLOAT_VEC_SIZE * 16)
        {
            vector float *vx = (vector float *)&x[i];
            vector float *vy = (vector float *)&y[i];
            vres0 += vx[0] * vy[0];
            vres1 += vx[1] * vy[1];
            vres2 += vx[2] * vy[2];
            vres3 += vx[3] * vy[3];
            vres4 += vx[4] * vy[4];
            vres5 += vx[5] * vy[5];
            vres6 += vx[6] * vy[6];
            vres7 += vx[7] * vy[7];
            vres8 += vx[8] * vy[8];
            vres9 += vx[9] * vy[9];
            vres10 += vx[10] * vy[10];
            vres11 += vx[11] * vy[11];
            vres12 += vx[12] * vy[12];
            vres13 += vx[13] * vy[13];
            vres14 += vx[14] * vy[14];
            vres15 += vx[15] * vy[15];
        }

        for (i = base; i < d; ++i)
            res += x[i] * y[i];

        vres0 += vres1;
        vres0 += vres2;
        vres0 += vres3;
        vres0 += vres4;
        vres0 += vres5;
        vres0 += vres6;
        vres0 += vres7;
        vres0 += vres8;
        vres0 += vres9;
        vres0 += vres10;
        vres0 += vres11;
        vres0 += vres12;
        vres0 += vres13;
        vres0 += vres14;
        vres0 += vres15;
        return res + (vres0[0] + vres0[1] + vres0[2] + vres0[3]);
    }
}

float
fvec_norm_L2sqr_ppc(const float* x, size_t d) {
    /* Vector implmentaion uses vector size of FLOAT_VEC_SIZE.  Do the
       operation as double, then return result as a float.  If the input array
       size is not a power of FLOAT_VEC_SIZE, do the remaining elements in
       scalar mode.  */
    double res = 0;
    size_t base;

    vector float vx;
    vector double vxde, vxdo;
    vector double vtmpo = {0, 0}, vtmpe = {0, 0};
    vector double vreso = {0, 0}, vrese = {0, 0};

    base = (d / FLOAT_VEC_SIZE) * FLOAT_VEC_SIZE;

    for (size_t i = 0; i < base; i += FLOAT_VEC_SIZE) {
        vx = vec_xl(i * sizeof(float), x);

        /* Convert even/odd floats to double then square elements. */
        vxdo = vec_doubleo(vx);
        vtmpo = vec_mul(vxdo, vxdo);
        vreso = vec_add(vreso, vtmpo);
        vxde = vec_doublee(vx);
        vtmpe = vec_mul(vxde, vxde);
        vrese = vec_add(vrese, vtmpe);
    }

    /* Handle any remaining data elements */
    for (size_t i = base; i < d; i++) {
        res += x[i] * x[i];
    }
    return res + vreso[0] + vreso[1] + vrese[0] + vrese[1];
}

void
fvec_L2sqr_ny_ppc(float* dis, const float* x, const float* y, size_t d, size_t ny) {
    for (size_t i = 0; i < ny; i++) {
        dis[i] = fvec_L2sqr_ppc(x, y, d);
        y += d;
    }
}

void
fvec_inner_products_ny_ppc(float* ip, const float* x, const float* y, size_t d, size_t ny) {
    for (size_t i = 0; i < ny; i++) {
        ip[i] = fvec_inner_product_ppc(x, y, d);
        y += d;
    }
}

/// compute ny square L2 distance between x and a set of transposed contiguous
/// y vectors. squared lengths of y should be provided as well
void
fvec_L2sqr_ny_transposed_ppc(float* __restrict dis, const float* __restrict x, const float* __restrict y,
                             const float* __restrict y_sqlen, size_t d, size_t d_offset, size_t ny) {
    /* Vector implmentaion uses vector size of FLOAT_VEC_SIZE.  If the input
       array size is not a power of FLOAT_VEC_SIZE, do the remaining elements
       in scalar mode.  */
    size_t base;

    float x_sqlen = 0;
    vector float vx, vy, vy_sqlen, vdp = {0, 0, 0, 0};
    vector float vx_sqlen = {0, 0, 0, 0};
    vector float vres = {0, 0, 0, 0};
    vector float vzero = {0, 0, 0, 0};

    base = (d / FLOAT_VEC_SIZE) * FLOAT_VEC_SIZE;

    for (size_t i = 0; i < base; i += FLOAT_VEC_SIZE) {
        vx = vec_xl(i * sizeof(float), x);
        vx_sqlen = vec_madd(vx, vx, vx_sqlen);
    }

    x_sqlen = vx_sqlen[0] + vx_sqlen[1] + vx_sqlen[2] + vx_sqlen[3];

    /* Handle any remaining x data elements, in scalar mode. */
    for (size_t j = base; j < d; j++) {
        x_sqlen += x[j + base] * x[j + base];
    }

    for (size_t i = 0; i < ny; i++) {
        float dp = 0;
        vdp = vzero;

        /* Unrolling gives better performance then trying to vectorize.  */
        base = (d / 16) * 16;
        for (size_t j = 0; j < base; j += 16) {
            dp += x[j] * y[i + j * d_offset];
            dp += x[j + 1] * y[i + (j + 1) * d_offset];
            dp += x[j + 2] * y[i + (j + 2) * d_offset];
            dp += x[j + 3] * y[i + (j + 3) * d_offset];
            dp += x[j + 4] * y[i + (j + 4) * d_offset];
            dp += x[j + 5] * y[i + (j + 5) * d_offset];
            dp += x[j + 6] * y[i + (j + 6) * d_offset];
            dp += x[j + 7] * y[i + (j + 7) * d_offset];
            dp += x[j + 8] * y[i + (j + 8) * d_offset];
            dp += x[j + 9] * y[i + (j + 9) * d_offset];
            dp += x[j + 10] * y[i + (j + 10) * d_offset];
            dp += x[j + 11] * y[i + (j + 11) * d_offset];
            dp += x[j + 12] * y[i + (j + 12) * d_offset];
            dp += x[j + 13] * y[i + (j + 13) * d_offset];
            dp += x[j + 14] * y[i + (j + 14) * d_offset];
            dp += x[j + 15] * y[i + (j + 15) * d_offset];
        }

        for (size_t j = base; j < d; j++) {
            dp += x[j] * y[i + j * d_offset];
        }

        dis[i] = x_sqlen + y_sqlen[i] - 2 * dp;
    }
}

/// compute ny square L2 distance between x and a set of contiguous y vectors
/// and return the index of the nearest vector.
/// return 0 if ny == 0.
size_t
fvec_L2sqr_ny_nearest_ppc(float* __restrict distances_tmp_buffer, const float* __restrict x, const float* __restrict y,
                          size_t d, size_t ny) {
    fvec_L2sqr_ny_ppc(distances_tmp_buffer, x, y, d, ny);

    size_t nearest_idx = 0;
    float min_dis = HUGE_VALF;

    for (size_t i = 0; i < ny; i++) {
        if (distances_tmp_buffer[i] < min_dis) {
            min_dis = distances_tmp_buffer[i];
            nearest_idx = i;
        }
    }
    return nearest_idx;
}

/// compute ny square L2 distance between x and a set of transposed contiguous
/// y vectors and return the index of the nearest vector.
/// squared lengths of y should be provided as well
/// return 0 if ny == 0.
size_t
fvec_L2sqr_ny_nearest_y_transposed_ppc(float* __restrict distances_tmp_buffer, const float* __restrict x,
                                       const float* __restrict y, const float* __restrict y_sqlen, size_t d,
                                       size_t d_offset, size_t ny) {
    fvec_L2sqr_ny_transposed_ppc(distances_tmp_buffer, x, y, y_sqlen, d, d_offset, ny);

    size_t nearest_idx = 0;
    float min_dis = HUGE_VALF;

    for (size_t i = 0; i < ny; i++) {
        if (distances_tmp_buffer[i] < min_dis) {
            min_dis = distances_tmp_buffer[i];
            nearest_idx = i;
        }
    }

    return nearest_idx;
}

void
fvec_madd_ppc(size_t n, const float* a, float bf, const float* b, float* c) {
    /* Vector implmentaion uses vector size of FLOAT_VEC_SIZE.  If the input
       array size is not a power of FLOAT_VEC_SIZE, do the remaining elements
       in scalar mode.  */
    size_t base;
    vector float va, vb, vc, vbf = {bf, bf, bf, bf};

    base = (n / FLOAT_VEC_SIZE) * FLOAT_VEC_SIZE;

    for (size_t i = 0; i < base; i += FLOAT_VEC_SIZE) {
        va = vec_xl(i * sizeof(float), a);
        vb = vec_xl(i * sizeof(float), b);

        vc = vec_madd(vb, vbf, va);
        vec_xst(vc, i * sizeof(float), c);
    }

    /* Handle any remaining data elements */
    for (size_t i = base; i < n; i++) {
        c[i] = a[i] + bf * b[i];
    }
}

int
fvec_madd_and_argmin_ppc(size_t n, const float* a, float bf, const float* b, float* c) {
    /* Vector implmentaion uses vector size of FLOAT_VEC_SIZE.  If the input
       array size is not a power of FLOAT_VEC_SIZE, do the remaining elements
       in scalar mode.  */
    vector float va, vb, vc;
    vector float vbf = {bf, bf, bf, bf};
    float vmin = 1.0e20;
    int imin = -1;
    size_t base;

    base = (n / FLOAT_VEC_SIZE) * FLOAT_VEC_SIZE;

    for (size_t i = 0; i < base; i += FLOAT_VEC_SIZE) {
        va = vec_xl(i * sizeof(float), a);
        vb = vec_xl(i * sizeof(float), b);

        vc = vec_madd(vbf, vb, va);

        /* Checke each vector element */
        for (int j = 0; j < FLOAT_VEC_SIZE; j++) {
            if (vc[j] < vmin) {
                vmin = c[i + j];
                imin = i + j;
            }
        }
    }

    /* Handle any remaining data elements */
    for (size_t i = base; i < n; i++) {
        if (c[i] < vmin) {
            vmin = c[i];
            imin = i;
        }
    }
    return imin;
}

void
fvec_L2sqr_batch_4_ppc(const float* x, const float* y0, const float* y1, const float* y2, const float* y3,
                       const size_t d, float& dis0, float& dis1, float& dis2, float& dis3) {
    /* Vector implmentaion uses vector size of FLOAT_VEC_SIZE.  If the input
       array size is not a power of FLOAT_VEC_SIZE, do the remaining elements
       in scalar mode.  */
    size_t base, remainder;

    vector float *vx, *vy0, *vy1, *vy2, *vy3;
    vector float vd0 = {0, 0, 0, 0};
    vector float vd1 = {0, 0, 0, 0};
    vector float vd2 = {0, 0, 0, 0};
    vector float vd3 = {0, 0, 0, 0};
    vector float vq0, vq1, vq2, vq3;
    float d0 = 0;
    float d1 = 0;
    float d2 = 0;
    float d3 = 0;

    base = (d / FLOAT_VEC_SIZE) * FLOAT_VEC_SIZE;
    remainder = d % FLOAT_VEC_SIZE;

    for (size_t i = 0; i < base; i += FLOAT_VEC_SIZE) {
        /* Load up the data vectors */
        vx = (vector float*)(&x[i]);
        vy0 = (vector float*)(&y0[i]);
        vy1 = (vector float*)(&y1[i]);
        vy2 = (vector float*)(&y2[i]);
        vy3 = (vector float*)(&y3[i]);

        /* Replace scalar subtract with vector subtract built-in.  */
        vq0 = vx[0] - vy0[0];
        vq1 = vx[0] - vy1[0];
        vq2 = vx[0] - vy2[0];
        vq3 = vx[0] - vy3[0];

        /* Replace scalar multiply add with vector multiply add built-in.  */
        vd0 += vq0 * vq0;
        vd1 += vq1 * vq1;
        vd2 += vq2 * vq2;
        vd3 += vq3 * vq3;
    }

    /* Handle the remainder of the elments in scalar mode.  */
    for (size_t i = base; i < d; ++i) {
        const float q0 = x[i] - y0[i];
        const float q1 = x[i] - y1[i];
        const float q2 = x[i] - y2[i];
        const float q3 = x[i] - y3[i];

        d0 += q0 * q0;
        d1 += q1 * q1;
        d2 += q2 * q2;
        d3 += q3 * q3;
    }

    /* Replace result assignment of the scalar result with sum of the
       corresponding vector elements to get the equivalent result.  */
    dis0 = vd0[0] + vd0[1] + vd0[2] + vd0[3] + d0;
    dis1 = vd1[0] + vd1[1] + vd1[2] + vd1[3] + d1;
    dis2 = vd2[0] + vd2[1] + vd2[2] + vd2[3] + d2;
    dis3 = vd3[0] + vd3[1] + vd3[2] + vd3[3] + d3;
}

void
fvec_inner_product_batch_4_ppc(const float* __restrict x, const float* __restrict y0, const float* __restrict y1,
                               const float* __restrict y2, const float* __restrict y3, const size_t d, float& dis0,
                               float& dis1, float& dis2, float& dis3) {
    /* Vector implmentaion uses vector size of FLOAT_VEC_SIZE.  If the input
       array size is not a power of FLOAT_VEC_SIZE, do the remaining elements
       in scalar mode.  */

    size_t base, remainder;
    vector float vx, vy0, vy1, vy2, vy3;
    vector float vd0 = {0.0, 0.0, 0.0, 0.0};
    vector float vd1 = {0.0, 0.0, 0.0, 0.0};
    vector float vd2 = {0.0, 0.0, 0.0, 0.0};
    vector float vd3 = {0.0, 0.0, 0.0, 0.0};

    base = (d / FLOAT_VEC_SIZE) * FLOAT_VEC_SIZE;
    remainder = d % FLOAT_VEC_SIZE;

    for (size_t i = 0; i < base; i += FLOAT_VEC_SIZE) {
        vx = vec_xl(i * sizeof(float), x);
        vy0 = vec_xl(i * sizeof(float), y0);
        vy1 = vec_xl(i * sizeof(float), y1);
        vy2 = vec_xl(i * sizeof(float), y2);
        vy3 = vec_xl(i * sizeof(float), y3);

        vd0 = vec_madd(vx, vy0, vd0);
        vd1 = vec_madd(vx, vy1, vd1);
        vd2 = vec_madd(vx, vy2, vd2);
        vd3 = vec_madd(vx, vy3, vd3);
    }

    dis0 = vd0[0] + vd0[1] + vd0[2] + vd0[3];
    dis1 = vd1[0] + vd1[1] + vd1[2] + vd1[3];
    dis2 = vd2[0] + vd2[1] + vd2[2] + vd2[3];
    dis3 = vd3[0] + vd3[1] + vd3[2] + vd3[3];

    /* Handle any remaining data elements */
    if (remainder != 0) {
        float d0 = 0;
        float d1 = 0;
        float d2 = 0;
        float d3 = 0;

        for (size_t i = base; i < d; i++) {
            d0 += x[i] * y0[i];
            d1 += x[i] * y1[i];
            d2 += x[i] * y2[i];
            d3 += x[i] * y3[i];
        }

        dis0 += d0;
        dis1 += d1;
        dis2 += d2;
        dis3 += d3;
    }
}

int32_t
ivec_inner_product_ppc(const int8_t* x, const int8_t* y, size_t d) {
    int32_t res = 0;

    /* Attempts to mannually vectorize and manually unroll the loop
       do not seem to improve the performance. */
    for (size_t i = 0; i < d; i++) {
        res += (int32_t)x[i] * y[i];
    }
    return res;
}

int32_t
ivec_L2sqr_ppc(const int8_t* x, const int8_t* y, size_t d) {
    int32_t res = 0;

    /* Attempts to mannually vectorize and manually unroll the loop
       do not seem to improve the performance. */
    for (size_t i = 0; i < d; i++) {
        const int32_t tmp = (int32_t)x[i] - (int32_t)y[i];
        res += tmp * tmp;
    }
    return res;
}

}  // namespace faiss

#endif
