// MinGW libstdc++ types carry hidden visibility; this is not a DLL interface.
#if defined(__GNUC__) && defined(_WIN32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif
#pragma once
#include <chrono>
#include <atomic>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <spawn.h>
extern char** environ;
#endif
// Dedicated child processes isolate uncooperative native/managed code. No shell
// interprets executable paths or user input. All exchanges have byte/deadline caps.
class WorkerProcess {
#ifdef _WIN32
  HANDLE Process=nullptr,Input=nullptr,Output=nullptr;
#else
  pid_t Process=-1;int Input=-1,Output=-1;
#endif
  std::string Pending;
public:
  WorkerProcess()=default;WorkerProcess(const WorkerProcess&)=delete;WorkerProcess& operator=(const WorkerProcess&)=delete;
  ~WorkerProcess(){Stop();}
  bool Running()const{
#ifdef _WIN32
    return Process!=nullptr;
#else
    return Process>0;
#endif
  }
  void Stop(){
#ifdef _WIN32
    if(Process){TerminateProcess(Process,1);WaitForSingleObject(Process,2000);CloseHandle(Process);Process=nullptr;}if(Input){CloseHandle(Input);Input=nullptr;}if(Output){CloseHandle(Output);Output=nullptr;}
#else
    if(Process>0){kill(Process,SIGKILL);int status=0;while(waitpid(Process,&status,0)<0&&errno==EINTR){}Process=-1;}if(Input>=0){close(Input);Input=-1;}if(Output>=0){close(Output);Output=-1;}
#endif
    Pending.clear();
  }
  void Start(const std::string& executable,const std::vector<std::string>& arguments={}){
    Stop();
#ifdef _WIN32
    SECURITY_ATTRIBUTES security={sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};HANDLE childInput=nullptr,childOutput=nullptr;
    if(!CreatePipe(&childInput,&Input,&security,0)||!CreatePipe(&Output,&childOutput,&security,0)){if(childInput)CloseHandle(childInput);if(childOutput)CloseHandle(childOutput);Stop();throw std::runtime_error("Cannot create worker pipes");}
    SetHandleInformation(Input,HANDLE_FLAG_INHERIT,0);SetHandleInformation(Output,HANDLE_FLAG_INHERIT,0);
    auto quote=[](const std::string& value){std::wstring result=L"\"";size_t slashes=0;for(wchar_t c:WidePath(value)){if(c==L'\\'){++slashes;continue;}if(c==L'\"')result.append(slashes*2+1,L'\\');else result.append(slashes,L'\\');slashes=0;result+=c;}result.append(slashes*2,L'\\');return result+L"\"";};
    auto command=quote(executable);for(const auto& arg:arguments)command+=L" "+quote(arg);
    STARTUPINFOW startup={};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdInput=childInput;startup.hStdOutput=childOutput;startup.hStdError=GetStdHandle(STD_ERROR_HANDLE);PROCESS_INFORMATION info={};
    bool ok=CreateProcessW(WidePath(executable).c_str(),&command[0],nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&info)!=0;CloseHandle(childInput);CloseHandle(childOutput);
    if(!ok){Stop();throw std::runtime_error("Cannot start script/geometry worker: "+executable);}Process=info.hProcess;CloseHandle(info.hThread);
#else
    int input[2],output[2];if(pipe(input))throw std::runtime_error("Cannot create worker input");if(pipe(output)){close(input[0]);close(input[1]);throw std::runtime_error("Cannot create worker output");}
    signal(SIGPIPE,SIG_IGN);
    std::vector<char*> args;args.push_back(const_cast<char*>(executable.c_str()));for(const auto& arg:arguments)args.push_back(const_cast<char*>(arg.c_str()));args.push_back(nullptr);
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);posix_spawn_file_actions_adddup2(&actions,input[0],STDIN_FILENO);posix_spawn_file_actions_adddup2(&actions,output[1],STDOUT_FILENO);for(int descriptor:{input[0],input[1],output[0],output[1]})posix_spawn_file_actions_addclose(&actions,descriptor);
    int result=posix_spawn(&Process,executable.c_str(),&actions,nullptr,args.data(),environ);posix_spawn_file_actions_destroy(&actions);close(input[0]);close(output[1]);if(result){close(input[1]);close(output[0]);Process=-1;throw std::runtime_error("Cannot start worker");}Input=input[1];Output=output[0];fcntl(Input,F_SETFL,fcntl(Input,F_GETFL)|O_NONBLOCK);fcntl(Output,F_SETFL,fcntl(Output,F_GETFL)|O_NONBLOCK);fcntl(Input,F_SETFD,FD_CLOEXEC);fcntl(Output,F_SETFD,FD_CLOEXEC);

#endif
  }
  std::string Exchange(const std::string& request,int milliseconds=3000,size_t limit=64*1024*1024){
    if(!Running()||request.size()>limit)throw std::runtime_error("Worker request unavailable or exceeds budget");
    auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);std::string input=request+"\n";
