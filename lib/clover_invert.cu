#include <tune_quda.h>
#include <clover_field_order.h>
#include <complex_quda.h>

namespace quda {

  template <typename Clover>
  struct CloverInvertArg {
    const Clover clover;
    Clover inverse;
    double *trlogA;
//extra attributes for twisted mass clover
    bool twist;
    double mu2;
    CloverInvertArg(Clover &inverse, const Clover &clover) :
      inverse(inverse), clover(clover), twist(clover.Twisted()), mu2(clover.Mu2()){ }
  };

  /**
     Use a Cholesky decomposition to invert the clover matrix
     Here we use an inplace inversion which hopefully reduces register pressure
   */
  // FIXME - compute the trlog in this kernel
  template <typename Float, typename Clover>
  __device__ __host__ void cloverInvertCompute(CloverInvertArg<Clover> arg, int x, int parity) {

    Float A[72];
    double trlogA = 0.0; // fixme - write this out

    // load the clover term into memory
    arg.clover.load(A, x, parity);

    for (int ch=0; ch<2; ch++) {

      Float diag[6];
      Float tmp[6]; // temporary storage
      complex<Float> tri[15];

      // hack into the right order as MILC just to copy algorithm directly
      // FIXME use native ordering in the Cholseky 
      // factor of two is inherent to QUDA clover storage
      for (int i=0; i<6; i++) diag[i] = 2.0*A[ch*36+i];
      for (int i=0; i<2; i++) tri[i] = complex<Float>(2.0*A[ch*36+6+2*i], 2.0*A[ch*36+6+2*i+1]);
      tri[2] = complex<Float>(2.0*A[ch*36+6+2*5], 2.0*A[ch*36+6+2*5+1]);
      tri[3] = complex<Float>(2.0*A[ch*36+6+2*2], 2.0*A[ch*36+6+2*2+1]);
      tri[4] = complex<Float>(2.0*A[ch*36+6+2*6], 2.0*A[ch*36+6+2*6+1]);
      tri[5] = complex<Float>(2.0*A[ch*36+6+2*9], 2.0*A[ch*36+6+2*9+1]);
      tri[6] = complex<Float>(2.0*A[ch*36+6+2*3], 2.0*A[ch*36+6+2*3+1]);
      tri[7] = complex<Float>(2.0*A[ch*36+6+2*7], 2.0*A[ch*36+6+2*7+1]);
      tri[8] = complex<Float>(2.0*A[ch*36+6+2*10], 2.0*A[ch*36+6+2*10+1]);
      tri[9] = complex<Float>(2.0*A[ch*36+6+2*12], 2.0*A[ch*36+6+2*12+1]);
      tri[10] = complex<Float>(2.0*A[ch*36+6+2*4], 2.0*A[ch*36+6+2*4+1]);
      tri[11] = complex<Float>(2.0*A[ch*36+6+2*8], 2.0*A[ch*36+6+2*8+1]);
      tri[12] = complex<Float>(2.0*A[ch*36+6+2*11], 2.0*A[ch*36+6+2*11+1]);
      for (int i=13; i<15; i++) tri[i] = complex<Float>(2.0*A[ch*36+6+2*i], 2.0*A[ch*36+6+2*i+1]);
//Compute (T^2 + mu2) first, then invert (not optimized!):
      if(arg.twist)
      {
         //complex<Float> aux[15];//hmmm, better to reuse A-regs...
         //another solution just to define (but compiler may not be happy with this, swapping everything in
         //the global buffer):
         complex<Float>* aux = (complex<Float>*)A[ch*36];
         //compute off-diagonal terms:
//
         aux[ 0] = tri[0]*diag[0]+diag[1]*tri[0]+conj(tri[2])*tri[1]+conj(tri[4])*tri[3]+conj(tri[7])*tri[6]+conj(tri[11])*tri[10];
//
         aux[ 1] = tri[1]*diag[0]+diag[2]*tri[1]+tri[2]*tri[0]+conj(tri[5])*tri[3]+conj(tri[8])*tri[6]+conj(tri[12])*tri[10];

         aux[ 2] = tri[2]*diag[1]+diag[2]*tri[2]+tri[1]*conj(tri[0])+conj(tri[5])*tri[4]+conj(tri[8])*tri[7]+conj(tri[12])*tri[11];
//
         aux[ 3] = tri[3]*diag[0]+diag[3]*tri[3]+tri[4]*tri[0]+tri[5]*tri[1]+conj(tri[9])*tri[6]+conj(tri[13])*tri[10];

         aux[ 4] = tri[4]*diag[1]+diag[3]*tri[4]+tri[3]*conj(tri[0])+tri[5]*tri[2]+conj(tri[9])*tri[7]+conj(tri[13])*tri[11];

         aux[ 5] = tri[5]*diag[2]+diag[3]*tri[5]+tri[3]*conj(tri[1])+tri[4]*conj(tri[2])+conj(tri[9])*tri[8]+conj(tri[13])*tri[12];
//
         aux[ 6] = tri[6]*diag[0]+diag[4]*tri[6]+tri[7]*tri[0]+tri[8]*tri[1]+tri[9]*tri[3]+conj(tri[14])*tri[10];

         aux[ 7] = tri[7]*diag[1]+diag[4]*tri[7]+tri[6]*conj(tri[0])+tri[8]*tri[2]+tri[9]*tri[4]+conj(tri[14])*tri[11];

         aux[ 8] = tri[8]*diag[2]+diag[4]*tri[8]+tri[6]*conj(tri[1])+tri[7]*conj(tri[2])+tri[9]*tri[5]+conj(tri[14])*tri[12];

         aux[ 9] = tri[9]*diag[3]+diag[4]*tri[9]+tri[6]*conj(tri[3])+tri[7]*conj(tri[4])+tri[8]*conj(tri[5])+conj(tri[14])*tri[13];
//
         aux[10] = tri[10]*diag[0]+diag[5]*tri[10]+tri[11]*tri[0]+tri[12]*tri[1]+tri[13]*tri[3]+tri[14]*tri[6];

         aux[11] = tri[11]*diag[1]+diag[5]*tri[11]+tri[10]*conj(tri[0])+tri[12]*tri[2]+tri[13]*tri[4]+tri[14]*tri[7];

         aux[12] = tri[12]*diag[2]+diag[5]*tri[12]+tri[10]*conj(tri[1])+tri[11]*conj(tri[2])+tri[13]*tri[5]+tri[14]*tri[8];

         aux[13] = tri[13]*diag[3]+diag[5]*tri[13]+tri[10]*conj(tri[3])+tri[11]*conj(tri[4])+tri[12]*conj(tri[5])+tri[14]*tri[9];

         aux[14] = tri[14]*diag[4]+diag[5]*tri[14]+tri[10]*conj(tri[6])+tri[11]*conj(tri[7])+tri[12]*conj(tri[8])+tri[13]*conj(tri[9]);


         //update diagonal elements:
         diag[0] = (Float)arg.mu2+diag[0]*diag[0]+norm(tri[ 0])+norm(tri[ 1])+norm(tri[ 3])+norm(tri[ 6])+norm(tri[10]);
         diag[1] = (Float)arg.mu2+diag[1]*diag[1]+norm(tri[ 0])+norm(tri[ 2])+norm(tri[ 4])+norm(tri[ 7])+norm(tri[11]); 
         diag[2] = (Float)arg.mu2+diag[2]*diag[2]+norm(tri[ 1])+norm(tri[ 2])+norm(tri[ 5])+norm(tri[ 8])+norm(tri[12]); 
         diag[3] = (Float)arg.mu2+diag[3]*diag[3]+norm(tri[ 3])+norm(tri[ 4])+norm(tri[ 5])+norm(tri[ 9])+norm(tri[13]); 
         diag[4] = (Float)arg.mu2+diag[4]*diag[4]+norm(tri[ 6])+norm(tri[ 7])+norm(tri[ 8])+norm(tri[ 9])+norm(tri[14]);
         diag[5] = (Float)arg.mu2+diag[5]*diag[5]+norm(tri[10])+norm(tri[11])+norm(tri[12])+norm(tri[13])+norm(tri[14]);

        //update off-diagonal elements:
         for(int i = 0; i < 15; i++) tri[i] = aux[i];
      }
//
      for (int j=0; j<6; j++) {
	diag[j] = sqrt(diag[j]);
	tmp[j] = 1.0 / diag[j];

	for (int k=j+1; k<6; k++) {
	  int kj = k*(k-1)/2+j;
	  tri[kj] *= tmp[j];
	}

	for(int k=j+1;k<6;k++){
	  int kj=k*(k-1)/2+j;
	  diag[k] -= (tri[kj] * conj(tri[kj])).real();
	  for(int l=k+1;l<6;l++){
	    int lj=l*(l-1)/2+j;
	    int lk=l*(l-1)/2+k;
	    tri[lk] -= tri[lj] * conj(tri[kj]);
	  }
	}	
      }
      
      /* Accumulate trlogA */
      for (int j=0;j<6;j++) trlogA += (double)2.0*log((double)(diag[j]));

      /* Now use forward and backward substitution to construct inverse */
      complex<Float> v1[6];
      for (int k=0;k<6;k++) {
	for(int l=0;l<k;l++) v1[l] = complex<Float>(0.0, 0.0);

	/* Forward substitute */
	v1[k] = complex<Float>(tmp[k], 0.0);
	for(int l=k+1;l<6;l++){
	  complex<Float> sum = complex<Float>(0.0, 0.0);
	  for(int j=k;j<l;j++){
	    int lj=l*(l-1)/2+j;		    
	    sum -= tri[lj] * v1[j];
	  }
	  v1[l] = sum * tmp[l];
	}
	
	/* Backward substitute */
	v1[5] = v1[5] * tmp[5];
	for(int l=4;l>=k;l--){
	  complex<Float> sum = v1[l];
	  for(int j=l+1;j<6;j++){
	    int jl=j*(j-1)/2+l;
	    sum -= conj(tri[jl]) * v1[j];
	  }
	  v1[l] = sum * tmp[l];
	}
	
	/* Overwrite column k */
	diag[k] = v1[k].real();
	for(int l=k+1;l<6;l++){
	  int lk=l*(l-1)/2+k;
	  tri[lk] = v1[l];
	}
      }

      for (int i=0; i<6; i++) A[ch*36+i] = 0.5 * diag[i];
      for (int i=0; i<2; i++) {
	A[ch*36+6+2*i] = 0.5 * tri[i].real(); A[ch*36+6+2*i+1] = 0.5 * tri[i].imag();
      }
      A[ch*36+6+2*5] = 0.5 * tri[2].real(); A[ch*36+6+2*5+1] = 0.5 * tri[2].imag();
      A[ch*36+6+2*2] = 0.5 * tri[3].real(); A[ch*36+6+2*2+1] = 0.5 * tri[3].imag();
      A[ch*36+6+2*6] = 0.5 * tri[4].real(); A[ch*36+6+2*6+1] = 0.5 * tri[4].imag();
      A[ch*36+6+2*9] = 0.5 * tri[5].real(); A[ch*36+6+2*9+1] = 0.5 * tri[5].imag();
      A[ch*36+6+2*3] = 0.5 * tri[6].real(); A[ch*36+6+2*3+1] = 0.5 * tri[6].imag();
      A[ch*36+6+2*7] = 0.5 * tri[7].real(); A[ch*36+6+2*7+1] = 0.5 * tri[7].imag();
      A[ch*36+6+2*10] = 0.5 * tri[8].real(); A[ch*36+6+2*10+1] = 0.5 * tri[8].imag();
      A[ch*36+6+2*12] = 0.5 * tri[9].real(); A[ch*36+6+2*12+1] = 0.5 * tri[9].imag();
      A[ch*36+6+2*4] = 0.5 * tri[10].real(); A[ch*36+6+2*4+1] = 0.5 * tri[10].imag();
      A[ch*36+6+2*8] = 0.5 * tri[11].real(); A[ch*36+6+2*8+1] = 0.5 * tri[11].imag();
      A[ch*36+6+2*11] = 0.5 * tri[12].real(); A[ch*36+6+2*11+1] = 0.5 * tri[12].imag();

      for (int i=13; i<15; i++) {
	A[ch*36+6+2*i] = 0.5 * tri[i].real(); A[ch*36+6+2*i+1] = 0.5 * tri[i].imag();
      }
    }	     

    // save the inverted matrix
    arg.inverse.save(A, x, parity);
  }

