#ifndef _TWIST_QUDA_CONTRACT_PLUS
#define _TWIST_QUDA_CONTRACT_PLUS

#define tmp_re tmp.x
#define tmp_im tmp.y

#define TOTAL_COMPONENTS 16 

#if (__COMPUTE_CAPABILITY__ >= 130)
__global__ void contractGamma5PlusKernel(double2 *out, double2 *in1, double2 *in2, int maxThreads, int myStride, const int XL, const int YL, const int ZL, const int Parity, const DslashParam param)
{
	int	sid	 = blockIdx.x*blockDim.x + threadIdx.x;
	int	outId	 = sid;
	int	eutId, xCoord1, xCoord2, xCoord3, xCoord4, auxCoord1, auxCoord2;

	if	(sid >= maxThreads)
		return;

	#ifndef USE_TEXTURE_OBJECTS
		#define SPINORTEX	spinorTexDouble
		#define INTERTEX	interTexDouble
	#else
		#define SPINORTEX	param.inTex
		#define INTERTEX	param.xTex
	#endif

	volatile double2		tmp;
	extern __shared__ double	sm[];							//used for data accumulation: blockDim.x * 2 * 16 * sizeof(double)
   
	volatile double			*accum_re = sm + threadIdx.x;				//address it like idx*blockDim, where idx = 4*spinor_idx1 + spinor_idx2
	volatile double			*accum_im = accum_re + TOTAL_COMPONENTS*blockDim.x;

	eutId		 = 2*sid;
	auxCoord1	 = eutId / XL;
	xCoord1		 = eutId - auxCoord1 * XL;
	auxCoord2	 = auxCoord1 / YL;
	xCoord2		 = auxCoord1 - auxCoord2 * YL;
	xCoord4		 = auxCoord2 / ZL;
	xCoord3		 = auxCoord2 - xCoord4 * ZL;

	auxCoord1	 = (Parity + xCoord4 + xCoord3 + xCoord2) & 1;
	xCoord1		+= auxCoord1;
	outId		 = xCoord1 + XL*(xCoord2 + YL*(xCoord3 + ZL*xCoord4));			//AQUI

	//Load the first color component for each input spinor:
	{

	double2 I0	 = fetch_double2(SPINORTEX, sid + 0 * myStride);   
	double2 I1	 = fetch_double2(SPINORTEX, sid + 3 * myStride);   
	double2 I2	 = fetch_double2(SPINORTEX, sid + 6 * myStride);   
	double2 I3	 = fetch_double2(SPINORTEX, sid + 9 * myStride);   
	double2 J0	 = fetch_double2(INTERTEX,  sid + 0 * myStride);   
	double2 J1	 = fetch_double2(INTERTEX,  sid + 3 * myStride);   
	double2 J2	 = fetch_double2(INTERTEX,  sid + 6 * myStride);   
	double2 J3	 = fetch_double2(INTERTEX,  sid + 9 * myStride);   
	
	//compute in1^dag * gamma5:

	tmp_re	 = +I0.x;
	tmp_im	 = -I0.y;
	I0.x	 = +I2.x;
	I0.y	 = -I2.y;
	I2.x	 = tmp_re;
	I2.y	 = tmp_im;	

	tmp_re	 = +I1.x;
	tmp_im	 = -I1.y;
	I1.x	 = +I3.x;
	I1.y	 = -I3.y;
	I3.x	 = tmp_re;
	I3.y	 = tmp_im;	

	//do products for first color component here:
	//00 component:
	tmp_re = I0.x * J0.x - I0.y * J0.y;
	tmp_im = I0.x * J0.y + I0.y * J0.x;	
	accum_re[0*blockDim.x] = tmp_re;
	accum_im[0*blockDim.x] = tmp_im;	
	
	//01 component:
	tmp_re = I0.x * J1.x - I0.y * J1.y;
	tmp_im = I0.x * J1.y + I0.y * J1.x;	
	accum_re[1*blockDim.x] = tmp_re;
	accum_im[1*blockDim.x] = tmp_im;	

	//02 component:
	tmp_re = I0.x * J2.x - I0.y * J2.y;
	tmp_im = I0.x * J2.y + I0.y * J2.x;	
	accum_re[2*blockDim.x] = tmp_re;
	accum_im[2*blockDim.x] = tmp_im;	
      
	//03 component:
	tmp_re = I0.x * J3.x - I0.y * J3.y;
	tmp_im = I0.x * J3.y + I0.y * J3.x;	
	accum_re[3*blockDim.x] = tmp_re;
	accum_im[3*blockDim.x] = tmp_im;	
      
	//10 component:
	tmp_re = I1.x * J0.x - I1.y * J0.y;
	tmp_im = I1.x * J0.y + I1.y * J0.x;	
	accum_re[4*blockDim.x] = tmp_re;
	accum_im[4*blockDim.x] = tmp_im;	

	//11 component:
	tmp_re = I1.x * J1.x - I1.y * J1.y;
	tmp_im = I1.x * J1.y + I1.y * J1.x;	
	accum_re[5*blockDim.x] = tmp_re;
	accum_im[5*blockDim.x] = tmp_im;	

	//12 component:
	tmp_re = I1.x * J2.x - I1.y * J2.y;
	tmp_im = I1.x * J2.y + I1.y * J2.x;	
	accum_re[6*blockDim.x] = tmp_re;
	accum_im[6*blockDim.x] = tmp_im;	

	//13 component:
	tmp_re = I1.x * J3.x - I1.y * J3.y;
	tmp_im = I1.x * J3.y + I1.y * J3.x;	
	accum_re[7*blockDim.x] = tmp_re;
	accum_im[7*blockDim.x] = tmp_im;	

	//20 component:
	tmp_re = I2.x * J0.x - I2.y * J0.y;
	tmp_im = I2.x * J0.y + I2.y * J0.x;	
	accum_re[8*blockDim.x] = tmp_re;
	accum_im[8*blockDim.x] = tmp_im;	

	//21 component:
	tmp_re = I2.x * J1.x - I2.y * J1.y;
	tmp_im = I2.x * J1.y + I2.y * J1.x;	
	accum_re[9*blockDim.x] = tmp_re;
	accum_im[9*blockDim.x] = tmp_im;	

	//22 component:
	tmp_re = I2.x * J2.x - I2.y * J2.y;
	tmp_im = I2.x * J2.y + I2.y * J2.x;	
	accum_re[10*blockDim.x] = tmp_re;
	accum_im[10*blockDim.x] = tmp_im;	

	//23 component:
	tmp_re = I2.x * J3.x - I2.y * J3.y;
	tmp_im = I2.x * J3.y + I2.y * J3.x;	
	accum_re[11*blockDim.x] = tmp_re;
	accum_im[11*blockDim.x] = tmp_im;	

	//30 component:
	tmp_re = I3.x * J0.x - I3.y * J0.y;
	tmp_im = I3.x * J0.y + I3.y * J0.x;	
	accum_re[12*blockDim.x] = tmp_re;
	accum_im[12*blockDim.x] = tmp_im;	

	//31 component:
	tmp_re = I3.x * J1.x - I3.y * J1.y;
	tmp_im = I3.x * J1.y + I3.y * J1.x;	
	accum_re[13*blockDim.x] = tmp_re;
	accum_im[13*blockDim.x] = tmp_im;	

	//32 component:
	tmp_re = I3.x * J2.x - I3.y * J2.y;
	tmp_im = I3.x * J2.y + I3.y * J2.x;	
	accum_re[14*blockDim.x] = tmp_re;
	accum_im[14*blockDim.x] = tmp_im;	

	//33 component:
	tmp_re = I3.x * J3.x - I3.y * J3.y;
	tmp_im = I3.x * J3.y + I3.y * J3.x;	
	accum_re[15*blockDim.x] = tmp_re;
	accum_im[15*blockDim.x] = tmp_im;	
	}

	//Load the second color component for each input spinor:
	{

	double2 I0	 = fetch_double2(SPINORTEX, sid + 1 * myStride);   
	double2 I1	 = fetch_double2(SPINORTEX, sid + 4 * myStride);   
	double2 I2	 = fetch_double2(SPINORTEX, sid + 7 * myStride);   
	double2 I3	 = fetch_double2(SPINORTEX, sid + 10* myStride);   
	double2 J0	 = fetch_double2(INTERTEX,  sid + 1 * myStride);   
	double2 J1	 = fetch_double2(INTERTEX,  sid + 4 * myStride);   
	double2 J2	 = fetch_double2(INTERTEX,  sid + 7 * myStride);   
	double2 J3	 = fetch_double2(INTERTEX,  sid + 10* myStride);   

	//compute in1^dag * gamma5:
	tmp_re	 = +I0.x;
	tmp_im	 = -I0.y;
	I0.x	 = +I2.x;
	I0.y	 = -I2.y;
	I2.x	 = tmp_re;
	I2.y	 = tmp_im;	

	tmp_re	 = +I1.x;
	tmp_im	 = -I1.y;
	I1.x	 = +I3.x;
	I1.y	 = -I3.y;
	I3.x	 = tmp_re;
	I3.y	 = tmp_im;

	//do products for first color component here:
	//00 component:
	tmp_re = I0.x * J0.x - I0.y * J0.y;
	tmp_im = I0.x * J0.y + I0.y * J0.x;	
	accum_re[0*blockDim.x] += tmp_re;
	accum_im[0*blockDim.x] += tmp_im;	

	//01 component:
	tmp_re = I0.x * J1.x - I0.y * J1.y;
	tmp_im = I0.x * J1.y + I0.y * J1.x;	
	accum_re[1*blockDim.x] += tmp_re;
	accum_im[1*blockDim.x] += tmp_im;	
       
	//02 component:
	tmp_re = I0.x * J2.x - I0.y * J2.y;
	tmp_im = I0.x * J2.y + I0.y * J2.x;	
	accum_re[2*blockDim.x] += tmp_re;
	accum_im[2*blockDim.x] += tmp_im;	

	//03 component:
	tmp_re = I0.x * J3.x - I0.y * J3.y;
	tmp_im = I0.x * J3.y + I0.y * J3.x;	
	accum_re[3*blockDim.x] += tmp_re;
	accum_im[3*blockDim.x] += tmp_im;	

	//10 component:
	tmp_re = I1.x * J0.x - I1.y * J0.y;
	tmp_im = I1.x * J0.y + I1.y * J0.x;	
	accum_re[4*blockDim.x] += tmp_re;
	accum_im[4*blockDim.x] += tmp_im;	

	//11 component:
	tmp_re = I1.x * J1.x - I1.y * J1.y;
	tmp_im = I1.x * J1.y + I1.y * J1.x;	
	accum_re[5*blockDim.x] += tmp_re;
	accum_im[5*blockDim.x] += tmp_im;	
       
	//12 component:
	tmp_re = I1.x * J2.x - I1.y * J2.y;
	tmp_im = I1.x * J2.y + I1.y * J2.x;	
	accum_re[6*blockDim.x] += tmp_re;
	accum_im[6*blockDim.x] += tmp_im;	

	//13 component:
	tmp_re = I1.x * J3.x - I1.y * J3.y;
	tmp_im = I1.x * J3.y + I1.y * J3.x;	
	accum_re[7*blockDim.x] += tmp_re;
	accum_im[7*blockDim.x] += tmp_im;	
       
	//20 component:
	tmp_re = I2.x * J0.x - I2.y * J0.y;
	tmp_im = I2.x * J0.y + I2.y * J0.x;	
	accum_re[8*blockDim.x] += tmp_re;
	accum_im[8*blockDim.x] += tmp_im;	

	//21 component:
	tmp_re = I2.x * J1.x - I2.y * J1.y;
	tmp_im = I2.x * J1.y + I2.y * J1.x;	
	accum_re[9*blockDim.x] += tmp_re;
	accum_im[9*blockDim.x] += tmp_im;	
       
	//22 component:
	tmp_re = I2.x * J2.x - I2.y * J2.y;
	tmp_im = I2.x * J2.y + I2.y * J2.x;	
	accum_re[10*blockDim.x] += tmp_re;
	accum_im[10*blockDim.x] += tmp_im;	

	//23 component:
	tmp_re = I2.x * J3.x - I2.y * J3.y;
	tmp_im = I2.x * J3.y + I2.y * J3.x;	
	accum_re[11*blockDim.x] += tmp_re;
	accum_im[11*blockDim.x] += tmp_im;	
       
	//30 component:
	tmp_re = I3.x * J0.x - I3.y * J0.y;
	tmp_im = I3.x * J0.y + I3.y * J0.x;	
	accum_re[12*blockDim.x] += tmp_re;
	accum_im[12*blockDim.x] += tmp_im;	

	//31 component:
	tmp_re = I3.x * J1.x - I3.y * J1.y;
	tmp_im = I3.x * J1.y + I3.y * J1.x;	
	accum_re[13*blockDim.x] += tmp_re;
	accum_im[13*blockDim.x] += tmp_im;	
       
	//32 component:
	tmp_re = I3.x * J2.x - I3.y * J2.y;
	tmp_im = I3.x * J2.y + I3.y * J2.x;	
	accum_re[14*blockDim.x] += tmp_re;
	accum_im[14*blockDim.x] += tmp_im;	

	//33 component:
	tmp_re = I3.x * J3.x - I3.y * J3.y;
	tmp_im = I3.x * J3.y + I3.y * J3.x;	
	accum_re[15*blockDim.x] += tmp_re;
	accum_im[15*blockDim.x] += tmp_im;	
	}

	//Load the third color component for each input spinor:
	{

        double2 I0  = fetch_double2(SPINORTEX, sid + 2 * myStride);   
	double2 I1  = fetch_double2(SPINORTEX, sid + 5 * myStride);   
	double2 I2  = fetch_double2(SPINORTEX, sid + 8 * myStride);   
	double2 I3  = fetch_double2(SPINORTEX, sid + 11* myStride);   
	double2 J0  = fetch_double2(INTERTEX,  sid + 2 * myStride);   
	double2 J1  = fetch_double2(INTERTEX,  sid + 5 * myStride);   
	double2 J2  = fetch_double2(INTERTEX,  sid + 8 * myStride);   
	double2 J3  = fetch_double2(INTERTEX,  sid + 11* myStride);   

	//compute in1^dag * gamma5:
	tmp_re	 = +I0.x;
	tmp_im	 = -I0.y;
	I0.x	 = +I2.x;
	I0.y	 = -I2.y;
	I2.x	 = tmp_re;
	I2.y	 = tmp_im;	

	tmp_re	 = +I1.x;
	tmp_im	 = -I1.y;
	I1.x	 = +I3.x;
	I1.y	 = -I3.y;
	I3.x	 = tmp_re;
	I3.y	 = tmp_im;	

	//do products for first color component here:
	//00 component:
	tmp_re = I0.x * J0.x - I0.y * J0.y;
	tmp_im = I0.x * J0.y + I0.y * J0.x;	
	accum_re[0*blockDim.x] += tmp_re;
	accum_im[0*blockDim.x] += tmp_im;	

	//01 component:
	tmp_re = I0.x * J1.x - I0.y * J1.y;
	tmp_im = I0.x * J1.y + I0.y * J1.x;	
	accum_re[1*blockDim.x] += tmp_re;
	accum_im[1*blockDim.x] += tmp_im;	

	//02 component:
	tmp_re = I0.x * J2.x - I0.y * J2.y;
	tmp_im = I0.x * J2.y + I0.y * J2.x;	
	accum_re[2*blockDim.x] += tmp_re;
	accum_im[2*blockDim.x] += tmp_im;	

	//03 component:
	tmp_re = I0.x * J3.x - I0.y * J3.y;
	tmp_im = I0.x * J3.y + I0.y * J3.x;	
	accum_re[3*blockDim.x] += tmp_re;
	accum_im[3*blockDim.x] += tmp_im;	

	//10 component:
	tmp_re = I1.x * J0.x - I1.y * J0.y;
	tmp_im = I1.x * J0.y + I1.y * J0.x;	
	accum_re[4*blockDim.x] += tmp_re;
	accum_im[4*blockDim.x] += tmp_im;	

	//11 component:
	tmp_re = I1.x * J1.x - I1.y * J1.y;
	tmp_im = I1.x * J1.y + I1.y * J1.x;	
	accum_re[5*blockDim.x] += tmp_re;
	accum_im[5*blockDim.x] += tmp_im;	

	//12 component:
	tmp_re = I1.x * J2.x - I1.y * J2.y;
	tmp_im = I1.x * J2.y + I1.y * J2.x;	
	accum_re[6*blockDim.x] += tmp_re;
	accum_im[6*blockDim.x] += tmp_im;	

	//13 component:
	tmp_re = I1.x * J3.x - I1.y * J3.y;
	tmp_im = I1.x * J3.y + I1.y * J3.x;
	accum_re[7*blockDim.x] += tmp_re;
	accum_im[7*blockDim.x] += tmp_im;

	//20 component:
	tmp_re = I2.x * J0.x - I2.y * J0.y;
	tmp_im = I2.x * J0.y + I2.y * J0.x;
	accum_re[8*blockDim.x] += tmp_re;
	accum_im[8*blockDim.x] += tmp_im;

	//21 component:
	tmp_re = I2.x * J1.x - I2.y * J1.y;
	tmp_im = I2.x * J1.y + I2.y * J1.x;
	accum_re[9*blockDim.x] += tmp_re;
	accum_im[9*blockDim.x] += tmp_im;

	//22 component:
	tmp_re = I2.x * J2.x - I2.y * J2.y;
	tmp_im = I2.x * J2.y + I2.y * J2.x;
	accum_re[10*blockDim.x] += tmp_re;
	accum_im[10*blockDim.x] += tmp_im;

	//23 component:
	tmp_re = I2.x * J3.x - I2.y * J3.y;
	tmp_im = I2.x * J3.y + I2.y * J3.x;
	accum_re[11*blockDim.x] += tmp_re;
	accum_im[11*blockDim.x] += tmp_im;

	//30 component:
	tmp_re = I3.x * J0.x - I3.y * J0.y;
	tmp_im = I3.x * J0.y + I3.y * J0.x;
	accum_re[12*blockDim.x] += tmp_re;
	accum_im[12*blockDim.x] += tmp_im;

	//31 component:
	tmp_re = I3.x * J1.x - I3.y * J1.y;
	tmp_im = I3.x * J1.y + I3.y * J1.x;
	accum_re[13*blockDim.x] += tmp_re;
	accum_im[13*blockDim.x] += tmp_im;

	//32 component:
	tmp_re = I3.x * J2.x - I3.y * J2.y;
	tmp_im = I3.x * J2.y + I3.y * J2.x;
	accum_re[14*blockDim.x] += tmp_re;
	accum_im[14*blockDim.x] += tmp_im;

	//33 component:
	tmp_re = I3.x * J3.x - I3.y * J3.y;
	tmp_im = I3.x * J3.y + I3.y * J3.x;
	accum_re[15*blockDim.x] += tmp_re;
	accum_im[15*blockDim.x] += tmp_im;
	}

   //Store output back to global buffer:


/*	CONTRACTION FULL VOLUME		*/

	out[outId + 0 *maxThreads*2].x	+= accum_re[ 0*blockDim.x];
	out[outId + 1 *maxThreads*2].x	+= accum_re[ 1*blockDim.x];
	out[outId + 2 *maxThreads*2].x	+= accum_re[ 2*blockDim.x];
	out[outId + 3 *maxThreads*2].x	+= accum_re[ 3*blockDim.x];
	out[outId + 4 *maxThreads*2].x	+= accum_re[ 4*blockDim.x];
	out[outId + 5 *maxThreads*2].x	+= accum_re[ 5*blockDim.x];
	out[outId + 6 *maxThreads*2].x	+= accum_re[ 6*blockDim.x];
	out[outId + 7 *maxThreads*2].x	+= accum_re[ 7*blockDim.x];
	out[outId + 8 *maxThreads*2].x	+= accum_re[ 8*blockDim.x];
	out[outId + 9 *maxThreads*2].x	+= accum_re[ 9*blockDim.x];
	out[outId + 10*maxThreads*2].x	+= accum_re[10*blockDim.x];
	out[outId + 11*maxThreads*2].x	+= accum_re[11*blockDim.x];
	out[outId + 12*maxThreads*2].x	+= accum_re[12*blockDim.x];
	out[outId + 13*maxThreads*2].x	+= accum_re[13*blockDim.x];
	out[outId + 14*maxThreads*2].x	+= accum_re[14*blockDim.x];
	out[outId + 15*maxThreads*2].x	+= accum_re[15*blockDim.x];

	out[outId + 0 *maxThreads*2].y	+= accum_im[ 0*blockDim.x];
	out[outId + 1 *maxThreads*2].y	+= accum_im[ 1*blockDim.x];
	out[outId + 2 *maxThreads*2].y	+= accum_im[ 2*blockDim.x];
	out[outId + 3 *maxThreads*2].y	+= accum_im[ 3*blockDim.x];
	out[outId + 4 *maxThreads*2].y	+= accum_im[ 4*blockDim.x];
	out[outId + 5 *maxThreads*2].y	+= accum_im[ 5*blockDim.x];
	out[outId + 6 *maxThreads*2].y	+= accum_im[ 6*blockDim.x];
	out[outId + 7 *maxThreads*2].y	+= accum_im[ 7*blockDim.x];
	out[outId + 8 *maxThreads*2].y	+= accum_im[ 8*blockDim.x];
	out[outId + 9 *maxThreads*2].y	+= accum_im[ 9*blockDim.x];
	out[outId + 10*maxThreads*2].y	+= accum_im[10*blockDim.x];
	out[outId + 11*maxThreads*2].y	+= accum_im[11*blockDim.x];
	out[outId + 12*maxThreads*2].y	+= accum_im[12*blockDim.x];
	out[outId + 13*maxThreads*2].y	+= accum_im[13*blockDim.x];
	out[outId + 14*maxThreads*2].y	+= accum_im[14*blockDim.x];
	out[outId + 15*maxThreads*2].y	+= accum_im[15*blockDim.x];

	#undef SPINORTEX
	#undef INTERTEX

	return;
}

