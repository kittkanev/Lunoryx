#pragma once
static void CheckV13Regressions() {
    StopSimulation();
    assert(CheckedScriptText("\xEF\xBB\xBF" "-- Привет\r\nreturn 1\r")=="-- Привет\nreturn 1\n");
    bool encodingRejected=false;try{CheckedScriptText(std::string(1,char(255)));}catch(const std::exception&){encodingRejected=true;}assert(encodingRejected);
    g_Objects.clear();g_Sounds.clear();g_Game=StudioGameSettings();
    for(auto& doc:StudioScripts())doc=ScriptDocument();
    // More events/overlaps than the previous truncation thresholds.
    for(int i=0;i<100;++i){g_Objects.emplace_back("zone");g_Objects.back().Trigger=3;}
    for(int i=0;i<100;++i){g_Objects.emplace_back("actor");g_Objects.back().Dynamic=true;}
    ResetGameplay();assert(CollectTriggerEvents());
    assert(g_TriggerEvents.size()==10000&&g_TriggerOverlaps.size()==10000);
    assert(CollectTriggerEvents()&&g_TriggerEvents.size()==10000);
    for(const auto& event:g_TriggerEvents)assert(event.Event=="stay");
    for(auto& object:g_Objects)if(object.Dynamic)object.Position[0]=10;
    assert(CollectTriggerEvents()&&g_TriggerEvents.size()==10000&&g_TriggerOverlaps.empty());
    for(const auto& event:g_TriggerEvents)assert(event.Event=="exit");
    for(int i=0;i<230;++i){g_Objects.emplace_back("extra actor");g_Objects.back().Dynamic=true;}
    assert(!ValidTriggerBudget());g_Objects.clear();ResetGameplay();

    // Fast movers stop at thin static colliders.
    std::vector<GameObject> objects;objects.emplace_back("wall");objects[0].Scale[0]=.02f;
    objects.emplace_back("fast");objects[1].Dynamic=true;objects[1].Position[0]=-4;
    objects[1].Velocity[0]=600;objects[1].Restitution=0;
    PhysicsState physics;physics.Ground=false;physics.Gravity=0;PhysicsStep(objects,1.f/60,physics);
    assert(objects[1].Position[0]<0&&LastPhysicsMetrics().ContinuousHits>0);

    const auto folder=ProjectFS::temp_directory_path()/ProjectFS::path("lunoryx-test-"+std::to_string(std::random_device{}()));
    assert(CreateProjectFolder("UTF-8 project / Проект",folder.u8string()));
    CreateAssetDirectories(g_ProjectDirectory,"pinned");
    auto confined=ConfinedAssetPath("pinned/asset.txt",g_ProjectDirectory);
#ifdef _WIN32
    {AssetParentAnchor anchor(confined);
    auto writable=CreateFileW(WidePath(g_ProjectDirectory+"/pinned").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    assert(writable==INVALID_HANDLE_VALUE);assert(GetLastError()==ERROR_SHARING_VIOLATION||GetLastError()==ERROR_ACCESS_DENIED);
    const char safe[]="safe";WriteAssetFileAtomic(confined,safe,4);assert(ReadAssetFile(confined)==std::vector<unsigned char>({'s','a','f','e'}));}
#else
    auto outside=folder/"outside";ProjectFS::create_directory(outside);const std::string sentinel="outside";
    WriteAssetFileAtomic((outside/"asset.txt").u8string(),sentinel.data(),sentinel.size());
    ProjectFS::rename(folder/"pinned",folder/"original");ProjectFS::create_directory_symlink(outside,folder/"pinned");
    bool readRejected=false,writeRejected=false;try{ReadAssetFile(confined);}catch(const std::exception&){readRejected=true;}
    try{WriteAssetFileAtomic(confined,"bad",3);}catch(const std::exception&){writeRejected=true;}
    assert(readRejected&&writeRejected);assert(ReadAssetFile((outside/"asset.txt").u8string())==std::vector<unsigned char>(sentinel.begin(),sentinel.end()));
    ProjectFS::remove(folder/"pinned");ProjectFS::rename(folder/"original",folder/"pinned");
#endif
    auto& module=StudioScripts()[1];module.Allocated=true;module.Module=true;
    std::strcpy(module.Name,"Helpers.luaryx");std::strcpy(module.Code,"return {answer=42}");
    InitLuaState();CheckLua("assert(require('Helpers').answer==42); assert(require('Helpers')==require('Helpers'))");
    CheckLua("local ok=pcall(require,'../outside');assert(not ok)");
    g_Objects.emplace_back("discard");g_Objects.emplace_back("Shared");uint64_t shared=g_Objects.back().Id;
    assert(SaveCurrentProject());
    auto sourcePath=ConfinedAssetPath("scripts/Helpers.luaryx",g_ProjectDirectory);
    const std::string external="return {answer=43}";WriteAssetFileAtomic(sourcePath,external.data(),external.size());
    assert(OpenProjectFolder(folder.u8string()));InitLuaState();CheckLua("assert(require('Helpers').answer==43)");
    g_ObjectHistory.Capture(g_Objects);std::strcpy(StudioScripts()[1].Code,"-- edited source");g_ObjectHistory.Capture(g_Objects);
    assert(g_ObjectHistory.Undo(g_Objects)&&std::string(StudioScripts()[1].Code)==external);
    assert(g_ObjectHistory.Redo(g_Objects)&&std::string(StudioScripts()[1].Code)=="-- edited source");assert(g_ObjectHistory.Undo(g_Objects));
    auto& duplicate=StudioScripts()[4];duplicate.Allocated=true;std::strcpy(duplicate.Name,"helpers.LUARYX");bool duplicateRejected=false;
    try{CheckUniqueScriptNames(StudioScripts());}catch(const std::exception&){duplicateRejected=true;}assert(duplicateRejected);duplicate=ScriptDocument();
    // Recovery while playing keeps the edit-world geometry and current source.
    StartSimulation();g_Objects.back().Position[0]=999;
    std::strcpy(StudioScripts()[1].Code,"return {answer=44}");assert(SaveRecoverySnapshot());
    auto recovery=ReadAssetFile(ProjectRecoveryPath());auto root=nlohmann::json::parse(recovery);
    assert(root["objects"].back()["position"][0]==0&&root["scripts"][1]["code"]=="return {answer=44}");
    StopSimulation();

    const char* configured=TestEnvironment("LUNORYX_CSHARP_WORKER");
    if(configured||AssetExists(ManagedWorkerPath())){
        auto& normal=StudioScripts()[2];normal.Allocated=true;normal.Language=1;
        std::strcpy(normal.Name,"Mover.cs");std::strcpy(normal.Code,R"CS(
using Luaryx;using System.Text.Json.Nodes;
public sealed class Mover:Script {
    Entity cube=null!;
    public override void OnStart(){cube=Scene.Find("Shared")!;cube.SetProperty("score",Helper.Value);Scene.Emit("managed",7);}
    public override void OnStop(){cube.SetProperty("stopped",true);}
    public override void OnUpdate(float dt){cube.Position=cube.Position+new Vector3(dt,0,0);}
    public override void OnMessage(string name,JsonNode? value){if(name=="lua")cube.SetProperty("reply",value);}
})CS");
        auto& helper=StudioScripts()[3];helper.Allocated=true;helper.Module=true;helper.Language=1;
        std::strcpy(helper.Name,"Helper.cs");std::strcpy(helper.Code,"public static class Helper { public const int Value=17; }");
        InitLuaState();CheckLua("function on_message(name,value) if name=='managed' then luaryx.find('Shared'):property('received',tonumber(value)) end end");
        g_CSharpTrusted=true;StartManagedScripts();if(!g_ManagedWorker.Running())std::cerr<<"C# start failure: "<<g_LastLuaError<<"\n"<<g_ConsoleOutput.substr(g_ConsoleOutput.size()>4096?g_ConsoleOutput.size()-4096:0);assert(g_ManagedWorker.Running());
        assert(g_Objects.back().Properties.at("score").NumberValue==17);
        assert(g_Objects.back().Properties.at("received").NumberValue==7);
        CheckLua("remove_object(0);luaryx.emit('lua','23')");
        UpdateManagedScripts(.25f);assert(g_ManagedWorker.Running());
        assert(g_Objects[0].Id==shared&&g_Objects[0].Position[0]==.25f);
        assert(g_Objects[0].Properties.at("reply").NumberValue==23);
        StopManagedScripts();assert(g_Objects[0].Properties.at("stopped").BoolValue);std::strcpy(normal.Code,"public class Broken { INVALID }");
        StartManagedScripts();assert(!g_ManagedWorker.Running()&&!g_CSharpDiagnostics.empty());
        std::strcpy(normal.Code,"using Luaryx;public class Hang:Script{public override void OnUpdate(float dt){while(true){}}}");
        StartManagedScripts();assert(g_ManagedWorker.Running());auto start=std::chrono::steady_clock::now();
        UpdateManagedScripts(.1f);assert(!g_ManagedWorker.Running());
        assert(std::chrono::steady_clock::now()-start<std::chrono::seconds(3));
        std::cout<<"PASS: real C# compilation/modules, Lua messages, stable IDs, diagnostics and deadline cancellation\n";
    }else std::cout<<"SKIP: C# worker not installed; release CI must provide LUNORYX_CSHARP_WORKER\n";
    ShutdownLuaState();g_ProjectDirectory.clear();g_ProjectName.clear();g_OpenedProject.clear();
    // Drop pinned handles before cleanup on Windows.
    {std::lock_guard<std::recursive_mutex> lock(AssetPolicyMutex());ConfinedAssetPolicies().clear();AssetRootAnchors().clear();}
    ProjectFS::remove_all(folder);
    std::cout<<"PASS: dense triggers, thin-wall CCD, directory projects, disk modules and Play recovery\n";
}