  template <typename Float, typename Clover>
  void cloverInvert(CloverInvertArg<Clover> arg) {  
    for (int parity=0; parity<2; parity++) {
      for (int x=0; x<arg.clover.volumeCB; x++) {
	cloverInvertCompute<Float>(arg, x, parity);
      }
    }
  }

  template <typename Float, typename Clover>
  __global__ void cloverInvertKernel(CloverInvertArg<Clover> arg) {  
    int idx = blockIdx.x*blockDim.x + threadIdx.x;
    if (idx >= 2*arg.clover.volumeCB) return;
    int parity = (idx >= arg.clover.volumeCB) ? 1 : 0;
    idx -= parity*arg.clover.volumeCB;
    
    cloverInvertCompute<Float>(arg, idx, parity);
  }

  template <typename Float, typename Clover>
  class CloverInvert : Tunable {
    CloverInvertArg<Clover> arg;
    const QudaFieldLocation location;

  private:
    unsigned int sharedBytesPerThread() const { return 0; }
    unsigned int sharedBytesPerBlock(const TuneParam &param) const { return 0 ;}

    bool tuneGridDim() const { return false; } // Don't tune the grid dimensions.
    unsigned int minThreads() const { return 2*arg.clover.volumeCB; }

  public:
    CloverInvert(CloverInvertArg<Clover> &arg, QudaFieldLocation location) 
      : arg(arg), location(location) { ; }
    virtual ~CloverInvert() { ; }
  
