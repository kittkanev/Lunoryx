#include "src/io/hmac_sha256.h"
#include <cassert>
#include <iostream>
#include <vector>
static std::string Hex(const std::array<unsigned char,32>& data){char out[65];for(size_t i=0;i<data.size();++i)std::snprintf(out+i*2,3,"%02x",data[i]);return out;}
int main(){
  {std::vector<unsigned char> key(20,0x0b);HmacSha256 mac(key.data(),key.size());mac.Add("Hi There",8);assert(Hex(mac.Finish())=="b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");}
  {HmacSha256 mac("Jefe",4);mac.Add("what do ya want for nothing?",28);assert(Hex(mac.Finish())=="5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");}
  {std::vector<unsigned char> key(20,0xaa),data(50,0xdd);HmacSha256 mac(key.data(),key.size());mac.Add(data.data(),data.size());assert(Hex(mac.Finish())=="773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");}
  {std::vector<unsigned char> key(131,0xaa);const std::string text="Test Using Larger Than Block-Size Key - Hash Key First";HmacSha256 mac(key.data(),key.size());mac.Add(text.data(),text.size());assert(Hex(mac.Finish())=="60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");}
  for(size_t count:{size_t(0),size_t(1),size_t(55),size_t(56),size_t(63),size_t(64),size_t(65),size_t(1024*1024+1)}){
    std::vector<unsigned char> data(count,0xe1);HmacSha256 whole("chunk key",9),chunks("chunk key",9);whole.Add(data.data(),data.size());
    for(size_t at=0;at<count;){auto n=std::min(size_t(17),count-at);chunks.Add(data.data()+at,n);at+=n;}
    auto a=whole.Finish(),b=chunks.Finish();assert(EqualAuthenticationTag(a.data(),b.data(),a.size()));
    for(size_t index=0;index<a.size();++index){auto bad=a;bad[index]^=1;assert(!EqualAuthenticationTag(a.data(),bad.data(),a.size()));}
  }
  std::cout<<"PASS: RFC 4231 HMAC-SHA-256 vectors, long key, streamed chunks and tag corruption\n";
}
