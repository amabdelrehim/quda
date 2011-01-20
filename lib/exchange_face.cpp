
#include <stdio.h>
#include <string.h>

#include <quda_internal.h>
#include <quda.h>
#include <util_quda.h>
#include "exchange_face.h"
#include "mpicomm.h"
#include <sys/time.h>
#include <color_spinor_field.h>
#include "gauge_quda.h"

static int V;
static int Vh;
static int Vs;
static int Vsh;
static int dims[4];

void* fwd_nbr_spinor_sendbuf = NULL;
void* back_nbr_spinor_sendbuf = NULL;
void* f_norm_sendbuf = NULL;
void* b_norm_sendbuf = NULL;

void* fwd_nbr_spinor = NULL;
void* back_nbr_spinor = NULL;
void* f_norm = NULL;
void* b_norm = NULL;


static void* pagable_fwd_nbr_spinor_sendbuf = NULL;
static void* pagable_back_nbr_spinor_sendbuf = NULL;
static void* pagable_f_norm_sendbuf = NULL;
static void* pagable_b_norm_sendbuf = NULL;
  
static void* pagable_fwd_nbr_spinor = NULL;
static void* pagable_back_nbr_spinor = NULL;
static void* pagable_f_norm = NULL;
static void* pagable_b_norm = NULL;

extern float fat_link_max;

#define gaugeSiteSize 18
#define mySpinorSiteSize 6

static void
setup_dims(int* X)
{
  V = 1;
  for (int d=0; d< 4; d++) {
    V *= X[d];
    dims[d] = X[d];
  }
  Vh = V/2;
  
  Vs = X[0]*X[1]*X[2];
  Vsh = Vs/2;
}
void exchange_init(cudaColorSpinorField* cudaSpinor)
{
  
  static int exchange_initialized = 0;
  static int hp_initialized = 0;
  if (exchange_initialized && hp_initialized){
    return;
  }
  
  //int len = 3*Vsh*mySpinorSiteSize*cudaSpinor->Precision();
  int len = 3*Vsh*mySpinorSiteSize*QUDA_DOUBLE_PRECISION; //use maximum precision size
  int normlen = 3*Vsh*sizeof(float);

  if (!exchange_initialized){
    cudaMallocHost((void**)&fwd_nbr_spinor_sendbuf, len); CUERR;
    cudaMallocHost((void**)&back_nbr_spinor_sendbuf, len); CUERR;
    if (fwd_nbr_spinor_sendbuf == NULL || back_nbr_spinor_sendbuf == NULL){
      printf("ERROR: malloc failed for fwd_nbr_spinor_sendbuf/back_nbr_spinor_sendbuf\n"); 
      comm_exit(1);
    }
    
    cudaMallocHost((void**)&fwd_nbr_spinor, len); CUERR;
    cudaMallocHost((void**)&back_nbr_spinor, len); CUERR;
    if (fwd_nbr_spinor == NULL || back_nbr_spinor == NULL){
      printf("ERROR: malloc failed for fwd_nbr_spinor/back_nbr_spinor\n"); 
      comm_exit(1);
    }  
    
    pagable_fwd_nbr_spinor_sendbuf = malloc(len);
    pagable_back_nbr_spinor_sendbuf = malloc(len);
    if (pagable_fwd_nbr_spinor_sendbuf == NULL || pagable_back_nbr_spinor_sendbuf == NULL){
      printf("ERROR: malloc failed for pagable_fwd_nbr_spinor_sendbuf/pagable_back_nbr_spinor_sendbuf\n");
      comm_exit(1);
    }
    
    pagable_fwd_nbr_spinor=malloc(len);
    pagable_back_nbr_spinor=malloc(len);
    if (pagable_fwd_nbr_spinor == NULL || pagable_back_nbr_spinor == NULL){
      printf("ERROR: malloc failed for pagable_fwd_nbr_spinor/pagable_back_nbr_spinor\n"); 
      comm_exit(1);
    }
    exchange_initialized = 1;
  }

  
  if (cudaSpinor->Precision() == QUDA_HALF_PRECISION && !hp_initialized){
    cudaMallocHost(&f_norm_sendbuf, normlen);CUERR;
    cudaMallocHost(&b_norm_sendbuf, normlen);CUERR;
    if (f_norm_sendbuf == NULL || b_norm_sendbuf == NULL){
      printf("ERROR: malloc failed for b_norm_sendbuf/f_norm_sendbuf\n");
      comm_exit(1);
    } 
    
    cudaMallocHost(&f_norm, normlen);CUERR;
    cudaMallocHost(&b_norm, normlen);CUERR;
    if (f_norm== NULL || b_norm== NULL){
      printf("ERROR: malloc failed for b_norm/f_norm\n");
      comm_exit(1);
    } 
    
    pagable_f_norm_sendbuf=malloc(normlen);
    pagable_b_norm_sendbuf=malloc(normlen);
    if (pagable_f_norm_sendbuf == NULL || pagable_b_norm_sendbuf == NULL){
      printf("ERROR: malloc failed for pagable_b_norm_sendbuf/pagable_f_norm_sendbuf\n");
      comm_exit(1);
    }   

    pagable_f_norm=malloc(normlen);
    pagable_b_norm=malloc(normlen);
    if (pagable_f_norm== NULL || pagable_b_norm== NULL){
      printf("ERROR: malloc failed for pagable_b_norm/pagable_f_norm\n");
      comm_exit(1);
    }
    hp_initialized = 1;
  }
  
  
  return;
}