    void apply(const cudaStream_t &stream) {
      TuneParam tp = tuneLaunch(*this, getTuning(), getVerbosity());
      if (location == QUDA_CUDA_FIELD_LOCATION) {
	cloverInvertKernel<Float, Clover> <<<tp.grid, tp.block, tp.shared_bytes, stream>>>(arg);
      } else {
	cloverInvert<Float, Clover>(arg);
      }
    }

    TuneKey tuneKey() const {
      std::stringstream vol, aux;
      vol << arg.clover.volumeCB; 
      aux << "stride=" << arg.clover.stride;
      return TuneKey(vol.str(), typeid(*this).name(), aux.str());
    }

    std::string paramString(const TuneParam &param) const { // Don't bother printing the grid dim.
      std::stringstream ps;
      ps << "block=(" << param.block.x << "," << param.block.y << "," << param.block.z << "), ";
      ps << "shared=" << param.shared_bytes;
      return ps.str();
    }

    long long flops() const { return 0; } 
    long long bytes() const { return 2*arg.clover.volumeCB*(arg.inverse.Bytes() + arg.clover.Bytes()); } 
  };

  template <typename Float, typename Clover>
  void cloverInvert(Clover inverse, const Clover clover, QudaFieldLocation location) {
    CloverInvertArg<Clover> arg(inverse, clover);
    CloverInvert<Float,Clover> invert(arg, location);
    invert.apply(0);
  }

