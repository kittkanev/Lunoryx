#pragma once
#ifdef _WIN32
#include <shobjidl.h>
static std::string ChooseNativePath(bool save,bool folder){
  HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);IFileDialog* dialog=nullptr;
  HRESULT result=save?CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)):CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog));std::string output;
  if(SUCCEEDED(result)){DWORD options=0;dialog->GetOptions(&options);dialog->SetOptions(options|FOS_FORCEFILESYSTEM|(folder?DWORD(FOS_PICKFOLDERS):DWORD(0)));if(SUCCEEDED(dialog->Show(nullptr))){IShellItem* item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){int n=WideCharToMultiByte(CP_UTF8,0,path,-1,nullptr,0,nullptr,nullptr);if(n>0){output.resize(size_t(n));WideCharToMultiByte(CP_UTF8,0,path,-1,&output[0],n,nullptr,nullptr);output.resize(size_t(n-1));}CoTaskMemFree(path);}item->Release();}}dialog->Release();}
  if(SUCCEEDED(initialized))CoUninitialize();
  return output;
}
#elif defined(__APPLE__)
extern "C" char* LuaryxNativeDialog(int,int);
static std::string ChooseNativePath(bool save,bool folder){char* value=LuaryxNativeDialog(save?1:0,folder?1:0);std::string result=value?value:"";std::free(value);return result;}
#else
static std::string ChooseNativePath(bool save,bool folder){
  std::string command="zenity --file-selection";if(folder)command+=" --directory";if(save)command+=" --save --confirm-overwrite";command+=" 2>/dev/null";
  FILE* pipe=popen(command.c_str(),"r");if(!pipe)return {};char path[8192];std::string result;if(std::fgets(path,sizeof(path),pipe))result=path;int status=pclose(pipe);while(!result.empty()&&(result.back()=='\n'||result.back()=='\r'))result.pop_back();if(status||result.empty())return {};return result;
}
#endif