//Perform trace in color space only and for a given tslice 
//since the file is included in dslash_quda.h, no need to add dslash_constants.h file here (for, e.g., Vsh)
__global__ void contractTslicePlusKernelD(double2 *out, double2 *in1, double2 *in2, int maxThreads, int myStride, const int XL, const int YL, const int ZL, const int Tslice, const int Parity, const DslashParam param)
{
	int	sid	 = blockIdx.x*blockDim.x + threadIdx.x;					//number of threads is equal to Tslice volume
												//Adjust sid to correct tslice (exe domain must be Tslice volume!)
	int	inId	 = sid + Vsh*Tslice;							//Vsh - 3d space volume for the parity spinor (equale to exe domain!)
	int	outId; 
	int	eutId, xCoord1, xCoord2, xCoord3, xCoord4, auxCoord1, auxCoord2;

	if	(sid >= maxThreads)								//maxThreads == tslice volume
		return;

	#ifndef USE_TEXTURE_OBJECTS
		#define SPINORTEX	spinorTexDouble
		#define INTERTEX	interTexDouble
	#else
		#define SPINORTEX	param.inTex
		#define INTERTEX	param.xTex
	#endif

	volatile double2		tmp;
	extern __shared__ double	sm[];							//used for data accumulation: blockDim.x * 2 * 16 * sizeof(double)
   
	volatile double			*accum_re = sm + threadIdx.x;				//address it like idx*blockDim, where idx = 4*spinor_idx1 + spinor_idx2
	volatile double			*accum_im = accum_re + TOTAL_COMPONENTS*blockDim.x;

//The output only for a given tslice (for the full tslice content, i.e., both parities!):

	eutId		 = 2*inId;
	auxCoord1	 = eutId / XL;
	xCoord1		 = eutId - auxCoord1 * XL;
	auxCoord2	 = auxCoord1 / YL;
	xCoord2		 = auxCoord1 - auxCoord2 * YL;
	xCoord4		 = auxCoord2 / ZL;

//	if	(Tslice != xCoord4)
//		return;

	xCoord3		 = auxCoord2 - xCoord4 * ZL;

	auxCoord1	 = (Parity + xCoord4 + xCoord3 + xCoord2) & 1;
	xCoord1		+= auxCoord1;
	outId		 = xCoord1 + XL*(xCoord2 + YL*xCoord3);					//AQUI

	//Load the first color component for each input spinor:
	{

	double2 I0	 = fetch_double2(SPINORTEX, inId + 0 * myStride);   
	double2 I1	 = fetch_double2(SPINORTEX, inId + 3 * myStride);   
	double2 I2	 = fetch_double2(SPINORTEX, inId + 6 * myStride);   
	double2 I3	 = fetch_double2(SPINORTEX, inId + 9 * myStride);   
	double2 J0	 = fetch_double2(INTERTEX,  inId + 0 * myStride);   
	double2 J1	 = fetch_double2(INTERTEX,  inId + 3 * myStride);   
	double2 J2	 = fetch_double2(INTERTEX,  inId + 6 * myStride);   
	double2 J3	 = fetch_double2(INTERTEX,  inId + 9 * myStride);   
	
	//compute in1^dag:

	I0.y	 = -I0.y;
	I1.y	 = -I1.y;	
	I2.y	 = -I2.y;
	I3.y	 = -I3.y;	
	
	//do products for first color component here:
	//00 component:
	tmp_re = I0.x * J0.x - I0.y * J0.y;
	tmp_im = I0.x * J0.y + I0.y * J0.x;	
	accum_re[0*blockDim.x] = tmp_re;
	accum_im[0*blockDim.x] = tmp_im;	
	
	//01 component:
	tmp_re = I0.x * J1.x - I0.y * J1.y;
	tmp_im = I0.x * J1.y + I0.y * J1.x;	
	accum_re[1*blockDim.x] = tmp_re;
	accum_im[1*blockDim.x] = tmp_im;	

	//02 component:
	tmp_re = I0.x * J2.x - I0.y * J2.y;
	tmp_im = I0.x * J2.y + I0.y * J2.x;	
	accum_re[2*blockDim.x] = tmp_re;
	accum_im[2*blockDim.x] = tmp_im;	
      
	//03 component:
	tmp_re = I0.x * J3.x - I0.y * J3.y;
	tmp_im = I0.x * J3.y + I0.y * J3.x;	
	accum_re[3*blockDim.x] = tmp_re;
	accum_im[3*blockDim.x] = tmp_im;	
      
	//10 component:
	tmp_re = I1.x * J0.x - I1.y * J0.y;
	tmp_im = I1.x * J0.y + I1.y * J0.x;	
	accum_re[4*blockDim.x] = tmp_re;
	accum_im[4*blockDim.x] = tmp_im;	

	//11 component:
	tmp_re = I1.x * J1.x - I1.y * J1.y;
	tmp_im = I1.x * J1.y + I1.y * J1.x;	
	accum_re[5*blockDim.x] = tmp_re;
	accum_im[5*blockDim.x] = tmp_im;	

	//12 component:
	tmp_re = I1.x * J2.x - I1.y * J2.y;
	tmp_im = I1.x * J2.y + I1.y * J2.x;	
	accum_re[6*blockDim.x] = tmp_re;
	accum_im[6*blockDim.x] = tmp_im;	

	//13 component:
	tmp_re = I1.x * J3.x - I1.y * J3.y;
	tmp_im = I1.x * J3.y + I1.y * J3.x;	
	accum_re[7*blockDim.x] = tmp_re;
	accum_im[7*blockDim.x] = tmp_im;	

	//20 component:
	tmp_re = I2.x * J0.x - I2.y * J0.y;
	tmp_im = I2.x * J0.y + I2.y * J0.x;	
	accum_re[8*blockDim.x] = tmp_re;
	accum_im[8*blockDim.x] = tmp_im;	

	//21 component:
	tmp_re = I2.x * J1.x - I2.y * J1.y;
	tmp_im = I2.x * J1.y + I2.y * J1.x;	
	accum_re[9*blockDim.x] = tmp_re;
	accum_im[9*blockDim.x] = tmp_im;	

	//22 component:
	tmp_re = I2.x * J2.x - I2.y * J2.y;
	tmp_im = I2.x * J2.y + I2.y * J2.x;	
	accum_re[10*blockDim.x] = tmp_re;
	accum_im[10*blockDim.x] = tmp_im;	

	//23 component:
	tmp_re = I2.x * J3.x - I2.y * J3.y;
	tmp_im = I2.x * J3.y + I2.y * J3.x;	
	accum_re[11*blockDim.x] = tmp_re;
	accum_im[11*blockDim.x] = tmp_im;	

	//30 component:
	tmp_re = I3.x * J0.x - I3.y * J0.y;
	tmp_im = I3.x * J0.y + I3.y * J0.x;	
	accum_re[12*blockDim.x] = tmp_re;
	accum_im[12*blockDim.x] = tmp_im;	

	//31 component:
	tmp_re = I3.x * J1.x - I3.y * J1.y;
	tmp_im = I3.x * J1.y + I3.y * J1.x;	
	accum_re[13*blockDim.x] = tmp_re;
	accum_im[13*blockDim.x] = tmp_im;	

	//32 component:
	tmp_re = I3.x * J2.x - I3.y * J2.y;
	tmp_im = I3.x * J2.y + I3.y * J2.x;	
	accum_re[14*blockDim.x] = tmp_re;
	accum_im[14*blockDim.x] = tmp_im;	

	//33 component:
	tmp_re = I3.x * J3.x - I3.y * J3.y;
	tmp_im = I3.x * J3.y + I3.y * J3.x;	
	accum_re[15*blockDim.x] = tmp_re;
	accum_im[15*blockDim.x] = tmp_im;
	}

	//Load the second color component for each input spinor:
	{

	double2 I0	 = fetch_double2(SPINORTEX, inId + 1 * myStride);   
	double2 I1	 = fetch_double2(SPINORTEX, inId + 4 * myStride);   
	double2 I2	 = fetch_double2(SPINORTEX, inId + 7 * myStride);   
	double2 I3	 = fetch_double2(SPINORTEX, inId + 10* myStride);   
	double2 J0	 = fetch_double2(INTERTEX,  inId + 1 * myStride);   
	double2 J1	 = fetch_double2(INTERTEX,  inId + 4 * myStride);   
	double2 J2	 = fetch_double2(INTERTEX,  inId + 7 * myStride);   
	double2 J3	 = fetch_double2(INTERTEX,  inId + 10* myStride);   

	//compute in^dag
	I0.y	 = -I0.y;
	I1.y	 = -I1.y;	
	I2.y	 = -I2.y;
	I3.y	 = -I3.y;	

	//do products for first color component here:
	//00 component:
	tmp_re = I0.x * J0.x - I0.y * J0.y;
	tmp_im = I0.x * J0.y + I0.y * J0.x;	
	accum_re[0*blockDim.x] += tmp_re;
	accum_im[0*blockDim.x] += tmp_im;	

	//01 component:
	tmp_re = I0.x * J1.x - I0.y * J1.y;
	tmp_im = I0.x * J1.y + I0.y * J1.x;	
	accum_re[1*blockDim.x] += tmp_re;
	accum_im[1*blockDim.x] += tmp_im;	
       
	//02 component:
	tmp_re = I0.x * J2.x - I0.y * J2.y;
	tmp_im = I0.x * J2.y + I0.y * J2.x;	
	accum_re[2*blockDim.x] += tmp_re;
	accum_im[2*blockDim.x] += tmp_im;	

	//03 component:
	tmp_re = I0.x * J3.x - I0.y * J3.y;
	tmp_im = I0.x * J3.y + I0.y * J3.x;	
	accum_re[3*blockDim.x] += tmp_re;
	accum_im[3*blockDim.x] += tmp_im;	

	//10 component:
	tmp_re = I1.x * J0.x - I1.y * J0.y;
	tmp_im = I1.x * J0.y + I1.y * J0.x;	
	accum_re[4*blockDim.x] += tmp_re;
	accum_im[4*blockDim.x] += tmp_im;	

	//11 component:
	tmp_re = I1.x * J1.x - I1.y * J1.y;
	tmp_im = I1.x * J1.y + I1.y * J1.x;	
	accum_re[5*blockDim.x] += tmp_re;
	accum_im[5*blockDim.x] += tmp_im;	
       
	//12 component:
	tmp_re = I1.x * J2.x - I1.y * J2.y;
	tmp_im = I1.x * J2.y + I1.y * J2.x;	
	accum_re[6*blockDim.x] += tmp_re;
	accum_im[6*blockDim.x] += tmp_im;	

	//13 component:
	tmp_re = I1.x * J3.x - I1.y * J3.y;
	tmp_im = I1.x * J3.y + I1.y * J3.x;	
	accum_re[7*blockDim.x] += tmp_re;
	accum_im[7*blockDim.x] += tmp_im;	
       
	//20 component:
	tmp_re = I2.x * J0.x - I2.y * J0.y;
	tmp_im = I2.x * J0.y + I2.y * J0.x;	
	accum_re[8*blockDim.x] += tmp_re;
	accum_im[8*blockDim.x] += tmp_im;	

	//21 component:
	tmp_re = I2.x * J1.x - I2.y * J1.y;
	tmp_im = I2.x * J1.y + I2.y * J1.x;	
	accum_re[9*blockDim.x] += tmp_re;
	accum_im[9*blockDim.x] += tmp_im;	
       
	//22 component:
	tmp_re = I2.x * J2.x - I2.y * J2.y;
	tmp_im = I2.x * J2.y + I2.y * J2.x;	
	accum_re[10*blockDim.x] += tmp_re;
	accum_im[10*blockDim.x] += tmp_im;

	//23 component:
	tmp_re = I2.x * J3.x - I2.y * J3.y;
	tmp_im = I2.x * J3.y + I2.y * J3.x;	
	accum_re[11*blockDim.x] += tmp_re;
	accum_im[11*blockDim.x] += tmp_im;
       
	//30 component:
	tmp_re = I3.x * J0.x - I3.y * J0.y;
	tmp_im = I3.x * J0.y + I3.y * J0.x;	
	accum_re[12*blockDim.x] += tmp_re;
	accum_im[12*blockDim.x] += tmp_im;

	//31 component:
	tmp_re = I3.x * J1.x - I3.y * J1.y;
	tmp_im = I3.x * J1.y + I3.y * J1.x;	
	accum_re[13*blockDim.x] += tmp_re;
	accum_im[13*blockDim.x] += tmp_im;
       
	//32 component:
	tmp_re = I3.x * J2.x - I3.y * J2.y;
	tmp_im = I3.x * J2.y + I3.y * J2.x;	
	accum_re[14*blockDim.x] += tmp_re;
	accum_im[14*blockDim.x] += tmp_im;

	//33 component:
	tmp_re = I3.x * J3.x - I3.y * J3.y;
	tmp_im = I3.x * J3.y + I3.y * J3.x;	
	accum_re[15*blockDim.x] += tmp_re;
	accum_im[15*blockDim.x] += tmp_im;
	}

	//Load the third color component for each input spinor:
	{

        double2 I0  = fetch_double2(SPINORTEX, inId + 2 * myStride);   
	double2 I1  = fetch_double2(SPINORTEX, inId + 5 * myStride);   
	double2 I2  = fetch_double2(SPINORTEX, inId + 8 * myStride);   
	double2 I3  = fetch_double2(SPINORTEX, inId + 11* myStride);   
	double2 J0  = fetch_double2(INTERTEX,  inId + 2 * myStride);   
	double2 J1  = fetch_double2(INTERTEX,  inId + 5 * myStride);   
	double2 J2  = fetch_double2(INTERTEX,  inId + 8 * myStride);   
	double2 J3  = fetch_double2(INTERTEX,  inId + 11* myStride);   

	//compute in^dag
	I0.y	 = -I0.y;
	I1.y	 = -I1.y;	
	I2.y	 = -I2.y;
	I3.y	 = -I3.y;	

	//do products for first color component here:
	//00 component:
	tmp_re = I0.x * J0.x - I0.y * J0.y;
	tmp_im = I0.x * J0.y + I0.y * J0.x;	
	accum_re[0*blockDim.x] += tmp_re;
	accum_im[0*blockDim.x] += tmp_im;	

	//01 component:
	tmp_re = I0.x * J1.x - I0.y * J1.y;
	tmp_im = I0.x * J1.y + I0.y * J1.x;	
	accum_re[1*blockDim.x] += tmp_re;
	accum_im[1*blockDim.x] += tmp_im;	

	//02 component:
	tmp_re = I0.x * J2.x - I0.y * J2.y;
	tmp_im = I0.x * J2.y + I0.y * J2.x;	
	accum_re[2*blockDim.x] += tmp_re;
	accum_im[2*blockDim.x] += tmp_im;	

	//03 component:
	tmp_re = I0.x * J3.x - I0.y * J3.y;
	tmp_im = I0.x * J3.y + I0.y * J3.x;	
	accum_re[3*blockDim.x] += tmp_re;
	accum_im[3*blockDim.x] += tmp_im;	

	//10 component:
	tmp_re = I1.x * J0.x - I1.y * J0.y;
	tmp_im = I1.x * J0.y + I1.y * J0.x;	
	accum_re[4*blockDim.x] += tmp_re;
	accum_im[4*blockDim.x] += tmp_im;	

	//11 component:
	tmp_re = I1.x * J1.x - I1.y * J1.y;
	tmp_im = I1.x * J1.y + I1.y * J1.x;	
	accum_re[5*blockDim.x] += tmp_re;
	accum_im[5*blockDim.x] += tmp_im;	

	//12 component:
	tmp_re = I1.x * J2.x - I1.y * J2.y;
	tmp_im = I1.x * J2.y + I1.y * J2.x;	
	accum_re[6*blockDim.x] += tmp_re;
	accum_im[6*blockDim.x] += tmp_im;	

	//13 component:
	tmp_re = I1.x * J3.x - I1.y * J3.y;
	tmp_im = I1.x * J3.y + I1.y * J3.x;
	accum_re[7*blockDim.x] += tmp_re;
	accum_im[7*blockDim.x] += tmp_im;

	//20 component:
	tmp_re = I2.x * J0.x - I2.y * J0.y;
	tmp_im = I2.x * J0.y + I2.y * J0.x;
	accum_re[8*blockDim.x] += tmp_re;
	accum_im[8*blockDim.x] += tmp_im;

	//21 component:
	tmp_re = I2.x * J1.x - I2.y * J1.y;
	tmp_im = I2.x * J1.y + I2.y * J1.x;
	accum_re[9*blockDim.x] += tmp_re;
	accum_im[9*blockDim.x] += tmp_im;

	//22 component:
	tmp_re = I2.x * J2.x - I2.y * J2.y;
	tmp_im = I2.x * J2.y + I2.y * J2.x;
	accum_re[10*blockDim.x] += tmp_re;
	accum_im[10*blockDim.x] += tmp_im;

	//23 component:
	tmp_re = I2.x * J3.x - I2.y * J3.y;
	tmp_im = I2.x * J3.y + I2.y * J3.x;
	accum_re[11*blockDim.x] += tmp_re;
	accum_im[11*blockDim.x] += tmp_im;

	//30 component:
	tmp_re = I3.x * J0.x - I3.y * J0.y;
	tmp_im = I3.x * J0.y + I3.y * J0.x;
	accum_re[12*blockDim.x] += tmp_re;
	accum_im[12*blockDim.x] += tmp_im;

	//31 component:
	tmp_re = I3.x * J1.x - I3.y * J1.y;
	tmp_im = I3.x * J1.y + I3.y * J1.x;
	accum_re[13*blockDim.x] += tmp_re;
	accum_im[13*blockDim.x] += tmp_im;

	//32 component:
	tmp_re = I3.x * J2.x - I3.y * J2.y;
	tmp_im = I3.x * J2.y + I3.y * J2.x;
	accum_re[14*blockDim.x] += tmp_re;
	accum_im[14*blockDim.x] += tmp_im;

	//33 component:
	tmp_re = I3.x * J3.x - I3.y * J3.y;
	tmp_im = I3.x * J3.y + I3.y * J3.x;
	accum_re[15*blockDim.x] += tmp_re;
	accum_im[15*blockDim.x] += tmp_im;
	}


   //Store output back to global buffer:


/*	CONTRACTION FULL VOLUME		*/

	out[outId + 0 *maxThreads*2].x	+= accum_re[ 0*blockDim.x];
	out[outId + 1 *maxThreads*2].x	+= accum_re[ 1*blockDim.x];
	out[outId + 2 *maxThreads*2].x	+= accum_re[ 2*blockDim.x];
	out[outId + 3 *maxThreads*2].x	+= accum_re[ 3*blockDim.x];
	out[outId + 4 *maxThreads*2].x	+= accum_re[ 4*blockDim.x];
	out[outId + 5 *maxThreads*2].x	+= accum_re[ 5*blockDim.x];
	out[outId + 6 *maxThreads*2].x	+= accum_re[ 6*blockDim.x];
	out[outId + 7 *maxThreads*2].x	+= accum_re[ 7*blockDim.x];
	out[outId + 8 *maxThreads*2].x	+= accum_re[ 8*blockDim.x];
	out[outId + 9 *maxThreads*2].x	+= accum_re[ 9*blockDim.x];
	out[outId + 10*maxThreads*2].x	+= accum_re[10*blockDim.x];
	out[outId + 11*maxThreads*2].x	+= accum_re[11*blockDim.x];
	out[outId + 12*maxThreads*2].x	+= accum_re[12*blockDim.x];
	out[outId + 13*maxThreads*2].x	+= accum_re[13*blockDim.x];
	out[outId + 14*maxThreads*2].x	+= accum_re[14*blockDim.x];
	out[outId + 15*maxThreads*2].x	+= accum_re[15*blockDim.x];

	out[outId + 0 *maxThreads*2].y	+= accum_im[ 0*blockDim.x];
	out[outId + 1 *maxThreads*2].y	+= accum_im[ 1*blockDim.x];
	out[outId + 2 *maxThreads*2].y	+= accum_im[ 2*blockDim.x];
	out[outId + 3 *maxThreads*2].y	+= accum_im[ 3*blockDim.x];
	out[outId + 4 *maxThreads*2].y	+= accum_im[ 4*blockDim.x];
	out[outId + 5 *maxThreads*2].y	+= accum_im[ 5*blockDim.x];
	out[outId + 6 *maxThreads*2].y	+= accum_im[ 6*blockDim.x];
	out[outId + 7 *maxThreads*2].y	+= accum_im[ 7*blockDim.x];
	out[outId + 8 *maxThreads*2].y	+= accum_im[ 8*blockDim.x];
	out[outId + 9 *maxThreads*2].y	+= accum_im[ 9*blockDim.x];
	out[outId + 10*maxThreads*2].y	+= accum_im[10*blockDim.x];
	out[outId + 11*maxThreads*2].y	+= accum_im[11*blockDim.x];
	out[outId + 12*maxThreads*2].y	+= accum_im[12*blockDim.x];
	out[outId + 13*maxThreads*2].y	+= accum_im[13*blockDim.x];
	out[outId + 14*maxThreads*2].y	+= accum_im[14*blockDim.x];
	out[outId + 15*maxThreads*2].y	+= accum_im[15*blockDim.x];

	#undef SPINORTEX
	#undef INTERTEX

	return;
}

