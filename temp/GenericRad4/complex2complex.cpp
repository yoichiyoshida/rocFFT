
#include "./common.h"

#define NEAR_ZERO_TOLERATE true



#define FORMAT_INTERLEAVED
//#define DoublePrecision
//#define LIST_RESULT
#define FFT_FWD
//#define SEE_HEX
#define BUILD_LOG_SIZE (1024 * 1024)
#define CHECK_RESULT


int main(int argc, char ** argv)
{
	// arg list
	// filename, N, T, B

	char **ps;
	size_t flines;

	size_t B = atoi(argv[2]);
	size_t N = atoi(argv[3]);

	// 1. Get a platform.
	cl_platform_id platform;
	clGetPlatformIDs( 1, &platform, NULL );


	// 2. Find a gpu device.
	cl_device_id device;
	clGetDeviceIDs( platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);

	// 3. Create a context and command queue on that device.
	cl_context context = clCreateContext( NULL, 1, &device,	NULL, NULL, NULL);

	cl_command_queue queue = clCreateCommandQueue( context, device, CL_QUEUE_PROFILING_ENABLE, NULL );

	// 4. Perform runtime source compilation, and obtain kernel entry point.
	flines = file_lines_get(argv[1], &ps);
	cl_program program = clCreateProgramWithSource( context, flines, (const char **)ps, NULL, NULL );
	file_lines_clear(flines, ps);
	cl_int err = clBuildProgram( program, 1, &device, "-I.", NULL, NULL );
	if(err != CL_SUCCESS)
	{
		char *build_log = new char[BUILD_LOG_SIZE];
		size_t log_sz;
		fprintf(stderr, "build program failed, err=%d\n", err);
		err = clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, BUILD_LOG_SIZE, build_log, &log_sz);
		if (err != CL_SUCCESS)
			fprintf(stderr, "failed to get build log, err=%d\n", err);
		else
			fprintf(stderr, "----- Build Log -----\n%s\n----- ----- --- -----\n", build_log);
		delete[] build_log;
		clReleaseProgram(program);
		return -1;

	}

	const char *KERN_NAME = NULL;

	const char *KERN_NAME_1 = NULL;
	const char *KERN_NAME_2 = NULL;

	switch (N)
	{
	case 65536:	KERN_NAME_1 = "fft_65536_1";
				KERN_NAME_2 = "fft_65536_2";
				break;
	case 32768:	KERN_NAME_1 = "fft_32768_1";
				KERN_NAME_2 = "fft_32768_2";
				break;
	case 16384:	KERN_NAME_1 = "fft_16384_1";
				KERN_NAME_2 = "fft_16384_2";
				break;
	case 8192:	KERN_NAME_1 = "fft_8192_1";
				KERN_NAME_2 = "fft_8192_2";
				break;
	case 4096: KERN_NAME = "fft_4096"; break;
	case 2048: KERN_NAME = "fft_2048"; break;
	case 1024: KERN_NAME = "fft_1024"; break;
	case 512:  KERN_NAME = "fft_512";  break;
	case 256:  KERN_NAME = "fft_256";  break;
	case 128:  KERN_NAME = "fft_128";  break;
	case 64:   KERN_NAME = "fft_64";   break;
	case 32:   KERN_NAME = "fft_32";   break;
	case 16:   KERN_NAME = "fft_16";   break;
	case 8:    KERN_NAME = "fft_8";    break;
	case 4:    KERN_NAME = "fft_4";    break;
	case 2:    KERN_NAME = "fft_2";    break;
	case 1:    KERN_NAME = "fft_1";    break;
	}

	cl_kernel kernel;
	cl_kernel kernel_1;
	cl_kernel kernel_2;

	switch (N)
	{
	case 65536:
	case 32768:
	case 16384:
	case 8192:	kernel_1 = clCreateKernel(program, KERN_NAME_1, NULL);
				kernel_2 = clCreateKernel(program, KERN_NAME_2, NULL);

				break;
	case 4096: 
	case 2048: 
	case 1024: 
	case 512:  
	case 256:  
	case 128:  
	case 64:   
	case 32:   
	case 16:   
	case 8:    
	case 4:    
	case 2:    
	case 1:    kernel = clCreateKernel(program, KERN_NAME, NULL);
	}
	// Start FFT

	Type *yr, *yi, *xr, *xi, *refr, *refi;
	Type *xc, *yc;
	yr = new Type [N*B];
	yi = new Type [N*B];
	xr = new Type [N*B];
	xi = new Type [N*B];
	refr = new Type [N*B];
	refi = new Type [N*B];
	xc = new Type [2*N*B];
	yc = new Type [2*N*B];

	for(uint i=0; i<N*B; i++)
	{
		xr[i] =   (Type)(rand()%101);
		xi[i] =   (Type)(rand()%101);
		xc[2*i] = xr[i];
		xc[2*i + 1] = xi[i];
	}

