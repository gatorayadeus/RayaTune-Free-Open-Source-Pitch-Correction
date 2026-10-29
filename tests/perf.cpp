#include <functional>
#include <chrono>
#include <cstdio>
#include "AutoTuneEngine.h"
int main(){
  double sr=48000; size_t N=(size_t)(sr*10);
  std::vector<float> x(N); double ph=0;
  for(size_t i=0;i<N;++i){ double t=i/sr; double f=200*std::pow(2,(t*0.15))*(1+0.012*std::sin(2*M_PI*5.5*t)); ph+=2*M_PI*f/sr;
    double s=0; for(int h=1;h<=10;++h) s+=std::sin(h*ph)/h; x[i]=0.25f*(float)s*(t<9.5?1.f:0.f);}
  AutoTuneEngine e; e.prepare(sr,1); TuneParams p; p.retuneMs=10; e.setParams(p);
  auto y=x; auto t0=std::chrono::steady_clock::now();
  for(size_t i=0;i+256<=N;i+=256){ float* b[1]={y.data()+i}; e.process(b,1,256);}
  double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
  double maxStep=0, maxIn=0; for(size_t i=1;i<N;++i){ maxStep=std::max(maxStep,(double)std::abs(y[i]-y[i-1])); maxIn=std::max(maxIn,(double)std::abs(x[i]-x[i-1]));}
  std::printf("CPU: %.2f s de audio en %.3f s -> %.1f%% de un nucleo\n",10.0,sec,100*sec/10);
  std::printf("max salto muestra a muestra: salida=%.3f entrada=%.3f (ratio %.2f)\n",maxStep,maxIn,maxStep/maxIn);
}
