/* ************************************************************************
 * Copyright 2016 Advanced Micro Devices, Inc.
 *
 * ************************************************************************ */

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <sys/time.h>
#include <vector>

#include "cblas_interface.h"
#include "flops.h"
#include "hipblas.hpp"
#include "norm.h"
#include "unit.h"
#include "utility.h"
#include <typeinfo>

using namespace std;

/* ============================================================================================ */

template <typename T>
hipblasStatus_t testing_gemm(Arguments argus)
{
    int M = argus.M;
    int N = argus.N;
    int K = argus.K;

    int lda = argus.lda;
    int ldb = argus.ldb;
    int ldc = argus.ldc;

    hipblasOperation_t transA = char2hipblas_operation(argus.transA_option);
    hipblasOperation_t transB = char2hipblas_operation(argus.transB_option);

    T alpha = argus.alpha;
    T beta  = argus.beta;

    int A_size, B_size, C_size, A_row, A_col, B_row, B_col;

    hipblasHandle_t handle;
    hipblasStatus_t status = HIPBLAS_STATUS_SUCCESS;
    hipblasCreate(&handle);

    if(transA == HIPBLAS_OP_N)
    {
        A_row = M;
        A_col = K;
    }
    else
    {
        A_row = K;
        A_col = M;
    }

    if(transB == HIPBLAS_OP_N)
    {
        B_row = K;
        B_col = N;
    }
    else
    {
        B_row = N;
        B_col = K;
    }

    A_size = lda * A_col;
    B_size = ldb * B_col;
    C_size = ldc * N;

    // check here to prevent undefined memory allocation error
    if(M < 0 || N < 0 || K < 0 || lda < A_row || ldb < B_row || ldc < M)
    {
        return HIPBLAS_STATUS_INVALID_VALUE;
    }
    // Naming: dX is in GPU (device) memory. hK is in CPU (host) memory, plz follow this practice
    vector<T> hA(A_size);
    vector<T> hB(B_size);
    vector<T> hC(C_size);
    vector<T> hC_copy(C_size);

    T *dA, *dB, *dC;

    // allocate memory on device
    CHECK_HIP_ERROR(hipMalloc(&dA, A_size * sizeof(T)));
    CHECK_HIP_ERROR(hipMalloc(&dB, B_size * sizeof(T)));
    CHECK_HIP_ERROR(hipMalloc(&dC, C_size * sizeof(T)));

    // Initial Data on CPU
    srand(1);
    hipblas_init<T>(hA, A_row, A_col, lda);
    hipblas_init<T>(hB, B_row, B_col, ldb);
    hipblas_init<T>(hC, M, N, ldc);

    // copy vector is easy in STL; hz = hx: save a copy in hC_copy which will be output of CPU BLAS
    hC_copy = hC;

    // copy data from CPU to device, does not work for lda != A_row
    CHECK_HIP_ERROR(hipMemcpy(dA, hA.data(), sizeof(T) * lda * A_col, hipMemcpyHostToDevice));
    CHECK_HIP_ERROR(hipMemcpy(dB, hB.data(), sizeof(T) * ldb * B_col, hipMemcpyHostToDevice));
    CHECK_HIP_ERROR(hipMemcpy(dC, hC.data(), sizeof(T) * ldc * N, hipMemcpyHostToDevice));

    /* =====================================================================
         ROCBLAS
    =================================================================== */

    // library interface
    status
        = hipblasGemm<T>(handle, transA, transB, M, N, K, &alpha, dA, lda, dB, ldb, &beta, dC, ldc);

    // copy output from device to CPU
    CHECK_HIP_ERROR(hipMemcpy(hC.data(), dC, sizeof(T) * ldc * N, hipMemcpyDeviceToHost));

    if(argus.unit_check)
    {

        /* =====================================================================
                    CPU BLAS
        =================================================================== */
        if(status != HIPBLAS_STATUS_INVALID_VALUE)
        { // only valid size compare with cblas
            cblas_gemm<T>(transA,
                          transB,
                          M,
                          N,
                          K,
                          alpha,
                          hA.data(),
                          lda,
                          hB.data(),
                          ldb,
                          beta,
                          hC_copy.data(),
                          ldc);
        }

#ifndef NDEBUG
        print_matrix(hC_copy, hC, min(M, 3), min(N, 3), ldc);
#endif

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(argus.unit_check)
        {
            unit_check_general<T>(M, N, ldc, hC_copy.data(), hC.data());
        }

    } // end of if unit/norm check

    CHECK_HIP_ERROR(hipFree(dA));
    CHECK_HIP_ERROR(hipFree(dB));
    CHECK_HIP_ERROR(hipFree(dC));

    hipblasDestroy(handle);
    return status;
}
