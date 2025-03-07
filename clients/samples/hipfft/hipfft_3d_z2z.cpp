/******************************************************************************
* Copyright (c) 2019 - present Advanced Micro Devices, Inc. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*******************************************************************************/

#include <complex>
#include <iostream>

#include <hip/hip_runtime.h>
#include <hipfft.h>

// Kernel for initializing the real-valued input data on the GPU.
__global__ void initdata(hipfftDoubleComplex* x, const int Nx, const int Ny, const int Nz)
{
    const int idx = blockIdx.x * blockDim.x + threadIdx.x;
    const int idy = blockIdx.y * blockDim.y + threadIdx.y;
    const int idz = blockIdx.z * blockDim.z + threadIdx.z;
    if(idx < Nx && idy < Ny && idz < Nz)
    {
        const int pos = (idx * Ny + idy) * Nz + idz;
        x[pos].x      = idx + 10 * idz;
        x[pos].y      = idy;
    }
}

// Helper function for determining grid dimensions
template <typename Tint1, typename Tint2>
Tint1 ceildiv(const Tint1 nominator, const Tint2 denominator)
{
    return (nominator + denominator - 1) / denominator;
}

int main()
{
    std::cout << "hipfft 3D double-precision complex-to-complex transform\n";

    const int Nx        = 4;
    const int Ny        = 4;
    const int Nz        = 4;
    int       direction = HIPFFT_FORWARD; // forward=-1, backward=1

    std::vector<std::complex<double>> cdata(Nx * Ny * Nz);
    size_t complex_bytes = sizeof(decltype(cdata)::value_type) * cdata.size();

    // Create HIP device object and copy data to device:
    // hipfftComplex for single-precision
    hipfftDoubleComplex* x;
    hipMalloc(&x, complex_bytes);

    // Inititalize the data on the device
    hipError_t rt;
    const dim3 blockdim(8, 8, 8);
    const dim3 griddim(ceildiv(Nx, blockdim.x), ceildiv(Ny, blockdim.y), ceildiv(Nz, blockdim.z));
    hipLaunchKernelGGL(initdata, blockdim, griddim, 0, 0, x, Nx, Ny, Nz);
    hipDeviceSynchronize();
    rt = hipGetLastError();
    assert(rt == hipSuccess);

    std::cout << "input:\n";
    hipMemcpy(cdata.data(), x, complex_bytes, hipMemcpyDefault);
    for(int i = 0; i < Nx; i++)
    {
        for(int j = 0; j < Ny; j++)
        {
            for(int k = 0; k < Nz; k++)
            {
                int pos = (i * Ny + j) * Nz + k;
                std::cout << cdata[pos] << " ";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
    std::cout << std::endl;

    // Create plan
    hipfftResult rc   = HIPFFT_SUCCESS;
    hipfftHandle plan = NULL;
    rc                = hipfftCreate(&plan);
    assert(rc == HIPFFT_SUCCESS);
    rc = hipfftPlan3d(&plan, // plan handle
                      Nx, // transform length
                      Ny, // transform length
                      Nz, // transform length
                      HIPFFT_Z2Z); // transform type (HIPFFT_C2C for single-precisoin)
    assert(rc == HIPFFT_SUCCESS);

    // Execute plan
    // hipfftExecZ2Z: double precision, hipfftExecC2C: for single-precision
    rc = hipfftExecZ2Z(plan, x, x, direction);
    assert(rc == HIPFFT_SUCCESS);

    std::cout << "output:\n";
    hipMemcpy(cdata.data(), x, complex_bytes, hipMemcpyDeviceToHost);
    for(int i = 0; i < Nx; i++)
    {
        for(int j = 0; j < Ny; j++)
        {
            for(int k = 0; k < Nz; k++)
            {
                int pos = (i * Ny + j) * Nz + k;
                std::cout << cdata[pos] << " ";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
    std::cout << std::endl;

    hipfftDestroy(plan);
    hipFree(x);

    return 0;
}