#ifdef LIST_RESULT
	std::cout << "**** INPUT ****" << std::endl;
	for(uint j=0; j<B; j++)
	{
		for(uint i=0; i<N; i++)
		{
			std::cout << "(" << xr[j*N + i] << ", " << xi[j*N + i] << ") " << std::endl;
		}
		std::cout << "=======================BATCH ENDS====================" << std::endl;
	}
#endif

	// Compute reference FFT
	FftwComplex *in, *out;
	Type *ind, *outd;
	FftwPlan p;
	in = (FftwComplex*)FftwMalloc(sizeof(FftwComplex) * N);
	out = (FftwComplex*)FftwMalloc(sizeof(FftwComplex) * N);
	ind = (Type *)in;
	outd = (Type *)out;

#ifdef FFT_FWD
	p = FftwPlanFn(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
#else
	p = FftwPlanFn(N, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
#endif

	for(uint j=0; j<B; j++)
	{
		for(uint i=0; i<N; i++)
		{
			ind[2*i] = xr[j*N + i];
			ind[2*i + 1] = xi[j*N + i];
		}

		FftwExecute(p);

		for(uint i=0; i<N; i++)
		{
			refr[j*N + i] = outd[2*i];
			refi[j*N + i] = outd[2*i + 1];
		}
	}
	
#ifdef LIST_RESULT
	std::cout << "**** REF ****" << std::endl;
	for (uint j = 0; j < B; j++)
	{
		for (uint i = 0; i < N; i++)
		{
#ifndef SEE_HEX
			std::cout << "(" << refr[j*N + i] << ", " << refi[j*N + i] << ") " << std::endl;
#else
			Utype rv, iv;
			rv.f = refr[j*N + i]; iv.f = refi[j*N + i];
			printf("(%0x, %0x)\n", rv.u, iv.u);
#endif
		}
		std::cout << "=======================BATCH ENDS====================" << std::endl;
	}
#endif


#ifndef FORMAT_INTERLEAVED
	cl_mem bufferReal = clCreateBuffer(context, CL_MEM_READ_WRITE, N*B * sizeof(ClType), NULL, NULL);
	cl_mem bufferImag = clCreateBuffer(context, CL_MEM_READ_WRITE, N*B * sizeof(ClType), NULL, NULL);
#else
	cl_mem bufferCplx = clCreateBuffer(context, CL_MEM_READ_WRITE, N*B * sizeof(ClType2), NULL, NULL);
	cl_mem bufferTemp = clCreateBuffer(context, CL_MEM_READ_WRITE, N*B * sizeof(ClType2), NULL, NULL);
#endif

	cl_mem radix = clCreateBuffer(context, CL_MEM_READ_ONLY, 8 * sizeof(cl_uint), NULL, NULL);

	cl_mem dbg = clCreateBuffer(context, CL_MEM_READ_WRITE, N*B * sizeof(ClType2), NULL, NULL);

	// Fill buffers for FFT
#ifndef FORMAT_INTERLEAVED
	clEnqueueWriteBuffer(queue, bufferReal, CL_TRUE, 0, N*B * sizeof(ClType), (void *)xr, 0, NULL, NULL);
	clEnqueueWriteBuffer(queue, bufferImag, CL_TRUE, 0, N*B * sizeof(ClType), (void *)xi, 0, NULL, NULL);
#else
	clEnqueueWriteBuffer(queue, bufferCplx, CL_TRUE, 0, N*B * sizeof(ClType2), (void *)xc, 0, NULL, NULL);
#endif


	cl_uint k_count = B;
	cl_int dir = -1;

#ifndef FORMAT_INTERLEAVED
	clSetKernelArg(kernel, 0, sizeof(bufferReal), (void*)&bufferReal);
	clSetKernelArg(kernel, 1, sizeof(bufferImag), (void*)&bufferImag);
#else
	switch (N)
	{
	case 65536:
	case 32768:
	case 16384:
	case 8192:	clSetKernelArg(kernel_1, 0, sizeof(bufferCplx), (void*)&bufferCplx);
				clSetKernelArg(kernel_1, 1, sizeof(bufferTemp), (void*)&bufferTemp);

				clSetKernelArg(kernel_2, 0, sizeof(bufferTemp), (void*)&bufferTemp);
				clSetKernelArg(kernel_2, 1, sizeof(bufferCplx), (void*)&bufferCplx);

				clSetKernelArg(kernel_1, 2, sizeof(cl_uint), &k_count);
				clSetKernelArg(kernel_1, 3, sizeof(cl_int), &dir);

				clSetKernelArg(kernel_2, 2, sizeof(cl_uint), &k_count);
				clSetKernelArg(kernel_2, 3, sizeof(cl_int), &dir);
		break;
	case 4096:
	case 2048:
	case 1024:
	case 512:
	case 256:
	case 128:
	case 64:
	case 32:
	case 16:
	case 8:
	case 4:
	case 2:
	case 1:    clSetKernelArg(kernel, 0, sizeof(bufferCplx), (void*)&bufferCplx);
	}

#endif

	cl_uint radixp[8] = { 1,1,1,1,1,1,1,1 };
	size_t T = 1;
	size_t WGS = 64;
	size_t NT = 1;

	switch (N)
	{
	case 4096:	T = 4; WGS = 256; NT = 1;

		radixp[0] = 8;
		radixp[1] = 8;
		radixp[2] = 8;
		radixp[3] = 8;

		break;
	case 2048:	T = 4; WGS = 256; NT = 1;

		radixp[0] = 8;
		radixp[1] = 8;
		radixp[2] = 8;
		radixp[3] = 4;

		break;
	case 1024:	T = 4; WGS = 128; NT = 1;

		radixp[0] = 8;
		radixp[1] = 8;
		radixp[2] = 4;
		radixp[3] = 4;

		break;
	case 512:	T = 3; WGS = 64; NT = 1;

		radixp[0] = 8;
		radixp[1] = 8;
		radixp[2] = 8;

		break;
	case 256:	T = 4; WGS = 64; NT = 1;

		radixp[0] = 4;
		radixp[1] = 4;
		radixp[2] = 4;
		radixp[3] = 4;

		break;
	case 128:	T = 3; WGS = 64; NT = 4;

		radixp[0] = 8;
		radixp[1] = 4;
		radixp[2] = 4;

		break;
	case 64:	T = 3; WGS = 64; NT = 4;

		radixp[0] = 4;
		radixp[1] = 4;
		radixp[2] = 4;

		break;
	case 32:	T = 2; WGS = 64; NT = 16;

		radixp[0] = 8;
		radixp[1] = 4;

		break;
	case 16:	T = 2; WGS = 64; NT = 16;

		radixp[0] = 4;
		radixp[1] = 4;

		break;
	case 8:		T = 2; WGS = 64; NT = 32;

		radixp[0] = 4;
		radixp[1] = 2;

		break;
	case 4:		T = 2; WGS = 64; NT = 32;

		radixp[0] = 2;
		radixp[1] = 2;

		break;
	case 2:		T = 1; WGS = 64; NT = 64;

		radixp[0] = 2;

		break;

	default:
		break;
	}

	clEnqueueWriteBuffer(queue, radix, CL_TRUE, 0, 8 * sizeof(cl_uint), (void *)radixp, 0, NULL, NULL);

	// 6. Launch the kernel
	size_t global_work_size[1];
	size_t local_work_size[1];

	size_t global_work_size_1[1];
	size_t local_work_size_1[1];
	size_t global_work_size_2[1];
	size_t local_work_size_2[1];



	switch (N)
	{
	case 65536:
	{
		local_work_size_1[0] = 256;
		global_work_size_1[0] = local_work_size_1[0] * 32 * B;

		local_work_size_2[0] = 256;
		global_work_size_2[0] = local_work_size_2[0] * 32 * B;
	}
	break;
	case 32768:
	{
		local_work_size_1[0] = 128;
		global_work_size_1[0] = local_work_size_1[0] * 32 * B;

		local_work_size_2[0] = 256;
		global_work_size_2[0] = local_work_size_2[0] * 16 * B;
	}
	break;
	case 16384:
		{
			local_work_size_1[0] = 128;
			global_work_size_1[0] = local_work_size_1[0] * 16 * B;

			local_work_size_2[0] = 256;
			global_work_size_2[0] = local_work_size_2[0] * 8 * B;
		}
		break;
	case 8192:
		{
			local_work_size_1[0] = 128;
			global_work_size_1[0] = local_work_size_1[0] * 8 * B;

			local_work_size_2[0] = 128;
			global_work_size_2[0] = local_work_size_2[0] * 8 * B;
		}
		break;
	case 4096:
	case 2048:
	case 1024:
	case 512:
	case 256:
	case 128:
	case 64:
	case 32:
	case 16:
	case 8:
	case 4:
	case 2:
	case 1:
		{
			clSetKernelArg(kernel, 1, sizeof(cl_uint), &k_count);
			clSetKernelArg(kernel, 2, sizeof(cl_int), &dir);

			size_t gw = (B%NT) ? 1 + (B / NT) : (B / NT);
			global_work_size[0] = WGS * gw;
			local_work_size[0] = WGS;
		}
	}




	//clSetKernelArg(kernel, 6, sizeof(dbg), (void*) &dbg);

	std::cout << "count: " << k_count << std::endl;
	std::cout << "N: " << N << std::endl;

	clFinish(queue);
	double tev = 0, time = 0;

	switch (N)
	{
	case 65536:
	case 32768:
	case 16384:
	case 8192:
		{
			std::cout << "globalws_1: " << global_work_size_1[0] << std::endl;
			std::cout << "localws_1: " << local_work_size_1[0] << std::endl;

			std::cout << "globalws_2: " << global_work_size_2[0] << std::endl;
			std::cout << "localws_2: " << local_work_size_2[0] << std::endl;

			for (uint i = 0; i < 10; i++)
			{
				clEnqueueWriteBuffer(queue, bufferCplx, CL_TRUE, 0, N*B * sizeof(ClType2), (void *)xc, 0, NULL, NULL);

				cl_event ev1, ev2;
				Timer tr;
				tr.Start();
				err = clEnqueueNDRangeKernel(queue, kernel_1, 1, NULL, global_work_size_1, local_work_size_1, 0, NULL, &ev1);
				err = clEnqueueNDRangeKernel(queue, kernel_2, 1, NULL, global_work_size_2, local_work_size_2, 1, &ev1, &ev2);
				clWaitForEvents(1, &ev2);
				double timep = tr.Sample();

				time = time == 0 ? timep : time;
				time = timep < time ? timep : time;

				cl_int ks;
				clGetEventInfo(ev1, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(ks), &ks, NULL);
				if (ks != CL_COMPLETE)
					std::cout << "kernel 1 execution not complete" << std::endl;
				clGetEventInfo(ev2, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(ks), &ks, NULL);
				if (ks != CL_COMPLETE)
					std::cout << "kernel 2 execution not complete" << std::endl;

				cl_ulong kbeg1, kbeg2;
				cl_ulong kend1, kend2;
				clGetEventProfilingInfo(ev1, CL_PROFILING_COMMAND_START, sizeof(kbeg1), &kbeg1, NULL);
				clGetEventProfilingInfo(ev1, CL_PROFILING_COMMAND_END, sizeof(kend1), &kend1, NULL);
				clGetEventProfilingInfo(ev2, CL_PROFILING_COMMAND_START, sizeof(kbeg2), &kbeg2, NULL);
				clGetEventProfilingInfo(ev2, CL_PROFILING_COMMAND_END, sizeof(kend2), &kend2, NULL);

				double tevp = 0, tev1 = 0, tev2 = 0;
				tev1 = (double)(kend1 - kbeg1);
				//std::cout << "gpu event1: " << tev1 << std::endl;
				tev2 = (double)(kend2 - kbeg2);
				//std::cout << "gpu event2: " << tev2 << std::endl;
				tevp = tev1 + tev2;

				tev = tev == 0 ? tevp : tev;
				tev = tevp < tev ? tevp : tev;

				clReleaseEvent(ev1);
				clReleaseEvent(ev2);
				clFinish(queue);
			}
		}

		break;
	case 4096:
	case 2048:
	case 1024:
	case 512:
	case 256:
	case 128:
	case 64:
	case 32:
	case 16:
	case 8:
	case 4:
	case 2:
	case 1:
		{
			std::cout << "T: " << T << std::endl;
			std::cout << "NT: " << NT << std::endl;
			std::cout << "globalws: " << global_work_size[0] << std::endl;
			std::cout << "localws: " << local_work_size[0] << std::endl;


			for (uint i = 0; i < 10; i++)
			{
				clEnqueueWriteBuffer(queue, bufferCplx, CL_TRUE, 0, N*B * sizeof(ClType2), (void *)xc, 0, NULL, NULL);

				cl_event ev;
				Timer tr;
				tr.Start();
				err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, global_work_size, local_work_size, 0, NULL, &ev);
				clWaitForEvents(1, &ev);
				double timep = tr.Sample();

				time = time == 0 ? timep : time;
				time = timep < time ? timep : time;

				cl_int ks;
				clGetEventInfo(ev, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(ks), &ks, NULL);
				if (ks != CL_COMPLETE)
					std::cout << "kernel execution not complete" << std::endl;

				cl_ulong kbeg;
				cl_ulong kend;
				clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_START, sizeof(kbeg), &kbeg, NULL);
				clGetEventProfilingInfo(ev, CL_PROFILING_COMMAND_END, sizeof(kend), &kend, NULL);

				double tevp = 0;
				tevp = (double)(kend - kbeg);

				tev = tev == 0 ? tevp : tev;
				tev = tevp < tev ? tevp : tev;

				clReleaseEvent(ev);
				clFinish(queue);
			}
		}
	}




	std::cout << "gpu event time (milliseconds): " << tev/1000000.0 << std::endl;
	double opsconst = 5.0 * (double)N * log((double)N) / log(2.0);
	std::cout << "gflops: " << ((double)B * opsconst)/tev << std::endl;

	std::cout << "cpu time (milliseconds): " << time*1000.0 << std::endl;

	// 7. Look at the results
