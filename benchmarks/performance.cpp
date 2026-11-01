#include "../scene_tools.h"
#define LUNORYX_KERNEL_BENCHMARK
#include "../extensions/rust-kernels/kernels.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cassert>
#include <array>
extern "C" uint32_t lunoryx_crc32_reference(const unsigned char*,size_t);
extern "C" void lunoryx_bounds_reference(const float*,size_t,float*);
static volatile uint64_t sink=0;
static uint32_t CRC(const unsigned char* data,size_t n){uint32_t crc=~uint32_t(0);for(size_t i=0;i<n;++i){crc^=data[i];for(int k=0;k<8;++k)crc=(crc>>1)^(0xedb88320u&uint32_t(-int(crc&1)));}return ~crc;}
static uint32_t FastCRC(const unsigned char* data,size_t n){
 static const auto tables=[](){std::array<std::array<uint32_t,256>,8> t={};for(int i=0;i<256;++i){uint32_t c=i;for(int k=0;k<8;++k)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));t[0][i]=c;}for(int k=1;k<8;++k)for(int i=0;i<256;++i){auto c=t[k-1][i];t[k][i]=(c>>8)^t[0][c&255];}return t;}();
 uint32_t crc=~uint32_t(0);size_t i=0;for(;i+8<=n;i+=8){uint32_t a=crc^uint32_t(data[i])^(uint32_t(data[i+1])<<8)^(uint32_t(data[i+2])<<16)^(uint32_t(data[i+3])<<24);crc=tables[7][a&255]^tables[6][(a>>8)&255]^tables[5][(a>>16)&255]^tables[4][a>>24]^tables[3][data[i+4]]^tables[2][data[i+5]]^tables[1][data[i+6]]^tables[0][data[i+7]];}for(;i<n;++i)crc=(crc>>8)^tables[0][(crc^data[i])&255];return ~crc;
}
static std::vector<uint32_t> WeldReference(const std::vector<float>& p,const std::vector<uint32_t>& selected,float tolerance){std::map<uint32_t,uint32_t> remap;std::vector<uint32_t> out;for(auto v:selected){remap[v]=v;for(const auto& r:remap)if(r.first!=v){Vec3 d=Subtract({p[v*3],p[v*3+1],p[v*3+2]},{p[r.first*3],p[r.first*3+1],p[r.first*3+2]});if(Dot(d,d)<=double(tolerance)*tolerance){remap[v]=r.second;break;}}out.push_back(remap[v]);}return out;}
template<class F> static double Measure(F f,int count){std::vector<double> samples;f();for(int run=0;run<5;++run){auto start=std::chrono::steady_clock::now();for(int i=0;i<count;++i){asm volatile("" ::: "memory");f();}auto end=std::chrono::steady_clock::now();samples.push_back(std::chrono::duration<double,std::milli>(end-start).count()/count);}std::sort(samples.begin(),samples.end());return samples[2];}
static void Row(const char* scenario,const char* implementation,double ms,size_t n){std::cout<<scenario<<","<<implementation<<","<<n<<","<<std::fixed<<std::setprecision(6)<<ms<<"\n";}
int main(){std::cout<<"scenario,implementation,items,median_ms\n";
 for(size_t n:{size_t(1024),size_t(16*1024*1024)}){std::vector<unsigned char> bytes(n);uint32_t seed=73;for(auto& b:bytes){seed=seed*1664525u+1013904223u;b=seed>>24;}auto expected=CRC(bytes.data(),n);assert(expected==lunoryx_crc32(bytes.data(),n)&&expected==FastCRC(bytes.data(),n));int repeats=n>1024?2:1000;Row("crc32","cpp_1.0.1",Measure([&](){sink=CRC(bytes.data(),n);},repeats),n);Row("crc32","rust_direct_port",Measure([&](){sink=lunoryx_crc32_reference(bytes.data(),n);},repeats),n);Row("crc32","cpp_same_algorithm_control",Measure([&](){sink=FastCRC(bytes.data(),n);},repeats),n);Row("crc32","rust_slicing8_ffi",Measure([&](){sink=lunoryx_crc32(bytes.data(),n);},repeats),n);}
 for(size_t count:{size_t(1000),size_t(10000)}){std::vector<float> p;std::vector<uint32_t> selected;for(size_t i=0;i<count;++i){p.insert(p.end(),{float(i%100)*.1f,float(i/100)*.1f,0});selected.push_back(uint32_t(i));}std::vector<LunoryxWeldCell> work(count);std::vector<uint32_t> out(count);auto old=WeldReference(p,selected,.001f);assert(!lunoryx_weld_map(p.data(),count,selected.data(),count,.001f,work.data(),out.data())&&out==old);Row("weld_map","cpp_1.0.1",Measure([&](){sink=WeldReference(p,selected,.001f).back();},1),count);Row("weld_map","rust_grid_ffi",Measure([&](){std::vector<LunoryxWeldCell> temporary(count);std::vector<uint32_t> mapped(count);assert(!lunoryx_weld_map(p.data(),count,selected.data(),count,.001f,temporary.data(),mapped.data()));sink=mapped.back();},3),count);}
 GameObject o("Bounds");for(size_t i=0;i<200000;++i)o.Vertices.insert(o.Vertices.end(),{float(i%123),float(i%31),float(i%17)});float bounds[6];Bounds b;Row("bounds_scan","cpp_1.0.1",Measure([&](){b.Min=b.Max={o.Vertices[0],o.Vertices[1],o.Vertices[2]};for(size_t i=3;i<o.Vertices.size();i+=3){b.Min.x=std::min(b.Min.x,o.Vertices[i]);b.Max.x=std::max(b.Max.x,o.Vertices[i]);b.Min.y=std::min(b.Min.y,o.Vertices[i+1]);b.Max.y=std::max(b.Max.y,o.Vertices[i+1]);b.Min.z=std::min(b.Min.z,o.Vertices[i+2]);b.Max.z=std::max(b.Max.z,o.Vertices[i+2]);}sink=uint64_t(b.Max.x);},100),200000);Row("bounds_scan","rust_direct_port_ffi",Measure([&](){lunoryx_bounds_reference(o.Vertices.data(),200000,bounds);sink=uint64_t(bounds[3]);},100),200000);
}
