#include <stdlib.h>
#include <stdio.h>

#include <color_spinor_field.h>
#include <clover_field.h>

#define BLOCK_DIM 64

// these control the Wilson-type actions
//#define DIRECT_ACCESS_LINK
//#define DIRECT_ACCESS_WILSON_SPINOR
//#define DIRECT_ACCESS_WILSON_ACCUM
//#define DIRECT_ACCESS_WILSON_INTER
//#define DIRECT_ACCESS_WILSON_PACK_SPINOR

//these are access control for staggered action
#if (__CUDA_ARCH__ >= 200)
//#define DIRECT_ACCESS_FAT_LINK
//#define DIRECT_ACCESS_LONG_LINK
#define DIRECT_ACCESS_SPINOR
#else
#define DIRECT_ACCESS_FAT_LINK
//#define DIRECT_ACCESS_LONG_LINK
//#define DIRECT_ACCESS_SPINOR
#endif

#include <quda_internal.h>
#include <dslash_quda.h>
#include <sys/time.h>

//#define PARALLEL_DIR

enum KernelType {
  INTERIOR_KERNEL = 5,
  EXTERIOR_KERNEL_X = 0,
  EXTERIOR_KERNEL_Y = 1,
  EXTERIOR_KERNEL_Z = 2,
  EXTERIOR_KERNEL_T = 3
};

struct DslashParam {
  int tOffset; // offset into the T dimension (multi gpu only)
  int tMul;    // spatial volume distance between the T faces being updated (multi gpu only)
  int threads; // the desired number of active threads
  int parity;  // Even-Odd or Odd-Even
  int commDim[QUDA_MAX_DIM]; // Whether to do comms or not
  int ghostDim[QUDA_MAX_DIM]; // Whether a ghost zone has been allocated for a given dimension
  int ghostOffset[QUDA_MAX_DIM];
  int ghostNormOffset[QUDA_MAX_DIM];
  KernelType kernel_type; //is it INTERIOR_KERNEL, EXTERIOR_KERNEL_X/Y/Z/T
};

// determines whether the temporal ghost zones are packed with a gather kernel,
// as opposed to multiple calls to cudaMemcpy()
bool kernelPackT = false;

DslashParam dslashParam;

// these are set in initDslashConst
int Vspatial;
#ifdef MULTI_GPU
static const int Nstream = 9;
#else
static const int Nstream = 1;
#endif
static cudaStream_t streams[Nstream];
static cudaEvent_t scatterEvent[Nstream];
static cudaEvent_t dslashEnd;

FaceBuffer *face;
cudaColorSpinorField *inSpinor;

#include <dslash_textures.h>
#include <dslash_constants.h>

#define SHORT_LENGTH 65536
#define SCALE_FLOAT ((SHORT_LENGTH-1) * 0.5) // 32767.5
#define SHIFT_FLOAT (-1.f / (SHORT_LENGTH-1)) // 1.5259021897e-5

#if defined(DIRECT_ACCESS_LINK) || defined(DIRECT_ACCESS_WILSON_SPINOR) || \
  defined(DIRECT_ACCESS_WILSON_ACCUM) || defined(DIRECT_ACCESS_WILSON_PACK_SPINOR)
static inline __device__ short float2short(float c, float a) {
  //return (short)(a*MAX_SHORT);
  short rtn = (short)((a+SHIFT_FLOAT)*SCALE_FLOAT*c);
  return rtn;
}

static inline __device__ float short2float(short a) {
  return (float)a/SCALE_FLOAT - SHIFT_FLOAT;
}

static inline __device__ short4 float42short4(float c, float4 a) {
  return make_short4(float2short(c, a.x), float2short(c, a.y), float2short(c, a.z), float2short(c, a.w));
}

static inline __device__ float4 short42float4(short4 a) {
  return make_float4(short2float(a.x), short2float(a.y), short2float(a.z), short2float(a.w));
}

static inline __device__ float2 short22float2(short2 a) {
  return make_float2(short2float(a.x), short2float(a.y));
}
#endif // DIRECT_ACCESS inclusions

#include <inline_ptx.h>

// dslashTuning = QUDA_TUNE_YES turns off error checking
static QudaTune dslashTuning = QUDA_TUNE_NO;

void setDslashTuning(QudaTune tune)
{
  dslashTuning = tune;
}

#if (CUDA_VERSION <= 4000)
#define VOLATILE volatile
#else
#define VOLATILE 
#endif

#include <pack_face_def.h>        // kernels for packing the ghost zones and general indexing
#include <staggered_dslash_def.h> // staggered Dslash kernels
#include <wilson_dslash_def.h>    // Wilson Dslash kernels (including clover)
#include <dw_dslash_def.h>        // Domain Wall kernels
#include <tm_dslash_def.h>        // Twisted Mass kernels
#include <tm_core.h>              // solo twisted mass kernel
#include <clover_def.h>           // kernels for applying the clover term alone

#undef VOLATILE

#ifndef DSLASH_SHARED_FLOATS_PER_THREAD
#define DSLASH_SHARED_FLOATS_PER_THREAD 0
#endif

#ifndef CLOVER_SHARED_FLOATS_PER_THREAD
#define CLOVER_SHARED_FLOATS_PER_THREAD 0
#endif

#ifndef SHARED_COORDS
#define SHARED_COORDS 0
#endif

#include <blas_quda.h>
#include <face_quda.h>


__global__ void dummyKernel() {
  // do nothing
}

void initCache() {

#if (__CUDA_ARCH__ >= 200)

  static int firsttime = 1;
  if (firsttime){	
    cudaFuncSetCacheConfig(dummyKernel, cudaFuncCachePreferL1);
    dummyKernel<<<1,1>>>();
    firsttime=0;
  }

#endif

}

void setFace(const FaceBuffer &Face) {
  face = (FaceBuffer*)&Face; // nasty
}