__global__ void contractPlusKernelD		(double2 *out, double2 *in1, double2 *in2, int maxThreads, int myStride, const int XL, const int YL, const int ZL, const int Parity, const DslashParam param)
{
	int	sid	 = blockIdx.x*blockDim.x + threadIdx.x;
	int	outId	 = sid;
	int	eutId, xCoord1, xCoord2, xCoord3, xCoord4, auxCoord1, auxCoord2;

	if	(sid >= maxThreads)
		return;

	#ifndef USE_TEXTURE_OBJECTS
		#define SPINORTEX	spinorTexDouble
		#define INTERTEX	interTexDouble
	#else
		#define SPINORTEX	param.inTex
		#define INTERTEX	param.xTex
	#endif

	volatile double2		tmp;
	extern __shared__ double	sm[];								//used for data accumulation: blockDim.x * 2 * 16 * sizeof(double)
   
	volatile double			*accum_re	 = sm + threadIdx.x;				//address it like idx*blockDim, where idx = 4*spinor_idx1 + spinor_idx2
	volatile double			*accum_im	 = accum_re + TOTAL_COMPONENTS*blockDim.x;

	eutId		 = 2*sid;
	auxCoord1	 = eutId / XL;
	xCoord1		 = eutId - auxCoord1 * XL;
	auxCoord2	 = auxCoord1 / YL;
	xCoord2		 = auxCoord1 - auxCoord2 * YL;
	xCoord4		 = auxCoord2 / ZL;
	xCoord3		 = auxCoord2 - xCoord4 * ZL;

	auxCoord1	 = (Parity + xCoord4 + xCoord3 + xCoord2) & 1;
	xCoord1		+= auxCoord1;
	outId		 = xCoord1 + XL*(xCoord2 + YL*(xCoord3 + ZL*xCoord4));				//AQUI

	//Load the first color component for each input spinor:
	{

	double2 I0	 = fetch_double2(SPINORTEX, sid + 0 * myStride);   
	double2 I1	 = fetch_double2(SPINORTEX, sid + 3 * myStride);   
	double2 I2	 = fetch_double2(SPINORTEX, sid + 6 * myStride);   
	double2 I3	 = fetch_double2(SPINORTEX, sid + 9 * myStride);   
	double2 J0	 = fetch_double2(INTERTEX,  sid + 0 * myStride);   
	double2 J1	 = fetch_double2(INTERTEX,  sid + 3 * myStride);   
	double2 J2	 = fetch_double2(INTERTEX,  sid + 6 * myStride);   
	double2 J3	 = fetch_double2(INTERTEX,  sid + 9 * myStride);   
	
	//compute in1^dag

	I0.y	 = -I0.y;
	I1.y	 = -I1.y;
	I2.y	 = -I2.y;
	I3.y	 = -I3.y;

	//do products for first color component here:
	//00 component:
	tmp_re = I0.x * J0.x - I0.y * J0.y;
	tmp_im = I0.x * J0.y + I0.y * J0.x;	
	accum_re[0*blockDim.x] = tmp_re;
	accum_im[0*blockDim.x] = tmp_im;	
	
	//01 component:
	tmp_re = I0.x * J1.x - I0.y * J1.y;
	tmp_im = I0.x * J1.y + I0.y * J1.x;	
	accum_re[1*blockDim.x] = tmp_re;
	accum_im[1*blockDim.x] = tmp_im;	

	//02 component:
	tmp_re = I0.x * J2.x - I0.y * J2.y;
	tmp_im = I0.x * J2.y + I0.y * J2.x;	
	accum_re[2*blockDim.x] = tmp_re;
	accum_im[2*blockDim.x] = tmp_im;	
      
	//03 component:
	tmp_re = I0.x * J3.x - I0.y * J3.y;
	tmp_im = I0.x * J3.y + I0.y * J3.x;	
	accum_re[3*blockDim.x] = tmp_re;
	accum_im[3*blockDim.x] = tmp_im;	
      
	//10 component:
	tmp_re = I1.x * J0.x - I1.y * J0.y;
	tmp_im = I1.x * J0.y + I1.y * J0.x;	
	accum_re[4*blockDim.x] = tmp_re;
	accum_im[4*blockDim.x] = tmp_im;	

	//11 component:
	tmp_re = I1.x * J1.x - I1.y * J1.y;
	tmp_im = I1.x * J1.y + I1.y * J1.x;	
	accum_re[5*blockDim.x] = tmp_re;
	accum_im[5*blockDim.x] = tmp_im;	

	//12 component:
	tmp_re = I1.x * J2.x - I1.y * J2.y;
	tmp_im = I1.x * J2.y + I1.y * J2.x;	
	accum_re[6*blockDim.x] = tmp_re;
	accum_im[6*blockDim.x] = tmp_im;	

	//13 component:
	tmp_re = I1.x * J3.x - I1.y * J3.y;
	tmp_im = I1.x * J3.y + I1.y * J3.x;	
	accum_re[7*blockDim.x] = tmp_re;
	accum_im[7*blockDim.x] = tmp_im;	

	//20 component:
	tmp_re = I2.x * J0.x - I2.y * J0.y;
	tmp_im = I2.x * J0.y + I2.y * J0.x;	
	accum_re[8*blockDim.x] = tmp_re;
	accum_im[8*blockDim.x] = tmp_im;	

	//21 component:
	tmp_re = I2.x * J1.x - I2.y * J1.y;
	tmp_im = I2.x * J1.y + I2.y * J1.x;	
	accum_re[9*blockDim.x] = tmp_re;
	accum_im[9*blockDim.x] = tmp_im;	

	//22 component:
	tmp_re = I2.x * J2.x - I2.y * J2.y;
	tmp_im = I2.x * J2.y + I2.y * J2.x;	
	accum_re[10*blockDim.x] = tmp_re;
	accum_im[10*blockDim.x] = tmp_im;	

	//23 component:
	tmp_re = I2.x * J3.x - I2.y * J3.y;
	tmp_im = I2.x * J3.y + I2.y * J3.x;	
	accum_re[11*blockDim.x] = tmp_re;
	accum_im[11*blockDim.x] = tmp_im;	

	//30 component:
	tmp_re = I3.x * J0.x - I3.y * J0.y;
	tmp_im = I3.x * J0.y + I3.y * J0.x;	
	accum_re[12*blockDim.x] = tmp_re;
	accum_im[12*blockDim.x] = tmp_im;	

	//31 component:
	tmp_re = I3.x * J1.x - I3.y * J1.y;
	tmp_im = I3.x * J1.y + I3.y * J1.x;	
	accum_re[13*blockDim.x] = tmp_re;
	accum_im[13*blockDim.x] = tmp_im;	

	//32 component:
	tmp_re = I3.x * J2.x - I3.y * J2.y;
	tmp_im = I3.x * J2.y + I3.y * J2.x;	
	accum_re[14*blockDim.x] = tmp_re;
	accum_im[14*blockDim.x] = tmp_im;	

	//33 component:
	tmp_re = I3.x * J3.x - I3.y * J3.y;
	tmp_im = I3.x * J3.y + I3.y * J3.x;	
	accum_re[15*blockDim.x] = tmp_re;
	accum_im[15*blockDim.x] = tmp_im;	
	}

	//Load the second color component for each input spinor:
	{

	double2 I0	 = fetch_double2(SPINORTEX, sid + 1 * myStride);   
	double2 I1	 = fetch_double2(SPINORTEX, sid + 4 * myStride);   
	double2 I2	 = fetch_double2(SPINORTEX, sid + 7 * myStride);   
	double2 I3	 = fetch_double2(SPINORTEX, sid + 10* myStride);   
	double2 J0	 = fetch_double2(INTERTEX,  sid + 1 * myStride);   
	double2 J1	 = fetch_double2(INTERTEX,  sid + 4 * myStride);   
	double2 J2	 = fetch_double2(INTERTEX,  sid + 7 * myStride);   
	double2 J3	 = fetch_double2(INTERTEX,  sid + 10* myStride);   

	//compute in1^dag * gamma5:

	I0.y	 = -I0.y;
	I1.y	 = -I1.y;
	I2.y	 = -I2.y;
	I3.y	 = -I3.y;

	//do products for first color component here:
	//00 component:
	tmp_re = I0.x * J0.x - I0.y * J0.y;
	tmp_im = I0.x * J0.y + I0.y * J0.x;	
	accum_re[0*blockDim.x] += tmp_re;
	accum_im[0*blockDim.x] += tmp_im;	
	
	//01 component:
	tmp_re = I0.x * J1.x - I0.y * J1.y;
	tmp_im = I0.x * J1.y + I0.y * J1.x;	
	accum_re[1*blockDim.x] += tmp_re;
	accum_im[1*blockDim.x] += tmp_im;	

	//02 component:
	tmp_re = I0.x * J2.x - I0.y * J2.y;
	tmp_im = I0.x * J2.y + I0.y * J2.x;	
	accum_re[2*blockDim.x] += tmp_re;
	accum_im[2*blockDim.x] += tmp_im;	
      
	//03 component:
	tmp_re = I0.x * J3.x - I0.y * J3.y;
	tmp_im = I0.x * J3.y + I0.y * J3.x;	
	accum_re[3*blockDim.x] += tmp_re;
	accum_im[3*blockDim.x] += tmp_im;	
      
	//10 component:
	tmp_re = I1.x * J0.x - I1.y * J0.y;
	tmp_im = I1.x * J0.y + I1.y * J0.x;	
	accum_re[4*blockDim.x] += tmp_re;
	accum_im[4*blockDim.x] += tmp_im;	

	//11 component:
	tmp_re = I1.x * J1.x - I1.y * J1.y;
	tmp_im = I1.x * J1.y + I1.y * J1.x;	
	accum_re[5*blockDim.x] += tmp_re;
	accum_im[5*blockDim.x] += tmp_im;	

	//12 component:
	tmp_re = I1.x * J2.x - I1.y * J2.y;
	tmp_im = I1.x * J2.y + I1.y * J2.x;	
	accum_re[6*blockDim.x] += tmp_re;
	accum_im[6*blockDim.x] += tmp_im;	

	//13 component:
	tmp_re = I1.x * J3.x - I1.y * J3.y;
	tmp_im = I1.x * J3.y + I1.y * J3.x;	
	accum_re[7*blockDim.x] += tmp_re;
	accum_im[7*blockDim.x] += tmp_im;	

	//20 component:
	tmp_re = I2.x * J0.x - I2.y * J0.y;
	tmp_im = I2.x * J0.y + I2.y * J0.x;	
	accum_re[8*blockDim.x] += tmp_re;
	accum_im[8*blockDim.x] += tmp_im;	

	//21 component:
	tmp_re = I2.x * J1.x - I2.y * J1.y;
	tmp_im = I2.x * J1.y + I2.y * J1.x;	
	accum_re[9*blockDim.x] += tmp_re;
	accum_im[9*blockDim.x] += tmp_im;	

	//22 component:
	tmp_re = I2.x * J2.x - I2.y * J2.y;
	tmp_im = I2.x * J2.y + I2.y * J2.x;	
	accum_re[10*blockDim.x] += tmp_re;
	accum_im[10*blockDim.x] += tmp_im;	

	//23 component:
	tmp_re = I2.x * J3.x - I2.y * J3.y;
	tmp_im = I2.x * J3.y + I2.y * J3.x;	
	accum_re[11*blockDim.x] += tmp_re;
	accum_im[11*blockDim.x] += tmp_im;	

	//30 component:
	tmp_re = I3.x * J0.x - I3.y * J0.y;
	tmp_im = I3.x * J0.y + I3.y * J0.x;	
	accum_re[12*blockDim.x] += tmp_re;
	accum_im[12*blockDim.x] += tmp_im;	

	//31 component:
	tmp_re = I3.x * J1.x - I3.y * J1.y;
	tmp_im = I3.x * J1.y + I3.y * J1.x;	
	accum_re[13*blockDim.x] += tmp_re;
	accum_im[13*blockDim.x] += tmp_im;	

	//32 component:
	tmp_re = I3.x * J2.x - I3.y * J2.y;
	tmp_im = I3.x * J2.y + I3.y * J2.x;	
	accum_re[14*blockDim.x] += tmp_re;
	accum_im[14*blockDim.x] += tmp_im;	

	//33 component:
	tmp_re = I3.x * J3.x - I3.y * J3.y;
	tmp_im = I3.x * J3.y + I3.y * J3.x;	
	accum_re[15*blockDim.x] += tmp_re;
	accum_im[15*blockDim.x] += tmp_im;	
	}

	//Load the third color component for each input spinor:
	{

        double2 I0  = fetch_double2(SPINORTEX, sid + 2 * myStride);   
	double2 I1  = fetch_double2(SPINORTEX, sid + 5 * myStride);   
	double2 I2  = fetch_double2(SPINORTEX, sid + 8 * myStride);   
	double2 I3  = fetch_double2(SPINORTEX, sid + 11* myStride);   
	double2 J0  = fetch_double2(INTERTEX,  sid + 2 * myStride);   
	double2 J1  = fetch_double2(INTERTEX,  sid + 5 * myStride);   
	double2 J2  = fetch_double2(INTERTEX,  sid + 8 * myStride);   
	double2 J3  = fetch_double2(INTERTEX,  sid + 11* myStride);   

	//compute in1^dag

	I0.y	 = -I0.y;
	I1.y	 = -I1.y;
	I2.y	 = -I2.y;
	I3.y	 = -I3.y;

	//do products for first color component here:
	//00 component:
	tmp_re = I0.x * J0.x - I0.y * J0.y;
	tmp_im = I0.x * J0.y + I0.y * J0.x;	
	accum_re[0*blockDim.x] += tmp_re;
	accum_im[0*blockDim.x] += tmp_im;	
	
	//01 component:
	tmp_re = I0.x * J1.x - I0.y * J1.y;
	tmp_im = I0.x * J1.y + I0.y * J1.x;	
	accum_re[1*blockDim.x] += tmp_re;
	accum_im[1*blockDim.x] += tmp_im;	

	//02 component:
	tmp_re = I0.x * J2.x - I0.y * J2.y;
	tmp_im = I0.x * J2.y + I0.y * J2.x;	
	accum_re[2*blockDim.x] += tmp_re;
	accum_im[2*blockDim.x] += tmp_im;	
      
	//03 component:
	tmp_re = I0.x * J3.x - I0.y * J3.y;
	tmp_im = I0.x * J3.y + I0.y * J3.x;	
	accum_re[3*blockDim.x] += tmp_re;
	accum_im[3*blockDim.x] += tmp_im;	
      
	//10 component:
	tmp_re = I1.x * J0.x - I1.y * J0.y;
	tmp_im = I1.x * J0.y + I1.y * J0.x;	
	accum_re[4*blockDim.x] += tmp_re;
	accum_im[4*blockDim.x] += tmp_im;	

	//11 component:
	tmp_re = I1.x * J1.x - I1.y * J1.y;
	tmp_im = I1.x * J1.y + I1.y * J1.x;	
	accum_re[5*blockDim.x] += tmp_re;
	accum_im[5*blockDim.x] += tmp_im;	

	//12 component:
	tmp_re = I1.x * J2.x - I1.y * J2.y;
	tmp_im = I1.x * J2.y + I1.y * J2.x;	
	accum_re[6*blockDim.x] += tmp_re;
	accum_im[6*blockDim.x] += tmp_im;	

	//13 component:
	tmp_re = I1.x * J3.x - I1.y * J3.y;
	tmp_im = I1.x * J3.y + I1.y * J3.x;	
	accum_re[7*blockDim.x] += tmp_re;
	accum_im[7*blockDim.x] += tmp_im;	

	//20 component:
	tmp_re = I2.x * J0.x - I2.y * J0.y;
	tmp_im = I2.x * J0.y + I2.y * J0.x;	
	accum_re[8*blockDim.x] += tmp_re;
	accum_im[8*blockDim.x] += tmp_im;	

	//21 component:
	tmp_re = I2.x * J1.x - I2.y * J1.y;
	tmp_im = I2.x * J1.y + I2.y * J1.x;	
	accum_re[9*blockDim.x] += tmp_re;
	accum_im[9*blockDim.x] += tmp_im;	

	//22 component:
	tmp_re = I2.x * J2.x - I2.y * J2.y;
	tmp_im = I2.x * J2.y + I2.y * J2.x;	
	accum_re[10*blockDim.x] += tmp_re;
	accum_im[10*blockDim.x] += tmp_im;	

	//23 component:
	tmp_re = I2.x * J3.x - I2.y * J3.y;
	tmp_im = I2.x * J3.y + I2.y * J3.x;	
	accum_re[11*blockDim.x] += tmp_re;
	accum_im[11*blockDim.x] += tmp_im;	

	//30 component:
	tmp_re = I3.x * J0.x - I3.y * J0.y;
	tmp_im = I3.x * J0.y + I3.y * J0.x;	
	accum_re[12*blockDim.x] += tmp_re;
	accum_im[12*blockDim.x] += tmp_im;	

	//31 component:
	tmp_re = I3.x * J1.x - I3.y * J1.y;
	tmp_im = I3.x * J1.y + I3.y * J1.x;	
	accum_re[13*blockDim.x] += tmp_re;
	accum_im[13*blockDim.x] += tmp_im;	

	//32 component:
	tmp_re = I3.x * J2.x - I3.y * J2.y;
	tmp_im = I3.x * J2.y + I3.y * J2.x;	
	accum_re[14*blockDim.x] += tmp_re;
	accum_im[14*blockDim.x] += tmp_im;	

	//33 component:
	tmp_re = I3.x * J3.x - I3.y * J3.y;
	tmp_im = I3.x * J3.y + I3.y * J3.x;	
	accum_re[15*blockDim.x] += tmp_re;
	accum_im[15*blockDim.x] += tmp_im;	
	}

	//Store output back to global buffer:

/*	CONTRACTION FULL VOLUME		*/

	out[outId + 0 *maxThreads*2].x	+= accum_re[ 0*blockDim.x];
	out[outId + 1 *maxThreads*2].x	+= accum_re[ 1*blockDim.x];
	out[outId + 2 *maxThreads*2].x	+= accum_re[ 2*blockDim.x];
	out[outId + 3 *maxThreads*2].x	+= accum_re[ 3*blockDim.x];
	out[outId + 4 *maxThreads*2].x	+= accum_re[ 4*blockDim.x];
	out[outId + 5 *maxThreads*2].x	+= accum_re[ 5*blockDim.x];
	out[outId + 6 *maxThreads*2].x	+= accum_re[ 6*blockDim.x];
	out[outId + 7 *maxThreads*2].x	+= accum_re[ 7*blockDim.x];
	out[outId + 8 *maxThreads*2].x	+= accum_re[ 8*blockDim.x];
	out[outId + 9 *maxThreads*2].x	+= accum_re[ 9*blockDim.x];
	out[outId + 10*maxThreads*2].x	+= accum_re[10*blockDim.x];
	out[outId + 11*maxThreads*2].x	+= accum_re[11*blockDim.x];
	out[outId + 12*maxThreads*2].x	+= accum_re[12*blockDim.x];
	out[outId + 13*maxThreads*2].x	+= accum_re[13*blockDim.x];
	out[outId + 14*maxThreads*2].x	+= accum_re[14*blockDim.x];
	out[outId + 15*maxThreads*2].x	+= accum_re[15*blockDim.x];

	out[outId + 0 *maxThreads*2].y	+= accum_im[ 0*blockDim.x];
	out[outId + 1 *maxThreads*2].y	+= accum_im[ 1*blockDim.x];
	out[outId + 2 *maxThreads*2].y	+= accum_im[ 2*blockDim.x];
	out[outId + 3 *maxThreads*2].y	+= accum_im[ 3*blockDim.x];
	out[outId + 4 *maxThreads*2].y	+= accum_im[ 4*blockDim.x];
	out[outId + 5 *maxThreads*2].y	+= accum_im[ 5*blockDim.x];
	out[outId + 6 *maxThreads*2].y	+= accum_im[ 6*blockDim.x];
	out[outId + 7 *maxThreads*2].y	+= accum_im[ 7*blockDim.x];
	out[outId + 8 *maxThreads*2].y	+= accum_im[ 8*blockDim.x];
	out[outId + 9 *maxThreads*2].y	+= accum_im[ 9*blockDim.x];
	out[outId + 10*maxThreads*2].y	+= accum_im[10*blockDim.x];
	out[outId + 11*maxThreads*2].y	+= accum_im[11*blockDim.x];
	out[outId + 12*maxThreads*2].y	+= accum_im[12*blockDim.x];
	out[outId + 13*maxThreads*2].y	+= accum_im[13*blockDim.x];
	out[outId + 14*maxThreads*2].y	+= accum_im[14*blockDim.x];
	out[outId + 15*maxThreads*2].y	+= accum_im[15*blockDim.x];

	#undef SPINORTEX
	#undef INTERTEX

	return;
}