#ifdef _WIN32
    std::atomic<bool> sent(false),writeFailed(false);std::thread writer;
#else
    size_t at=0;
#endif
    try{
#ifdef _WIN32
      writer=std::thread([&](){size_t at=0;while(at<input.size()){DWORD n=0;if(!WriteFile(Input,input.data()+at,DWORD(std::min(size_t(16384),input.size()-at)),&n,nullptr)||!n){writeFailed=true;break;}at+=n;}sent=true;});
#endif
      for(;;){if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Worker deadline exceeded; operation cancelled");
#ifdef _WIN32
      if(writeFailed)throw std::runtime_error("Worker input closed");
      DWORD available=0;if(!PeekNamedPipe(Output,nullptr,0,nullptr,&available,nullptr))throw std::runtime_error("Worker exited");
      if(available){char buffer[8192];DWORD n=0;if(!ReadFile(Output,buffer,std::min(DWORD(sizeof(buffer)),available),&n,nullptr)||!n)throw std::runtime_error("Worker output closed");Pending.append(buffer,n);}else if(WaitForSingleObject(Process,0)==WAIT_OBJECT_0)throw std::runtime_error("Worker exited");
#else
      if(at<input.size()){ssize_t n=write(Input,input.data()+at,std::min(size_t(16384),input.size()-at));if(n>0)at+=size_t(n);else if(n<0&&errno!=EAGAIN&&errno!=EINTR)throw std::runtime_error("Worker input closed");}
      char buffer[8192];ssize_t n=read(Output,buffer,sizeof(buffer));if(n>0)Pending.append(buffer,size_t(n));else if(n==0)throw std::runtime_error("Worker output closed");else if(errno!=EAGAIN&&errno!=EINTR)throw std::runtime_error("Cannot read worker output");
#endif
      if(Pending.size()>limit)throw std::runtime_error("Worker output exceeds budget");
      size_t end=Pending.find('\n');if(end!=std::string::npos
#ifdef _WIN32
        &&sent
#endif
      ){
#ifdef _WIN32
        writer.join();
#endif
        auto line=Pending.substr(0,end);Pending.erase(0,end+1);return line;}
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }}catch(...){Stop();
#ifdef _WIN32
      if(writer.joinable())writer.join();
#endif
      throw;}
  }
};

#if defined(__GNUC__) && defined(_WIN32)
#pragma GCC diagnostic pop
#endif

#ifndef _WIN32
#include <sys/resource.h>
#endif
static void SetNativeWorkerLimits(){
#ifdef _WIN32
  static HANDLE job=CreateJobObjectW(nullptr,nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_PROCESS_MEMORY|JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;limits.ProcessMemoryLimit=1024ull*1024*1024;
  if(!job||!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))||!AssignProcessToJobObject(job,GetCurrentProcess()))throw std::runtime_error("Cannot enforce geometry worker memory limit");
#else
  struct rlimit memory={1024ull*1024*1024,1024ull*1024*1024};
#ifdef __APPLE__
  if(setrlimit(RLIMIT_DATA,&memory))throw std::runtime_error("Cannot enforce native worker data limit");
#else
  memory.rlim_cur=memory.rlim_max=2ull*1024*1024*1024;if(setrlimit(RLIMIT_AS,&memory))throw std::runtime_error("Cannot enforce native worker address-space limit");
#endif
  struct rlimit cpu={3,4};if(setrlimit(RLIMIT_CPU,&cpu))throw std::runtime_error("Cannot enforce native worker CPU limit");
#endif
}