#define MORE_GENERIC_DSLASH(FUNC, DAG, X, kernel_type, gridDim, blockDim, shared, stream, param,  ...)            \
  if (x==0) {                                                                                                     \
    if (reconstruct == QUDA_RECONSTRUCT_NO) {                                                                     \
      FUNC ## 18 ## DAG ## Kernel<kernel_type><<<gridDim, blockDim, shared, stream>>> ( __VA_ARGS__ , param);     \
    } else if (reconstruct == QUDA_RECONSTRUCT_12) {                                                              \
      FUNC ## 12 ## DAG ## Kernel<kernel_type><<<gridDim, blockDim, shared, stream>>> ( __VA_ARGS__ , param);     \
    } else {                                                                                                      \
      FUNC ## 8 ## DAG ## Kernel<kernel_type><<<gridDim, blockDim, shared, stream>>> ( __VA_ARGS__, param);       \
    }                                                                                                             \
  } else {                                                                                                        \
    if (reconstruct == QUDA_RECONSTRUCT_NO) {                                                                     \
      FUNC ## 18 ## DAG ## X ## Kernel<kernel_type><<<gridDim, blockDim, shared, stream>>> ( __VA_ARGS__, param); \
    } else if (reconstruct == QUDA_RECONSTRUCT_12) {                                                              \
      FUNC ## 12 ## DAG ## X ## Kernel<kernel_type><<<gridDim, blockDim, shared, stream>>> ( __VA_ARGS__, param); \
    } else if (reconstruct == QUDA_RECONSTRUCT_8) {                                                               \
      FUNC ## 8 ## DAG ## X ## Kernel<kernel_type> <<<gridDim, blockDim, shared, stream>>> ( __VA_ARGS__, param); \
    }                                                                                                             \
  }

#ifndef MULTI_GPU

#define GENERIC_DSLASH(FUNC, DAG, X, gridDim, blockDim, shared, stream, param,  ...)                          \
  switch(param.kernel_type) {						                                      \
  case INTERIOR_KERNEL:							                                      \
    MORE_GENERIC_DSLASH(FUNC, DAG, X, INTERIOR_KERNEL, gridDim, blockDim, shared, stream, param, __VA_ARGS__) \
    break;								                                      \
  default:								                                      \
    errorQuda("KernelType %d not defined for single GPU", param.kernel_type);                                 \
  }

#else

#define GENERIC_DSLASH(FUNC, DAG, X, gridDim, blockDim, shared, stream, param,  ...)                            \
  switch(param.kernel_type) {						                                        \
  case INTERIOR_KERNEL:							                                        \
    MORE_GENERIC_DSLASH(FUNC, DAG, X, INTERIOR_KERNEL,   gridDim, blockDim, shared, stream, param, __VA_ARGS__) \
    break;								                                        \
  case EXTERIOR_KERNEL_X:							                                \
    MORE_GENERIC_DSLASH(FUNC, DAG, X, EXTERIOR_KERNEL_X, gridDim, blockDim, shared, stream, param, __VA_ARGS__) \
    break;								                                        \
  case EXTERIOR_KERNEL_Y:							                                \
    MORE_GENERIC_DSLASH(FUNC, DAG, X, EXTERIOR_KERNEL_Y, gridDim, blockDim, shared, stream, param, __VA_ARGS__) \
    break;								                                        \
  case EXTERIOR_KERNEL_Z:							                                \
    MORE_GENERIC_DSLASH(FUNC, DAG, X, EXTERIOR_KERNEL_Z, gridDim, blockDim, shared, stream, param, __VA_ARGS__) \
    break;								                                        \
  case EXTERIOR_KERNEL_T:							                                \
    MORE_GENERIC_DSLASH(FUNC, DAG, X, EXTERIOR_KERNEL_T, gridDim, blockDim, shared, stream, param, __VA_ARGS__) \
    break;								                                        \
  }

#endif

// macro used for dslash types with dagger kernel defined (Wilson, domain wall, etc.)
#define DSLASH(FUNC, gridDim, blockDim, shared, stream, param, ...)	\
  if (!dagger) {							\
    GENERIC_DSLASH(FUNC, , Xpay, gridDim, blockDim, shared, stream, param, __VA_ARGS__) \
  } else {								\
    GENERIC_DSLASH(FUNC, Dagger, Xpay, gridDim, blockDim, shared, stream, param, __VA_ARGS__) \
 }

// macro used for staggered dslash
#define STAGGERED_DSLASH(gridDim, blockDim, shared, stream, param, ...)	\
    GENERIC_DSLASH(staggeredDslash, , Axpy, gridDim, blockDim, shared, stream, param, __VA_ARGS__)


// Use an abstract class interface to drive the different CUDA dslash
// kernels.  All parameters are curried into the derived classes to
// allow a simple interface.
class DslashCuda {
public:
  DslashCuda() { ; }
  virtual ~DslashCuda() { ; }
  virtual void apply(const dim3 &blockDim, const dim3 &gridDim, const int shared_bytes, const cudaStream_t &stream) = 0;
};


template <typename sFloat, typename gFloat>
class WilsonDslashCuda : public DslashCuda {

private:
  sFloat *out;
  float *outNorm;
  const sFloat *in, *x;
  const float *inNorm, *xNorm;
  const gFloat *gauge0, *gauge1;
  const QudaReconstructType reconstruct;
  const int dagger;
  const double a;

public:
  WilsonDslashCuda(sFloat *out, float *outNorm, const gFloat *gauge0, const gFloat *gauge1, 
		   const QudaReconstructType reconstruct, const sFloat *in, const float *inNorm,
		   const sFloat *x, const float *xNorm, const double a,
		   const int dagger, const size_t bytes, const size_t norm_bytes) :
    DslashCuda(), out(out), outNorm(outNorm), gauge0(gauge0), gauge1(gauge1), in(in), 
    inNorm(inNorm), reconstruct(reconstruct), dagger(dagger), x(x), xNorm(xNorm), a(a) { 
    bindSpinorTex(bytes, norm_bytes, in, inNorm, out, outNorm, x, xNorm); 
  }

