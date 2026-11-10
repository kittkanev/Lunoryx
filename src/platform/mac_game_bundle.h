#pragma once
#ifdef __APPLE__
extern "C" bool LuaryxGamePlist(const char*,const char*,bool,std::string*);
static void RunMacTool(const std::vector<std::string>& arguments){
    std::vector<char*> args;for(const auto& arg:arguments)args.push_back(const_cast<char*>(arg.c_str()));args.push_back(nullptr);
    pid_t child=-1;int result=posix_spawn(&child,args[0],nullptr,nullptr,args.data(),environ);
    if(result)throw std::runtime_error("Cannot start macOS bundle tool");
    int status=0;while(waitpid(child,&status,0)<0){if(errno!=EINTR)throw std::runtime_error("Cannot wait for macOS bundle tool");}
    if(!WIFEXITED(status)||WEXITSTATUS(status))throw std::runtime_error("macOS bundle signing failed");
}
static void WriteMacGameBundle(const std::string& requested,const std::string& runtime,const std::map<std::string,std::vector<unsigned char>>& assets,const std::vector<unsigned char>& icon){
    auto destination=AbsoluteAssetPath(requested);if(destination.size()<4||destination.substr(destination.size()-4)!=".app")destination+=".app";
    auto parent=PathDirectory(destination);auto base=destination.substr(destination.find_last_of('/')+1);
    AssetDirectoryAnchor pinned(CanonicalAssetPath(parent));
    struct stat existing={};if(fstatat(pinned.Descriptor,base.c_str(),&existing,AT_SYMLINK_NOFOLLOW)==0)throw std::runtime_error("Choose a new .app destination; existing applications are not overwritten");
    auto templateContents=NormalizePath(PathDirectory(runtime)+"/..");
    if(!ProjectFS::is_directory(ProjectNativePath(templateContents+"/Frameworks")))throw std::runtime_error("Build games from the installed Lunoryx.app so native libraries can be bundled");
    auto stageName=".lunoryx-game-"+std::to_string(std::random_device{}())+".app";
    CreateAssetDirectories(parent,stageName+"/Contents/MacOS");CreateAssetDirectories(parent,stageName+"/Contents/Resources");CreateAssetDirectories(parent,stageName+"/Contents/Frameworks");
    auto stage=parent+"/"+stageName;
    auto write=[&](const std::string& relative,const std::vector<unsigned char>& bytes){auto file=ConfinedAssetPath(relative,stage);WriteAssetFileAtomic(file,bytes.data(),bytes.size());};
    try{
        write("Contents/MacOS/Game",ReadAssetFile(runtime,128*1024*1024));
        {auto path=ConfinedAssetPath("Contents/MacOS/Game",stage);AssetParentAnchor anchor(path);int fd=openat(anchor.Descriptor,anchor.Name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)throw std::runtime_error("Cannot open game binary");int status=fchmod(fd,0755);close(fd);if(status)throw std::runtime_error("Cannot mark game binary executable");}
        for(const auto& entry:ProjectFS::directory_iterator(ProjectNativePath(templateContents+"/Frameworks"))){if(entry.is_symlink())continue;if(entry.is_regular_file())write("Contents/Frameworks/"+entry.path().filename().u8string(),ReadAssetFile(entry.path().u8string(),128*1024*1024));}
        std::string plist;if(!LuaryxGamePlist(g_Game.Title.c_str(),"Game",!icon.empty(),&plist))throw std::runtime_error("Cannot create game Info.plist");
        write("Contents/Info.plist",std::vector<unsigned char>(plist.begin(),plist.end()));
        if(!icon.empty()){
            StudioMaterial source;source.Image=icon;auto image=DecodeMaterialImage(source);tinygltf::Image square;square.width=square.height=256;square.component=4;square.bits=8;square.image.resize(256*256*4);
            for(int y=0;y<256;++y)for(int x=0;x<256;++x)for(int c=0;c<4;++c)square.image[(y*256+x)*4+c]=image.image[((y*image.height/256)*image.width+x*image.width/256)*4+c];
            auto png=EncodeImage(square);std::vector<unsigned char> icns;auto big=[&](size_t n){for(int k=3;k>=0;--k)icns.push_back(static_cast<unsigned char>(n>>(k*8)));};
            icns.insert(icns.end(),{'i','c','n','s'});big(png.size()+16);icns.insert(icns.end(),{'i','c','0','8'});big(png.size()+8);icns.insert(icns.end(),png.begin(),png.end());write("Contents/Resources/Game.icns",icns);
        }
        auto package=ConfinedAssetPath("Contents/Resources/Game.lnx",stage);GamePackage::Write(runtime,package,assets,{},true);
        RunMacTool({"/usr/bin/codesign","--force","--deep","--sign","-",stage});
        RunMacTool({"/usr/bin/codesign","--verify","--deep","--strict",stage});
        if(renameat(pinned.Descriptor,stageName.c_str(),pinned.Descriptor,base.c_str()))throw std::runtime_error("Cannot install game bundle");
    }catch(...){ProjectFS::remove_all(ProjectNativePath(stage));throw;}
}
#endif