#ifndef FORMAT_INTERLEAVED
	clEnqueueReadBuffer( queue, bufferReal, CL_TRUE, 0, N*B * sizeof(ClType), (void *)yr, 0, NULL, NULL );
	clEnqueueReadBuffer( queue, bufferImag, CL_TRUE, 0, N*B * sizeof(ClType), (void *)yi, 0, NULL, NULL );
#else
	clEnqueueReadBuffer( queue, bufferCplx, CL_TRUE, 0, N*B * sizeof(ClType2), (void *)yc, 0, NULL, NULL );
#endif

	uint *dq = new uint[N*B];
	clEnqueueReadBuffer( queue, dbg, CL_TRUE, 0, N*B * sizeof(cl_uint), (void *)dq, 0, NULL, NULL );
	clFinish( queue );

	//std::cout << "**** DQ ****" << std::endl;
	//for(uint i=0; i<N; i++)
	//{
	//	std::cout << dq[i] << std::endl;
	//}

#ifdef FORMAT_INTERLEAVED
	for(uint i=0; i<N*B; i++)
	{
		yr[i] = yc[2*i];
		yi[i] = yc[2*i + 1];
	}
#endif


#ifdef LIST_RESULT
	std::cout << "**** MY ****" << std::endl;
	for(uint j=0; j<B; j++)
	{
		for(uint i=0; i<N; i++)
		{
#ifndef SEE_HEX
			std::cout << "(" << yr[j*N + i] << ", " << yi[j*N + i] << ") " << std::endl;
#else
			Utype rv, iv;
			rv.f = yr[j*N + i]; iv.f = yi[j*N + i];
			printf("(%0x, %0x)\n", rv.u, iv.u);
#endif
		}
		std::cout << "=======================BATCH ENDS====================" << std::endl;
	}
