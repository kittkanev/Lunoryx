#import <Cocoa/Cocoa.h>
#include <cstring>
#include <cstdlib>
extern "C" char* LuaryxNativeDialog(int save,int folder){@autoreleasepool{[NSApp activateIgnoringOtherApps:YES];NSSavePanel* panel=save?[NSSavePanel savePanel]:[NSOpenPanel openPanel];if(!save){NSOpenPanel* open=(NSOpenPanel*)panel;open.canChooseFiles=!folder;open.canChooseDirectories=folder;open.allowsMultipleSelection=NO;}if([panel runModal]!=NSModalResponseOK)return nullptr;return strdup(panel.URL.path.UTF8String);}}
#include <string>
extern "C" bool LuaryxGamePlist(const char* title,const char* executable,bool icon,std::string* output){@autoreleasepool{
    NSMutableDictionary* info=[@{@"CFBundleIdentifier":@"org.lunoryx.game",@"CFBundleName":[NSString stringWithUTF8String:title],@"CFBundleDisplayName":[NSString stringWithUTF8String:title],@"CFBundleExecutable":[NSString stringWithUTF8String:executable],@"CFBundlePackageType":@"APPL",@"CFBundleVersion":@"1",@"LSMinimumSystemVersion":@"15.0",@"NSHighResolutionCapable":@YES} mutableCopy];
    if(icon)info[@"CFBundleIconFile"]=@"Game.icns";
    NSData* data=[NSPropertyListSerialization dataWithPropertyList:info format:NSPropertyListXMLFormat_v1_0 options:0 error:nullptr];if(!data)return false;output->assign(static_cast<const char*>(data.bytes),data.length);return true;
}}

extern "C" bool LuaryxScriptNamesEqual(const char* a,const char* b){@autoreleasepool{
  NSString* x=[NSString stringWithUTF8String:a];NSString* y=[NSString stringWithUTF8String:b];return x&&y&&[x compare:y options:NSCaseInsensitiveSearch]==NSOrderedSame;
}}