  virtual ~WilsonDslashCuda() { unbindSpinorTex(in, inNorm, out, outNorm, x, xNorm); }

  void apply(const dim3 &blockDim, const dim3 &gridDim, const int shared_bytes, const cudaStream_t &stream) {
    //dim3 gridDim( (dslashParam.threads+blockDim.x-1) / blockDim.x, 1, 1);
    //printfQuda("Applying dslash: threads = %d, type = %d\n", dslashParam.threads, dslashParam.kernel_type);
    DSLASH(dslash, gridDim, blockDim, shared_bytes, stream, dslashParam,
	   out, outNorm, gauge0, gauge1, in, inNorm, x, xNorm, a);
  }

};

template <typename sFloat, typename gFloat, typename cFloat>
class CloverDslashCuda : public DslashCuda {

private:
  sFloat *out;
  float *outNorm;
  const sFloat *in, *x;
  const float *inNorm, *xNorm;
  const gFloat *gauge0, *gauge1;
  const QudaReconstructType reconstruct;
  const cFloat *clover;
  const float *cloverNorm;
  const int dagger;
  const double a;

public:
  CloverDslashCuda(sFloat *out, float *outNorm, const gFloat *gauge0, const gFloat *gauge1, 
		   const QudaReconstructType reconstruct, const cFloat *clover, 
		   const float *cloverNorm, const sFloat *in, const float *inNorm,
		   const sFloat *x, const float *xNorm, const double a,
		   const int dagger, const size_t bytes, const size_t norm_bytes) :
    DslashCuda(), out(out), outNorm(outNorm), gauge0(gauge0), gauge1(gauge1), 
    clover(clover), cloverNorm(cloverNorm), in(in), inNorm(inNorm), 
    reconstruct(reconstruct), dagger(dagger), x(x), xNorm(xNorm), a(a) { 
    bindSpinorTex(bytes, norm_bytes, in, inNorm, out, outNorm, x, xNorm); 
  }
  virtual ~CloverDslashCuda() { unbindSpinorTex(in, inNorm, out, outNorm, x, xNorm); }

  void apply(const dim3 &blockDim, const dim3 &gridDim, const int shared_bytes, const cudaStream_t &stream) {
    //dim3 gridDim( (dslashParam.threads+blockDim.x-1) / blockDim.x, 1, 1);
    DSLASH(cloverDslash, gridDim, blockDim, shared_bytes, stream, dslashParam,
	   out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, x, xNorm, a);
  }

};

void setTwistParam(double &a, double &b, const double &kappa, const double &mu, 
		   const int dagger, const QudaTwistGamma5Type twist) {
  if (twist == QUDA_TWIST_GAMMA5_DIRECT) {
    a = 2.0 * kappa * mu;
    b = 1.0;
  } else if (twist == QUDA_TWIST_GAMMA5_INVERSE) {
    a = -2.0 * kappa * mu;
    b = 1.0 / (1.0 + a*a);
  } else {
    errorQuda("Twist type %d not defined\n", twist);
  }
  if (dagger) a *= -1.0;

}

template <typename sFloat, typename gFloat>
class TwistedDslashCuda : public DslashCuda {

private:
  sFloat *out;
  float *outNorm;
  const sFloat *in, *x;
  const float *inNorm, *xNorm;
  const gFloat *gauge0, *gauge1;
  const QudaReconstructType reconstruct;
  const int dagger;
  double a;
  double b;

public:
  TwistedDslashCuda(sFloat *out, float *outNorm, const gFloat *gauge0, const gFloat *gauge1, 
		    const QudaReconstructType reconstruct, const sFloat *in, const float *inNorm,
		    const sFloat *x, const float *xNorm, const double kappa, const double mu,
		    const double k, const int dagger, const size_t bytes, const size_t norm_bytes) :
    DslashCuda(), out(out), outNorm(outNorm), gauge0(gauge0), gauge1(gauge1), 
    in(in), inNorm(inNorm), reconstruct(reconstruct), dagger(dagger), x(x), xNorm(xNorm) { 
    bindSpinorTex(bytes, norm_bytes, in, inNorm, out, outNorm, x, xNorm); 
    setTwistParam(a, b, kappa, mu, dagger, QUDA_TWIST_GAMMA5_INVERSE);
    if (x) b *= k;
  }
  virtual ~TwistedDslashCuda() { unbindSpinorTex(in, inNorm, out, outNorm, x, xNorm); }

  void apply(const dim3 &blockDim, const dim3 &gridDim, const int shared_bytes, const cudaStream_t &stream) {
    //dim3 gridDim( (dslashParam.threads+blockDim.x-1) / blockDim.x, 1, 1);
    DSLASH(twistedMassDslash, gridDim, blockDim, shared_bytes, stream, dslashParam,
	   out, outNorm, gauge0, gauge1, in, inNorm, a, b, x, xNorm);
  }

};

template <typename sFloat, typename gFloat>
class DomainWallDslashCuda : public DslashCuda {

private:
  sFloat *out;
  float *outNorm;
  const sFloat *in, *x;
  const float *inNorm, *xNorm;
  const gFloat *gauge0, *gauge1;
  const QudaReconstructType reconstruct;
  const int dagger;
  const double mferm;
  const double a;

public:
  DomainWallDslashCuda(sFloat *out, float *outNorm, const gFloat *gauge0, const gFloat *gauge1, 
		       const QudaReconstructType reconstruct, const sFloat *in, 
		       const float *inNorm, const sFloat *x, const float *xNorm, const double mferm, 
		       const double a, const int dagger, const size_t bytes, const size_t norm_bytes) :
    DslashCuda(), out(out), outNorm(outNorm), gauge0(gauge0), gauge1(gauge1), 
    in(in), inNorm(inNorm), mferm(mferm), reconstruct(reconstruct), dagger(dagger), x(x), xNorm(xNorm), a(a) { 
    bindSpinorTex(bytes, norm_bytes, in, inNorm, out, outNorm, x, xNorm); 
  }
  virtual ~DomainWallDslashCuda() { unbindSpinorTex(in, inNorm, out, outNorm, x, xNorm); }