#endif


#ifdef CHECK_RESULT
	for(uint i=0; i<N*B; i++)
	{
		if(NEAR_ZERO_TOLERATE)
		{
			if( (abs(refr[i]) < 0.1f) && (abs(yr[i]) < 0.1f) )
				continue;
			if( (abs(refi[i]) < 0.1f) && (abs(yi[i]) < 0.1f) )
				continue;
		}

		if( abs(yr[i] -  refr[i]) > abs(0.03 * refr[i]) )
		{
			std::cout << "FAIL" << std::endl;
			std::cout << "B: " << (i/N) << " index: " << (i%N) << std::endl;
			std::cout << "refr: " << refr[i] << " yr: " << yr[i] << std::endl;
			break;
		}

		if( abs(yi[i] -  refi[i]) > abs(0.03 * refi[i]) )
		{
			std::cout << "FAIL" << std::endl;
			std::cout << "B: " << (i/N) << " index: " << (i%N) << std::endl;
			std::cout << "refi: " << refi[i] << " yi: " << yi[i] << std::endl;
			break;
		}
	}
#endif

	FftwDestroy(p);
    FftwFree(in);
	FftwFree(out);

	delete[] yr;
	delete[] yi;
	delete[] xr;
	delete[] xi;
	delete[] refr;
	delete[] refi;
	delete[] xc;
	delete[] yc;

	delete[] dq;

	return 0;
}

