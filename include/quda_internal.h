#ifndef _QUDA_INTERNAL_H
#define _QUDA_INTERNAL_H

#include <cuda.h>
#include <cuda_runtime.h>

#include "quda_mem.h"

#ifdef QMP_COMMS
#include <qmp.h>
#endif

#define MAX_SHORT 32767.0f

// The "Quda" prefix is added to avoid collisions with other libraries.

#define GaugeFieldOrder QudaGaugeFieldOrder
#define DiracFieldOrder QudaDiracFieldOrder
#define CloverFieldOrder QudaCloverFieldOrder
#define InverterType QudaInverterType  
//#define Precision QudaPrecision
#define MatPCType QudaMatPCType
#define SolutionType QudaSolutionType
#define MassNormalization QudaMassNormalization
#define PreserveSource QudaPreserveSource
#define DagType QudaDagType
#define TEX_ALIGN_REQ (512*2) //Fermi, factor 2 comes from even/odd
#define ALIGNMENT_ADJUST(n) ( (n+TEX_ALIGN_REQ-1)/TEX_ALIGN_REQ*TEX_ALIGN_REQ)
#include <enum_quda.h>
#include <quda.h>
#include <util_quda.h>

#ifdef __cplusplus
extern "C" {
#endif
  
  typedef void *ParityGauge;

  // replace below with ColorSpinorField
  typedef struct {
    size_t bytes;
    QudaPrecision precision;
    int length; // total length
    int volume; // geometric volume (single parity)
    int X[QUDA_MAX_DIM]; // the geometric lengths (single parity)
    int Nc; // length of color dimension
    int Ns; // length of spin dimension
    void *data; // either (double2*), (float4 *) or (short4 *), depending on precision
    float *dataNorm; // used only when precision is QUDA_HALF_PRECISION
  } ParityHw;
  
  typedef struct {
    ParityHw odd;
    ParityHw even;
  } FullHw;

  extern cudaDeviceProp deviceProp;  
  extern cudaStream_t *streams;
  
#ifdef __cplusplus
}
#endif

#ifdef MULTI_GPU
  const int Nstream = 9;
#else
  const int Nstream = 1;
#endif

class TuneParam {

 public:
  dim3 block;
  int shared_bytes;

  TuneParam() : block(32, 1, 1), shared_bytes(0) { ; }
  TuneParam(const TuneParam &param) : block(param.block), shared_bytes(param.shared_bytes) { ; }
  TuneParam& operator=(const TuneParam &param) {
    if (&param != this) {
      block = param.block;
      shared_bytes = param.shared_bytes;
    }
    return *this;
  }

};

#endif // _QUDA_INTERNAL_H