  void apply(const dim3 &blockDim, const dim3 &gridDim, const int shared_bytes, const cudaStream_t &stream) {
    //dim3 gridDim( (dslashParam.threads+blockDim.x-1) / blockDim.x, 1, 1);
    DSLASH(domainWallDslash, gridDim, blockDim, shared_bytes, stream, dslashParam,
    	   out, outNorm, gauge0, gauge1, in, inNorm, mferm, x, xNorm, a);
  }

};

void dslashCuda(DslashCuda &dslash, const size_t regSize, const int parity, const int dagger, 
		const int volume, const int *faceVolumeCB, const dim3 *blockDim, const dim3 *gridDim) {

  dslashParam.parity = parity;

  dslashParam.kernel_type = INTERIOR_KERNEL;
  dslashParam.tOffset = 0;
  dslashParam.tMul = 1;
  dslashParam.threads = volume;

#ifdef MULTI_GPU
  // wait for any previous outstanding dslashes to finish
  cudaStreamWaitEvent(0, dslashEnd, 0);

  // Gather from source spinor
  for(int dir = 3; dir >=0; dir--){ // count down for Wilson
    if (!dslashParam.commDim[dir]) continue;
    face->exchangeFacesStart(*inSpinor, 1-parity, dagger, dir, streams);
  }
#endif

  
  int block_xy = blockDim[0].x*blockDim[0].y;
  if (block_xy % 32 != 0) block_xy = ((block_xy / 32) + 1)*32;
  int block = block_xy*blockDim[0].z;
    
  int shared_bytes = block*(DSLASH_SHARED_FLOATS_PER_THREAD*regSize + SHARED_COORDS);
  dslash.apply(blockDim[0], gridDim[0], shared_bytes, streams[Nstream-1]); // stream 0 or 8

#ifdef MULTI_GPU

  for (int i=3; i>=0; i--) { // count down for Wilson
    if (!dslashParam.commDim[i]) continue;

    // Finish gather and start comms
    face->exchangeFacesComms(i);
    
    // Wait for comms to finish, and scatter into the end zone
    face->exchangeFacesWait(*inSpinor, dagger, i);

    // Record the end of the scattering
    cudaEventRecord(scatterEvent[2*i], streams[2*i]);
    cudaEventRecord(scatterEvent[2*i+1], streams[2*i+1]);
  }

  for (int i=3; i>=0; i--) { // count down for Wilson
    if (!dslashParam.commDim[i]) continue;

    shared_bytes = blockDim[i+1].x*(DSLASH_SHARED_FLOATS_PER_THREAD*regSize + SHARED_COORDS);
    
    //cudaStreamSynchronize(streams[2*i]);
    //cudaStreamSynchronize(streams[2*i + 1]);
    
    dslashParam.kernel_type = static_cast<KernelType>(i);
    //dslashParam.tOffset = dims[i]-2; // is this redundant?
    dslashParam.threads = 2*faceVolumeCB[i]; // updating 2 faces

    // wait for scattering to finish and then launch dslash
    cudaStreamWaitEvent(streams[Nstream-1], scatterEvent[2*i], 0);
    cudaStreamWaitEvent(streams[Nstream-1], scatterEvent[2*i+1], 0);
    dslash.apply(blockDim[i+1], gridDim[i+1], shared_bytes, streams[Nstream-1]); // all faces use this stream
  }

  cudaEventRecord(dslashEnd, streams[Nstream-1]);
  //cudaStreamSynchronize(streams[Nstream-1]);

#endif // MULTI_GPU
}

// Wilson wrappers
void wilsonDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const cudaColorSpinorField *in,
		      const int parity, const int dagger, const cudaColorSpinorField *x,
		      const double &k, const dim3 *blockDim, const dim3 *gridDim, const int *commOverride) {

  inSpinor = (cudaColorSpinorField*)in; // EVIL

#ifdef GPU_WILSON_DIRAC
  int Npad = (in->Ncolor()*in->Nspin()*2)/in->FieldOrder(); // SPINOR_HOP in old code
  for(int i=0;i<4;i++){
    dslashParam.ghostDim[i] = commDimPartitioned(i); // determines whether to use regular or ghost indexing at boundary
    dslashParam.ghostOffset[i] = Npad*(in->GhostOffset(i) + in->Stride());
    dslashParam.ghostNormOffset[i] = in->GhostNormOffset(i) + in->Stride();
    dslashParam.commDim[i] = (!commOverride[i]) ? 0 : commDimPartitioned(i); // switch off comms if override = 0
  }

  void *gauge0, *gauge1;
  bindGaugeTex(gauge, parity, &gauge0, &gauge1);

  if (in->Precision() != gauge.Precision())
    errorQuda("Mixing gauge and spinor precision not supported");

  const void *xv = (x ? x->V() : 0);
  const void *xn = (x ? x->Norm() : 0);

  DslashCuda *dslash = 0;
  size_t regSize = sizeof(float);
  if (in->Precision() == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    dslash = new WilsonDslashCuda<double2, double2>((double2*)out->V(), (float*)out->Norm(), (double2*)gauge0, (double2*)gauge1, 
						    gauge.Reconstruct(), (double2*)in->V(), (float*)in->Norm(), 
						    (double2*)xv, (float*)xn, k, dagger, in->Bytes(), in->NormBytes());
    regSize = sizeof(double);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->Precision() == QUDA_SINGLE_PRECISION) {
    dslash = new WilsonDslashCuda<float4, float4>((float4*)out->V(), (float*)out->Norm(), (float4*)gauge0, (float4*)gauge1,
						  gauge.Reconstruct(), (float4*)in->V(), (float*)in->Norm(), 
						  (float4*)xv, (float*)xn, k, dagger, in->Bytes(), in->NormBytes());
  } else if (in->Precision() == QUDA_HALF_PRECISION) {
    dslash = new WilsonDslashCuda<short4, short4>((short4*)out->V(), (float*)out->Norm(), (short4*)gauge0, (short4*)gauge1,
						  gauge.Reconstruct(), (short4*)in->V(), (float*)in->Norm(),
						  (short4*)xv, (float*)xn, k, dagger, in->Bytes(), in->NormBytes());
  }
  dslashCuda(*dslash, regSize, parity, dagger, in->Volume(), 
	     in->GhostFace(), blockDim, gridDim);

  delete dslash;
  unbindGaugeTex(gauge);

  if (!dslashTuning) checkCudaError();
#else
  errorQuda("Wilson dslash has not been built");
#endif // GPU_WILSON_DIRAC

}

void cloverDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const FullClover cloverInv,
		      const cudaColorSpinorField *in, const int parity, const int dagger, 
		      const cudaColorSpinorField *x, const double &a,
		      const dim3 *blockDim, const dim3 *gridDim, const int *commOverride) {

  inSpinor = (cudaColorSpinorField*)in; // EVIL

#ifdef GPU_CLOVER_DIRAC
  int Npad = (in->Ncolor()*in->Nspin()*2)/in->FieldOrder(); // SPINOR_HOP in old code
  for(int i=0;i<4;i++){
    dslashParam.ghostDim[i] = commDimPartitioned(i); // determines whether to use regular or ghost indexing at boundary
    dslashParam.ghostOffset[i] = Npad*(in->GhostOffset(i) + in->Stride());
    dslashParam.ghostNormOffset[i] = in->GhostNormOffset(i) + in->Stride();
    dslashParam.commDim[i] = (!commOverride[i]) ? 0 : commDimPartitioned(i); // switch off comms if override = 0
  }

  void *cloverP, *cloverNormP;
  QudaPrecision clover_prec = bindCloverTex(cloverInv, parity, &cloverP, &cloverNormP);

  void *gauge0, *gauge1;
  bindGaugeTex(gauge, parity, &gauge0, &gauge1);

  if (in->Precision() != gauge.Precision())
    errorQuda("Mixing gauge and spinor precision not supported");

  if (in->Precision() != clover_prec)
    errorQuda("Mixing clover and spinor precision not supported");

  const void *xv = x ? x->V() : 0;
  const void *xn = x ? x->Norm() : 0;

  DslashCuda *dslash = 0;
  size_t regSize = sizeof(float);

  if (in->Precision() == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    dslash = new CloverDslashCuda<double2, double2, double2>((double2*)out->V(), (float*)out->Norm(), (double2*)gauge0, 
							     (double2*)gauge1, gauge.Reconstruct(), (double2*)cloverP, 
							     (float*)cloverNormP, (double2*)in->V(), (float*)in->Norm(),
							     (double2*)xv, (float*)xn, a, dagger, in->Bytes(), in->NormBytes());
    regSize = sizeof(double);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->Precision() == QUDA_SINGLE_PRECISION) {
    dslash = new CloverDslashCuda<float4, float4, float4>((float4*)out->V(), (float*)out->Norm(), (float4*)gauge0, 
							  (float4*)gauge1, gauge.Reconstruct(), (float4*)cloverP, 
							  (float*)cloverNormP, (float4*)in->V(), (float*)in->Norm(), 
							  (float4*)xv, (float*)xn, a, dagger, in->Bytes(), in->NormBytes());
  } else if (in->Precision() == QUDA_HALF_PRECISION) {
    dslash = new CloverDslashCuda<short4, short4, short4>((short4*)out->V(), (float*)out->Norm(), (short4*)gauge0, 
							  (short4*)gauge1, gauge.Reconstruct(), (short4*)cloverP, 
							  (float*)cloverNormP, (short4*)in->V(), (float*)in->Norm(), 
							  (short4*)xv, (float*)xn, a, dagger, in->Bytes(), in->NormBytes());
  }

  dslashCuda(*dslash, regSize, parity, dagger, in->Volume(), 
	     in->GhostFace(), blockDim, gridDim);

  delete dslash;
  unbindGaugeTex(gauge);
  unbindCloverTex(cloverInv);

  if (!dslashTuning) checkCudaError();
#else
  errorQuda("Clover dslash has not been built");
#endif

}


void twistedMassDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, 
			   const cudaColorSpinorField *in, const int parity, const int dagger, 
			   const cudaColorSpinorField *x, const double &kappa, const double &mu, 
			   const double &a, const dim3 *blockDim, const dim3 *gridDim, const int *commOverride) {

  inSpinor = (cudaColorSpinorField*)in; // EVIL

#ifdef GPU_TWISTED_MASS_DIRAC
  int Npad = (in->Ncolor()*in->Nspin()*2)/in->FieldOrder(); // SPINOR_HOP in old code
  for(int i=0;i<4;i++){
    dslashParam.ghostDim[i] = commDimPartitioned(i); // determines whether to use regular or ghost indexing at boundary
    dslashParam.ghostOffset[i] = Npad*(in->GhostOffset(i) + in->Stride());
    dslashParam.ghostNormOffset[i] = in->GhostNormOffset(i) + in->Stride();
    dslashParam.commDim[i] = (!commOverride[i]) ? 0 : commDimPartitioned(i); // switch off comms if override = 0
  }

  void *gauge0, *gauge1;
  bindGaugeTex(gauge, parity, &gauge0, &gauge1);

  if (in->Precision() != gauge.Precision())
    errorQuda("Mixing gauge and spinor precision not supported");

  const void *xv = x ? x->V() : 0;
  const void *xn = x ? x->Norm() : 0;

  DslashCuda *dslash = 0;
  size_t regSize = sizeof(float);

  if (in->Precision() == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    dslash = new TwistedDslashCuda<double2,double2>((double2*)out->V(), (float*)out->Norm(), (double2*)gauge0, 
						    (double2*)gauge1, gauge.Reconstruct(), (double2*)in->V(), 
						    (float*)in->Norm(), (double2*)xv, (float*)xn, 
						    kappa, mu, a, dagger, in->Bytes(), in->NormBytes());
    regSize = sizeof(double);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->Precision() == QUDA_SINGLE_PRECISION) {
    dslash = new TwistedDslashCuda<float4,float4>((float4*)out->V(), (float*)out->Norm(), (float4*)gauge0, (float4*)gauge1, 
						  gauge.Reconstruct(), (float4*)in->V(), (float*)in->Norm(), 
						  (float4*)xv, (float*)xn, kappa, mu, a, dagger, in->Bytes(), in->NormBytes());
  } else if (in->Precision() == QUDA_HALF_PRECISION) {
    dslash = new TwistedDslashCuda<short4,short4>((short4*)out->V(), (float*)out->Norm(), (short4*)gauge0, (short4*)gauge1, 
						  gauge.Reconstruct(), (short4*)in->V(), (float*)in->Norm(), 
						  (short4*)xv, (float*)xn, kappa, mu, a, dagger, in->Bytes(), in->NormBytes());
    
  }

  dslashCuda(*dslash, regSize, parity, dagger, in->Volume(), 
	     in->GhostFace(), blockDim, gridDim);

  delete dslash;
  unbindGaugeTex(gauge);

  if (!dslashTuning) checkCudaError();
#else
  errorQuda("Twisted mass dslash has not been built");
#endif

}

void domainWallDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, 
			  const cudaColorSpinorField *in, const int parity, const int dagger, 
			  const cudaColorSpinorField *x, const double &m_f, const double &k2,
			  const dim3 *blockDim, const dim3 *gridDim) {

  inSpinor = (cudaColorSpinorField*)in; // EVIL

#ifdef MULTI_GPU
  errorQuda("Multi-GPU domain wall not implemented\n");
#endif

  dslashParam.parity = parity;
  dslashParam.threads = in->Volume();

#ifdef GPU_DOMAIN_WALL_DIRAC
  void *gauge0, *gauge1;
  bindGaugeTex(gauge, parity, &gauge0, &gauge1);

  if (in->Precision() != gauge.Precision())
    errorQuda("Mixing gauge and spinor precision not supported");

  const void *xv = x ? x->V() : 0;
  const void *xn = x ? x->Norm() : 0;

  DslashCuda *dslash = 0;
  size_t regSize = sizeof(float);

  if (in->Precision() == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    dslash = new DomainWallDslashCuda<double2,double2>((double2*)out->V(), (float*)out->Norm(), (double2*)gauge0, (double2*)gauge1, 
						       gauge.Reconstruct(), (double2*)in->V(), (float*)in->Norm(), (double2*)xv, 
						       (float*)xn, m_f, k2, dagger, in->Bytes(), in->NormBytes());
    regSize = sizeof(double);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->Precision() == QUDA_SINGLE_PRECISION) {
    dslash = new DomainWallDslashCuda<float4,float4>((float4*)out->V(), (float*)out->Norm(), (float4*)gauge0, (float4*)gauge1, 
						     gauge.Reconstruct(), (float4*)in->V(), (float*)in->Norm(), (float4*)xv, 
						     (float*)xn, m_f, k2, dagger, in->Bytes(), in->NormBytes());
  } else if (in->Precision() == QUDA_HALF_PRECISION) {
    dslash = new DomainWallDslashCuda<short4,short4>((short4*)out->V(), (float*)out->Norm(), (short4*)gauge0, (short4*)gauge1, 
						     gauge.Reconstruct(), (short4*)in->V(), (float*)in->Norm(), (short4*)xv, 
						     (float*)xn, m_f, k2, dagger, in->Bytes(), in->NormBytes());
  }

  dslashCuda(*dslash, regSize, parity, dagger, in->Volume(), 
	     in->GhostFace(), blockDim, gridDim);

  delete dslash;
  unbindGaugeTex(gauge);

  if (!dslashTuning) checkCudaError();
#else
  errorQuda("Domain wall dslash has not been built");
#endif

}


