#pragma once
static int RunNativeExportWorker(){
    try{
        std::string line;char block[8192];
        while(std::fgets(block,sizeof(block),stdin)){size_t n=std::strlen(block);if(line.size()+n>64*1024*1024)throw std::runtime_error("GLB worker request budget exceeded");line.append(block,n);if(n&&block[n-1]=='\n')break;}
        CheckJsonBudget(line.begin(),line.end());auto request=nlohmann::json::parse(line);
        std::vector<GameObject> objects;
        for(const auto& value:request){if(objects.size()>=10000)throw std::runtime_error("GLB object budget exceeded");auto object=ReadWorkerObject(value);
            for(const auto& saved:value.at("materials")){StudioMaterial material;material.Name=saved.at("name").get<std::string>();material.Image=ReadBinary(saved.at("image"));ReadVector(saved.at("color"),material.Color,true);material.Metallic=ReadRange(saved.at("metallic"),0,1);material.Roughness=ReadRange(saved.at("roughness"),0,1);object.Materials.push_back(std::move(material));}
            objects.push_back(std::move(object));}
        CheckSceneMeshBudget(objects,0);std::string report;auto model=BuildGLBModel(objects,report);
        std::ostringstream output(std::ios::binary);tinygltf::TinyGLTF writer;
        if(!writer.WriteGltfSceneToStream(&model,output,false,true))throw std::runtime_error("GLB writer failed");
        auto data=output.str();if(data.size()>48*1024*1024)throw std::runtime_error("GLB worker output budget is 48 MiB");
        auto encoded=SaveBinary(std::vector<unsigned char>(data.begin(),data.end()),48*1024*1024);
        std::cout<<nlohmann::json({{"ok",true},{"data",std::move(encoded)},{"report",report}}).dump()<<'\n'<<std::flush;return 0;
    }catch(const std::exception& error){std::cout<<nlohmann::json({{"ok",false},{"error",error.what()}}).dump()<<'\n'<<std::flush;return 1;}
}
static bool ExportSceneIsolated(const std::vector<GameObject>& objects,const std::string& filename,std::string& report){
    if(!g_ModelWorkerReady)return ExportGLBLocal(objects,filename,report);
    try{
        auto request=nlohmann::json::array();size_t images=0;
        for(const auto& object:objects)if(object.Visible){auto value=SaveWorkerObject(object);value["materials"]=nlohmann::json::array();
            for(const auto& material:object.Materials){auto bytes=material.Image.empty()&&material.Texture.empty()?std::vector<unsigned char>():MaterialImageBytes(material);if(bytes.size()>32*1024*1024-images)throw std::runtime_error("GLB encoded image budget is 32 MiB");images+=bytes.size();value["materials"].push_back({{"name",material.Name},{"image",SaveBinary(bytes)},{"color",SaveVector(material.Color)},{"metallic",material.Metallic},{"roughness",material.Roughness}});}
            request.push_back(std::move(value));}
        WorkerProcess worker;worker.Start(GamePackage::ExecutablePath(),{"--export-worker"});
        auto reply=worker.Exchange(request.dump(),3000,72*1024*1024);CheckJsonBudget(reply.begin(),reply.end());auto response=nlohmann::json::parse(reply);
        if(!response.value("ok",false))throw std::runtime_error(response.value("error",std::string("GLB worker failed")));
        auto data=ReadBinary(response.at("data"),48*1024*1024);WriteAssetFileAtomic(filename,data.data(),data.size());report+=response.value("report",std::string());return true;
    }catch(const std::exception& error){report+=std::string("GLB export: ")+error.what()+"\n";return false;}
}
static bool ExportGLB(const std::string& filename){std::string report;bool ok=ExportSceneIsolated(g_Objects,filename,report);if(!report.empty())AppendConsole(report);return ok;}
struct ExportResult{bool Success;std::string Report;};
static std::future<ExportResult> g_ExportJob;
static void BeginExportGLB(const std::string& filename){
    if(g_ExportJob.valid()){AppendConsole("An export is already running.\n");return;}
    auto snapshot=g_Objects;g_ExportJob=std::async(std::launch::async,[snapshot,filename](){ExportResult result;result.Success=ExportSceneIsolated(snapshot,filename,result.Report);return result;});
    AppendConsole("Exporting GLB in the background...\n");
}
static void PollExportGLB(){if(g_ExportJob.valid()&&g_ExportJob.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=g_ExportJob.get();AppendConsole(result.Report);AppendConsole(result.Success?"GLB export complete.\n":"GLB export failed.\n");}}