void exchange_dslash_cleanup(void)
{
  
  if(fwd_nbr_spinor_sendbuf){
    cudaFreeHost(fwd_nbr_spinor_sendbuf);
  }
  if(back_nbr_spinor_sendbuf){
    cudaFreeHost(back_nbr_spinor_sendbuf);
  }
  
  if (f_norm_sendbuf){
    cudaFreeHost(f_norm_sendbuf);
  }
  if (b_norm_sendbuf){
    cudaFreeHost(b_norm_sendbuf);
  }

  if(fwd_nbr_spinor){
    cudaFreeHost(fwd_nbr_spinor);
  }
  if(back_nbr_spinor){
    cudaFreeHost(back_nbr_spinor);
  }
  
  if (f_norm){
    cudaFreeHost(f_norm);
  }
  if (b_norm){
    cudaFreeHost(b_norm);
  }

}


template<typename Float>
void
exchange_fatlink(Float** fatlink, Float* ghost_fatlink, Float* fatlink_sendbuf)
{
  Float* even_fatlink_src = fatlink[3] + (Vh - Vsh)*gaugeSiteSize;
  Float* odd_fatlink_src = fatlink[3] + (V -Vsh)*gaugeSiteSize;
  
  Float* even_fatlink_dst = fatlink_sendbuf;
  Float* odd_fatlink_dst = fatlink_sendbuf + Vsh*gaugeSiteSize;

  int len = Vsh*gaugeSiteSize*sizeof(Float);
  if(dims[3] % 2 == 0){
    memcpy(even_fatlink_dst, even_fatlink_src, len); 
    memcpy(odd_fatlink_dst, odd_fatlink_src, len);
  }else{
    //switching odd and even ghost fatlink
    memcpy(even_fatlink_dst, odd_fatlink_src, len); 
    memcpy(odd_fatlink_dst, even_fatlink_src, len);
  }
  unsigned long recv_request = comm_recv(ghost_fatlink, 2*len, BACK_NBR);
  unsigned long send_request = comm_send(fatlink_sendbuf, 2*len, FWD_NBR);
  comm_wait(recv_request);
  comm_wait(send_request);

  //find out the max value for fatlink
  double max;
  for(int dir= 0;dir < 3; dir++){
    for(int i=0;i < V; i++){
      for(int j=0; j < 18; j++){
	if( fatlink[dir][i*18+j] > max){
	  max = fatlink[dir][i*18+j];
	}
      }//j      
    }//i
  }//dir
  
  comm_allreduce_max(&max);
  fat_link_max = max;
}

template<typename Float>
void
exchange_longlink(Float** longlink, Float* ghost_longlink, Float* longlink_sendbuf)
{
  Float* even_longlink_src = longlink[3] + (Vh -3*Vsh)*gaugeSiteSize;
  Float* odd_longlink_src = longlink[3] + (V - 3*Vsh)*gaugeSiteSize;
  
  Float* even_longlink_dst = longlink_sendbuf;
  Float* odd_longlink_dst = longlink_sendbuf + 3*Vsh*gaugeSiteSize;
  int len  = 3*Vsh*gaugeSiteSize*sizeof(Float);
  if (dims[3] % 2 == 0){
    memcpy(even_longlink_dst, even_longlink_src, len);
    memcpy(odd_longlink_dst, odd_longlink_src, len);
  }else{
    //switching odd and even long link
    memcpy(even_longlink_dst, odd_longlink_src, len);
    memcpy(odd_longlink_dst, even_longlink_src, len);
  }

  unsigned long recv_request = comm_recv(ghost_longlink, 2*len, BACK_NBR);
  unsigned long send_request = comm_send(longlink_sendbuf, 2*len, FWD_NBR);
  comm_wait(recv_request);
  comm_wait(send_request);
  
}