template <typename spinorFloat, typename fatGaugeFloat, typename longGaugeFloat>
void staggeredDslashCuda(spinorFloat *out, float *outNorm, const fatGaugeFloat *fatGauge0, const fatGaugeFloat *fatGauge1, 
			   const longGaugeFloat* longGauge0, const longGaugeFloat* longGauge1, 
			   const QudaReconstructType reconstruct, const spinorFloat *in, const float *inNorm,
			   const int parity, const int dagger, const spinorFloat *x, const float *xNorm, 
			   const double &a, const int volume, const int* Vsh, const int* dims,
			 const int length, const int ghost_length, const dim3 *blockDim) {
    
  // calculate grid based on 4d volume
  dim3 interiorGridDim( (dslashParam.threads + blockDim[0].x -1)/blockDim[0].x, 1, 1);
  dim3 exteriorGridDim[4]  = {
    dim3((6*Vsh[0] + blockDim[1].x -1)/blockDim[1].x, 1, 1),
    dim3((6*Vsh[1] + blockDim[2].x -1)/blockDim[2].x, 1, 1),
    dim3((6*Vsh[2] + blockDim[3].x -1)/blockDim[3].x, 1, 1),
    dim3((6*Vsh[3] + blockDim[4].x -1)/blockDim[4].x, 1, 1)
  };

  // number of sources is set by the fifth dimension
  const int Nsrc = dims[4];  

#ifdef MULTI_GPU
  if (Nsrc > 1) errorQuda("Multi-source dslash does not yet support multi-gpu");
#endif

#ifdef PARALLEL_DIR    
  int blockz = 2;
#else
  int blockz = 1;
#endif

  dim3 blockDim2d(blockDim[0].x, Nsrc, blockz);

  size_t regSize = bindSpinorTex_mg(length, ghost_length, in, inNorm, x, xNorm);
  int shared_bytes = blockDim[0].x*6*regSize*Nsrc;
  
  dslashParam.kernel_type = INTERIOR_KERNEL;

#ifdef MULTI_GPU
  // wait for any previous outstanding dslashes to finish
  cudaStreamWaitEvent(0, dslashEnd, 0);

  // Gather from source spinor
  for(int dir = 3; dir >=0 ; dir--){
    if (!dslashParam.commDim[dir]) continue;
    face->exchangeFacesStart(*inSpinor, 1-parity, dagger, dir, streams);
  }
#endif

  /*  printf("Launching (%d, %d, %d) threads on (%d, %d, %d) grid\n", 
	 blockDim2d.x, blockDim2d.y, blockDim2d.z, 
	 interiorGridDim.x, interiorGridDim.y, interiorGridDim.z);*/

  STAGGERED_DSLASH(interiorGridDim, blockDim2d, shared_bytes, streams[Nstream-1], dslashParam,
		   out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, x, xNorm, a); 

  //if (!dslashTuning) checkCudaError();

#ifdef MULTI_GPU

  for(int i=3 ;i >= 0;i--){
    if (!dslashParam.commDim[i]) continue;

    // Finish gather and start comms
    face->exchangeFacesComms(i);
    // Wait for comms to finish, and scatter into the end zone
    face->exchangeFacesWait(*inSpinor, dagger,i);    

    // Record the end of the scattering
    cudaEventRecord(scatterEvent[2*i], streams[2*i]);
    cudaEventRecord(scatterEvent[2*i+1], streams[2*i+1]);
  }

  for(int i=3 ;i >= 0;i--){
    if(!dslashParam.commDim[i]) continue;

    shared_bytes = blockDim[i+1].x*6*regSize;

    //cudaStreamSynchronize(streams[2*i]);
    //cudaStreamSynchronize(streams[2*i + 1]);
    dslashParam.kernel_type = static_cast<KernelType>(i);
    dslashParam.tOffset =  dims[i]-6;
    dslashParam.threads = 6*Vsh[i];
    cudaStreamWaitEvent(streams[Nstream-1], scatterEvent[2*i], 0);
    cudaStreamWaitEvent(streams[Nstream-1], scatterEvent[2*i+1], 0);
    STAGGERED_DSLASH(exteriorGridDim[i], blockDim[i+1], shared_bytes, streams[Nstream-1], dslashParam,
		     out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, x, xNorm, a);
    if (!dslashTuning) checkCudaError();
  }

  cudaEventRecord(dslashEnd, streams[Nstream-1]);
  //cudaStreamSynchronize(streams[Nstream-1]);

#endif
}

void staggeredDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &fatGauge, 
			 const cudaGaugeField &longGauge, const cudaColorSpinorField *in,
			 const int parity, const int dagger, const cudaColorSpinorField *x,
			 const double &k, const dim3 *block, const dim3 *grid, const int *commOverride)
{
  
  inSpinor = (cudaColorSpinorField*)in; // EVIL

#ifdef GPU_STAGGERED_DIRAC

  dslashParam.parity = parity;
  dslashParam.threads = in->Volume() / in->X(4); // only the 4d volume is used here (grid dims + thread exit)
  
  int Npad = (in->Ncolor()*in->Nspin()*2)/in->FieldOrder(); // SPINOR_HOP in old code
  for(int i=0;i<4;i++){
    dslashParam.ghostDim[i] = commDimPartitioned(i); // determines whether to use regular or ghost indexing at boundary
    dslashParam.ghostOffset[i] = Npad*(in->GhostOffset(i) + in->Stride());
    dslashParam.ghostNormOffset[i] = in->GhostNormOffset(i) + in->Stride();
    dslashParam.commDim[i] = (!commOverride[i]) ? 0 : commDimPartitioned(i); // switch off comms if override = 0
  }
  void *fatGauge0, *fatGauge1;
  void* longGauge0, *longGauge1;
  bindFatGaugeTex(fatGauge, parity, &fatGauge0, &fatGauge1);
  bindLongGaugeTex(longGauge, parity, &longGauge0, &longGauge1);
  
  if (in->Precision() != fatGauge.Precision() || in->Precision() != longGauge.Precision()){
    errorQuda("Mixing gauge and spinor precision not supported"
	      "(precision=%d, fatlinkGauge.precision=%d, longGauge.precision=%d",
	      in->Precision(), fatGauge.Precision(), longGauge.Precision());
  }
  
  const void *xv = x ? x->V() : 0;
  const void *xn = x ? x->Norm() : 0;
  
  if (in->Precision() == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    staggeredDslashCuda((double2*)out->V(), (float*)out->Norm(), (double2*)fatGauge0, (double2*)fatGauge1,
			(double2*)longGauge0, (double2*)longGauge1, longGauge.Reconstruct(), 
			(double2*)in->V(), (float*)in->Norm(), parity, dagger, 
			(double2*)xv, (float*)x, k, in->Volume(), in->GhostFace(), 
			in->X(), in->Length(), in->GhostLength(), block);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->Precision() == QUDA_SINGLE_PRECISION) {
    staggeredDslashCuda((float2*)out->V(), (float*)out->Norm(), (float2*)fatGauge0, (float2*)fatGauge1,
			  (float4*)longGauge0, (float4*)longGauge1, longGauge.Reconstruct(), 
			(float2*)in->V(), (float*)in->Norm(), parity, dagger, 
			(float2*)xv, (float*)xn, k, in->Volume(), in->GhostFace(), 
			in->X(), in->Length(), in->GhostLength(), block);
  } else if (in->Precision() == QUDA_HALF_PRECISION) {	
    staggeredDslashCuda((short2*)out->V(), (float*)out->Norm(), (short2*)fatGauge0, (short2*)fatGauge1,
			(short4*)longGauge0, (short4*)longGauge1, longGauge.Reconstruct(), 
			(short2*)in->V(), (float*)in->Norm(), parity, dagger, 
			(short2*)xv, (float*)xn, k, in->Volume(), in->GhostFace(), 
			in->X(), in->Length(), in->GhostLength(), block);
  }
    
  if (!dslashTuning) checkCudaError();

#else
  errorQuda("Staggered dslash has not been built");
#endif  
}


