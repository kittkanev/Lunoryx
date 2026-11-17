#pragma once
#include "src/io/sha256.h"

inline void EraseSecret(void* value,size_t bytes) {
  auto out=static_cast<volatile unsigned char*>(value);
  while(bytes--)*out++=0;
}
inline std::array<unsigned char,32> Sha256Bytes(const Sha256& hash) {
  const auto hex=hash.Finish();std::array<unsigned char,32> bytes={};
  auto nibble=[](char c){return c>='0'&&c<='9'?c-'0':c-'a'+10;};
  for(size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<unsigned char>((nibble(hex[2*i])<<4)|nibble(hex[2*i+1]));
  return bytes;
}
struct HmacSha256 {
  Sha256 Inner,Outer;
  HmacSha256(const void* secret,size_t size) {
    std::array<unsigned char,64> key={};
    if(size>key.size()){Sha256 hash;hash.Add(secret,size);auto digest=Sha256Bytes(hash);std::copy(digest.begin(),digest.end(),key.begin());EraseSecret(digest.data(),digest.size());}
    else if(size)std::memcpy(key.data(),secret,size);
    std::array<unsigned char,64> pad;
    for(size_t i=0;i<pad.size();++i)pad[i]=key[i]^0x36;
    Inner.Add(pad.data(),pad.size());
    for(size_t i=0;i<pad.size();++i)pad[i]=key[i]^0x5c;
    Outer.Add(pad.data(),pad.size());
    EraseSecret(key.data(),key.size());EraseSecret(pad.data(),pad.size());
  }
  HmacSha256(const HmacSha256&)=delete;
  HmacSha256& operator=(const HmacSha256&)=delete;
  ~HmacSha256(){EraseSecret(&Inner,sizeof(Inner));EraseSecret(&Outer,sizeof(Outer));}
  void Add(const void* bytes,size_t size){Inner.Add(bytes,size);}
  std::array<unsigned char,32> Finish()const {
    auto digest=Sha256Bytes(Inner);auto outer=Outer;outer.Add(digest.data(),digest.size());
    auto result=Sha256Bytes(outer);EraseSecret(digest.data(),digest.size());EraseSecret(&outer,sizeof(outer));return result;
  }
};
inline bool EqualAuthenticationTag(const unsigned char* a,const unsigned char* b,size_t size) {
  volatile unsigned difference=0;
  for(size_t i=0;i<size;++i)difference|=unsigned(a[i]^b[i]);
  return difference==0;
}