template<typename Float>
void
exchange_cpu_spinor(Float* spinorField, Float* fwd_nbr_spinor, Float* back_nbr_spinor)
{
  Float* fwd_nbr_spinor_send = spinorField + (Vh -3*Vsh)*mySpinorSiteSize;
  Float* back_nbr_spinor_send = spinorField;
  int len = 3*Vsh*mySpinorSiteSize*sizeof(Float);
  

  unsigned long recv_request1 = comm_recv(back_nbr_spinor, len, BACK_NBR);
  unsigned long recv_request2 = comm_recv(fwd_nbr_spinor, len, FWD_NBR);
  
  unsigned long send_request1= comm_send(fwd_nbr_spinor_send, len, FWD_NBR);
  unsigned long send_request2 = comm_send(back_nbr_spinor_send, len, BACK_NBR);
  
  comm_wait(recv_request1);
  comm_wait(recv_request2);
  
  comm_wait(send_request1);
  comm_wait(send_request2);

}

void
exchange_gpu_spinor(void* _cudaSpinor, cudaStream_t* mystream)
{
  
  cudaColorSpinorField* cudaSpinor = (cudaColorSpinorField*) _cudaSpinor;
  exchange_init(cudaSpinor);
  struct timeval t0, t1, t2, t3;
  
  int len = 3*Vsh*mySpinorSiteSize*cudaSpinor->Precision();
  int normlen = 3*Vsh*sizeof(float);

  gettimeofday(&t0, NULL);
  cudaSpinor->packGhostSpinor(fwd_nbr_spinor_sendbuf, back_nbr_spinor_sendbuf, f_norm_sendbuf, b_norm_sendbuf, mystream);
  cudaStreamSynchronize(*mystream);
  gettimeofday(&t1, NULL);
 
  unsigned long recv_request1 = comm_recv(back_nbr_spinor, len, BACK_NBR);
  unsigned long recv_request2 = comm_recv(fwd_nbr_spinor, len, FWD_NBR);
  
  unsigned long send_request1= comm_send(fwd_nbr_spinor_sendbuf, len, FWD_NBR);
  unsigned long send_request2 = comm_send(back_nbr_spinor_sendbuf, len, BACK_NBR);

  unsigned long recv_request3 = 0;
  unsigned long recv_request4 = 0;
  unsigned long send_request3 = 0;
  unsigned long send_request4 = 0;
  
  if (cudaSpinor->Precision() == QUDA_HALF_PRECISION){
    recv_request3 = comm_recv(b_norm, normlen, BACK_NBR);
    recv_request4 = comm_recv(f_norm, normlen, FWD_NBR);
    send_request3 = comm_send(f_norm_sendbuf, normlen, FWD_NBR);
    send_request4 = comm_send(b_norm_sendbuf, normlen, BACK_NBR);
  }
  
  
  
  comm_wait(recv_request1);
  comm_wait(recv_request2);  
  comm_wait(send_request1);
  comm_wait(send_request2);
  
  if (cudaSpinor->Precision() == QUDA_HALF_PRECISION){
    comm_wait(recv_request3);
    comm_wait(recv_request4);
    comm_wait(send_request3);
    comm_wait(send_request4);
  }
  
  gettimeofday(&t2, NULL);
  cudaSpinor->unpackGhostSpinor(fwd_nbr_spinor, back_nbr_spinor, f_norm, b_norm, mystream);
  
  cudaStreamSynchronize(*mystream);
  gettimeofday(&t3, NULL);

  float pack_time = TDIFF(t1, t0)*1000;
  float send_recv_time = TDIFF(t2, t1)*1000;
  float unpack_time = TDIFF(t3, t2)*1000;
  
  PRINTF("Pack_time=%.2f(ms)(%.2f GB/s), send_recv_time=%.2f(ms)(%.2f GB/s), unpack_time=%.2f(ms)(%.2f GB/s)\n",
	 pack_time, 2*len/pack_time*1e-6, send_recv_time, 2*len/send_recv_time*1e-6, 
	 unpack_time, 2*len/unpack_time*1e-6);
  

}




