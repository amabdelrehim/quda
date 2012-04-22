#include <complex>
#include <stdlib.h>
#include <stdio.h>

#include <short.h>

//#include <wilson_dslash_reference.h>
#include <test_util.h>

#define XUP 0
#define YUP 1
#define ZUP 2
#define TUP 3

using namespace std;

extern int Z[4];
extern int Vh;
extern int V;

template <typename Float>
static void printVector(Float *v) {
  printf("{(%f %f) (%f %f) (%f %f)}\n", v[0], v[1], v[2], v[3], v[4], v[5]);
}

// X indexes the lattice site
void printSpinorElement(void *spinor, int X, QudaPrecision precision) {
  if (precision == QUDA_DOUBLE_PRECISION)
    for (int s=0; s<4; s++) printVector((double*)spinor+X*24+s*6);
  else
    for (int s=0; s<4; s++) printVector((float*)spinor+X*24+s*6);
}

// X indexes the full lattice
void printGaugeElement(void *gauge, int X, QudaPrecision precision) {
  if (getOddBit(X) == 0) {
    if (precision == QUDA_DOUBLE_PRECISION)
      for (int m=0; m<3; m++) printVector((double*)gauge +(X/2)*gaugeSiteSize + m*3*2);
    else
      for (int m=0; m<3; m++) printVector((float*)gauge +(X/2)*gaugeSiteSize + m*3*2);
      
  } else {
    if (precision == QUDA_DOUBLE_PRECISION)
      for (int m = 0; m < 3; m++) printVector((double*)gauge + (X/2+Vh)*gaugeSiteSize + m*3*2);
    else
      for (int m = 0; m < 3; m++) printVector((float*)gauge + (X/2+Vh)*gaugeSiteSize + m*3*2);
  }
}

// returns 0 or 1 if the full lattice index X is even or odd
int getOddBit(int Y) {
  int x4 = Y/(Z[2]*Z[1]*Z[0]);
  int x3 = (Y/(Z[1]*Z[0])) % Z[2];
  int x2 = (Y/Z[0]) % Z[1];
  int x1 = Y % Z[0];
  return (x4+x3+x2+x1) % 2;
}

// a+=b
template <typename Float>
inline void complexAddTo(Float *a, Float *b) {
  a[0] += b[0];
  a[1] += b[1];
}

// a = b*c
template <typename Float>
inline void complexProduct(Float *a, Float *b, Float *c) {
    a[0] = b[0]*c[0] - b[1]*c[1];
    a[1] = b[0]*c[1] + b[1]*c[0];
}

// a = conj(b)*conj(c)
template <typename Float>
inline void complexConjugateProduct(Float *a, Float *b, Float *c) {
    a[0] = b[0]*c[0] - b[1]*c[1];
    a[1] = -b[0]*c[1] - b[1]*c[0];
}

// a = conj(b)*c
template <typename Float>
inline void complexDotProduct(Float *a, Float *b, Float *c) {
    a[0] = b[0]*c[0] + b[1]*c[1];
    a[1] = b[0]*c[1] - b[1]*c[0];
}

// a += b*c
template <typename Float>
inline void accumulateComplexProduct(Float *a, Float *b, Float *c, Float sign) {
  a[0] += sign*(b[0]*c[0] - b[1]*c[1]);
  a[1] += sign*(b[0]*c[1] + b[1]*c[0]);
}

// a += conj(b)*c)
template <typename Float>
inline void accumulateComplexDotProduct(Float *a, Float *b, Float *c) {
    a[0] += b[0]*c[0] + b[1]*c[1];
    a[1] += b[0]*c[1] - b[1]*c[0];
}

template <typename Float>
inline void accumulateConjugateProduct(Float *a, Float *b, Float *c, int sign) {
  a[0] += sign * (b[0]*c[0] - b[1]*c[1]);
  a[1] -= sign * (b[0]*c[1] + b[1]*c[0]);
}

template <typename Float>
inline void su3Construct12(Float *mat) {
  Float *w = mat+12;
  w[0] = 0.0;
  w[1] = 0.0;
  w[2] = 0.0;
  w[3] = 0.0;
  w[4] = 0.0;
  w[5] = 0.0;
}

// Stabilized Bunk and Sommer
template <typename Float>
inline void su3Construct8(Float *mat) {
  mat[0] = atan2(mat[1], mat[0]);
  mat[1] = atan2(mat[13], mat[12]);
  for (int i=8; i<18; i++) mat[i] = 0.0;
}

void su3_construct(void *mat, QudaReconstructType reconstruct, QudaPrecision precision) {
  if (reconstruct == QUDA_RECONSTRUCT_12) {
    if (precision == QUDA_DOUBLE_PRECISION) su3Construct12((double*)mat);
    else su3Construct12((float*)mat);
  } else {
    if (precision == QUDA_DOUBLE_PRECISION) su3Construct8((double*)mat);
    else su3Construct8((float*)mat);
  }
}

// given first two rows (u,v) of SU(3) matrix mat, reconstruct the third row
// as the cross product of the conjugate vectors: w = u* x v*
// 
// 48 flops
template <typename Float>
static void su3Reconstruct12(Float *mat, int dir, int ga_idx, QudaGaugeParam *param) {
  Float *u = &mat[0*(3*2)];
  Float *v = &mat[1*(3*2)];
  Float *w = &mat[2*(3*2)];
  w[0] = 0.0; w[1] = 0.0; w[2] = 0.0; w[3] = 0.0; w[4] = 0.0; w[5] = 0.0;
  accumulateConjugateProduct(w+0*(2), u+1*(2), v+2*(2), +1);
  accumulateConjugateProduct(w+0*(2), u+2*(2), v+1*(2), -1);
  accumulateConjugateProduct(w+1*(2), u+2*(2), v+0*(2), +1);
  accumulateConjugateProduct(w+1*(2), u+0*(2), v+2*(2), -1);
  accumulateConjugateProduct(w+2*(2), u+0*(2), v+1*(2), +1);
  accumulateConjugateProduct(w+2*(2), u+1*(2), v+0*(2), -1);
  Float u0 = (dir < 3 ? param->anisotropy :
	      (ga_idx >= (Z[3]-1)*Z[0]*Z[1]*Z[2]/2 ? param->t_boundary : 1));
  w[0]*=u0; w[1]*=u0; w[2]*=u0; w[3]*=u0; w[4]*=u0; w[5]*=u0;
}