#endif // (__CUDA_ARCH__ >= 130)


#define READ_SPINOR_SINGLE(spinor, stride, sp_idx, norm_idx)	   \
  float4 I0 = spinor[sp_idx + 0*(stride)];   \
  float4 I1 = spinor[sp_idx + 1*(stride)];   \
  float4 I2 = spinor[sp_idx + 2*(stride)];   \
  float4 I3 = spinor[sp_idx + 3*(stride)];   \
  float4 I4 = spinor[sp_idx + 4*(stride)];   \
  float4 I5 = spinor[sp_idx + 5*(stride)];

#define READ_SPINOR_SINGLE_TEX(spinor, stride, sp_idx, norm_idx)	\
  float4 I0 = TEX1DFETCH(float4, (spinor), sp_idx + 0*(stride));	\
  float4 I1 = TEX1DFETCH(float4, (spinor), sp_idx + 1*(stride));	\
  float4 I2 = TEX1DFETCH(float4, (spinor), sp_idx + 2*(stride));	\
  float4 I3 = TEX1DFETCH(float4, (spinor), sp_idx + 3*(stride));	\
  float4 I4 = TEX1DFETCH(float4, (spinor), sp_idx + 4*(stride));	\
  float4 I5 = TEX1DFETCH(float4, (spinor), sp_idx + 5*(stride));

