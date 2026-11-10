#pragma once
#ifdef _WIN32
#include <commdlg.h>
static std::string GameFileDialog(bool save) {
  wchar_t path[32768]={};OPENFILENAMEW dialog={};dialog.lStructSize=sizeof(dialog);dialog.lpstrFile=path;dialog.nMaxFile=32768;
  dialog.lpstrFilter=save?L"Windows game (*.exe)\0*.exe\0\0":L"PNG icon (*.png)\0*.png\0\0";
  dialog.lpstrDefExt=save?L"exe":L"png";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
  if(!(save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog)))return "";
  int n=WideCharToMultiByte(CP_UTF8,0,path,-1,nullptr,0,nullptr,nullptr);std::string result(size_t(n),' ');WideCharToMultiByte(CP_UTF8,0,path,-1,&result[0],n,nullptr,nullptr);result.resize(size_t(n-1));return result;
}
#endif

#ifdef __APPLE__
static std::string GameFileDialog(bool save){return ChooseNativePath(save,false);}
#endif