template <typename Float>
static void su3Reconstruct8(Float *mat, int dir, int ga_idx, QudaGaugeParam *param) {
  // First reconstruct first row
  Float row_sum = 0.0;
  row_sum += mat[2]*mat[2];
  row_sum += mat[3]*mat[3];
  row_sum += mat[4]*mat[4];
  row_sum += mat[5]*mat[5];
  Float u0 = (dir < 3 ? param->anisotropy :
	      (ga_idx >= (Z[3]-1)*Z[0]*Z[1]*Z[2]/2 ? param->t_boundary : 1));
  Float U00_mag = sqrt(1.f/(u0*u0) - row_sum);

  mat[14] = mat[0];
  mat[15] = mat[1];

  mat[0] = U00_mag * cos(mat[14]);
  mat[1] = U00_mag * sin(mat[14]);

  Float column_sum = 0.0;
  for (int i=0; i<2; i++) column_sum += mat[i]*mat[i];
  for (int i=6; i<8; i++) column_sum += mat[i]*mat[i];
  Float U20_mag = sqrt(1.f/(u0*u0) - column_sum);

  mat[12] = U20_mag * cos(mat[15]);
  mat[13] = U20_mag * sin(mat[15]);

  // First column now restored

  // finally reconstruct last elements from SU(2) rotation
  Float r_inv2 = 1.0/(u0*row_sum);

  // U11
  Float A[2];
  complexDotProduct(A, mat+0, mat+6);
  complexConjugateProduct(mat+8, mat+12, mat+4);
  accumulateComplexProduct(mat+8, A, mat+2, u0);
  mat[8] *= -r_inv2;
  mat[9] *= -r_inv2;

  // U12
  complexConjugateProduct(mat+10, mat+12, mat+2);
  accumulateComplexProduct(mat+10, A, mat+4, -u0);
  mat[10] *= r_inv2;
  mat[11] *= r_inv2;

  // U21
  complexDotProduct(A, mat+0, mat+12);
  complexConjugateProduct(mat+14, mat+6, mat+4);
  accumulateComplexProduct(mat+14, A, mat+2, -u0);
  mat[14] *= r_inv2;
  mat[15] *= r_inv2;

  // U12
  complexConjugateProduct(mat+16, mat+6, mat+2);
  accumulateComplexProduct(mat+16, A, mat+4, u0);
  mat[16] *= -r_inv2;
  mat[17] *= -r_inv2;
}

void su3_reconstruct(void *mat, int dir, int ga_idx, QudaReconstructType reconstruct, QudaPrecision precision, QudaGaugeParam *param) {
  if (reconstruct == QUDA_RECONSTRUCT_12) {
    if (precision == QUDA_DOUBLE_PRECISION) su3Reconstruct12((double*)mat, dir, ga_idx, param);
    else su3Reconstruct12((float*)mat, dir, ga_idx, param);
  } else {
    if (precision == QUDA_DOUBLE_PRECISION) su3Reconstruct8((double*)mat, dir, ga_idx, param);
    else su3Reconstruct8((float*)mat, dir, ga_idx, param);
  }
}

/*
void su3_construct_8_half(float *mat, short *mat_half) {
  su3Construct8(mat);

  mat_half[0] = floatToShort(mat[0] / M_PI);
  mat_half[1] = floatToShort(mat[1] / M_PI);
  for (int i=2; i<18; i++) {
    mat_half[i] = floatToShort(mat[i]);
  }
}

void su3_reconstruct_8_half(float *mat, short *mat_half, int dir, int ga_idx, QudaGaugeParam *param) {

  for (int i=0; i<18; i++) {
    mat[i] = shortToFloat(mat_half[i]);
  }
  mat[0] *= M_PI;
  mat[1] *= M_PI;

  su3Reconstruct8(mat, dir, ga_idx, param);
  }*/

template <typename Float>
static int compareFloats(Float *a, Float *b, int len, double epsilon) {
  for (int i = 0; i < len; i++) {
    double diff = fabs(a[i] - b[i]);
    if (diff > epsilon) {
      printf("error: i=%d, a[%d]=%f, b[%d]=%f\n", i, i, a[i], i, b[i]);
      return 0;
    }
  }
  return 1;
}

int compare_floats(void *a, void *b, int len, double epsilon, QudaPrecision precision) {
  if  (precision == QUDA_DOUBLE_PRECISION) return compareFloats((double*)a, (double*)b, len, epsilon);
  else return compareFloats((float*)a, (float*)b, len, epsilon);
}



// given a "half index" i into either an even or odd half lattice (corresponding
// to oddBit = {0, 1}), returns the corresponding full lattice index.
int fullLatticeIndex(int i, int oddBit) {
  int boundaryCrossings = i/(Z[0]/2) + i/(Z[1]*Z[0]/2) + i/(Z[2]*Z[1]*Z[0]/2);
  return 2*i + (boundaryCrossings + oddBit) % 2;
}


// i represents a "half index" into an even or odd "half lattice".
// when oddBit={0,1} the half lattice is {even,odd}.
// 
// the displacements, such as dx, refer to the full lattice coordinates. 
//
// neighborIndex() takes a "half index", displaces it, and returns the
// new "half index", which can be an index into either the even or odd lattices.
// displacements of magnitude one always interchange odd and even lattices.
//

int neighborIndex(int i, int oddBit, int dx4, int dx3, int dx2, int dx1) {
  int Y = fullLatticeIndex(i, oddBit);
  int x4 = Y/(Z[2]*Z[1]*Z[0]);
  int x3 = (Y/(Z[1]*Z[0])) % Z[2];
  int x2 = (Y/Z[0]) % Z[1];
  int x1 = Y % Z[0];
  // assert (oddBit == (x+y+z+t)%2);
  
  x4 = (x4+dx4+Z[3]) % Z[3];
  x3 = (x3+dx3+Z[2]) % Z[2];
  x2 = (x2+dx2+Z[1]) % Z[1];
  x1 = (x1+dx1+Z[0]) % Z[0];
 
  return (x4*(Z[2]*Z[1]*Z[0]) + x3*(Z[1]*Z[0]) + x2*(Z[0]) + x1) / 2;
}

/*  
 * This is a computation of neighbor using the full index and the displacement in each direction
 *
 */

