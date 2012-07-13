#ifndef _DSLASH_QUDA_H
#define _DSLASH_QUDA_H

#include <quda_internal.h>
#include <tune_quda.h>
#include <face_quda.h>
#include <gauge_field.h>

void setFace(const FaceBuffer &face);

#ifdef __cplusplus
extern "C" {
#endif

  void setDslashTuning(QudaTune tune, QudaVerbosity verbose);

#ifdef DSLASH_PROFILING
  void printDslashProfile();
#endif

#ifdef __cplusplus
}
#endif

bool getDslashLaunch();

void createDslashEvents();
void destroyDslashEvents();

void initLatticeConstants(const LatticeField &lat);
void initGaugeConstants(const cudaGaugeField &gauge);
void initSpinorConstants(const cudaColorSpinorField &spinor);
void initDslashConstants();
void initCloverConstants (const cudaCloverField &clover);
void initStaggeredConstants(const cudaGaugeField &fatgauge, const cudaGaugeField &longgauge);
//!NDEGTM New:
void initTwistedMassConstants(const int flv_stride);

// plain Wilson Dslash  
void wilsonDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const cudaColorSpinorField *in,
		      const int oddBit, const int daggerBit, const cudaColorSpinorField *x,
		      const double &k, const int *commDim);

// clover Dslash
void cloverDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, 
		      const FullClover cloverInv, const cudaColorSpinorField *in, 
		      const int oddBit, const int daggerBit, const cudaColorSpinorField *x,
		      const double &k, const int *commDim);

// clover Dslash
void asymCloverDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, 
			  const FullClover cloverInv, const cudaColorSpinorField *in, 
			  const int oddBit, const int daggerBit, const cudaColorSpinorField *x,
			  const double &k, const int *commDim);

// solo clover term
void cloverCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const FullClover clover, 
		const cudaColorSpinorField *in, const int oddBit);

// domain wall Dslash  
void domainWallDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const cudaColorSpinorField *in, 
			  const int parity, const int dagger, const cudaColorSpinorField *x, 
			  const double &m_f, const double &k, const int *commDim);//!NEW:extra argument			  

// staggered Dslash    
void staggeredDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &fatGauge, const cudaGaugeField &longGauge,
			 const cudaColorSpinorField *in, const int parity, const int dagger, 
			 const cudaColorSpinorField *x, const double &k, 
			 const int *commDim);

// twisted mass Dslash  
void twistedMassDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const cudaColorSpinorField *in,
			   const int parity, const int dagger, const cudaColorSpinorField *x, 
			   const double &kappa, const double &mu, const double &a, const int *commDim);

// solo twist term
void twistGamma5Cuda(cudaColorSpinorField *out, const cudaColorSpinorField *in,
		     const int dagger, const double &kappa, const double &mu,
		     const QudaTwistGamma5Type);
		     
//!NDEGTM NEW
// twisted mass Dslash  
void twistedMassDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const cudaColorSpinorField *in, const int parity, const int dagger, const cudaColorSpinorField *x, const double &kappa, const double &mu, const double &epsilon, const int *commDim);
//!NDEGTM NEW
// solo twist term
void twistGamma5Cuda(cudaColorSpinorField *out, const cudaColorSpinorField *in, const int dagger, const double &kappa, const double &mu, const double &epsilon, const QudaTwistGamma5Type);		     
				     

// face packing routines
void packFace(void *ghost_buf, cudaColorSpinorField &in, const int dim, const int dagger, 
	      const int parity, const cudaStream_t &stream);
//BEGIN NEW	
//currently as a separate function (can be integrated into packFace)
void packFaceDW(void *ghost_buf, cudaColorSpinorField &in, const int dim, const int dagger, 
		    const int parity, const cudaStream_t &stream);
//!NEW: added for ndeg tm action		    
void packFaceNdegTM(void *ghost_buf, cudaColorSpinorField &in, const int dim, const int dagger, 
		    const int parity, const cudaStream_t &stream);		    
//END NEW	      

#endif // _DSLASH_QUDA_H