void
exchange_gpu_spinor_start(void* _cudaSpinor,cudaStream_t* mystream)
{
  cudaColorSpinorField* cudaSpinor = (cudaColorSpinorField*) _cudaSpinor;
 
  exchange_init(cudaSpinor);
  cudaSpinor->packGhostSpinor(fwd_nbr_spinor_sendbuf, back_nbr_spinor_sendbuf, f_norm_sendbuf, b_norm_sendbuf, mystream);
  
}

void
exchange_gpu_spinor_wait(void* _cudaSpinor, cudaStream_t* mystream)
{
  cudaColorSpinorField* cudaSpinor = (cudaColorSpinorField*) _cudaSpinor;
 
  int len = 3*Vsh*mySpinorSiteSize*cudaSpinor->Precision();
  int normlen = 3*Vsh*sizeof(float);
  
  cudaStreamSynchronize(*mystream); //required the data to be there before sending out

  memcpy(pagable_back_nbr_spinor_sendbuf, back_nbr_spinor_sendbuf, len);
  memcpy(pagable_fwd_nbr_spinor_sendbuf, fwd_nbr_spinor_sendbuf, len);

  unsigned long recv_request1 = comm_recv(pagable_back_nbr_spinor, len, BACK_NBR);
  unsigned long recv_request2 = comm_recv(pagable_fwd_nbr_spinor, len, FWD_NBR);
  
  unsigned long send_request1= comm_send(pagable_fwd_nbr_spinor_sendbuf, len, FWD_NBR);
  unsigned long send_request2 = comm_send(pagable_back_nbr_spinor_sendbuf, len, BACK_NBR);

  unsigned long recv_request3 = 0;
  unsigned long recv_request4 = 0;
  unsigned long send_request3 = 0;
  unsigned long send_request4 = 0;
  
  if (cudaSpinor->Precision() == QUDA_HALF_PRECISION){

    memcpy(pagable_b_norm_sendbuf, b_norm_sendbuf, normlen);
    memcpy(pagable_f_norm_sendbuf, f_norm_sendbuf, normlen);

    recv_request3 = comm_recv(pagable_b_norm, normlen, BACK_NBR);
    recv_request4 = comm_recv(pagable_f_norm, normlen, FWD_NBR);
    send_request3 = comm_send(pagable_f_norm_sendbuf, normlen, FWD_NBR);
    send_request4 = comm_send(pagable_b_norm_sendbuf, normlen, BACK_NBR);
  }
  
  
  
  comm_wait(recv_request1);
  comm_wait(recv_request2);  
  comm_wait(send_request1);
  comm_wait(send_request2);
  
  memcpy(fwd_nbr_spinor, pagable_fwd_nbr_spinor, len);
  memcpy(back_nbr_spinor, pagable_back_nbr_spinor, len);

  if (cudaSpinor->Precision() == QUDA_HALF_PRECISION){

    memcpy(f_norm, pagable_f_norm, normlen);
    memcpy(b_norm, pagable_b_norm, normlen);

    comm_wait(recv_request3);
    comm_wait(recv_request4);
    comm_wait(send_request3);
    comm_wait(send_request4);
  }
  cudaSpinor->unpackGhostSpinor(fwd_nbr_spinor, back_nbr_spinor, f_norm, b_norm, mystream);  
  cudaStreamSynchronize(*mystream);
  
}