int
neighborIndexFullLattice(int i, int dx4, int dx3, int dx2, int dx1) 
{
    int oddBit = 0;
    int half_idx = i;
    if (i >= Vh){
	oddBit =1;
	half_idx = i - Vh;
    }
    
    int nbr_half_idx = neighborIndex(half_idx, oddBit, dx4,dx3,dx2,dx1);
    int oddBitChanged = (dx4+dx3+dx2+dx1)%2;
    if (oddBitChanged){
	oddBit = 1 - oddBit;
    }
    int ret = nbr_half_idx;
    if (oddBit){
	ret = Vh + nbr_half_idx;
    }
    
    return ret;
}

// 4d checkerboard.
// given a "half index" i into either an even or odd half lattice (corresponding
// to oddBit = {0, 1}), returns the corresponding full lattice index.
// Cf. GPGPU code in dslash_core_ante.h.
// There, i is the thread index.
int fullLatticeIndex_4d(int i, int oddBit) {
  if (i >= Vh || i < 0) {printf("i out of range in fullLatticeIndex_4d"); exit(-1);}
  int boundaryCrossings = i/(Z[0]/2) + i/(Z[1]*Z[0]/2) + i/(Z[2]*Z[1]*Z[0]/2);
  return 2*i + (boundaryCrossings + oddBit) % 2;
}

// 5d checkerboard.
// given a "half index" i into either an even or odd half lattice (corresponding
// to oddBit = {0, 1}), returns the corresponding full lattice index.
// Cf. GPGPU code in dslash_core_ante.h.
// There, i is the thread index sid.
// This function is used by neighborIndex_5d in dslash_reference.cpp.
//ok
int fullLatticeIndex_5d(int i, int oddBit) {
  int boundaryCrossings = i/(Z[0]/2) + i/(Z[1]*Z[0]/2) + i/(Z[2]*Z[1]*Z[0]/2) + i/(Z[3]*Z[2]*Z[1]*Z[0]/2);
  return 2*i + (boundaryCrossings + oddBit) % 2;
}

template <typename Float>
static void applyGaugeFieldScaling(Float **gauge, int Vh, QudaGaugeParam *param) {
  // Apply spatial scaling factor (u0) to spatial links
  for (int d = 0; d < 3; d++) {
    for (int i = 0; i < gaugeSiteSize*Vh*2; i++) {
      gauge[d][i] /= param->anisotropy;
    }
  }
    
  // Apply boundary conditions to temporal links
  if (param->t_boundary == QUDA_ANTI_PERIODIC_T) {
    for (int j = (Z[0]/2)*Z[1]*Z[2]*(Z[3]-1); j < Vh; j++) {
      for (int i = 0; i < gaugeSiteSize; i++) {
	gauge[3][j*gaugeSiteSize+i] *= -1.0;
	gauge[3][(Vh+j)*gaugeSiteSize+i] *= -1.0;
      }
    }
  }
    
  if (param->gauge_fix) {
    // set all gauge links (except for the first Z[0]*Z[1]*Z[2]/2) to the identity,
    // to simulate fixing to the temporal gauge.
    int dir = 3; // time direction only
    Float *even = gauge[dir];
    Float *odd  = gauge[dir]+Vh*gaugeSiteSize;
    for (int i = Z[0]*Z[1]*Z[2]/2; i < Vh; i++) {
      for (int m = 0; m < 3; m++) {
	for (int n = 0; n < 3; n++) {
	  even[i*(3*3*2) + m*(3*2) + n*(2) + 0] = (m==n) ? 1 : 0;
	  even[i*(3*3*2) + m*(3*2) + n*(2) + 1] = 0.0;
	  odd [i*(3*3*2) + m*(3*2) + n*(2) + 0] = (m==n) ? 1 : 0;
	  odd [i*(3*3*2) + m*(3*2) + n*(2) + 1] = 0.0;
	}
      }
    }
  }
}

template <typename Float>
void applyGaugeFieldScaling_long(Float **gauge, int Vh, QudaGaugeParam *param)
{

    int X1h=param->X[0]/2;
    int X1 =param->X[0];
    int X2 =param->X[1];
    int X3 =param->X[2];
    int X4 =param->X[3];

    // rescale long links by the appropriate coefficient
    for(int d=0; d<4; d++){
        for(int i=0; i < V*gaugeSiteSize; i++){
            gauge[d][i] /= (-24*param->tadpole_coeff*param->tadpole_coeff);
        }
    }

    // apply the staggered phases
    for (int d = 0; d < 3; d++) {

        //even
        for (int i = 0; i < Vh; i++) {

            int index = fullLatticeIndex(i, 0);
            int i4 = index /(X3*X2*X1);
            int i3 = (index - i4*(X3*X2*X1))/(X2*X1);
            int i2 = (index - i4*(X3*X2*X1) - i3*(X2*X1))/X1;
            int i1 = index - i4*(X3*X2*X1) - i3*(X2*X1) - i2*X1;
            int sign=1;

            if (d == 0) {
                if (i4 % 2 == 1){
                    sign= -1;
                }
            }

            if (d == 1){
                if ((i4+i1) % 2 == 1){
                    sign= -1;
                }
            }
            if (d == 2){
                if ( (i4+i1+i2) % 2 == 1){
                    sign= -1;
                }
            }

            for (int j=0;j < 6; j++){
                gauge[d][i*gaugeSiteSize + 12+ j] *= sign;
            }
        }
        //odd
        for (int i = 0; i < Vh; i++) {
            int index = fullLatticeIndex(i, 1);
            int i4 = index /(X3*X2*X1);
            int i3 = (index - i4*(X3*X2*X1))/(X2*X1);
            int i2 = (index - i4*(X3*X2*X1) - i3*(X2*X1))/X1;
            int i1 = index - i4*(X3*X2*X1) - i3*(X2*X1) - i2*X1;
            int sign=1;

            if (d == 0) {
                if (i4 % 2 == 1){
                    sign= -1;
                }
            }

            if (d == 1){
                if ((i4+i1) % 2 == 1){
                    sign= -1;
                }
            }
            if (d == 2){
                if ( (i4+i1+i2) % 2 == 1){
                    sign = -1;
                }
            }

            for (int j=0;j < 6; j++){
                gauge[d][(Vh+i)*gaugeSiteSize + 12 + j] *= sign;
            }
        }

    }

    // Apply boundary conditions to temporal links
    if (param->t_boundary == QUDA_ANTI_PERIODIC_T) {
        for (int j = 0; j < Vh; j++) {
            int sign =1;
            if (j >= (X4-3)*X1h*X2*X3 ){
                sign= -1;
            }

            for (int i = 0; i < 6; i++) {
                gauge[3][j*gaugeSiteSize+ 12+ i ] *= sign;
                gauge[3][(Vh+j)*gaugeSiteSize+12 +i] *= sign;
            }
        }
    }
}



