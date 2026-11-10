#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>
struct Sha256 {
  std::array<uint32_t,8> State={{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
  unsigned char Block[64]={};size_t Used=0;uint64_t Length=0;
  static uint32_t R(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
  void Transform(){static const uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};uint32_t w[64];for(int i=0;i<16;++i)w[i]=(uint32_t(Block[4*i])<<24)|(uint32_t(Block[4*i+1])<<16)|(uint32_t(Block[4*i+2])<<8)|Block[4*i+3];for(int i=16;i<64;++i){auto a=w[i-15],b=w[i-2];w[i]=w[i-16]+(R(a,7)^R(a,18)^(a>>3))+w[i-7]+(R(b,17)^R(b,19)^(b>>10));}auto a=State[0],b=State[1],c=State[2],d=State[3],e=State[4],f=State[5],g=State[6],h=State[7];for(int i=0;i<64;++i){auto t1=h+(R(e,6)^R(e,11)^R(e,25))+((e&f)^(~e&g))+k[i]+w[i];auto t2=(R(a,2)^R(a,13)^R(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}State[0]+=a;State[1]+=b;State[2]+=c;State[3]+=d;State[4]+=e;State[5]+=f;State[6]+=g;State[7]+=h;}
  void Add(const void* source,size_t size){auto data=static_cast<const unsigned char*>(source);Length+=size;while(size){size_t n=std::min(size_t(64)-Used,size);std::memcpy(Block+Used,data,n);Used+=n;data+=n;size-=n;if(Used==64){Transform();Used=0;}}}
  std::string Finish()const{auto copy=*this;uint64_t bits=Length*8;unsigned char one=0x80,zero=0;copy.Add(&one,1);while(copy.Used!=56)copy.Add(&zero,1);unsigned char length[8];for(int i=0;i<8;++i)length[i]=static_cast<unsigned char>(bits>>(56-8*i));copy.Add(length,8);char text[65];for(int i=0;i<8;++i)std::snprintf(text+i*8,9,"%08x",copy.State[i]);return text;}
};