template<typename Float>
void
exchange_sitelink(Float** sitelink, Float* ghost_sitelink, Float* sitelink_fwd_sendbuf, Float* sitelink_back_sendbuf)
{

  int i;
  int len = Vsh*gaugeSiteSize*sizeof(Float);
  for(i=0;i < 4;i++){
    Float* even_sitelink_back_src = sitelink[i];
    Float* odd_sitelink_back_src = sitelink[i] + Vh*gaugeSiteSize;
    Float* sitelink_back_dst = sitelink_back_sendbuf + 2*i*Vsh*gaugeSiteSize;

    if(dims[3] % 2 == 0){    
      memcpy(sitelink_back_dst, even_sitelink_back_src, len);
      memcpy(sitelink_back_dst + Vsh*gaugeSiteSize, odd_sitelink_back_src, len);
    }else{
      //switching odd and even ghost sitelink
      memcpy(sitelink_back_dst, odd_sitelink_back_src, len);
      memcpy(sitelink_back_dst + Vsh*gaugeSiteSize, even_sitelink_back_src, len);
    }
  }

  for(i=0;i < 4;i++){
    Float* even_sitelink_fwd_src = sitelink[i] + (Vh - Vsh)*gaugeSiteSize;
    Float* odd_sitelink_fwd_src = sitelink[i] + Vh*gaugeSiteSize + (Vh - Vsh)*gaugeSiteSize;
    Float* sitelink_fwd_dst = sitelink_fwd_sendbuf + 2*i*Vsh*gaugeSiteSize;
    if(dims[3] % 2 == 0){    
      memcpy(sitelink_fwd_dst, even_sitelink_fwd_src, len);
      memcpy(sitelink_fwd_dst + Vsh*gaugeSiteSize, odd_sitelink_fwd_src, len);
    }else{
      //switching odd and even ghost sitelink
      memcpy(sitelink_fwd_dst, odd_sitelink_fwd_src, len);
      memcpy(sitelink_fwd_dst + Vsh*gaugeSiteSize, even_sitelink_fwd_src, len);
    }
    
  }
  
  Float* ghost_sitelink_back = ghost_sitelink;
  Float* ghost_sitelink_fwd = ghost_sitelink + 8*Vsh*gaugeSiteSize;

  unsigned long recv_request1 = comm_recv(ghost_sitelink_back, 8*len, BACK_NBR);
  unsigned long recv_request2 = comm_recv(ghost_sitelink_fwd, 8*len, FWD_NBR);
  unsigned long send_request1 = comm_send(sitelink_fwd_sendbuf, 8*len, FWD_NBR);
  unsigned long send_request2 = comm_send(sitelink_back_sendbuf, 8*len, BACK_NBR);
  comm_wait(recv_request1);
  comm_wait(recv_request2);
  comm_wait(send_request1);
  comm_wait(send_request2);
}

//this function is used for link fattening computation
void exchange_cpu_sitelink(int* X,
			   void** sitelink, void* ghost_sitelink,
			   QudaPrecision gPrecision)
{
  

  V = 1;
  for (int d=0; d< 4; d++) {
    V *= X[d];
    dims[d] = X[d];
  }
  Vh = V/2;

  Vs = X[0]*X[1]*X[2];
  Vsh = Vs/2;

  void*  sitelink_fwd_sendbuf = malloc(4*Vs*gaugeSiteSize*gPrecision);
  void*  sitelink_back_sendbuf = malloc(4*Vs*gaugeSiteSize*gPrecision);
  if (sitelink_fwd_sendbuf == NULL|| sitelink_back_sendbuf == NULL){
    printf("ERROR: malloc failed for sitelink_sendbuf/site_link_back_sendbuf\n");
    exit(1);
  }
  
  if (gPrecision == QUDA_DOUBLE_PRECISION){
    exchange_sitelink((double**)sitelink, (double*)ghost_sitelink, 
		      (double*)sitelink_fwd_sendbuf, (double*)sitelink_back_sendbuf);
  }else{ //single
    exchange_sitelink((float**)sitelink, (float*)ghost_sitelink, 
		      (float*)sitelink_fwd_sendbuf, (float*)sitelink_back_sendbuf);
  }

  free(sitelink_fwd_sendbuf);
  free(sitelink_back_sendbuf);

}