template <typename Float>
static void constructUnitGaugeField(Float **res, QudaGaugeParam *param) {
  Float *resOdd[4], *resEven[4];
  for (int dir = 0; dir < 4; dir++) {  
    resEven[dir] = res[dir];
    resOdd[dir]  = res[dir]+Vh*gaugeSiteSize;
  }
    
  for (int dir = 0; dir < 4; dir++) {
    for (int i = 0; i < Vh; i++) {
      for (int m = 0; m < 3; m++) {
	for (int n = 0; n < 3; n++) {
	  resEven[dir][i*(3*3*2) + m*(3*2) + n*(2) + 0] = (m==n) ? 1 : 0;
	  resEven[dir][i*(3*3*2) + m*(3*2) + n*(2) + 1] = 0.0;
	  resOdd[dir][i*(3*3*2) + m*(3*2) + n*(2) + 0] = (m==n) ? 1 : 0;
	  resOdd[dir][i*(3*3*2) + m*(3*2) + n*(2) + 1] = 0.0;
	}
      }
    }
  }
    
  applyGaugeFieldScaling(res, Vh, param);
}

// normalize the vector a
template <typename Float>
static void normalize(complex<Float> *a, int len) {
  double sum = 0.0;
  for (int i=0; i<len; i++) sum += norm(a[i]);
  for (int i=0; i<len; i++) a[i] /= sqrt(sum);
}

// orthogonalize vector b to vector a
template <typename Float>
static void orthogonalize(complex<Float> *a, complex<Float> *b, int len) {
  complex<double> dot = 0.0;
  for (int i=0; i<len; i++) dot += conj(a[i])*b[i];
  for (int i=0; i<len; i++) b[i] -= (complex<Float>)dot*a[i];
}

template <typename Float> 
static void constructGaugeField(Float **res, QudaGaugeParam *param) {
  Float *resOdd[4], *resEven[4];
  for (int dir = 0; dir < 4; dir++) {  
    resEven[dir] = res[dir];
    resOdd[dir]  = res[dir]+Vh*gaugeSiteSize;
  }
    
  for (int dir = 0; dir < 4; dir++) {
    for (int i = 0; i < Vh; i++) {
      for (int m = 1; m < 3; m++) { // last 2 rows
	for (int n = 0; n < 3; n++) { // 3 columns
	  resEven[dir][i*(3*3*2) + m*(3*2) + n*(2) + 0] = rand() / (Float)RAND_MAX;
	  resEven[dir][i*(3*3*2) + m*(3*2) + n*(2) + 1] = rand() / (Float)RAND_MAX;
	  resOdd[dir][i*(3*3*2) + m*(3*2) + n*(2) + 0] = rand() / (Float)RAND_MAX;
	  resOdd[dir][i*(3*3*2) + m*(3*2) + n*(2) + 1] = rand() / (Float)RAND_MAX;                    
	}
      }
      normalize((complex<Float>*)(resEven[dir] + (i*3+1)*3*2), 3);
      orthogonalize((complex<Float>*)(resEven[dir] + (i*3+1)*3*2), (complex<Float>*)(resEven[dir] + (i*3+2)*3*2), 3);
      normalize((complex<Float>*)(resEven[dir] + (i*3 + 2)*3*2), 3);
      
      normalize((complex<Float>*)(resOdd[dir] + (i*3+1)*3*2), 3);
      orthogonalize((complex<Float>*)(resOdd[dir] + (i*3+1)*3*2), (complex<Float>*)(resOdd[dir] + (i*3+2)*3*2), 3);
      normalize((complex<Float>*)(resOdd[dir] + (i*3 + 2)*3*2), 3);

      {
	Float *w = resEven[dir]+(i*3+0)*3*2;
	Float *u = resEven[dir]+(i*3+1)*3*2;
	Float *v = resEven[dir]+(i*3+2)*3*2;
	
	for (int n = 0; n < 6; n++) w[n] = 0.0;
	accumulateConjugateProduct(w+0*(2), u+1*(2), v+2*(2), +1);
	accumulateConjugateProduct(w+0*(2), u+2*(2), v+1*(2), -1);
	accumulateConjugateProduct(w+1*(2), u+2*(2), v+0*(2), +1);
	accumulateConjugateProduct(w+1*(2), u+0*(2), v+2*(2), -1);
	accumulateConjugateProduct(w+2*(2), u+0*(2), v+1*(2), +1);
	accumulateConjugateProduct(w+2*(2), u+1*(2), v+0*(2), -1);
      }

      {
	Float *w = resOdd[dir]+(i*3+0)*3*2;
	Float *u = resOdd[dir]+(i*3+1)*3*2;
	Float *v = resOdd[dir]+(i*3+2)*3*2;
	
	for (int n = 0; n < 6; n++) w[n] = 0.0;
	accumulateConjugateProduct(w+0*(2), u+1*(2), v+2*(2), +1);
	accumulateConjugateProduct(w+0*(2), u+2*(2), v+1*(2), -1);
	accumulateConjugateProduct(w+1*(2), u+2*(2), v+0*(2), +1);
	accumulateConjugateProduct(w+1*(2), u+0*(2), v+2*(2), -1);
	accumulateConjugateProduct(w+2*(2), u+0*(2), v+1*(2), +1);
	accumulateConjugateProduct(w+2*(2), u+1*(2), v+0*(2), -1);
      }

    }
  }
  if (param->type == QUDA_WILSON_LINKS){  
      applyGaugeFieldScaling(res, Vh, param);
  }else if (param->type == QUDA_ASQTAD_LONG_LINKS){
      applyGaugeFieldScaling_long(res, Vh, param);      
  }
  
  
}