  template <typename Float>
 void cloverInvert(const CloverField &clover, QudaFieldLocation location) {
    if (clover.Order() == QUDA_FLOAT2_CLOVER_ORDER) {
      cloverInvert<Float>(FloatNOrder<Float,72,2>(clover, 1), 
			  FloatNOrder<Float,72,2>(clover, 0), location);
    } else if (clover.Order() == QUDA_FLOAT4_CLOVER_ORDER) {
      cloverInvert<Float>(FloatNOrder<Float,72,4>(clover, 1), 
			  FloatNOrder<Float,72,4>(clover, 0), location);
    } else if (clover.Order() == QUDA_PACKED_CLOVER_ORDER) {
      cloverInvert<Float>(QDPOrder<Float,72>(clover, 1), 
			  QDPOrder<Float,72>(clover, 0), location);
    } else if (clover.Order() == QUDA_QDPJIT_CLOVER_ORDER) {

#ifdef BUILD_QDPJIT_INTERFACE
      cloverInvert<Float>(QDPJITOrder<Float,72>(clover, 1), 
			  QDPJITOrder<Float,72>(clover, 0), location);
#else
      errorQuda("QDPJIT interface has not been built\n");
#endif

    } else if (clover.Order() == QUDA_BQCD_CLOVER_ORDER) {
      errorQuda("BQCD output not supported");
    } else {
      errorQuda("Clover field %d order not supported", clover.Order());
    }

  }

  // this is the function that is actually called, from here on down we instantiate all required templates
  void cloverInvert(CloverField &clover, QudaFieldLocation location) {
    if (clover.Precision() == QUDA_HALF_PRECISION && clover.Order() > 4) 
      errorQuda("Half precision not supported for order %d", clover.Order());
//ok, currently we clover is overwritten... so actually must be cloverInv...
    if (clover.Precision() == QUDA_DOUBLE_PRECISION) {
      cloverInvert<double>(clover, location);
    } else if (clover.Precision() == QUDA_SINGLE_PRECISION) {
      cloverInvert<float>(clover, location);
    } else {
      errorQuda("Precision %d not supported", clover.Precision());
    }
  }

} // namespace quda