template<typename Float>
void
exchange_staple(Float* staple, Float* ghost_staple, Float* staple_fwd_sendbuf, Float* staple_back_sendbuf)
{
  
  int len = Vsh*gaugeSiteSize*sizeof(Float);

  Float* even_staple_back_src = staple;
  Float* odd_staple_back_src = staple + Vh*gaugeSiteSize;
  Float* staple_back_dst = staple_back_sendbuf;
  
  if(dims[3] % 2 == 0){    
    memcpy(staple_back_dst, even_staple_back_src, len);
    memcpy(staple_back_dst + Vsh*gaugeSiteSize, odd_staple_back_src, len);
  }else{
    //switching odd and even ghost staple
    memcpy(staple_back_dst, odd_staple_back_src, len);
    memcpy(staple_back_dst + Vsh*gaugeSiteSize, even_staple_back_src, len);
  }
  
  
  Float* even_staple_fwd_src = staple + (Vh - Vsh)*gaugeSiteSize;
  Float* odd_staple_fwd_src = staple + Vh*gaugeSiteSize + (Vh - Vsh)*gaugeSiteSize;
  Float* staple_fwd_dst = staple_fwd_sendbuf;
  if(dims[3] % 2 == 0){    
    memcpy(staple_fwd_dst, even_staple_fwd_src, len);
    memcpy(staple_fwd_dst + Vsh*gaugeSiteSize, odd_staple_fwd_src, len);
  }else{
    //switching odd and even ghost staple
    memcpy(staple_fwd_dst, odd_staple_fwd_src, len);
    memcpy(staple_fwd_dst + Vsh*gaugeSiteSize, even_staple_fwd_src, len);
  }
  
  
  Float* ghost_staple_back = ghost_staple;
  Float* ghost_staple_fwd = ghost_staple + 2*Vsh*gaugeSiteSize;

  unsigned long recv_request1 = comm_recv(ghost_staple_back, 2*len, BACK_NBR);
  unsigned long recv_request2 = comm_recv(ghost_staple_fwd, 2*len, FWD_NBR);
  unsigned long send_request1 = comm_send(staple_fwd_sendbuf, 2*len, FWD_NBR);
  unsigned long send_request2 = comm_send(staple_back_sendbuf, 2*len, BACK_NBR);
  comm_wait(recv_request1);
  comm_wait(recv_request2);
  comm_wait(send_request1);
  comm_wait(send_request2);
}
//this function is used for link fattening computation
void exchange_cpu_staple(int* X,
			 void* staple, void* ghost_staple,
			 QudaPrecision gPrecision)
{
  
  
  V = 1;
  for (int d=0; d< 4; d++) {
    V *= X[d];
    dims[d] = X[d];
  }
  Vh = V/2;
  
  Vs = X[0]*X[1]*X[2];
  Vsh = Vs/2;
  
  void*  staple_fwd_sendbuf = malloc(Vs*gaugeSiteSize*gPrecision);
  void*  staple_back_sendbuf = malloc(Vs*gaugeSiteSize*gPrecision);
  if (staple_fwd_sendbuf == NULL|| staple_back_sendbuf == NULL){
    printf("ERROR: malloc failed for staple_sendbuf/site_link_back_sendbuf\n");
    exit(1);
  }
  
  if (gPrecision == QUDA_DOUBLE_PRECISION){
    exchange_staple((double*)staple, (double*)ghost_staple, 
		    (double*)staple_fwd_sendbuf, (double*)staple_back_sendbuf);
  }else{ //single
    exchange_staple((float*)staple, (float*)ghost_staple, 
		    (float*)staple_fwd_sendbuf, (float*)staple_back_sendbuf);
  }
  
  free(staple_fwd_sendbuf);
  free(staple_back_sendbuf);
}

void exchange_cpu_links(int* X,
			void** fatlink, void* ghost_fatlink, 
			void** longlink, void* ghost_longlink,
			QudaPrecision gPrecision)
{
  
  
  V = 1;
  for (int d=0; d< 4; d++) {
    V *= X[d];
    dims[d] = X[d];
  }
  Vh = V/2;
  
  Vs = X[0]*X[1]*X[2];
  Vsh = Vs/2;
  
  void*  fatlink_sendbuf = malloc(Vs*gaugeSiteSize*gPrecision);
  void*  longlink_sendbuf = malloc(3*Vs*gaugeSiteSize*gPrecision);
  if (fatlink_sendbuf == NULL || longlink_sendbuf == NULL){
    printf("ERROR: malloc failed for fatlink_sendbuf/longlink_sendbuf\n");
    exit(1);
  }
  
 
  if (gPrecision == QUDA_DOUBLE_PRECISION){
    exchange_fatlink((double**)fatlink, (double*)ghost_fatlink, (double*)fatlink_sendbuf);
    exchange_longlink((double**)longlink, (double*)ghost_longlink, (double*)longlink_sendbuf);
  }else{ //single
    exchange_fatlink((float**)fatlink, (float*)ghost_fatlink, (float*)fatlink_sendbuf);
    exchange_longlink((float**)longlink, (float*)ghost_longlink, (float*)longlink_sendbuf);    
  }
  
  free(fatlink_sendbuf);
  free(longlink_sendbuf);

}