template <typename Float> 
void constructUnitaryGaugeField(Float **res) 
{
    Float *resOdd[4], *resEven[4];
    for (int dir = 0; dir < 4; dir++) {  
	resEven[dir] = res[dir];
	resOdd[dir]  = res[dir]+Vh*gaugeSiteSize;
    }
    
    for (int dir = 0; dir < 4; dir++) {
	for (int i = 0; i < Vh; i++) {
	    for (int m = 1; m < 3; m++) { // last 2 rows
		for (int n = 0; n < 3; n++) { // 3 columns
		    resEven[dir][i*(3*3*2) + m*(3*2) + n*(2) + 0] = rand() / (Float)RAND_MAX;
		    resEven[dir][i*(3*3*2) + m*(3*2) + n*(2) + 1] = rand() / (Float)RAND_MAX;
		    resOdd[dir][i*(3*3*2) + m*(3*2) + n*(2) + 0] = rand() / (Float)RAND_MAX;
		    resOdd[dir][i*(3*3*2) + m*(3*2) + n*(2) + 1] = rand() / (Float)RAND_MAX;                    
		}
	    }
	    normalize((complex<Float>*)(resEven[dir] + (i*3+1)*3*2), 3);
	    orthogonalize((complex<Float>*)(resEven[dir] + (i*3+1)*3*2), (complex<Float>*)(resEven[dir] + (i*3+2)*3*2), 3);
	    normalize((complex<Float>*)(resEven[dir] + (i*3 + 2)*3*2), 3);
      
	    normalize((complex<Float>*)(resOdd[dir] + (i*3+1)*3*2), 3);
	    orthogonalize((complex<Float>*)(resOdd[dir] + (i*3+1)*3*2), (complex<Float>*)(resOdd[dir] + (i*3+2)*3*2), 3);
	    normalize((complex<Float>*)(resOdd[dir] + (i*3 + 2)*3*2), 3);

	    {
		Float *w = resEven[dir]+(i*3+0)*3*2;
		Float *u = resEven[dir]+(i*3+1)*3*2;
		Float *v = resEven[dir]+(i*3+2)*3*2;
	
		for (int n = 0; n < 6; n++) w[n] = 0.0;
		accumulateConjugateProduct(w+0*(2), u+1*(2), v+2*(2), +1);
		accumulateConjugateProduct(w+0*(2), u+2*(2), v+1*(2), -1);
		accumulateConjugateProduct(w+1*(2), u+2*(2), v+0*(2), +1);
		accumulateConjugateProduct(w+1*(2), u+0*(2), v+2*(2), -1);
		accumulateConjugateProduct(w+2*(2), u+0*(2), v+1*(2), +1);
		accumulateConjugateProduct(w+2*(2), u+1*(2), v+0*(2), -1);
	    }

	    {
		Float *w = resOdd[dir]+(i*3+0)*3*2;
		Float *u = resOdd[dir]+(i*3+1)*3*2;
		Float *v = resOdd[dir]+(i*3+2)*3*2;
	
		for (int n = 0; n < 6; n++) w[n] = 0.0;
		accumulateConjugateProduct(w+0*(2), u+1*(2), v+2*(2), +1);
		accumulateConjugateProduct(w+0*(2), u+2*(2), v+1*(2), -1);
		accumulateConjugateProduct(w+1*(2), u+2*(2), v+0*(2), +1);
		accumulateConjugateProduct(w+1*(2), u+0*(2), v+2*(2), -1);
		accumulateConjugateProduct(w+2*(2), u+0*(2), v+1*(2), +1);
		accumulateConjugateProduct(w+2*(2), u+1*(2), v+0*(2), -1);
	    }

	}
    }
}


void construct_gauge_field(void **gauge, int type, QudaPrecision precision, QudaGaugeParam *param) {
  if (type == 0) {
    if (precision == QUDA_DOUBLE_PRECISION) constructUnitGaugeField((double**)gauge, param);
    else constructUnitGaugeField((float**)gauge, param);
   } else {
    if (precision == QUDA_DOUBLE_PRECISION) constructGaugeField((double**)gauge, param);
    else constructGaugeField((float**)gauge, param);
  }
}

void
construct_fat_long_gauge_field(void **fatlink, void** longlink,  
			       int type, QudaPrecision precision, QudaGaugeParam* param)
{
    if (type == 0) {
	if (precision == QUDA_DOUBLE_PRECISION) {
	    constructUnitGaugeField((double**)fatlink, param);
	    constructUnitGaugeField((double**)longlink, param);
	}else {
	    constructUnitGaugeField((float**)fatlink, param);
	    constructUnitGaugeField((float**)longlink, param);
	}
    } else {
	if (precision == QUDA_DOUBLE_PRECISION) {
	    param->type = QUDA_ASQTAD_FAT_LINKS;
	    constructGaugeField((double**)fatlink, param);
	    param->type = QUDA_ASQTAD_LONG_LINKS;
	    constructGaugeField((double**)longlink, param);
	}else {
	    param->type = QUDA_ASQTAD_FAT_LINKS;
	    constructGaugeField((float**)fatlink, param);
	    param->type = QUDA_ASQTAD_LONG_LINKS;
	    constructGaugeField((float**)longlink, param);
	}
    }
}


template <typename Float>
static void constructCloverField(Float *res, double norm, double diag) {

  Float c = 2.0 * norm / RAND_MAX;

  for(int i = 0; i < V; i++) {
    for (int j = 0; j < 72; j++) {
      res[i*72 + j] = c*rand() - norm;
    }
    for (int j = 0; j< 6; j++) {
      res[i*72 + j] += diag;
      res[i*72 + j+36] += diag;
    }
  }
}

void construct_clover_field(void *clover, double norm, double diag, QudaPrecision precision) {

  if (precision == QUDA_DOUBLE_PRECISION) constructCloverField((double *)clover, norm, diag);
  else constructCloverField((float *)clover, norm, diag);
}

/*void strong_check(void *spinorRef, void *spinorGPU, int len, QudaPrecision prec) {
  printf("Reference:\n");
  printSpinorElement(spinorRef, 0, prec); printf("...\n");
  printSpinorElement(spinorRef, len-1, prec); printf("\n");    
    
  printf("\nCUDA:\n");
  printSpinorElement(spinorGPU, 0, prec); printf("...\n");
  printSpinorElement(spinorGPU, len-1, prec); printf("\n");

  compare_spinor(spinorRef, spinorGPU, len, prec);
  }*/

