#pragma once
// Exact Task034 row-vector construction, performed locally from the consumed b1 bytes.
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace eye036 {
using Matrix=std::array<double,16>;
using MatrixBytes=std::array<unsigned char,256>;
inline Matrix identity(){Matrix m{};for(unsigned i=0;i<4;++i)m[i*4+i]=1;return m;}
inline Matrix mul(const Matrix&a,const Matrix&b){Matrix r{};for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)for(unsigned k=0;k<4;++k)r[y*4+x]+=a[y*4+k]*b[k*4+x];return r;}
inline Matrix inverse(const Matrix&m){
 double a[4][8]{};for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){a[y][x]=m[y*4+x];a[y][x+4]=y==x?1:0;}
 for(unsigned k=0;k<4;++k){unsigned p=k;for(unsigned y=k+1;y<4;++y)if(std::abs(a[y][k])>std::abs(a[p][k]))p=y;
  if(!std::isfinite(a[p][k])||std::abs(a[p][k])<1e-12)throw std::runtime_error("SINGULAR_MATRIX");
  for(unsigned x=0;x<8;++x)std::swap(a[p][x],a[k][x]);double d=a[k][k];for(double&v:a[k])v/=d;
  for(unsigned y=0;y<4;++y)if(y!=k){d=a[y][k];for(unsigned x=0;x<8;++x)a[y][x]-=d*a[k][x];}}
 Matrix out{};for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)out[y*4+x]=a[y][x+4];return out;
}
inline MatrixBytes matrices(const MatrixBytes&raw,double eye){
 if(!std::isfinite(eye)||std::abs(eye)>.032000001)throw std::runtime_error("EYE_BOUND");
 std::array<Matrix,4> m{};for(unsigned j=0;j<64;++j){float f;std::memcpy(&f,raw.data()+j*4,4);if(!std::isfinite(f))throw std::runtime_error("NONFINITE_MATRIX");m[j/16][j%16]=f;}
 if(eye==0)return raw;
 auto view=mul(inverse(m[0]),m[1]);auto projection=mul(inverse(m[1]),m[2]);auto t=identity();t[12]=-eye;
 view=mul(view,t);m[1]=mul(m[0],view);m[2]=mul(m[1],projection);m[3]=inverse(view);
 MatrixBytes out{};for(unsigned j=0;j<64;++j){float f=static_cast<float>(m[j/16][j%16]);if(!std::isfinite(f))throw std::runtime_error("NONFINITE_EYE_MATRIX");std::memcpy(out.data()+j*4,&f,4);}return out;
}
} // namespace eye036