void exchange_cpu_spinor(int* X,
			 void* spinorField, void* fwd_nbr_spinor, void* back_nbr_spinor,
			 QudaPrecision sPrecision)
{
  
  V = 1;
  for (int d=0; d< 4; d++) {
    V *= X[d];
    dims[d] = X[d];
  }
  Vh = V/2;
  
  Vs = X[0]*X[1]*X[2];
  Vsh = Vs/2;

  if (sPrecision == QUDA_DOUBLE_PRECISION){
    exchange_cpu_spinor((double*)spinorField, (double*)fwd_nbr_spinor, (double*)back_nbr_spinor);
  }else{//single
    exchange_cpu_spinor((float*)spinorField, (float*)fwd_nbr_spinor, (float*)back_nbr_spinor);    
  }

}

/**************************************************************
 * Staple exchange routine
 * used in fat link computation
 */

static void* fwd_nbr_staple_cpu = NULL;
static void* back_nbr_staple_cpu = NULL;
static void* fwd_nbr_staple_sendbuf_cpu = NULL;
static void* back_nbr_staple_sendbuf_cpu = NULL;

static void* fwd_nbr_staple = NULL;
static void* back_nbr_staple = NULL;
static void* fwd_nbr_staple_sendbuf = NULL;
static void* back_nbr_staple_sendbuf = NULL;

void 
exchange_llfat_init(FullStaple* cudaStaple)
{
  static int initialized = 0;
  if (initialized){
    return;
  }
  initialized = 1;
  
  QudaPrecision prec = cudaStaple->precision;

  cudaMallocHost((void**)&fwd_nbr_staple, Vs*gaugeSiteSize*prec);
  cudaMallocHost((void**)&back_nbr_staple, Vs*gaugeSiteSize*prec);
  cudaMallocHost((void**)&fwd_nbr_staple_sendbuf, Vs*gaugeSiteSize*prec);
  cudaMallocHost((void**)&back_nbr_staple_sendbuf, Vs*gaugeSiteSize*prec);

  CUERR;

  fwd_nbr_staple_cpu = malloc(Vs*gaugeSiteSize*prec);
  back_nbr_staple_cpu = malloc(Vs*gaugeSiteSize*prec);
  if (fwd_nbr_staple_cpu == NULL||back_nbr_staple_cpu == NULL){
    printf("ERROR: malloc failed for fwd_nbr_staple/back_nbr_staple\n");
    comm_exit(1);
  }
  
  fwd_nbr_staple_sendbuf_cpu = malloc(Vs*gaugeSiteSize*prec);
  back_nbr_staple_sendbuf_cpu = malloc(Vs*gaugeSiteSize*prec);
  if (fwd_nbr_staple_sendbuf_cpu == NULL || back_nbr_staple_sendbuf_cpu == NULL){
    printf("ERROR: malloc failed for fwd_nbr_staple_sendbuf/back_nbr_staple_sendbuf\n");
    comm_exit(1);
  }
  
  return;
}

void
exchange_gpu_staple(int* X, void* _cudaStaple, cudaStream_t * stream)
{
  setup_dims(X);
  
  FullStaple* cudaStaple = (FullStaple*) _cudaStaple;
  exchange_llfat_init(cudaStaple);
  
  int len = Vs*gaugeSiteSize*cudaStaple->precision;
  int normlen = Vs*sizeof(float);
  
  packGhostStaple(cudaStaple, fwd_nbr_staple_sendbuf, back_nbr_staple_sendbuf, f_norm_sendbuf, b_norm_sendbuf, stream);
  cudaStreamSynchronize(*stream);
  

  unsigned long recv_request1 = comm_recv(back_nbr_staple_cpu, len, BACK_NBR);
  unsigned long recv_request2 = comm_recv(fwd_nbr_staple_cpu, len, FWD_NBR);
  
  memcpy(fwd_nbr_staple_sendbuf_cpu, fwd_nbr_staple_sendbuf, len);
  memcpy(back_nbr_staple_sendbuf_cpu, back_nbr_staple_sendbuf, len);

  unsigned long send_request1= comm_send(fwd_nbr_staple_sendbuf_cpu, len, FWD_NBR);
  unsigned long send_request2 = comm_send(back_nbr_staple_sendbuf_cpu, len, BACK_NBR);

  unsigned long recv_request3 = 0;
  unsigned long recv_request4 = 0;
  unsigned long send_request3 = 0;
  unsigned long send_request4 = 0;
  
  if (cudaStaple->precision == QUDA_HALF_PRECISION){
    recv_request3 = comm_recv(b_norm, normlen, BACK_NBR);
    recv_request4 = comm_recv(f_norm, normlen, FWD_NBR);
    send_request3 = comm_send(f_norm_sendbuf, normlen, FWD_NBR);
    send_request4 = comm_send(b_norm_sendbuf, normlen, BACK_NBR);
  }
  
  
  
  comm_wait(recv_request1);
  comm_wait(recv_request2);  
  comm_wait(send_request1);
  comm_wait(send_request2);
  
  if (cudaStaple->precision == QUDA_HALF_PRECISION){
    comm_wait(recv_request3);
    comm_wait(recv_request4);
    comm_wait(send_request3);
    comm_wait(send_request4);
  }
  
  memcpy(fwd_nbr_staple, fwd_nbr_staple_cpu, len);
  memcpy(back_nbr_staple, back_nbr_staple_cpu, len);

  unpackGhostStaple(cudaStaple, fwd_nbr_staple, back_nbr_staple, f_norm, b_norm, stream);
  cudaStreamSynchronize(*stream);
}
void
exchange_gpu_staple_start(int* X, void* _cudaStaple, cudaStream_t * stream)
{
  setup_dims(X);
  
  FullStaple* cudaStaple = (FullStaple*) _cudaStaple;
  exchange_llfat_init(cudaStaple);
  
  packGhostStaple(cudaStaple, fwd_nbr_staple_sendbuf, back_nbr_staple_sendbuf, f_norm_sendbuf, b_norm_sendbuf, stream);
}