template <typename Float>
static void checkGauge(Float **oldG, Float **newG, double epsilon) {

  int fail_check = 17;
  int *fail = new int[fail_check];
  int iter[18];
  for (int i=0; i<fail_check; i++) fail[i] = 0;
  for (int i=0; i<18; i++) iter[i] = 0;

  for (int d=0; d<4; d++) {
    for (int eo=0; eo<2; eo++) {
      for (int i=0; i<Vh; i++) {
	int ga_idx = (eo*Vh+i);
	for (int j=0; j<18; j++) {
	  double diff = fabs(newG[d][ga_idx*18+j] - oldG[d][ga_idx*18+j]);

	  for (int f=0; f<fail_check; f++) if (diff > pow(10.0,-(f+1))) fail[f]++;
	  if (diff > epsilon) iter[j]++;
	}
      }
    }
  }

  for (int i=0; i<18; i++) printf("%d fails = %d\n", i, iter[i]);

  for (int f=0; f<fail_check; f++) {
    printf("%e Failures = %d / %d  = %e\n", pow(10.0,-(f+1)), fail[f], V*4*18, fail[f] / (double)(4*V*18));
  }

  delete []fail;

}

void check_gauge(void **oldG, void **newG, double epsilon, QudaPrecision precision) {
  if (precision == QUDA_DOUBLE_PRECISION) 
    checkGauge((double**)oldG, (double**)newG, epsilon);
  else 
    checkGauge((float**)oldG, (float**)newG, epsilon);
}


void 
createSiteLinkCPU(void* link,  QudaPrecision precision, int phase) 
{
    void* temp[4];
    
    size_t gSize = (precision == QUDA_DOUBLE_PRECISION) ? sizeof(double) : sizeof(float);
    for(int i=0;i < 4;i++){
	temp[i] = malloc(V*gaugeSiteSize*gSize);
	if (temp[i] == NULL){
	    fprintf(stderr, "Error: malloc failed for temp in function %s\n", __FUNCTION__);
	    exit(1);
	}
    }
    
    if (precision == QUDA_DOUBLE_PRECISION) {
	constructUnitaryGaugeField((double**)temp);
    }else {
	constructUnitaryGaugeField((float**)temp);
    }
        
    for(int i=0;i < V;i++){
	for(int dir=0;dir < 4;dir++){
	    if (precision == QUDA_DOUBLE_PRECISION){
		double** src= (double**)temp;
		double* dst = (double*)link;
		for(int k=0; k < gaugeSiteSize; k++){
		    dst[ (4*i+dir)*gaugeSiteSize + k ] = src[dir][i*gaugeSiteSize + k];

		}
	    }else{
		float** src= (float**)temp;
		float* dst = (float*)link;
		for(int k=0; k < gaugeSiteSize; k++){
		    dst[ (4*i+dir)*gaugeSiteSize + k ] = src[dir][i*gaugeSiteSize + k];
		    
		}
		
	    }
	}
    }

    if(phase){
	
	for(int i=0;i < V;i++){
	    for(int dir =XUP; dir <= TUP; dir++){
		int idx = i;
		int oddBit =0;
		if (i >= Vh) {
		    idx = i - Vh;
		    oddBit = 1;
		}

		int X1 = Z[0];
		int X2 = Z[1];
		int X3 = Z[2];
		int X4 = Z[3];

		int full_idx = fullLatticeIndex(idx, oddBit);
		int i4 = full_idx /(X3*X2*X1);
		int i3 = (full_idx - i4*(X3*X2*X1))/(X2*X1);
		int i2 = (full_idx - i4*(X3*X2*X1) - i3*(X2*X1))/X1;
		int i1 = full_idx - i4*(X3*X2*X1) - i3*(X2*X1) - i2*X1;	    

		double coeff= 1.0;
		switch(dir){
		case XUP:
		    if ( (i4 & 1) == 1){
			coeff *= -1;
		    }
		    break;

		case YUP:
		    if ( ((i4+i1) & 1) == 1){
			coeff *= -1;
		    }
		    break;

		case ZUP:
		    if ( ((i4+i1+i2) & 1) == 1){
			coeff *= -1;
		    }
		    break;
		
		case TUP:
		    if (i4 == (X4-1) ){
			coeff *= -1;
		    }
		    break;

		default:
		    printf("ERROR: wrong dir(%d)\n", dir);
		    exit(1);
		}
	    
	    
		if (precision == QUDA_DOUBLE_PRECISION){
		    double* mylink = (double*)link;
		    mylink = mylink + (4*i + dir)*gaugeSiteSize;
		
		    mylink[12] *= coeff;
		    mylink[13] *= coeff;
		    mylink[14] *= coeff;
		    mylink[15] *= coeff;
		    mylink[16] *= coeff;
		    mylink[17] *= coeff;
		
		}else{
		    float* mylink = (float*)link;
		    mylink = mylink + (4*i + dir)*gaugeSiteSize;
		
		    mylink[12] *= coeff;
		    mylink[13] *= coeff;
		    mylink[14] *= coeff;
		    mylink[15] *= coeff;
		    mylink[16] *= coeff;
		    mylink[17] *= coeff;
		
		}
	    }
	}
    }    

    
#if 1
    for(int i=0;i< 4*V*gaugeSiteSize;i++){
	if (precision ==QUDA_SINGLE_PRECISION){
	    float* f = (float*)link;
	    if (f[i] != f[i] || (fabsf(f[i]) > 1.e+3) ){
		fprintf(stderr, "ERROR:  %dth: bad number(%f) in function %s \n",i, f[i], __FUNCTION__);
		exit(1);
	    }
	}else{
	    double* f = (double*)link;
	    if (f[i] != f[i] || (fabs(f[i]) > 1.e+3)){
		fprintf(stderr, "ERROR:  %dth: bad number(%f) in function %s \n",i, f[i], __FUNCTION__);
		exit(1);
	    }
	    
	}
	
    }
#endif

    for(int i=0;i < 4;i++){
	free(temp[i]);
    }
    return;
}