#define READ_INTERMEDIATE_SPINOR_SINGLE(spinor, stride, sp_idx, norm_idx)	   \
  float4 J0 = spinor[sp_idx + 0*(stride)];   \
  float4 J1 = spinor[sp_idx + 1*(stride)];   \
  float4 J2 = spinor[sp_idx + 2*(stride)];   \
  float4 J3 = spinor[sp_idx + 3*(stride)];   \
  float4 J4 = spinor[sp_idx + 4*(stride)];   \
  float4 J5 = spinor[sp_idx + 5*(stride)];

#define READ_INTERMEDIATE_SPINOR_SINGLE_TEX(spinor, stride, sp_idx, norm_idx)	\
  float4 J0 = TEX1DFETCH(float4, (spinor), sp_idx + 0*(stride));	\
  float4 J1 = TEX1DFETCH(float4, (spinor), sp_idx + 1*(stride));	\
  float4 J2 = TEX1DFETCH(float4, (spinor), sp_idx + 2*(stride));	\
  float4 J3 = TEX1DFETCH(float4, (spinor), sp_idx + 3*(stride));	\
  float4 J4 = TEX1DFETCH(float4, (spinor), sp_idx + 4*(stride));	\
  float4 J5 = TEX1DFETCH(float4, (spinor), sp_idx + 5*(stride));

#ifdef DIRECT_ACCESS_WILSON_SPINOR
	#define READ_SPINOR READ_SPINOR_SINGLE
	#define SPINORTEX in
#else
	#define READ_SPINOR READ_SPINOR_SINGLE_TEX

	#ifdef USE_TEXTURE_OBJECTS
		#define SPINORTEX param.inTex
	#else
		#define SPINORTEX spinorTexSingle
	#endif	// USE_TEXTURE_OBJECTS
#endif

#ifdef DIRECT_ACCESS_WILSON_INTER
	#define READ_INTERMEDIATE_SPINOR READ_INTERMEDIATE_SPINOR_SINGLE
	#define INTERTEX in2
#else
	#define READ_INTERMEDIATE_SPINOR READ_INTERMEDIATE_SPINOR_SINGLE_TEX

	#ifdef USE_TEXTURE_OBJECTS
		#define INTERTEX param.xTex
	#else
		#define INTERTEX interTexSingle
	#endif	// USE_TEXTURE_OBJECTS
#endif

#define SPINOR_HOP 6

__global__ void contractGamma5PlusKernel(float2 *out, float4 *in1, float4 *in2, int maxThreads, int myStride, const int XL, const int YL, const int ZL, const int Parity, const DslashParam param)
{
	int	sid	 = blockIdx.x*blockDim.x + threadIdx.x;
	int	outId	 = sid;
	int	eutId, xCoord1, xCoord2, xCoord3, xCoord4, auxCoord1, auxCoord2;

	if	(sid >= maxThreads)
		return;

	volatile float2		tmp;
	extern __shared__ float	sms[];							//used for data accumulation: blockDim.x * 2 * 16 * sizeof(double)
   
	volatile float		*accum_re = sms + threadIdx.x;				//address it like idx*blockDim, where idx = 4*spinor_idx1 + spinor_idx2
	volatile float		*accum_im = accum_re + TOTAL_COMPONENTS*blockDim.x;

	eutId		 = 2*sid;
	auxCoord1	 = eutId / XL;
	xCoord1		 = eutId - auxCoord1 * XL;
	auxCoord2	 = auxCoord1 / YL;
	xCoord2		 = auxCoord1 - auxCoord2 * YL;
	xCoord4		 = auxCoord2 / ZL;
	xCoord3		 = auxCoord2 - xCoord4 * ZL;

	auxCoord1	 = (Parity + xCoord4 + xCoord3 + xCoord2) & 1;
	xCoord1		+= auxCoord1;
	outId		 = xCoord1 + XL*(xCoord2 + YL*(xCoord3 + ZL*xCoord4));			//AQUI

	//Load the full input spinors:

	READ_SPINOR			(SPINORTEX, myStride, sid, sid);
	READ_INTERMEDIATE_SPINOR	(INTERTEX,  myStride, sid, sid);

	//compute in1^dag * gamma5:

	//First color component

	tmp_re	 = +I0.x;
	tmp_im	 = -I0.y;
	I0.x	 = +I3.x;
	I0.y	 = -I3.y;
	I3.x	 = tmp_re;
	I3.y	 = tmp_im;	

	tmp_re	 = +I1.z;
	tmp_im	 = -I1.w;
	I1.z	 = +I4.z;
	I1.w	 = -I4.w;
	I4.z	 = tmp_re;
	I4.w	 = tmp_im;	

	//Second color component

	tmp_re	 = +I0.z;
	tmp_im	 = -I0.w;
	I0.z	 = +I3.z;
	I0.w	 = -I3.w;
	I3.z	 = tmp_re;
	I3.w	 = tmp_im;	

	tmp_re	 = +I2.x;
	tmp_im	 = -I2.y;
	I2.x	 = +I5.x;
	I2.y	 = -I5.y;
	I5.x	 = tmp_re;
	I5.y	 = tmp_im;	

	//Third color component

	tmp_re	 = +I1.x;
	tmp_im	 = -I1.y;
	I1.x	 = +I4.x;
	I1.y	 = -I4.y;
	I4.x	 = tmp_re;
	I4.y	 = tmp_im;	

	tmp_re	 = +I2.z;
	tmp_im	 = -I2.w;
	I2.z	 = +I5.z;
	I2.w	 = -I5.w;
	I5.z	 = tmp_re;
	I5.w	 = tmp_im;	

	//do products for first color component here:
	//00 component:
	tmp_re = I0.x * J0.x - I0.y * J0.y;
	tmp_im = I0.x * J0.y + I0.y * J0.x;	
	accum_re[0*blockDim.x] = tmp_re;
	accum_im[0*blockDim.x] = tmp_im;	
	
	//01 component:
	tmp_re = I0.x * J1.z - I0.y * J1.w;
	tmp_im = I0.x * J1.w + I0.y * J1.z;	
	accum_re[1*blockDim.x] = tmp_re;
	accum_im[1*blockDim.x] = tmp_im;	

	//02 component:
	tmp_re = I0.x * J3.x - I0.y * J3.y;
	tmp_im = I0.x * J3.y + I0.y * J3.x;	
	accum_re[2*blockDim.x] = tmp_re;
	accum_im[2*blockDim.x] = tmp_im;	
      
	//03 component:
	tmp_re = I0.x * J4.z - I0.y * J4.w;
	tmp_im = I0.x * J4.w + I0.y * J4.z;	
	accum_re[3*blockDim.x] = tmp_re;
	accum_im[3*blockDim.x] = tmp_im;	
      
	//10 component:
	tmp_re = I1.z * J0.x - I1.w * J0.y;
	tmp_im = I1.z * J0.y + I1.w * J0.x;	
	accum_re[4*blockDim.x] = tmp_re;
	accum_im[4*blockDim.x] = tmp_im;	

	//11 component:
	tmp_re = I1.z * J1.z - I1.w * J1.w;
	tmp_im = I1.z * J1.w + I1.w * J1.z;	
	accum_re[5*blockDim.x] = tmp_re;
	accum_im[5*blockDim.x] = tmp_im;	

	//12 component:
	tmp_re = I1.z * J3.x - I1.w * J3.y;
	tmp_im = I1.z * J3.y + I1.w * J3.x;	
	accum_re[6*blockDim.x] = tmp_re;
	accum_im[6*blockDim.x] = tmp_im;	

	//13 component:
	tmp_re = I1.z * J4.z - I1.w * J4.w;
	tmp_im = I1.z * J4.w + I1.w * J4.z;	
	accum_re[7*blockDim.x] = tmp_re;
	accum_im[7*blockDim.x] = tmp_im;	

	//20 component:
	tmp_re = I3.x * J0.x - I3.y * J0.y;
	tmp_im = I3.x * J0.y + I3.y * J0.x;	
	accum_re[8*blockDim.x] = tmp_re;
	accum_im[8*blockDim.x] = tmp_im;	

	//21 component:
	tmp_re = I3.x * J1.z - I3.y * J1.w;
	tmp_im = I3.x * J1.w + I3.y * J1.z;	
	accum_re[9*blockDim.x] = tmp_re;
	accum_im[9*blockDim.x] = tmp_im;	

	//22 component:
	tmp_re = I3.x * J3.x - I3.y * J3.y;
	tmp_im = I3.x * J3.y + I3.y * J3.x;	
	accum_re[10*blockDim.x] = tmp_re;
	accum_im[10*blockDim.x] = tmp_im;	

	//23 component:
	tmp_re = I3.x * J4.z - I3.y * J4.w;
	tmp_im = I3.x * J4.w + I3.y * J4.z;	
	accum_re[11*blockDim.x] = tmp_re;
	accum_im[11*blockDim.x] = tmp_im;	

	//30 component:
	tmp_re = I4.z * J0.x - I4.w * J0.y;
	tmp_im = I4.z * J0.y + I4.w * J0.x;	
	accum_re[12*blockDim.x] = tmp_re;
	accum_im[12*blockDim.x] = tmp_im;	

	//31 component:
	tmp_re = I4.z * J1.z - I4.w * J1.w;
	tmp_im = I4.z * J1.w + I4.w * J1.z;	
	accum_re[13*blockDim.x] = tmp_re;
	accum_im[13*blockDim.x] = tmp_im;	

	//32 component:
	tmp_re = I4.z * J3.x - I4.w * J3.y;
	tmp_im = I4.z * J3.y + I4.w * J3.x;	
	accum_re[14*blockDim.x] = tmp_re;
	accum_im[14*blockDim.x] = tmp_im;	

	//33 component:
	tmp_re = I4.z * J4.z - I4.w * J4.w;
	tmp_im = I4.z * J4.w + I4.w * J4.z;	
	accum_re[15*blockDim.x] = tmp_re;
	accum_im[15*blockDim.x] = tmp_im;	


	//do products for second color component here:
	//00 component:
	tmp_re = I0.z * J0.z - I0.w * J0.w;
	tmp_im = I0.z * J0.w + I0.w * J0.z;	
	accum_re[0*blockDim.x] += tmp_re;
	accum_im[0*blockDim.x] += tmp_im;	
	
	//01 component:
	tmp_re = I0.z * J2.x - I0.w * J2.y;
	tmp_im = I0.z * J2.y + I0.w * J2.x;	
	accum_re[1*blockDim.x] += tmp_re;
	accum_im[1*blockDim.x] += tmp_im;	

	//02 component:
	tmp_re = I0.z * J3.z - I0.w * J3.w;
	tmp_im = I0.z * J3.w + I0.w * J3.z;	
	accum_re[2*blockDim.x] += tmp_re;
	accum_im[2*blockDim.x] += tmp_im;	
      
	//03 component:
	tmp_re = I0.z * J5.x - I0.w * J5.x;
	tmp_im = I0.z * J5.y + I0.w * J5.y;	
	accum_re[3*blockDim.x] += tmp_re;
	accum_im[3*blockDim.x] += tmp_im;	
      
	//10 component:
	tmp_re = I2.x * J0.z - I2.y * J0.w;
	tmp_im = I2.x * J0.w + I2.y * J0.z;	
	accum_re[4*blockDim.x] += tmp_re;
	accum_im[4*blockDim.x] += tmp_im;	

	//11 component:
	tmp_re = I2.x * J2.x - I2.y * J2.y;
	tmp_im = I2.x * J2.y + I2.y * J2.x;	
	accum_re[5*blockDim.x] += tmp_re;
	accum_im[5*blockDim.x] += tmp_im;	

	//12 component:
	tmp_re = I2.x * J3.z - I2.y * J3.w;
	tmp_im = I2.x * J3.w + I2.y * J3.z;	
	accum_re[6*blockDim.x] += tmp_re;
	accum_im[6*blockDim.x] += tmp_im;	

	//13 component:
	tmp_re = I2.x * J5.x - I2.y * J5.y;
	tmp_im = I2.x * J5.y + I2.y * J5.x;	
	accum_re[7*blockDim.x] += tmp_re;
	accum_im[7*blockDim.x] += tmp_im;	

	//20 component:
	tmp_re = I3.z * J0.z - I3.w * J0.w;
	tmp_im = I3.z * J0.w + I3.w * J0.z;	
	accum_re[8*blockDim.x] += tmp_re;
	accum_im[8*blockDim.x] += tmp_im;	

	//21 component:
	tmp_re = I3.z * J2.x - I3.w * J2.y;
	tmp_im = I3.z * J2.y + I3.w * J2.x;	
	accum_re[9*blockDim.x] += tmp_re;
	accum_im[9*blockDim.x] += tmp_im;	

	//22 component:
	tmp_re = I3.z * J3.z - I3.w * J3.w;
	tmp_im = I3.z * J3.w + I3.w * J3.z;	
	accum_re[10*blockDim.x] += tmp_re;
	accum_im[10*blockDim.x] += tmp_im;	

	//23 component:
	tmp_re = I3.z * J5.x - I3.w * J5.y;
	tmp_im = I3.z * J5.y + I3.w * J5.x;	
	accum_re[11*blockDim.x] += tmp_re;
	accum_im[11*blockDim.x] += tmp_im;	

	//30 component:
	tmp_re = I5.x * J0.z - I5.y * J0.w;
	tmp_im = I5.x * J0.w + I5.y * J0.z;	
	accum_re[12*blockDim.x] += tmp_re;
	accum_im[12*blockDim.x] += tmp_im;	

	//31 component:
	tmp_re = I5.x * J2.x - I5.y * J2.y;
	tmp_im = I5.x * J2.y + I5.y * J2.x;	
	accum_re[13*blockDim.x] += tmp_re;
	accum_im[13*blockDim.x] += tmp_im;	

	//32 component:
	tmp_re = I5.x * J3.z - I5.y * J3.w;
	tmp_im = I5.x * J3.w + I5.y * J3.z;	
	accum_re[14*blockDim.x] += tmp_re;
	accum_im[14*blockDim.x] += tmp_im;	

	//33 component:
	tmp_re = I5.x * J5.x - I5.y * J5.y;
	tmp_im = I5.x * J5.y + I5.y * J5.x;	
	accum_re[15*blockDim.x] += tmp_re;
	accum_im[15*blockDim.x] += tmp_im;	


	//do products for third color component here:
	//00 component:
	tmp_re = I1.x * J1.x - I1.y * J1.y;
	tmp_im = I1.x * J1.y + I1.y * J1.x;	
	accum_re[0*blockDim.x] += tmp_re;
	accum_im[0*blockDim.x] += tmp_im;	
	
	//01 component:
	tmp_re = I1.x * J2.z - I1.y * J2.w;
	tmp_im = I1.x * J2.w + I1.y * J2.z;	
	accum_re[1*blockDim.x] += tmp_re;
	accum_im[1*blockDim.x] += tmp_im;	

	//02 component:
	tmp_re = I1.x * J4.x - I1.y * J4.y;
	tmp_im = I1.x * J4.y + I1.y * J4.x;	
	accum_re[2*blockDim.x] += tmp_re;
	accum_im[2*blockDim.x] += tmp_im;	
      
	//03 component:
	tmp_re = I1.x * J5.z - I1.y * J5.w;
	tmp_im = I1.x * J5.w + I1.y * J5.z;	
	accum_re[3*blockDim.x] += tmp_re;
	accum_im[3*blockDim.x] += tmp_im;	
      
	//10 component:
	tmp_re = I2.z * J1.x - I2.w * J1.y;
	tmp_im = I2.z * J1.y + I2.w * J1.x;	
	accum_re[4*blockDim.x] += tmp_re;
	accum_im[4*blockDim.x] += tmp_im;	

	//11 component:
	tmp_re = I2.z * J2.z - I2.w * J2.w;
	tmp_im = I2.z * J2.w + I2.w * J2.z;	
	accum_re[5*blockDim.x] += tmp_re;
	accum_im[5*blockDim.x] += tmp_im;	

	//12 component:
	tmp_re = I2.z * J4.x - I2.w * J4.y;
	tmp_im = I2.z * J4.y + I2.w * J4.x;	
	accum_re[6*blockDim.x] += tmp_re;
	accum_im[6*blockDim.x] += tmp_im;	

	//13 component:
	tmp_re = I2.z * J5.z - I2.w * J5.w;
	tmp_im = I2.z * J5.w + I2.w * J5.z;	
	accum_re[7*blockDim.x] += tmp_re;
	accum_im[7*blockDim.x] += tmp_im;	

	//20 component:
	tmp_re = I4.x * J1.x - I4.y * J1.y;
	tmp_im = I4.x * J1.y + I4.y * J1.x;	
	accum_re[8*blockDim.x] += tmp_re;
	accum_im[8*blockDim.x] += tmp_im;	

	//21 component:
	tmp_re = I4.x * J2.z - I4.y * J2.w;
	tmp_im = I4.x * J2.w + I4.y * J2.z;	
	accum_re[9*blockDim.x] += tmp_re;
	accum_im[9*blockDim.x] += tmp_im;	

	//22 component:
	tmp_re = I4.x * J4.x - I4.y * J4.y;
	tmp_im = I4.x * J4.y + I4.y * J4.x;	
	accum_re[10*blockDim.x] += tmp_re;
	accum_im[10*blockDim.x] += tmp_im;	

	//23 component:
	tmp_re = I4.x * J5.z - I4.y * J5.w;
	tmp_im = I4.x * J5.w + I4.y * J5.z;	
	accum_re[11*blockDim.x] += tmp_re;
	accum_im[11*blockDim.x] += tmp_im;	

	//30 component:
	tmp_re = I5.z * J1.x - I5.w * J1.y;
	tmp_im = I5.z * J1.y + I5.w * J1.x;	
	accum_re[12*blockDim.x] += tmp_re;
	accum_im[12*blockDim.x] += tmp_im;	

	//31 component:
	tmp_re = I5.z * J2.z - I5.w * J2.w;
	tmp_im = I5.z * J2.w + I5.w * J2.z;	
	accum_re[13*blockDim.x] += tmp_re;
	accum_im[13*blockDim.x] += tmp_im;	

	//32 component:
	tmp_re = I5.z * J4.x - I5.w * J4.y;
	tmp_im = I5.z * J4.y + I5.w * J4.x;	
	accum_re[14*blockDim.x] += tmp_re;
	accum_im[14*blockDim.x] += tmp_im;	

	//33 component:
	tmp_re = I5.z * J5.z - I5.w * J5.w;
	tmp_im = I5.z * J5.w + I5.w * J5.z;	
	accum_re[15*blockDim.x] += tmp_re;
	accum_im[15*blockDim.x] += tmp_im;	

   //Store output back to global buffer:


/*	CONTRACTION FULL VOLUME		*/

	out[outId + 0 *maxThreads*2].x	+= accum_re[ 0*blockDim.x];
	out[outId + 1 *maxThreads*2].x	+= accum_re[ 1*blockDim.x];
	out[outId + 2 *maxThreads*2].x	+= accum_re[ 2*blockDim.x];
	out[outId + 3 *maxThreads*2].x	+= accum_re[ 3*blockDim.x];
	out[outId + 4 *maxThreads*2].x	+= accum_re[ 4*blockDim.x];
	out[outId + 5 *maxThreads*2].x	+= accum_re[ 5*blockDim.x];
	out[outId + 6 *maxThreads*2].x	+= accum_re[ 6*blockDim.x];
	out[outId + 7 *maxThreads*2].x	+= accum_re[ 7*blockDim.x];
	out[outId + 8 *maxThreads*2].x	+= accum_re[ 8*blockDim.x];
	out[outId + 9 *maxThreads*2].x	+= accum_re[ 9*blockDim.x];
	out[outId + 10*maxThreads*2].x	+= accum_re[10*blockDim.x];
	out[outId + 11*maxThreads*2].x	+= accum_re[11*blockDim.x];
	out[outId + 12*maxThreads*2].x	+= accum_re[12*blockDim.x];
	out[outId + 13*maxThreads*2].x	+= accum_re[13*blockDim.x];
	out[outId + 14*maxThreads*2].x	+= accum_re[14*blockDim.x];
	out[outId + 15*maxThreads*2].x	+= accum_re[15*blockDim.x];

	out[outId + 0 *maxThreads*2].y	+= accum_im[ 0*blockDim.x];
	out[outId + 1 *maxThreads*2].y	+= accum_im[ 1*blockDim.x];
	out[outId + 2 *maxThreads*2].y	+= accum_im[ 2*blockDim.x];
	out[outId + 3 *maxThreads*2].y	+= accum_im[ 3*blockDim.x];
	out[outId + 4 *maxThreads*2].y	+= accum_im[ 4*blockDim.x];
	out[outId + 5 *maxThreads*2].y	+= accum_im[ 5*blockDim.x];
	out[outId + 6 *maxThreads*2].y	+= accum_im[ 6*blockDim.x];
	out[outId + 7 *maxThreads*2].y	+= accum_im[ 7*blockDim.x];
	out[outId + 8 *maxThreads*2].y	+= accum_im[ 8*blockDim.x];
	out[outId + 9 *maxThreads*2].y	+= accum_im[ 9*blockDim.x];
	out[outId + 10*maxThreads*2].y	+= accum_im[10*blockDim.x];
	out[outId + 11*maxThreads*2].y	+= accum_im[11*blockDim.x];
	out[outId + 12*maxThreads*2].y	+= accum_im[12*blockDim.x];
	out[outId + 13*maxThreads*2].y	+= accum_im[13*blockDim.x];
	out[outId + 14*maxThreads*2].y	+= accum_im[14*blockDim.x];
	out[outId + 15*maxThreads*2].y	+= accum_im[15*blockDim.x];

	#undef SPINORTEX
	#undef INTERTEX

	return;
}