template <typename spinorFloat, typename cloverFloat>
void cloverCuda(spinorFloat *out, float *outNorm, const cloverFloat *clover,
		const float *cloverNorm, const spinorFloat *in, const float *inNorm, 
		const size_t bytes, const size_t norm_bytes, const dim3 blockDim, const dim3 gridDim)
{
  //dim3 gridDim( (dslashParam.threads+blockDim.x-1) / blockDim.x, 1, 1);

  int shared_bytes = blockDim.x*CLOVER_SHARED_FLOATS_PER_THREAD*bindSpinorTex(bytes, norm_bytes, in, inNorm);
  cloverKernel<<<gridDim, blockDim, shared_bytes>>> 
    (out, outNorm, clover, cloverNorm, in, inNorm, dslashParam);
  unbindSpinorTex(in, inNorm);
}

void cloverCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const FullClover clover, 
		const cudaColorSpinorField *in, const int parity, const dim3 &blockDim, const dim3 &gridDim) {

  dslashParam.parity = parity;
  dslashParam.tOffset = 0;
  dslashParam.tMul = 1;
  dslashParam.threads = in->Volume();

#ifdef GPU_CLOVER_DIRAC
  void *cloverP, *cloverNormP;
  QudaPrecision clover_prec = bindCloverTex(clover, parity, &cloverP, &cloverNormP);

  if (in->Precision() != clover_prec)
    errorQuda("Mixing clover and spinor precision not supported");

  if (in->Precision() == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    cloverCuda((double2*)out->V(), (float*)out->Norm(), (double2*)cloverP, 
	       (float*)cloverNormP, (double2*)in->V(), (float*)in->Norm(), 
	       in->Bytes(), in->NormBytes(), blockDim, gridDim);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->Precision() == QUDA_SINGLE_PRECISION) {
    cloverCuda((float4*)out->V(), (float*)out->Norm(), (float4*)cloverP, 
	       (float*)cloverNormP, (float4*)in->V(), (float*)in->Norm(),
	       in->Bytes(), in->NormBytes(), blockDim, gridDim);
  } else if (in->Precision() == QUDA_HALF_PRECISION) {
    cloverCuda((short4*)out->V(), (float*)out->Norm(), (short4*)cloverP, 
	       (float*)cloverNormP, (short4*)in->V(), (float*)in->Norm(), 
	       in->Bytes(), in->NormBytes(), blockDim, gridDim);
  }
  unbindCloverTex(clover);

  if (!dslashTuning) checkCudaError();
#else
  errorQuda("Clover dslash has not been built");
#endif

}
// FIXME: twist kernel cannot be issued asynchronously because of texture unbinding
template <typename spinorFloat>
void twistGamma5Cuda(spinorFloat *out, float *outNorm, const spinorFloat *in, 
		     const float *inNorm, const int dagger, const double &kappa, 
		     const double &mu, const size_t bytes, const size_t norm_bytes, 
		     const QudaTwistGamma5Type twist, dim3 blockDim, dim3 gridDim)
{
  //  dim3 gridDim( (dslashParam.threads+blockDim.x-1) / blockDim.x, 1, 1);

  double a=0.0, b=0.0;
  setTwistParam(a, b, kappa, mu, dagger, twist);

  bindSpinorTex(bytes, norm_bytes, in, inNorm);
  twistGamma5Kernel<<<gridDim, blockDim, 0>>> (out, outNorm, a, b, dslashParam);
  unbindSpinorTex(in, inNorm);
}

void twistGamma5Cuda(cudaColorSpinorField *out, const cudaColorSpinorField *in,
		     const int dagger, const double &kappa, const double &mu,
		     const QudaTwistGamma5Type twist, const dim3 &block, const dim3 &grid) {

  dslashParam.tOffset = 0;
  dslashParam.tMul = 1;
  dslashParam.threads = in->Volume();

#ifdef GPU_TWISTED_MASS_DIRAC
  if (in->Precision() == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
<<<<<<< HEAD
    twistGamma5Cuda((double2*)out->V(), (float*)out->Norm(), 
		    (double2*)in->V(), (float*)in->Norm(), 
		    dagger, kappa, mu, in->Bytes(), 
		    in->NormBytes(), twist, block, grid);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->Precision() == QUDA_SINGLE_PRECISION) {
    twistGamma5Cuda((float4*)out->V(), (float*)out->Norm(),
		    (float4*)in->V(), (float*)in->Norm(), 
		    dagger, kappa, mu, in->Bytes(), 
		    in->NormBytes(), twist, block, grid);
  } else if (in->Precision() == QUDA_HALF_PRECISION) {
    twistGamma5Cuda((short4*)out->V(), (float*)out->Norm(),
		    (short4*)in->V(), (float*)in->Norm(), 
		    dagger, kappa, mu, in->Bytes(), 
		    in->NormBytes(), twist, block, grid);
  }
  if (!dslashTuning) checkCudaError();
#else
  errorQuda("Twisted mass dslash has not been built");
#endif // GPU_TWISTED_MASS_DIRAC
}


#include "misc_helpers.cu"


#if defined(GPU_FATLINK)||defined(GPU_GAUGE_FORCE)|| defined(GPU_FERMION_FORCE)
#include <force_common.h>
#include "force_kernel_common.cu"
#endif

#ifdef GPU_FATLINK
#include "llfat_quda.cu"
#endif

#ifdef GPU_GAUGE_FORCE
#include "gauge_force_quda.cu"
#endif

#ifdef GPU_FERMION_FORCE
#include "fermion_force_quda.cu"
#endif