template <typename Float>
void compareLink(Float *linkA, Float *linkB, int len) {
  int fail_check = 16;
  int fail[fail_check];
  for (int f=0; f<fail_check; f++) fail[f] = 0;

  int iter[18];
  for (int i=0; i<18; i++) iter[i] = 0;
  
  for (int i=0; i<len; i++) {
      for (int j=0; j<18; j++) {
	  int is = i*18+j;
	  double diff = fabs(linkA[is]-linkB[is]);
	  for (int f=0; f<fail_check; f++)
	      if (diff > pow(10.0,-(f+1))) fail[f]++;
	  //if (diff > 1e-1) printf("%d %d %e\n", i, j, diff);
	  if (diff > 1e-3) iter[j]++;
      }
  }
  
  for (int i=0; i<18; i++) printf("%d fails = %d\n", i, iter[i]);
  
  for (int f=0; f<fail_check; f++) {
      printf("%e Failures: %d / %d  = %e\n", pow(10.0,-(f+1)), fail[f], len*gaugeSiteSize, fail[f] / (double)(len*6));
  }
  
}

static void 
compare_link(void *linkA, void *linkB, int len, QudaPrecision precision)
{
    if (precision == QUDA_DOUBLE_PRECISION) compareLink((double*)linkA, (double*)linkB, len);
    else compareLink((float*)linkA, (float*)linkB, len);
    
    return;
}


// X indexes the lattice site
static void 
printLinkElement(void *link, int X, QudaPrecision precision) 
{
    if (precision == QUDA_DOUBLE_PRECISION){
	for(int i=0; i < 3;i++){
	    printVector((double*)link+ X*gaugeSiteSize + i*6);
	}
	
    }
    else{
	for(int i=0;i < 3;i++){
	    printVector((float*)link+X*gaugeSiteSize + i*6);
	}
    }
}

void strong_check_link(void * linkA, void *linkB, int len, QudaPrecision prec) 
{
    printf("LinkA:\n");
    printLinkElement(linkA, 0, prec); 
    printf("\n");
    printLinkElement(linkA, 1, prec); 
    printf("...\n");
    printLinkElement(linkA, len-1, prec); 
    printf("\n");    
    
    printf("\nlinkB:\n");
    printLinkElement(linkB, 0, prec); 
    printf("\n");
    printLinkElement(linkB, 1, prec); 
    printf("...\n");
    printLinkElement(linkB, len-1, prec); 
    printf("\n");
    
    compare_link(linkA, linkB, len, prec);
}



void 
createMomCPU(void* mom,  QudaPrecision precision) 
{
    void* temp;
    
    size_t gSize = (precision == QUDA_DOUBLE_PRECISION) ? sizeof(double) : sizeof(float);
    temp = malloc(4*V*gaugeSiteSize*gSize);
    if (temp == NULL){
	fprintf(stderr, "Error: malloc failed for temp in function %s\n", __FUNCTION__);
	exit(1);
    }
    
    
    
    for(int i=0;i < V;i++){
	if (precision == QUDA_DOUBLE_PRECISION){
	    for(int dir=0;dir < 4;dir++){
		double* thismom = (double*)mom;	    
		for(int k=0; k < momSiteSize; k++){
		    thismom[ (4*i+dir)*momSiteSize + k ]= 1.0* rand() /RAND_MAX;				
		}	    
	    }	    
	}else{
	    for(int dir=0;dir < 4;dir++){
		float* thismom=(float*)mom;
		for(int k=0; k < momSiteSize; k++){
		    thismom[ (4*i+dir)*momSiteSize + k ]= 1.0* rand() /RAND_MAX;		
		}	    
	    }
	}
    }
    
    free(temp);
    return;
}

void
createHwCPU(void* hw,  QudaPrecision precision)
{
    for(int i=0;i < V;i++){
        if (precision == QUDA_DOUBLE_PRECISION){
            for(int dir=0;dir < 4;dir++){
                double* thishw = (double*)hw;
                for(int k=0; k < hwSiteSize; k++){
                    thishw[ (4*i+dir)*hwSiteSize + k ]= 1.0* rand() /RAND_MAX;
                }
            }
        }else{
            for(int dir=0;dir < 4;dir++){
                float* thishw=(float*)hw;
                for(int k=0; k < hwSiteSize; k++){
                    thishw[ (4*i+dir)*hwSiteSize + k ]= 1.0* rand() /RAND_MAX;
                }
            }
        }
    }

    return;
}


template <typename Float>
void compare_mom(Float *momA, Float *momB, int len) {
  int fail_check = 16;
  int fail[fail_check];
  for (int f=0; f<fail_check; f++) fail[f] = 0;

  int iter[momSiteSize];
  for (int i=0; i<momSiteSize; i++) iter[i] = 0;
  
  for (int i=0; i<len; i++) {
      for (int j=0; j<momSiteSize; j++) {
	  int is = i*momSiteSize+j;
	  double diff = fabs(momA[is]-momB[is]);
	  for (int f=0; f<fail_check; f++)
	      if (diff > pow(10.0,-(f+1))) fail[f]++;
	  //if (diff > 1e-1) printf("%d %d %e\n", i, j, diff);
	  if (diff > 1e-3) iter[j]++;
      }
  }
  
  for (int i=0; i<momSiteSize; i++) printf("%d fails = %d\n", i, iter[i]);
  
  for (int f=0; f<fail_check; f++) {
      printf("%e Failures: %d / %d  = %e\n", pow(10.0,-(f+1)), fail[f], len*momSiteSize, fail[f] / (double)(len*6));
  }
  
}

static void 
printMomElement(void *mom, int X, QudaPrecision precision) 
{
    if (precision == QUDA_DOUBLE_PRECISION){
	double* thismom = ((double*)mom)+ X*momSiteSize;
	printVector(thismom);
	printf("(%9f,%9f) (%9f,%9f)\n", thismom[6], thismom[7], thismom[8], thismom[9]);
    }else{
	float* thismom = ((float*)mom)+ X*momSiteSize;
	printVector(thismom);
	printf("(%9f,%9f) (%9f,%9f)\n", thismom[6], thismom[7], thismom[8], thismom[9]);	
    }
}
void strong_check_mom(void * momA, void *momB, int len, QudaPrecision prec) 
{    
    printf("mom:\n");
    printMomElement(momA, 0, prec); 
    printf("\n");
    printMomElement(momA, 1, prec); 
    printf("\n");
    printMomElement(momA, 2, prec); 
    printf("\n");
    printMomElement(momA, 3, prec); 
    printf("...\n");

    printf("\nreference mom:\n");
    printMomElement(momB, 0, prec); 
    printf("\n");
    printMomElement(momB, 1, prec); 
    printf("\n");
    printMomElement(momB, 2, prec); 
    printf("\n");
    printMomElement(momB, 3, prec); 
    printf("\n");

    
    if (prec == QUDA_DOUBLE_PRECISION){
	compare_mom((double*)momA, (double*)momB, len);
    }else{
	compare_mom((float*)momA, (float*)momB, len);
    }
}