//Perform trace in color space only and for a given tslice 
//since the file is included in dslash_quda.h, no need to add dslash_constants.h file here (for, e.g., Vsh)
__global__ void contractTslicePlusKernelS(float2 *out, float4 *in1, float4 *in2, int maxThreads, int myStride, const int XL, const int YL, const int ZL, const int Tslice, const int Parity, const DslashParam param)
{
	int	sid	 = blockIdx.x*blockDim.x + threadIdx.x;					//number of threads is equal to Tslice volume
												//Adjust sid to correct tslice (exe domain must be Tslice volume!)
	int	inId	 = sid + Vsh*Tslice;							//Vsh - 3d space volume for the parity spinor (equale to exe domain!)
	int	outId; 
	int	eutId, xCoord1, xCoord2, xCoord3, xCoord4, auxCoord1, auxCoord2;

	if	(sid >= maxThreads)								//maxThreads == tslice volume
		return;

	#ifndef USE_TEXTURE_OBJECTS
		#define SPINORTEX	spinorTexSingle
		#define INTERTEX	interTexSingle
	#else
		#define SPINORTEX	param.inTex
		#define INTERTEX	param.xTex
	#endif

	volatile float2		tmp;
	extern __shared__ float	sms[];							//used for data accumulation: blockDim.x * 2 * 16 * sizeof(double)
   
	volatile float			*accum_re = sms + threadIdx.x;				//address it like idx*blockDim, where idx = 4*spinor_idx1 + spinor_idx2
	volatile float			*accum_im = accum_re + TOTAL_COMPONENTS*blockDim.x;

//The output only for a given tslice (for the full tslice content, i.e., both parities!):

	eutId		 = 2*inId;
	auxCoord1	 = eutId / XL;
	xCoord1		 = eutId - auxCoord1 * XL;
	auxCoord2	 = auxCoord1 / YL;
	xCoord2		 = auxCoord1 - auxCoord2 * YL;
	xCoord4		 = auxCoord2 / ZL;

//	if	(Tslice != xCoord4)
//		return;

	xCoord3		 = auxCoord2 - xCoord4 * ZL;

	auxCoord1	 = (Parity + xCoord4 + xCoord3 + xCoord2) & 1;
	xCoord1		+= auxCoord1;
	outId		 = xCoord1 + XL*(xCoord2 + YL*xCoord3);					//AQUI

	//Load the full input spinors:

	READ_SPINOR			(SPINORTEX, myStride, sid, sid);
	READ_INTERMEDIATE_SPINOR	(INTERTEX,  myStride, sid, sid);

	//compute in1^dag:

	I0.y	 = -I0.y;
	I0.w	 = -I0.w;
	I1.y	 = -I1.y;
	I1.w	 = -I1.w;
	I2.y	 = -I2.y;
	I2.w	 = -I2.w;	
	I3.y	 = -I3.y;
	I3.w	 = -I3.w;
	I4.y	 = -I4.y;
	I4.w	 = -I4.w;
	I5.y	 = -I5.y;
	I5.w	 = -I5.w;	

	//do products for first color component here:
	//00 component:
	tmp_re = I0.x * J0.x - I0.y * J0.y;
	tmp_im = I0.x * J0.y + I0.y * J0.x;	
	accum_re[0*blockDim.x] = tmp_re;
	accum_im[0*blockDim.x] = tmp_im;	
	
	//01 component:
	tmp_re = I0.x * J1.z - I0.y * J1.w;
	tmp_im = I0.x * J1.w + I0.y * J1.z;	
	accum_re[1*blockDim.x] = tmp_re;
	accum_im[1*blockDim.x] = tmp_im;	

	//02 component:
	tmp_re = I0.x * J3.x - I0.y * J3.y;
	tmp_im = I0.x * J3.y + I0.y * J3.x;	
	accum_re[2*blockDim.x] = tmp_re;
	accum_im[2*blockDim.x] = tmp_im;	
      
	//03 component:
	tmp_re = I0.x * J4.z - I0.y * J4.w;
	tmp_im = I0.x * J4.w + I0.y * J4.z;	
	accum_re[3*blockDim.x] = tmp_re;
	accum_im[3*blockDim.x] = tmp_im;	
      
	//10 component:
	tmp_re = I1.z * J0.x - I1.w * J0.y;
	tmp_im = I1.z * J0.y + I1.w * J0.x;	
	accum_re[4*blockDim.x] = tmp_re;
	accum_im[4*blockDim.x] = tmp_im;	

	//11 component:
	tmp_re = I1.z * J1.z - I1.w * J1.w;
	tmp_im = I1.z * J1.w + I1.w * J1.z;	
	accum_re[5*blockDim.x] = tmp_re;
	accum_im[5*blockDim.x] = tmp_im;	

	//12 component:
	tmp_re = I1.z * J3.x - I1.w * J3.y;
	tmp_im = I1.z * J3.y + I1.w * J3.x;	
	accum_re[6*blockDim.x] = tmp_re;
	accum_im[6*blockDim.x] = tmp_im;	

	//13 component:
	tmp_re = I1.z * J4.z - I1.w * J4.w;
	tmp_im = I1.z * J4.w + I1.w * J4.z;	
	accum_re[7*blockDim.x] = tmp_re;
	accum_im[7*blockDim.x] = tmp_im;	

	//20 component:
	tmp_re = I3.x * J0.x - I3.y * J0.y;
	tmp_im = I3.x * J0.y + I3.y * J0.x;	
	accum_re[8*blockDim.x] = tmp_re;
	accum_im[8*blockDim.x] = tmp_im;	

	//21 component:
	tmp_re = I3.x * J1.z - I3.y * J1.w;
	tmp_im = I3.x * J1.w + I3.y * J1.z;	
	accum_re[9*blockDim.x] = tmp_re;
	accum_im[9*blockDim.x] = tmp_im;	

	//22 component:
	tmp_re = I3.x * J3.x - I3.y * J3.y;
	tmp_im = I3.x * J3.y + I3.y * J3.x;	
	accum_re[10*blockDim.x] = tmp_re;
	accum_im[10*blockDim.x] = tmp_im;	

	//23 component:
	tmp_re = I3.x * J4.z - I3.y * J4.w;
	tmp_im = I3.x * J4.w + I3.y * J4.z;	
	accum_re[11*blockDim.x] = tmp_re;
	accum_im[11*blockDim.x] = tmp_im;	

	//30 component:
	tmp_re = I4.z * J0.x - I4.w * J0.y;
	tmp_im = I4.z * J0.y + I4.w * J0.x;	
	accum_re[12*blockDim.x] = tmp_re;
	accum_im[12*blockDim.x] = tmp_im;	

	//31 component:
	tmp_re = I4.z * J1.z - I4.w * J1.w;
	tmp_im = I4.z * J1.w + I4.w * J1.z;	
	accum_re[13*blockDim.x] = tmp_re;
	accum_im[13*blockDim.x] = tmp_im;	

	//32 component:
	tmp_re = I4.z * J3.x - I4.w * J3.y;
	tmp_im = I4.z * J3.y + I4.w * J3.x;	
	accum_re[14*blockDim.x] = tmp_re;
	accum_im[14*blockDim.x] = tmp_im;	

	//33 component:
	tmp_re = I4.z * J4.z - I4.w * J4.w;
	tmp_im = I4.z * J4.w + I4.w * J4.z;	
	accum_re[15*blockDim.x] = tmp_re;
	accum_im[15*blockDim.x] = tmp_im;	


	//do products for second color component here:
	//00 component:
	tmp_re = I0.z * J0.z - I0.w * J0.w;
	tmp_im = I0.z * J0.w + I0.w * J0.z;	
	accum_re[0*blockDim.x] += tmp_re;
	accum_im[0*blockDim.x] += tmp_im;	
	
	//01 component:
	tmp_re = I0.z * J2.x - I0.w * J2.y;
	tmp_im = I0.z * J2.y + I0.w * J2.x;	
	accum_re[1*blockDim.x] += tmp_re;
	accum_im[1*blockDim.x] += tmp_im;	

	//02 component:
	tmp_re = I0.z * J3.z - I0.w * J3.w;
	tmp_im = I0.z * J3.w + I0.w * J3.z;	
	accum_re[2*blockDim.x] += tmp_re;
	accum_im[2*blockDim.x] += tmp_im;	
      
	//03 component:
	tmp_re = I0.z * J5.x - I0.w * J5.x;
	tmp_im = I0.z * J5.y + I0.w * J5.y;	
	accum_re[3*blockDim.x] += tmp_re;
	accum_im[3*blockDim.x] += tmp_im;	
      
	//10 component:
	tmp_re = I2.x * J0.z - I2.y * J0.w;
	tmp_im = I2.x * J0.w + I2.y * J0.z;	
	accum_re[4*blockDim.x] += tmp_re;
	accum_im[4*blockDim.x] += tmp_im;	

	//11 component:
	tmp_re = I2.x * J2.x - I2.y * J2.y;
	tmp_im = I2.x * J2.y + I2.y * J2.x;	
	accum_re[5*blockDim.x] += tmp_re;
	accum_im[5*blockDim.x] += tmp_im;	

	//12 component:
	tmp_re = I2.x * J3.z - I2.y * J3.w;
	tmp_im = I2.x * J3.w + I2.y * J3.z;	
	accum_re[6*blockDim.x] += tmp_re;
	accum_im[6*blockDim.x] += tmp_im;	

	//13 component:
	tmp_re = I2.x * J5.x - I2.y * J5.y;
	tmp_im = I2.x * J5.y + I2.y * J5.x;	
	accum_re[7*blockDim.x] += tmp_re;
	accum_im[7*blockDim.x] += tmp_im;	

	//20 component:
	tmp_re = I3.z * J0.z - I3.w * J0.w;
	tmp_im = I3.z * J0.w + I3.w * J0.z;	
	accum_re[8*blockDim.x] += tmp_re;
	accum_im[8*blockDim.x] += tmp_im;	

	//21 component:
	tmp_re = I3.z * J2.x - I3.w * J2.y;
	tmp_im = I3.z * J2.y + I3.w * J2.x;	
	accum_re[9*blockDim.x] += tmp_re;
	accum_im[9*blockDim.x] += tmp_im;	

	//22 component:
	tmp_re = I3.z * J3.z - I3.w * J3.w;
	tmp_im = I3.z * J3.w + I3.w * J3.z;	
	accum_re[10*blockDim.x] += tmp_re;
	accum_im[10*blockDim.x] += tmp_im;	

	//23 component:
	tmp_re = I3.z * J5.x - I3.w * J5.y;
	tmp_im = I3.z * J5.y + I3.w * J5.x;	
	accum_re[11*blockDim.x] += tmp_re;
	accum_im[11*blockDim.x] += tmp_im;	

	//30 component:
	tmp_re = I5.x * J0.z - I5.y * J0.w;
	tmp_im = I5.x * J0.w + I5.y * J0.z;	
	accum_re[12*blockDim.x] += tmp_re;
	accum_im[12*blockDim.x] += tmp_im;	

	//31 component:
	tmp_re = I5.x * J2.x - I5.y * J2.y;
	tmp_im = I5.x * J2.y + I5.y * J2.x;	
	accum_re[13*blockDim.x] += tmp_re;
	accum_im[13*blockDim.x] += tmp_im;	

	//32 component:
	tmp_re = I5.x * J3.z - I5.y * J3.w;
	tmp_im = I5.x * J3.w + I5.y * J3.z;	
	accum_re[14*blockDim.x] += tmp_re;
	accum_im[14*blockDim.x] += tmp_im;	

	//33 component:
	tmp_re = I5.x * J5.x - I5.y * J5.y;
	tmp_im = I5.x * J5.y + I5.y * J5.x;	
	accum_re[15*blockDim.x] += tmp_re;
	accum_im[15*blockDim.x] += tmp_im;	


	//do products for third color component here:
	//00 component:
	tmp_re = I1.x * J1.x - I1.y * J1.y;
	tmp_im = I1.x * J1.y + I1.y * J1.x;	
	accum_re[0*blockDim.x] += tmp_re;
	accum_im[0*blockDim.x] += tmp_im;	
	
	//01 component:
	tmp_re = I1.x * J2.z - I1.y * J2.w;
	tmp_im = I1.x * J2.w + I1.y * J2.z;	
	accum_re[1*blockDim.x] += tmp_re;
	accum_im[1*blockDim.x] += tmp_im;	

	//02 component:
	tmp_re = I1.x * J4.x - I1.y * J4.y;
	tmp_im = I1.x * J4.y + I1.y * J4.x;	
	accum_re[2*blockDim.x] += tmp_re;
	accum_im[2*blockDim.x] += tmp_im;	
      
	//03 component:
	tmp_re = I1.x * J5.z - I1.y * J5.w;
	tmp_im = I1.x * J5.w + I1.y * J5.z;	
	accum_re[3*blockDim.x] += tmp_re;
	accum_im[3*blockDim.x] += tmp_im;	
      
	//10 component:
	tmp_re = I2.z * J1.x - I2.w * J1.y;
	tmp_im = I2.z * J1.y + I2.w * J1.x;	
	accum_re[4*blockDim.x] += tmp_re;
	accum_im[4*blockDim.x] += tmp_im;	

	//11 component:
	tmp_re = I2.z * J2.z - I2.w * J2.w;
	tmp_im = I2.z * J2.w + I2.w * J2.z;	
	accum_re[5*blockDim.x] += tmp_re;
	accum_im[5*blockDim.x] += tmp_im;	

	//12 component:
	tmp_re = I2.z * J4.x - I2.w * J4.y;
	tmp_im = I2.z * J4.y + I2.w * J4.x;	
	accum_re[6*blockDim.x] += tmp_re;
	accum_im[6*blockDim.x] += tmp_im;	

	//13 component:
	tmp_re = I2.z * J5.z - I2.w * J5.w;
	tmp_im = I2.z * J5.w + I2.w * J5.z;	
	accum_re[7*blockDim.x] += tmp_re;
	accum_im[7*blockDim.x] += tmp_im;	

	//20 component:
	tmp_re = I4.x * J1.x - I4.y * J1.y;
	tmp_im = I4.x * J1.y + I4.y * J1.x;	
	accum_re[8*blockDim.x] += tmp_re;
	accum_im[8*blockDim.x] += tmp_im;	

	//21 component:
	tmp_re = I4.x * J2.z - I4.y * J2.w;
	tmp_im = I4.x * J2.w + I4.y * J2.z;	
	accum_re[9*blockDim.x] += tmp_re;
	accum_im[9*blockDim.x] += tmp_im;	

	//22 component:
	tmp_re = I4.x * J4.x - I4.y * J4.y;
	tmp_im = I4.x * J4.y + I4.y * J4.x;	
	accum_re[10*blockDim.x] += tmp_re;
	accum_im[10*blockDim.x] += tmp_im;	

	//23 component:
	tmp_re = I4.x * J5.z - I4.y * J5.w;
	tmp_im = I4.x * J5.w + I4.y * J5.z;	
	accum_re[11*blockDim.x] += tmp_re;
	accum_im[11*blockDim.x] += tmp_im;	

	//30 component:
	tmp_re = I5.z * J1.x - I5.w * J1.y;
	tmp_im = I5.z * J1.y + I5.w * J1.x;	
	accum_re[12*blockDim.x] += tmp_re;
	accum_im[12*blockDim.x] += tmp_im;	

	//31 component:
	tmp_re = I5.z * J2.z - I5.w * J2.w;
	tmp_im = I5.z * J2.w + I5.w * J2.z;	
	accum_re[13*blockDim.x] += tmp_re;
	accum_im[13*blockDim.x] += tmp_im;	

	//32 component:
	tmp_re = I5.z * J4.x - I5.w * J4.y;
	tmp_im = I5.z * J4.y + I5.w * J4.x;	
	accum_re[14*blockDim.x] += tmp_re;
	accum_im[14*blockDim.x] += tmp_im;	

	//33 component:
	tmp_re = I5.z * J5.z - I5.w * J5.w;
	tmp_im = I5.z * J5.w + I5.w * J5.z;	
	accum_re[15*blockDim.x] += tmp_re;
	accum_im[15*blockDim.x] += tmp_im;

   //Store output back to global buffer:


/*	CONTRACTION FULL VOLUME		*/

	out[outId + 0 *maxThreads*2].x	+= accum_re[ 0*blockDim.x];
	out[outId + 1 *maxThreads*2].x	+= accum_re[ 1*blockDim.x];
	out[outId + 2 *maxThreads*2].x	+= accum_re[ 2*blockDim.x];
	out[outId + 3 *maxThreads*2].x	+= accum_re[ 3*blockDim.x];
	out[outId + 4 *maxThreads*2].x	+= accum_re[ 4*blockDim.x];
	out[outId + 5 *maxThreads*2].x	+= accum_re[ 5*blockDim.x];
	out[outId + 6 *maxThreads*2].x	+= accum_re[ 6*blockDim.x];
	out[outId + 7 *maxThreads*2].x	+= accum_re[ 7*blockDim.x];
	out[outId + 8 *maxThreads*2].x	+= accum_re[ 8*blockDim.x];
	out[outId + 9 *maxThreads*2].x	+= accum_re[ 9*blockDim.x];
	out[outId + 10*maxThreads*2].x	+= accum_re[10*blockDim.x];
	out[outId + 11*maxThreads*2].x	+= accum_re[11*blockDim.x];
	out[outId + 12*maxThreads*2].x	+= accum_re[12*blockDim.x];
	out[outId + 13*maxThreads*2].x	+= accum_re[13*blockDim.x];
	out[outId + 14*maxThreads*2].x	+= accum_re[14*blockDim.x];
	out[outId + 15*maxThreads*2].x	+= accum_re[15*blockDim.x];

	out[outId + 0 *maxThreads*2].y	+= accum_im[ 0*blockDim.x];
	out[outId + 1 *maxThreads*2].y	+= accum_im[ 1*blockDim.x];
	out[outId + 2 *maxThreads*2].y	+= accum_im[ 2*blockDim.x];
	out[outId + 3 *maxThreads*2].y	+= accum_im[ 3*blockDim.x];
	out[outId + 4 *maxThreads*2].y	+= accum_im[ 4*blockDim.x];
	out[outId + 5 *maxThreads*2].y	+= accum_im[ 5*blockDim.x];
	out[outId + 6 *maxThreads*2].y	+= accum_im[ 6*blockDim.x];
	out[outId + 7 *maxThreads*2].y	+= accum_im[ 7*blockDim.x];
	out[outId + 8 *maxThreads*2].y	+= accum_im[ 8*blockDim.x];
	out[outId + 9 *maxThreads*2].y	+= accum_im[ 9*blockDim.x];
	out[outId + 10*maxThreads*2].y	+= accum_im[10*blockDim.x];
	out[outId + 11*maxThreads*2].y	+= accum_im[11*blockDim.x];
	out[outId + 12*maxThreads*2].y	+= accum_im[12*blockDim.x];
	out[outId + 13*maxThreads*2].y	+= accum_im[13*blockDim.x];
	out[outId + 14*maxThreads*2].y	+= accum_im[14*blockDim.x];
	out[outId + 15*maxThreads*2].y	+= accum_im[15*blockDim.x];

	#undef SPINORTEX
	#undef INTERTEX

	return;
}