void
exchange_gpu_staple_wait(int* X, void* _cudaStaple, cudaStream_t * stream)
{
  FullStaple* cudaStaple = (FullStaple*) _cudaStaple;  
  int len = Vs*gaugeSiteSize*cudaStaple->precision;
  int normlen = Vs*sizeof(float);
  
  cudaStreamSynchronize(*stream);  

  unsigned long recv_request1 = comm_recv(back_nbr_staple_cpu, len, BACK_NBR);
  unsigned long recv_request2 = comm_recv(fwd_nbr_staple_cpu, len, FWD_NBR);
  
  memcpy(fwd_nbr_staple_sendbuf_cpu, fwd_nbr_staple_sendbuf, len);
  memcpy(back_nbr_staple_sendbuf_cpu, back_nbr_staple_sendbuf, len);

  unsigned long send_request1= comm_send(fwd_nbr_staple_sendbuf_cpu, len, FWD_NBR);
  unsigned long send_request2 = comm_send(back_nbr_staple_sendbuf_cpu, len, BACK_NBR);

  unsigned long recv_request3 = 0;
  unsigned long recv_request4 = 0;
  unsigned long send_request3 = 0;
  unsigned long send_request4 = 0;
  
  if (cudaStaple->precision == QUDA_HALF_PRECISION){
    recv_request3 = comm_recv(b_norm, normlen, BACK_NBR);
    recv_request4 = comm_recv(f_norm, normlen, FWD_NBR);
    send_request3 = comm_send(f_norm_sendbuf, normlen, FWD_NBR);
    send_request4 = comm_send(b_norm_sendbuf, normlen, BACK_NBR);
  }
  
  
  
  comm_wait(recv_request1);
  comm_wait(recv_request2);  
  comm_wait(send_request1);
  comm_wait(send_request2);
  
  if (cudaStaple->precision == QUDA_HALF_PRECISION){
    comm_wait(recv_request3);
    comm_wait(recv_request4);
    comm_wait(send_request3);
    comm_wait(send_request4);
  }
  
  memcpy(fwd_nbr_staple, fwd_nbr_staple_cpu, len);
  memcpy(back_nbr_staple, back_nbr_staple_cpu, len);

  unpackGhostStaple(cudaStaple, fwd_nbr_staple, back_nbr_staple, f_norm, b_norm, stream);
  cudaStreamSynchronize(*stream);
}


static void
exchange_llfat_cleanup(void)
{
  
  free(fwd_nbr_staple_cpu);      
  free(back_nbr_staple_cpu);
  free(fwd_nbr_staple_sendbuf_cpu);
  free(back_nbr_staple_sendbuf_cpu);    
  cudaFreeHost(fwd_nbr_staple);
  cudaFreeHost(back_nbr_staple);
  cudaFreeHost(fwd_nbr_staple_sendbuf);
  cudaFreeHost(back_nbr_staple_sendbuf);
  

}

/*****************************************************************/ 

void exchange_cleanup()
{  
  exchange_llfat_cleanup();
  exchange_dslash_cleanup();
}
