#pragma once
// Exact815 uses World/WVP/ViewInverse, not unused WorldView. Rigid path is unchanged.
#include "eye_matrices.h"
namespace consumer055 {
using namespace eye036;
enum class Kind {Rigid,Skeletal815};
inline std::array<Matrix,4> unpack(const MatrixBytes&raw){
 std::array<Matrix,4>m{};for(unsigned i=0;i<64;++i){float f;memcpy(&f,raw.data()+4*i,4);if(!std::isfinite(f))throw std::runtime_error("NONFINITE_MATRIX");m[i/16][i%16]=f;}return m;
}
inline std::pair<Matrix,Matrix> camera(const MatrixBytes&raw,Kind kind){
 auto m=unpack(raw);if(kind==Kind::Rigid)return {mul(inverse(m[0]),m[1]),mul(inverse(m[1]),m[2])};
 return {inverse(m[3]),mul(m[3],mul(inverse(m[0]),m[2]))};
}
inline MatrixBytes matrices(const MatrixBytes&raw,double eye,Kind kind){
 if(kind==Kind::Rigid)return eye036::matrices(raw,eye);
 if(!std::isfinite(eye)||std::abs(eye)>.032000001)throw std::runtime_error("EYE_BOUND");
 auto m=unpack(raw);if(eye==0)return raw;auto[v,p]=camera(raw,kind);auto t=identity();t[12]=-eye;v=mul(v,t);
 m[2]=mul(mul(m[0],v),p);m[3]=inverse(v);MatrixBytes out=raw;
 // Preserve World and unused WorldView byte-exact, including signed zeros.
 for(unsigned i=32;i<64;++i){float f=static_cast<float>(m[i/16][i%16]);if(!std::isfinite(f))throw std::runtime_error("NONFINITE_EYE_MATRIX");memcpy(out.data()+4*i,&f,4);}return out;
}
inline double basis(const Matrix&a,const Matrix&b){double e=0;for(unsigned y=0;y<3;++y)for(unsigned x=0;x<3;++x)e=std::max(e,std::abs(a[4*y+x]-b[4*y+x]));return e;}
inline double position(const Matrix&a,const Matrix&b){double e=0;for(unsigned i=12;i<15;++i)e+=(a[i]-b[i])*(a[i]-b[i]);return std::sqrt(e);}
struct Pose {
 bool started=false;Matrix view{},projection{},vi{};
 const char* accept(const MatrixBytes&raw,Kind kind){
  try{
   auto m=unpack(raw);auto[v,p]=camera(raw,kind);auto current=inverse(v);
   if(kind==Kind::Rigid&&(basis(current,m[3])>5e-5||position(current,m[3])>.01))return "GPU_VIEWINVERSE_INCOHERENT";
   if(!started){if(kind!=Kind::Rigid)return "MISSING_RIGID_WITNESS";started=true;view=v;projection=p;vi=current;return nullptr;}
   double pe=0;for(unsigned i=0;i<16;++i)pe=std::max(pe,std::abs(p[i]-projection[i]));
   if(basis(v,view)>1e-5||position(current,vi)>.001||pe>1e-5)return "GPU_POSE_CHANGED";
   return nullptr;
  }catch(const std::exception&){return "SINGULAR_OR_NONFINITE_GPU_MATRIX";}
 }
};
}