__global__ void contractPlusKernelS		(float2 *out, float4 *in1, float4 *in2, int maxThreads, int myStride, const int XL, const int YL, const int ZL, const int Parity, const DslashParam param)
{
	int	sid	 = blockIdx.x*blockDim.x + threadIdx.x;
	int	outId	 = sid;
	int	eutId, xCoord1, xCoord2, xCoord3, xCoord4, auxCoord1, auxCoord2;

	if	(sid >= maxThreads)
		return;

	#ifndef USE_TEXTURE_OBJECTS
		#define SPINORTEX	spinorTexSingle
		#define INTERTEX	interTexSingle
	#else
		#define SPINORTEX	param.inTex
		#define INTERTEX	param.xTex
	#endif

	volatile float2		tmp;
	extern __shared__ float	sms[];								//used for data accumulation: blockDim.x * 2 * 16 * sizeof(double)
   
	volatile float			*accum_re	 = sms + threadIdx.x;				//address it like idx*blockDim, where idx = 4*spinor_idx1 + spinor_idx2
	volatile float			*accum_im	 = accum_re + TOTAL_COMPONENTS*blockDim.x;

	eutId		 = 2*sid;
	auxCoord1	 = eutId / XL;
	xCoord1		 = eutId - auxCoord1 * XL;
	auxCoord2	 = auxCoord1 / YL;
	xCoord2		 = auxCoord1 - auxCoord2 * YL;
	xCoord4		 = auxCoord2 / ZL;
	xCoord3		 = auxCoord2 - xCoord4 * ZL;

	auxCoord1	 = (Parity + xCoord4 + xCoord3 + xCoord2) & 1;
	xCoord1		+= auxCoord1;
	outId		 = xCoord1 + XL*(xCoord2 + YL*(xCoord3 + ZL*xCoord4));				//AQUI

	//Load the full input spinors:

	READ_SPINOR			(SPINORTEX, myStride, sid, sid);
	READ_INTERMEDIATE_SPINOR	(INTERTEX,  myStride, sid, sid);

	//compute in1^dag:

	I0.y	 = -I0.y;
	I0.w	 = -I0.w;
	I1.y	 = -I1.y;
	I1.w	 = -I1.w;
	I2.y	 = -I2.y;
	I2.w	 = -I2.w;	
	I3.y	 = -I3.y;
	I3.w	 = -I3.w;
	I4.y	 = -I4.y;
	I4.w	 = -I4.w;
	I5.y	 = -I5.y;
	I5.w	 = -I5.w;	

	//do products for first color component here:
	//00 component:
	tmp_re = I0.x * J0.x - I0.y * J0.y;
	tmp_im = I0.x * J0.y + I0.y * J0.x;	
	accum_re[0*blockDim.x] = tmp_re;
	accum_im[0*blockDim.x] = tmp_im;	
	
	//01 component:
	tmp_re = I0.x * J1.z - I0.y * J1.w;
	tmp_im = I0.x * J1.w + I0.y * J1.z;	
	accum_re[1*blockDim.x] = tmp_re;
	accum_im[1*blockDim.x] = tmp_im;	

	//02 component:
	tmp_re = I0.x * J3.x - I0.y * J3.y;
	tmp_im = I0.x * J3.y + I0.y * J3.x;	
	accum_re[2*blockDim.x] = tmp_re;
	accum_im[2*blockDim.x] = tmp_im;	
      
	//03 component:
	tmp_re = I0.x * J4.z - I0.y * J4.w;
	tmp_im = I0.x * J4.w + I0.y * J4.z;	
	accum_re[3*blockDim.x] = tmp_re;
	accum_im[3*blockDim.x] = tmp_im;	
      
	//10 component:
	tmp_re = I1.z * J0.x - I1.w * J0.y;
	tmp_im = I1.z * J0.y + I1.w * J0.x;	
	accum_re[4*blockDim.x] = tmp_re;
	accum_im[4*blockDim.x] = tmp_im;	

	//11 component:
	tmp_re = I1.z * J1.z - I1.w * J1.w;
	tmp_im = I1.z * J1.w + I1.w * J1.z;	
	accum_re[5*blockDim.x] = tmp_re;
	accum_im[5*blockDim.x] = tmp_im;	

	//12 component:
	tmp_re = I1.z * J3.x - I1.w * J3.y;
	tmp_im = I1.z * J3.y + I1.w * J3.x;	
	accum_re[6*blockDim.x] = tmp_re;
	accum_im[6*blockDim.x] = tmp_im;	

	//13 component:
	tmp_re = I1.z * J4.z - I1.w * J4.w;
	tmp_im = I1.z * J4.w + I1.w * J4.z;	
	accum_re[7*blockDim.x] = tmp_re;
	accum_im[7*blockDim.x] = tmp_im;	

	//20 component:
	tmp_re = I3.x * J0.x - I3.y * J0.y;
	tmp_im = I3.x * J0.y + I3.y * J0.x;	
	accum_re[8*blockDim.x] = tmp_re;
	accum_im[8*blockDim.x] = tmp_im;	

	//21 component:
	tmp_re = I3.x * J1.z - I3.y * J1.w;
	tmp_im = I3.x * J1.w + I3.y * J1.z;	
	accum_re[9*blockDim.x] = tmp_re;
	accum_im[9*blockDim.x] = tmp_im;	

	//22 component:
	tmp_re = I3.x * J3.x - I3.y * J3.y;
	tmp_im = I3.x * J3.y + I3.y * J3.x;	
	accum_re[10*blockDim.x] = tmp_re;
	accum_im[10*blockDim.x] = tmp_im;	

	//23 component:
	tmp_re = I3.x * J4.z - I3.y * J4.w;
	tmp_im = I3.x * J4.w + I3.y * J4.z;	
	accum_re[11*blockDim.x] = tmp_re;
	accum_im[11*blockDim.x] = tmp_im;	

	//30 component:
	tmp_re = I4.z * J0.x - I4.w * J0.y;
	tmp_im = I4.z * J0.y + I4.w * J0.x;	
	accum_re[12*blockDim.x] = tmp_re;
	accum_im[12*blockDim.x] = tmp_im;	

	//31 component:
	tmp_re = I4.z * J1.z - I4.w * J1.w;
	tmp_im = I4.z * J1.w + I4.w * J1.z;	
	accum_re[13*blockDim.x] = tmp_re;
	accum_im[13*blockDim.x] = tmp_im;	

	//32 component:
	tmp_re = I4.z * J3.x - I4.w * J3.y;
	tmp_im = I4.z * J3.y + I4.w * J3.x;	
	accum_re[14*blockDim.x] = tmp_re;
	accum_im[14*blockDim.x] = tmp_im;	

	//33 component:
	tmp_re = I4.z * J4.z - I4.w * J4.w;
	tmp_im = I4.z * J4.w + I4.w * J4.z;	
	accum_re[15*blockDim.x] = tmp_re;
	accum_im[15*blockDim.x] = tmp_im;	


	//do products for second color component here:
	//00 component:
	tmp_re = I0.z * J0.z - I0.w * J0.w;
	tmp_im = I0.z * J0.w + I0.w * J0.z;	
	accum_re[0*blockDim.x] += tmp_re;
	accum_im[0*blockDim.x] += tmp_im;	
	
	//01 component:
	tmp_re = I0.z * J2.x - I0.w * J2.y;
	tmp_im = I0.z * J2.y + I0.w * J2.x;	
	accum_re[1*blockDim.x] += tmp_re;
	accum_im[1*blockDim.x] += tmp_im;	

	//02 component:
	tmp_re = I0.z * J3.z - I0.w * J3.w;
	tmp_im = I0.z * J3.w + I0.w * J3.z;	
	accum_re[2*blockDim.x] += tmp_re;
	accum_im[2*blockDim.x] += tmp_im;	
      
	//03 component:
	tmp_re = I0.z * J5.x - I0.w * J5.x;
	tmp_im = I0.z * J5.y + I0.w * J5.y;	
	accum_re[3*blockDim.x] += tmp_re;
	accum_im[3*blockDim.x] += tmp_im;	
      
	//10 component:
	tmp_re = I2.x * J0.z - I2.y * J0.w;
	tmp_im = I2.x * J0.w + I2.y * J0.z;	
	accum_re[4*blockDim.x] += tmp_re;
	accum_im[4*blockDim.x] += tmp_im;	

	//11 component:
	tmp_re = I2.x * J2.x - I2.y * J2.y;
	tmp_im = I2.x * J2.y + I2.y * J2.x;	
	accum_re[5*blockDim.x] += tmp_re;
	accum_im[5*blockDim.x] += tmp_im;	

	//12 component:
	tmp_re = I2.x * J3.z - I2.y * J3.w;
	tmp_im = I2.x * J3.w + I2.y * J3.z;	
	accum_re[6*blockDim.x] += tmp_re;
	accum_im[6*blockDim.x] += tmp_im;	

	//13 component:
	tmp_re = I2.x * J5.x - I2.y * J5.y;
	tmp_im = I2.x * J5.y + I2.y * J5.x;	
	accum_re[7*blockDim.x] += tmp_re;
	accum_im[7*blockDim.x] += tmp_im;	

	//20 component:
	tmp_re = I3.z * J0.z - I3.w * J0.w;
	tmp_im = I3.z * J0.w + I3.w * J0.z;	
	accum_re[8*blockDim.x] += tmp_re;
	accum_im[8*blockDim.x] += tmp_im;	

	//21 component:
	tmp_re = I3.z * J2.x - I3.w * J2.y;
	tmp_im = I3.z * J2.y + I3.w * J2.x;	
	accum_re[9*blockDim.x] += tmp_re;
	accum_im[9*blockDim.x] += tmp_im;	

	//22 component:
	tmp_re = I3.z * J3.z - I3.w * J3.w;
	tmp_im = I3.z * J3.w + I3.w * J3.z;	
	accum_re[10*blockDim.x] += tmp_re;
	accum_im[10*blockDim.x] += tmp_im;	

	//23 component:
	tmp_re = I3.z * J5.x - I3.w * J5.y;
	tmp_im = I3.z * J5.y + I3.w * J5.x;	
	accum_re[11*blockDim.x] += tmp_re;
	accum_im[11*blockDim.x] += tmp_im;	

	//30 component:
	tmp_re = I5.x * J0.z - I5.y * J0.w;
	tmp_im = I5.x * J0.w + I5.y * J0.z;	
	accum_re[12*blockDim.x] += tmp_re;
	accum_im[12*blockDim.x] += tmp_im;	

	//31 component:
	tmp_re = I5.x * J2.x - I5.y * J2.y;
	tmp_im = I5.x * J2.y + I5.y * J2.x;	
	accum_re[13*blockDim.x] += tmp_re;
	accum_im[13*blockDim.x] += tmp_im;	

	//32 component:
	tmp_re = I5.x * J3.z - I5.y * J3.w;
	tmp_im = I5.x * J3.w + I5.y * J3.z;	
	accum_re[14*blockDim.x] += tmp_re;
	accum_im[14*blockDim.x] += tmp_im;	

	//33 component:
	tmp_re = I5.x * J5.x - I5.y * J5.y;
	tmp_im = I5.x * J5.y + I5.y * J5.x;	
	accum_re[15*blockDim.x] += tmp_re;
	accum_im[15*blockDim.x] += tmp_im;	


	//do products for third color component here:
	//00 component:
	tmp_re = I1.x * J1.x - I1.y * J1.y;
	tmp_im = I1.x * J1.y + I1.y * J1.x;	
	accum_re[0*blockDim.x] += tmp_re;
	accum_im[0*blockDim.x] += tmp_im;	
	
	//01 component:
	tmp_re = I1.x * J2.z - I1.y * J2.w;
	tmp_im = I1.x * J2.w + I1.y * J2.z;	
	accum_re[1*blockDim.x] += tmp_re;
	accum_im[1*blockDim.x] += tmp_im;	

	//02 component:
	tmp_re = I1.x * J4.x - I1.y * J4.y;
	tmp_im = I1.x * J4.y + I1.y * J4.x;	
	accum_re[2*blockDim.x] += tmp_re;
	accum_im[2*blockDim.x] += tmp_im;	
      
	//03 component:
	tmp_re = I1.x * J5.z - I1.y * J5.w;
	tmp_im = I1.x * J5.w + I1.y * J5.z;	
	accum_re[3*blockDim.x] += tmp_re;
	accum_im[3*blockDim.x] += tmp_im;	
      
	//10 component:
	tmp_re = I2.z * J1.x - I2.w * J1.y;
	tmp_im = I2.z * J1.y + I2.w * J1.x;	
	accum_re[4*blockDim.x] += tmp_re;
	accum_im[4*blockDim.x] += tmp_im;	

	//11 component:
	tmp_re = I2.z * J2.z - I2.w * J2.w;
	tmp_im = I2.z * J2.w + I2.w * J2.z;	
	accum_re[5*blockDim.x] += tmp_re;
	accum_im[5*blockDim.x] += tmp_im;	

	//12 component:
	tmp_re = I2.z * J4.x - I2.w * J4.y;
	tmp_im = I2.z * J4.y + I2.w * J4.x;	
	accum_re[6*blockDim.x] += tmp_re;
	accum_im[6*blockDim.x] += tmp_im;	

	//13 component:
	tmp_re = I2.z * J5.z - I2.w * J5.w;
	tmp_im = I2.z * J5.w + I2.w * J5.z;	
	accum_re[7*blockDim.x] += tmp_re;
	accum_im[7*blockDim.x] += tmp_im;	

	//20 component:
	tmp_re = I4.x * J1.x - I4.y * J1.y;
	tmp_im = I4.x * J1.y + I4.y * J1.x;	
	accum_re[8*blockDim.x] += tmp_re;
	accum_im[8*blockDim.x] += tmp_im;	

	//21 component:
	tmp_re = I4.x * J2.z - I4.y * J2.w;
	tmp_im = I4.x * J2.w + I4.y * J2.z;	
	accum_re[9*blockDim.x] += tmp_re;
	accum_im[9*blockDim.x] += tmp_im;	

	//22 component:
	tmp_re = I4.x * J4.x - I4.y * J4.y;
	tmp_im = I4.x * J4.y + I4.y * J4.x;	
	accum_re[10*blockDim.x] += tmp_re;
	accum_im[10*blockDim.x] += tmp_im;	

	//23 component:
	tmp_re = I4.x * J5.z - I4.y * J5.w;
	tmp_im = I4.x * J5.w + I4.y * J5.z;	
	accum_re[11*blockDim.x] += tmp_re;
	accum_im[11*blockDim.x] += tmp_im;	

	//30 component:
	tmp_re = I5.z * J1.x - I5.w * J1.y;
	tmp_im = I5.z * J1.y + I5.w * J1.x;	
	accum_re[12*blockDim.x] += tmp_re;
	accum_im[12*blockDim.x] += tmp_im;	

	//31 component:
	tmp_re = I5.z * J2.z - I5.w * J2.w;
	tmp_im = I5.z * J2.w + I5.w * J2.z;	
	accum_re[13*blockDim.x] += tmp_re;
	accum_im[13*blockDim.x] += tmp_im;	

	//32 component:
	tmp_re = I5.z * J4.x - I5.w * J4.y;
	tmp_im = I5.z * J4.y + I5.w * J4.x;	
	accum_re[14*blockDim.x] += tmp_re;
	accum_im[14*blockDim.x] += tmp_im;	

	//33 component:
	tmp_re = I5.z * J5.z - I5.w * J5.w;
	tmp_im = I5.z * J5.w + I5.w * J5.z;	
	accum_re[15*blockDim.x] += tmp_re;
	accum_im[15*blockDim.x] += tmp_im;

	//Store output back to global buffer:

/*	CONTRACTION FULL VOLUME		*/

	out[outId + 0 *maxThreads*2].x	+= accum_re[ 0*blockDim.x];
	out[outId + 1 *maxThreads*2].x	+= accum_re[ 1*blockDim.x];
	out[outId + 2 *maxThreads*2].x	+= accum_re[ 2*blockDim.x];
	out[outId + 3 *maxThreads*2].x	+= accum_re[ 3*blockDim.x];
	out[outId + 4 *maxThreads*2].x	+= accum_re[ 4*blockDim.x];
	out[outId + 5 *maxThreads*2].x	+= accum_re[ 5*blockDim.x];
	out[outId + 6 *maxThreads*2].x	+= accum_re[ 6*blockDim.x];
	out[outId + 7 *maxThreads*2].x	+= accum_re[ 7*blockDim.x];
	out[outId + 8 *maxThreads*2].x	+= accum_re[ 8*blockDim.x];
	out[outId + 9 *maxThreads*2].x	+= accum_re[ 9*blockDim.x];
	out[outId + 10*maxThreads*2].x	+= accum_re[10*blockDim.x];
	out[outId + 11*maxThreads*2].x	+= accum_re[11*blockDim.x];
	out[outId + 12*maxThreads*2].x	+= accum_re[12*blockDim.x];
	out[outId + 13*maxThreads*2].x	+= accum_re[13*blockDim.x];
	out[outId + 14*maxThreads*2].x	+= accum_re[14*blockDim.x];
	out[outId + 15*maxThreads*2].x	+= accum_re[15*blockDim.x];

	out[outId + 0 *maxThreads*2].y	+= accum_im[ 0*blockDim.x];
	out[outId + 1 *maxThreads*2].y	+= accum_im[ 1*blockDim.x];
	out[outId + 2 *maxThreads*2].y	+= accum_im[ 2*blockDim.x];
	out[outId + 3 *maxThreads*2].y	+= accum_im[ 3*blockDim.x];
	out[outId + 4 *maxThreads*2].y	+= accum_im[ 4*blockDim.x];
	out[outId + 5 *maxThreads*2].y	+= accum_im[ 5*blockDim.x];
	out[outId + 6 *maxThreads*2].y	+= accum_im[ 6*blockDim.x];
	out[outId + 7 *maxThreads*2].y	+= accum_im[ 7*blockDim.x];
	out[outId + 8 *maxThreads*2].y	+= accum_im[ 8*blockDim.x];
	out[outId + 9 *maxThreads*2].y	+= accum_im[ 9*blockDim.x];
	out[outId + 10*maxThreads*2].y	+= accum_im[10*blockDim.x];
	out[outId + 11*maxThreads*2].y	+= accum_im[11*blockDim.x];
	out[outId + 12*maxThreads*2].y	+= accum_im[12*blockDim.x];
	out[outId + 13*maxThreads*2].y	+= accum_im[13*blockDim.x];
	out[outId + 14*maxThreads*2].y	+= accum_im[14*blockDim.x];
	out[outId + 15*maxThreads*2].y	+= accum_im[15*blockDim.x];

	#undef SPINORTEX
	#undef INTERTEX

	return;
}

#undef tmp_re
#undef tmp_im

#endif //_TWIST_QUDA_CONTRACT