///RECOVERED!!!
template <typename Float>
static void constructPointSpinorField(Float *res, int i0, int s0, int c0) {
  Float *resEven = res;
  Float *resOdd = res + Vh*spinorSiteSize;
    
  for(int i = 0; i < Vh; i++) {
    for (int s = 0; s < 4; s++) {
      for (int m = 0; m < 3; m++) {
	resEven[i*(4*3*2) + s*(3*2) + m*(2) + 0] = 0;
	resEven[i*(4*3*2) + s*(3*2) + m*(2) + 1] = 0;
	resOdd[i*(4*3*2) + s*(3*2) + m*(2) + 0] = 0;
	resOdd[i*(4*3*2) + s*(3*2) + m*(2) + 1] = 0;
	if (s == s0 && m == c0) {
	  if (fullLatticeIndex(i, 0) == i0)
	    resEven[i*(4*3*2) + s*(3*2) + m*(2) + 0] = 1;
	  if (fullLatticeIndex(i, 1) == i0)
	    resOdd[i*(4*3*2) + s*(3*2) + m*(2) + 0] = 1;
	}
      }
    }
  }
}


template <typename Float>
static void constructSpinorField(Float *res) {
  for(int i = 0; i < V; i++) {
    for (int s = 0; s < 4; s++) {
      for (int m = 0; m < 3; m++) {
	res[i*(4*3*2) + s*(3*2) + m*(2) + 0] = rand() / (Float)RAND_MAX;
	res[i*(4*3*2) + s*(3*2) + m*(2) + 1] = rand() / (Float)RAND_MAX;
      }
    }
  }
}

void construct_spinor_field(void *spinor, int type, int i0, int s0, int c0, QudaPrecision precision) {
  if (type == 0) {
    if (precision == QUDA_DOUBLE_PRECISION) constructPointSpinorField((double*)spinor, i0, s0, c0);
    else constructPointSpinorField((float*)spinor, i0, s0, c0);
  } else {
    if (precision == QUDA_DOUBLE_PRECISION) constructSpinorField((double*)spinor);
    else constructSpinorField((float*)spinor);
  }
}

///ILDG routines:

int getOddBitFromHLattCoordinate(int hlatt_coord)
{
	int x2, x3, x4;			//x / 2, y, z, t normal coordinates on even/odd latice
	int z1, z2;
	

	z1  = (2 * hlatt_coord) / Z[0];
	z2  = z1 / Z[1];
	x2  = z1 - z2 * Z[1];
	x4  = z2 / Z[2];
	x3  = z2 - x4 * Z[2];

	return ((x2 + x3 + x4 + 0) & 1);
}


template<typename Float>
void readTMconfig(Float** gauge, char *filepath, QudaGaugeParam *param)
{
  Float *resEvn[4], *resOdd[4];
  
  int nsh  = Z[0] * Z[1] * Z[2] / 2;
  int nvh = nsh * Z[3];
  int oddBit, l;
  
  for(int dir = 0; dir < 4; dir++)
  {
    resEvn[dir] = gauge[dir];
    resOdd[dir] = gauge[dir] + nvh * gaugeSiteSize;
  }
  
  FILE *fp;
  fp = fopen(filepath,"rb");
  
  double tmp_real, tmp_imag;

  for(int dir = 0; dir < 4; dir++)
  {
    for(int t = 0; t < Z[3]; t++)
    {
      for(int s = 0; s < nsh; s++)
      {
	l = s + t * nsh;
	oddBit = getOddBitFromHLattCoordinate(l);
	if(!oddBit)
	{
	  for(int c1 = 0; c1 < 3; c1++)
	    for(int c2 = 0; c2 < 3; c2++)
	    {
	      fread(&tmp_real, sizeof(double),1,fp);
	      fread(&tmp_imag, sizeof(double),1,fp);
	      resEvn[dir][l*(3*3*2) + c1*(3*2) + c2*(2) + 0] = (Float)tmp_real;
	      resEvn[dir][l*(3*3*2) + c1*(3*2) + c2*(2) + 1] = (Float)tmp_imag;
	    }
	  for(int c1 = 0; c1 < 3; c1++)
	    for(int c2 = 0; c2 < 3; c2++)
	    {
	      fread(&tmp_real, sizeof(double),1,fp);
	      fread(&tmp_imag, sizeof(double),1,fp);
	      resOdd[dir][l*(3*3*2) + c1*(3*2) + c2*(2) + 0] = (Float)tmp_real;
	      resOdd[dir][l*(3*3*2) + c1*(3*2) + c2*(2) + 1] = (Float)tmp_imag;
	    }
	}
	else
	{
	  for(int c1 = 0; c1 < 3; c1++)
	    for(int c2 = 0; c2 < 3; c2++)
	    {
	      fread(&tmp_real, sizeof(double),1,fp);
	      fread(&tmp_imag, sizeof(double),1,fp);
	      resOdd[dir][l*(3*3*2) + c1*(3*2) + c2*(2) + 0] = (Float)tmp_real;
	      resOdd[dir][l*(3*3*2) + c1*(3*2) + c2*(2) + 1] = (Float)tmp_imag;
	    }
	  for(int c1 = 0; c1 < 3; c1++)
	    for(int c2 = 0; c2 < 3; c2++)
	    {
	      fread(&tmp_real, sizeof(double),1,fp);
	      fread(&tmp_imag, sizeof(double),1,fp);
	      resEvn[dir][l*(3*3*2) + c1*(3*2) + c2*(2) + 0] = (Float)tmp_real;
	      resEvn[dir][l*(3*3*2) + c1*(3*2) + c2*(2) + 1] = (Float)tmp_imag;
	    }
	}

      }
    }
  }	  
  fclose(fp);
  applyGaugeFieldScaling(gauge, Vh, param);  
}

void readILDGconfig(void** gauge, char *file_path, QudaGaugeParam *param)
{
  if(param->cpu_prec == QUDA_DOUBLE_PRECISION)
    readTMconfig<double>((double**) gauge, file_path, param);
  else//single precision is set
    readTMconfig<float>((float**) gauge, file_path, param);
}

