#include "src/io/asset_io.h"
#include "src/scripting/worker_process.h"
#include <cassert>
#include <iostream>

int main(int argc,char** argv){
    if(argc>1&&std::strcmp(argv[1],"--echo")==0){
        SetNativeWorkerLimits();std::string input;char block[16384];
        while(std::fgets(block,sizeof(block),stdin)){size_t n=std::strlen(block);input.append(block,n);if(n&&block[n-1]=='\n')break;}
        std::cout<<input<<std::flush;return 0;
    }
    if(argc>1&&std::strcmp(argv[1],"--hang")==0){SetNativeWorkerLimits();for(;;)std::this_thread::sleep_for(std::chrono::seconds(1));}
    const auto executable=AbsoluteAssetPath(argv[0]);
    WorkerProcess worker;worker.Start(executable,{"--echo"});
    std::string payload(32*1024*1024,'x');auto reply=worker.Exchange(payload,3000,40*1024*1024);
    assert(reply==payload);worker.Stop();
    worker.Start(executable,{"--hang"});auto start=std::chrono::steady_clock::now();bool cancelled=false;
    try{worker.Exchange("{}",100);}catch(const std::runtime_error& error){cancelled=std::string(error.what()).find("deadline")!=std::string::npos;}
    assert(cancelled&&!worker.Running());assert(std::chrono::steady_clock::now()-start<std::chrono::seconds(3));
    std::cout<<"PASS: 32 MiB native worker transfer and deadline-killed unresponsive worker\n";
}
