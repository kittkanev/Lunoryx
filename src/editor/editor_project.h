#pragma once
#include <memory>
#include "src/io/json_budget.h"
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
static void StopAnimationPreview();
static char g_ProjectPath[512]="project.lunoryx";
static std::string g_OpenedProject;
static uint64_t g_SavedDocumentHash=0;
static uint64_t CompleteDocumentHash();
static bool ProjectDirty(){return CompleteDocumentHash()!=g_SavedDocumentHash;}


static nlohmann::json SaveVector(const float v[3]) { return {v[0],v[1],v[2]}; }
static void ReadVector(const nlohmann::json& j,float v[3],bool color=false) {
    if(!j.is_array()||j.size()!=3)throw std::runtime_error("Expected a three-component vector");
    for(int k=0;k<3;++k) {
        if(!j[k].is_number())throw std::runtime_error("Vector components must be numbers");
        double value=j[k].get<double>();
        if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max()||(color&&(value<0||value>1)))throw std::runtime_error("Invalid vector value");
        v[k]=float(value);
    }
}
static float ReadRange(const nlohmann::json& value,float low,float high) {
    if(!value.is_number())throw std::runtime_error("Expected a number");
    double number=value.get<double>();if(!std::isfinite(number)||number<low||number>high)throw std::runtime_error("Number outside allowed range");return float(number);
}
static nlohmann::json SaveModelMesh(const StudioMesh& m){return {{"normals",m.Normals},{"positions",m.Positions},{"indices",m.Indices},{"colors",m.Colors},{"uv",m.UV},{"faces",m.Faces},{"face_materials",m.FaceMaterials},{"seams",m.Seams}};}
// Validate JSON numbers before narrowing: json::get<uint32_t>() wraps large
// integers and truncates fractions, which can silently change topology.
static uint64_t ReadUnsigned(const nlohmann::json &value, uint64_t maximum) {
  if (!value.is_number_integer() ||
      (!value.is_number_unsigned() && value.get<int64_t>() < 0))
    throw std::runtime_error("Expected a nonnegative integer");
  uint64_t number = value.get<uint64_t>();
  if (number > maximum)
    throw std::runtime_error("Integer outside allowed range");
  return number;
}
static int ReadInteger(const nlohmann::json &value, int low, int high) {
  if (!value.is_number_integer())
    throw std::runtime_error("Expected an integer");
  if (value.is_number_unsigned()) {
    auto number = value.get<uint64_t>();
    if (number > uint64_t(high) || (low > 0 && number < uint64_t(low)))
      throw std::runtime_error("Integer outside allowed range");
    return int(number);
  }
  auto number = value.get<int64_t>();
  if (number < low || number > high)
    throw std::runtime_error("Integer outside allowed range");
  return int(number);
}

template <typename T>
static std::vector<T> ReadUnsignedArray(const nlohmann::json &value,
                                        size_t limit) {
  if (!value.is_array() || value.size() > limit)
    throw std::runtime_error("Array exceeds size limit");
  std::vector<T> result;
  result.reserve(value.size());
  for (const auto &item : value)
    result.push_back(T(ReadUnsigned(item, std::numeric_limits<T>::max())));
  return result;
}
#include "src/io/binary_json.h"
#include "src/runtime/game_project.h"
static StudioMesh ReadModelMesh(const nlohmann::json &j) {
  StudioMesh m;
  m.Positions = j.at("positions").get<std::vector<float>>();
  m.Indices = ReadUnsignedArray<uint32_t>(j.at("indices"), MAX_EDIT_INDICES);
  m.Normals = j.value("normals", std::vector<float>());
  m.Colors = j.value("colors", std::vector<float>());
  m.UV = j.value("uv", std::vector<float>());
  const auto &faces = j.at("faces");
  if (!faces.is_array() || faces.size() > MAX_EDIT_INDICES / 3)
    throw std::runtime_error("Face count exceeds limit");
  for (const auto &face : faces)
    m.Faces.push_back(ReadUnsignedArray<uint32_t>(face, 4096));
  if (j.contains("face_materials"))
    m.FaceMaterials = ReadUnsignedArray<uint32_t>(j.at("face_materials"),
                                                  MAX_EDIT_INDICES / 3);
  if (j.contains("seams"))
    m.Seams = ReadUnsignedArray<uint64_t>(j.at("seams"), MAX_EDIT_INDICES);
  Model::Check(m);
  for (auto index : m.Indices)
    if (index >= m.Positions.size() / 3)
      throw std::runtime_error("Invalid mesh index");
  auto normals = m.Normals;
  Model::Triangulate(m);
  // Empty normals intentionally request flat shading; preserve that contract.
  m.Normals = std::move(normals);
  return m;
}
static void SaveModelFields(nlohmann::json& j,const GameObject& o,const std::string& path,bool forGame=false){
    j["id"]=o.Id;
    if(!ValidAnimation(o)||o.Trigger<0||o.Trigger>3)throw std::runtime_error("Invalid animation or trigger");
    j["solid"]=o.Solid;j["trigger"]=o.Trigger;
    j["animation"]={{"duration",o.AnimationDuration},{"speed",o.AnimationSpeed},{"loop",o.AnimationLoop},{"autoplay",o.AnimationAutoplay},{"keys",nlohmann::json::array()}};
    for(const auto& k:o.AnimationKeys)j["animation"]["keys"].push_back({{"time",k.Time},{"position",SaveVector(k.Pose.Position)},{"rotation",SaveVector(k.Pose.Rotation)},{"scale",SaveVector(k.Pose.Scale)},{"color",SaveVector(k.Pose.Color)},{"easing",k.Easing}});
    if(!o.GeometryNodes.empty()){
      GeometryNodes::Validate(o.GeometryNodes);
      j["geometry_nodes"]=nlohmann::json::array();
      for(const auto& n:o.GeometryNodes)j["geometry_nodes"].push_back({{"id",n.Id},{"input",n.Input},{"type",int(n.Type)},
        {"vector",SaveVector(n.Vector)},{"amount",n.Amount},{"frequency",n.Frequency},{"angle",n.Angle},{"count",n.Count},{"axis",n.Axis},{"x",n.X},{"y",n.Y}});
    }
    if(!o.Vertices.empty())j["mesh"]=SaveModelMesh(Model::FromObject(o));
    j["modifiers"]=nlohmann::json::array();
    for(const auto& m:o.Modifiers)j["modifiers"].push_back({{"type",int(m.Type)},{"enabled",m.Enabled},{"axis",m.Axis},{"count",m.Count},{"levels",m.Levels},{"amount",m.Amount},{"offset",SaveVector(m.Offset)}});
    if(!o.Modifiers.empty())j["base_mesh"]=SaveModelMesh(o.BaseMesh);
    j["materials"]=nlohmann::json::array();for(const auto& m:o.Materials){auto image=forGame?std::vector<unsigned char>():m.Image;std::string texture=m.Texture.empty()?"":RelativeAssetPath(m.Texture,path);if(!forGame&&!texture.empty()){try{ProjectAssetPath(texture,path);}catch(const std::exception&){image=MaterialImageBytes(m);texture.clear();}}j["materials"].push_back({{"image",SaveBinary(image)},{"name",m.Name},{"texture",texture},{"color",SaveVector(m.Color)},{"metallic",m.Metallic},{"roughness",m.Roughness}});}
}
static void ReadModelFields(const nlohmann::json& j,GameObject& o,const std::string& path){
    o.Solid=j.value("solid",true);if(j.contains("trigger"))o.Trigger=ReadInteger(j.at("trigger"),0,3);
    if(j.contains("animation")){
      const auto& a=j.at("animation");o.AnimationDuration=ReadRange(a.at("duration"),.01f,3600);o.AnimationSpeed=ReadRange(a.at("speed"),.01f,100);
      o.AnimationLoop=a.at("loop").get<bool>();o.AnimationAutoplay=a.at("autoplay").get<bool>();
      const auto& keys=a.at("keys");if(!keys.is_array()||keys.size()>256)throw std::runtime_error("Invalid animation key count");
      for(const auto& v:keys){LnxAnimationKey key;key.Time=ReadRange(v.at("time"),0,o.AnimationDuration);key.Easing=ReadInteger(v.at("easing"),0,2);
        ReadVector(v.at("position"),key.Pose.Position);ReadVector(v.at("rotation"),key.Pose.Rotation);ReadVector(v.at("scale"),key.Pose.Scale);ReadVector(v.at("color"),key.Pose.Color,true);o.AnimationKeys.push_back(key);}
      if(!ValidAnimation(o))throw std::runtime_error("Invalid animation track");
    }
    if(j.contains("id")){auto id=ReadUnsigned(j.at("id"),uint64_t(std::numeric_limits<int64_t>::max()));if(!id)throw std::runtime_error("Invalid object ID");o.Id=id;}
    if(j.contains("mesh"))Model::ToObject(o,ReadModelMesh(j.at("mesh")));
    if(j.contains("modifiers")){if(!j["modifiers"].is_array()||j["modifiers"].size()>16)throw std::runtime_error("Too many modifiers");for(const auto& v:j["modifiers"]){StudioModifier m;int type=ReadInteger(v.at("type"),0,3);if(type<0||type>3)throw std::runtime_error("Invalid modifier type");m.Type=StudioModifier::Kind(type);m.Enabled=v.at("enabled").get<bool>();m.Axis=ReadInteger(v.at("axis"),0,2);m.Count=ReadInteger(v.at("count"),2,64);m.Levels=ReadInteger(v.at("levels"),1,4);m.Amount=ReadRange(v.at("amount"),.00001f,1000);ReadVector(v.at("offset"),m.Offset);o.Modifiers.push_back(m);}}
    if(!o.Modifiers.empty()){o.BaseMesh=ReadModelMesh(j.at("base_mesh"));Model::Evaluate(o);}
    if(j.contains("materials")){if(!j["materials"].is_array()||j["materials"].size()>64)throw std::runtime_error("Material limit exceeded");for(const auto& v:j["materials"]){StudioMaterial m;m.Name=v.at("name").get<std::string>();m.Texture=v.at("texture").get<std::string>();if(v.contains("image"))m.Image=ReadBinary(v.at("image"));if(m.Image.size()>16*1024*1024)throw std::runtime_error("Embedded image exceeds limit");if(m.Name.size()>255||m.Texture.size()>511||m.Texture.find('\0')!=std::string::npos)throw std::runtime_error("Invalid material resource");ReadVector(v.at("color"),m.Color,true);m.Metallic=ReadRange(v.at("metallic"),0,1);m.Roughness=ReadRange(v.at("roughness"),0,1);m.Texture=ProjectAssetPath(m.Texture,path);o.Materials.push_back(m);}}
    if(j.contains("geometry_nodes")){
      const auto& graph=j.at("geometry_nodes");if(!graph.is_array()||graph.size()>32)throw std::runtime_error("Invalid geometry graph size");
      for(const auto& value:graph){StudioGeometryNode n;n.Id=uint32_t(ReadUnsigned(value.at("id"),1000000));n.Input=uint32_t(ReadUnsigned(value.at("input"),1000000));
        n.Type=StudioGeometryNode::Kind(ReadInteger(value.at("type"),0,7));ReadVector(value.at("vector"),n.Vector);
        n.Amount=ReadRange(value.at("amount"),-1000,1000);n.Frequency=ReadRange(value.at("frequency"),.001f,1000);n.Angle=ReadRange(value.at("angle"),-36000,36000);
        n.Count=ReadInteger(value.at("count"),2,32);n.Axis=ReadInteger(value.at("axis"),0,2);n.X=ReadRange(value.at("x"),-16384,16384);n.Y=ReadRange(value.at("y"),-16384,16384);o.GeometryNodes.push_back(n);
      }
      GeometryNodes::Validate(o.GeometryNodes);
      if(!o.Modifiers.empty())throw std::runtime_error("Bake geometry nodes before using modifiers");
    }
    if(!o.FaceMaterials.empty()){if(o.FaceMaterials.size()!=o.Faces.size())throw std::runtime_error("Invalid face material count");for(auto slot:o.FaceMaterials)if(slot>=o.Materials.size())throw std::runtime_error("Invalid material slot");}
}
static nlohmann::json StudioProjectJson(const std::string& path,bool forGame=false){
        CheckUniqueScriptNames(g_Studio.Scripts);
        const auto& sceneObjects=g_PhysicsRunning?g_PlaySnapshot:g_Objects;
        const auto& soundList=g_PhysicsRunning?g_PlaySoundSnapshot:g_Sounds;
        const auto& settings=g_PhysicsRunning?g_PlayGame:g_Game;
        const auto& physics=g_PhysicsRunning?g_PlayPhysics:g_Physics;
        const float* skyTop=g_PhysicsRunning?g_PlayTop:g_SkyTop;
        const float* skyBottom=g_PhysicsRunning?g_PlayBottom:g_SkyBottom;
        int skyPreset=g_PhysicsRunning?g_PlaySky:g_SkyPreset;

        if(path.empty()||path.find('\0')!=std::string::npos)throw std::runtime_error("Invalid project path");
        if(sceneObjects.size()>10000)throw std::runtime_error("Project object limit is 10000");
        size_t totalVertices=0;
        auto finiteRange=[](float value,float low,float high){return std::isfinite(value)&&value>=low&&value<=high;};
        for(const auto& o:sceneObjects) {
            ValidateTransform(o.Position,o.Rotation,o.Scale);
            if(o.Name.size()>255||o.Name.find('\0')!=std::string::npos)throw std::runtime_error("Invalid object name");
            for(int k=0;k<3;++k)if(!std::isfinite(o.Position[k])||!std::isfinite(o.Rotation[k])||!std::isfinite(o.Scale[k])||!finiteRange(o.Color[k],0,1)||!finiteRange(o.Velocity[k],-10000,10000))throw std::runtime_error("Invalid transform, color or velocity; stop simulation before saving");
            if(!finiteRange(o.Metallic,0,1)||!finiteRange(o.Roughness,0,1)||!finiteRange(o.Mass,.01f,10000)||!finiteRange(o.Restitution,0,1)||!finiteRange(o.Friction,0,20)||!finiteRange(o.MoveSpeed,0,100)||!finiteRange(o.JumpSpeed,0,100))throw std::runtime_error("Invalid material or physics property");
            if(o.Vertices.size()/3>MAX_EDIT_VERTICES||o.Vertices.size()%3||o.Indices.size()>MAX_EDIT_INDICES||o.Indices.size()%3||(o.Vertices.empty()!=o.Indices.empty())||(!o.VertexColors.empty()&&o.VertexColors.size()!=o.Vertices.size()))throw std::runtime_error("Invalid mesh size");
            totalVertices+=o.Vertices.size()/3;if(totalVertices>1000000)throw std::runtime_error("Project mesh budget exceeded");
            for(float v:o.Vertices)if(!std::isfinite(v))throw std::runtime_error("Invalid vertex");
            for(float v:o.VertexColors)if(!finiteRange(v,0,1))throw std::runtime_error("Invalid vertex color");
            for(uint32_t i:o.Indices)if(i>=o.Vertices.size()/3)throw std::runtime_error("Invalid mesh index");
            if(o.Properties.size()>256)throw std::runtime_error("Too many custom properties");
        }
        for(int k=0;k<3;++k)if(!finiteRange(skyTop[k],0,1)||!finiteRange(skyBottom[k],0,1))throw std::runtime_error("Invalid sky color");
        if(skyPreset<0||skyPreset>2||!finiteRange(physics.Gravity,-30,0))throw std::runtime_error("Invalid environment settings");
        nlohmann::json root={{"schema",3},{"objects",nlohmann::json::array()},{"scripts",nlohmann::json::array()},
            {"sky_top",SaveVector(skyTop)},{"sky_bottom",SaveVector(skyBottom)},{"sky_preset",skyPreset},{"gravity",physics.Gravity},{"ground",physics.Ground},{"continuous_collision",physics.Continuous}};
        for(const auto& o:sceneObjects) {
            nlohmann::json j={{"name",o.Name},{"type",int(o.Type)},{"position",SaveVector(o.Position)},{"rotation",SaveVector(o.Rotation)},{"scale",SaveVector(o.Scale)},
                {"color",SaveVector(o.Color)},{"metallic",o.Metallic},{"roughness",o.Roughness},{"visible",o.Visible},{"wireframe",o.Wireframe},
                {"dynamic",o.Dynamic},{"character",o.Character},{"mass",o.Mass},{"bounce",o.Restitution},{"friction",o.Friction},{"speed",o.MoveSpeed},{"jump",o.JumpSpeed},{"velocity",SaveVector(o.Velocity)},
                {"vertices",o.Vertices},{"indices",o.Indices},{"vertex_colors",o.VertexColors},{"properties",nlohmann::json::object()}};
            for(const auto& property:o.Properties) {
                const auto& v=property.second;
                j["properties"][property.first]={{"kind",int(v.Type)},{"number",v.NumberValue},{"boolean",v.BoolValue},{"text",v.TextValue}};
            }
            SaveModelFields(j,o,path,forGame);
            root["objects"].push_back(std::move(j));
        }
        root["format_version"]=3;
        if(!g_ProjectDirectory.empty())root["project"]={{"name",g_ProjectName},{"directory_format",1}};
        root["sounds"]=nlohmann::json::array();
        if(soundList.size()>64)throw std::runtime_error("At most 64 sounds per project");
        for(const auto& sound:soundList){
            if(sound.Name.size()>255||sound.Path.size()>511||sound.Name.find('\0')!=std::string::npos||sound.Path.find('\0')!=std::string::npos||!finiteRange(sound.Volume,0,2)||!finiteRange(sound.Pitch,.1f,4))throw std::runtime_error("Invalid sound settings");
            for(float p:sound.Position)if(!std::isfinite(p))throw std::runtime_error("Invalid sound position");
            auto bytes=forGame?std::vector<unsigned char>():sound.Data;std::string soundPath=sound.Path.empty()?"":RelativeAssetPath(sound.Path,path);if(!forGame&&!soundPath.empty()){try{ProjectAssetPath(soundPath,path);}catch(const std::exception&){bytes=ReadAssetFile(sound.Path,16*1024*1024);soundPath.clear();}}
            root["sounds"].push_back({{"data",SaveBinary(bytes)},{"name",sound.Name},{"path",soundPath},{"position",SaveVector(sound.Position)},{"volume",sound.Volume},{"pitch",sound.Pitch},{"loop",sound.Loop},{"spatial",sound.Spatial},{"autoplay",sound.Autoplay}});
        }
        for(const auto& script:g_Studio.Scripts)if(script.Allocated){root["scripts"].push_back({{"name",script.Name},{"code",script.Code},{"open",script.Open},{"language",script.Language},{"module",script.Module}});if(!g_ProjectDirectory.empty())root["scripts"].back()["file"]=std::string("scripts/")+script.Name;}

    int startup=-1,allocated=0;for(size_t i=0;i<kScriptSlots;++i)if(g_Studio.Scripts[size_t(i)].Allocated){if(int(i)==settings.StartupScript)startup=allocated;++allocated;}
    root["game"]=SaveGameSettings(settings,path);
    root["game"]["startup_script"]=startup;
    ReadGameSettings(root["game"],path); // Validate editor data before persisting.
    return root;
}
static bool SaveStudioProject(const std::string& path){
    // Never persist the disposable Play world over the editor document.
    if(g_PhysicsRunning){AppendConsole("Project save failed: stop simulation before saving\n");return false;}
    try {
        StopAnimationPreview();
        auto root=StudioProjectJson(path);
        const std::string document=root.dump();
        if(document.size()>32*1024*1024)throw std::runtime_error("Project exceeds 32 MiB limit");
        if(AssetExists(path)){auto previous=ReadAssetFile(path,32*1024*1024);try{CheckJsonBudget(previous.begin(),previous.end());auto old=nlohmann::json::parse(previous.begin(),previous.end());if(old.value("schema",1)<3&&!AssetExists(SiblingAssetPath(path,".pre-1.3.bak")))WriteAssetFileAtomic(SiblingAssetPath(path,".pre-1.3.bak"),previous.data(),previous.size());}catch(const nlohmann::json::exception&){} }
        WriteAssetFileAtomic(path,document.data(),document.size());
        g_OpenedProject=AbsoluteAssetPath(path);g_SavedDocumentHash=CompleteDocumentHash();
        AppendConsole("Saved project: "+path+"\n");return true;
    } catch(const std::exception& error) {
        AppendConsole(std::string("Project save failed: ")+error.what()+"\n");return false;
    }
}
static std::string ProjectRecoveryPath(){return SiblingAssetPath(g_OpenedProject.empty()?AbsoluteAssetPath(g_ProjectPath):g_OpenedProject,".autosave.lunoryx");}
static bool SaveRecoverySnapshot(){
    try{auto text=StudioProjectJson(g_OpenedProject.empty()?std::string(g_ProjectPath):g_OpenedProject).dump();if(text.size()>32*1024*1024)throw std::runtime_error("Recovery exceeds 32 MiB limit");WriteAssetFileAtomic(ProjectRecoveryPath(),text.data(),text.size());return true;}
    catch(const std::exception& e){AppendConsole(std::string("Recovery save failed: ")+e.what()+"\n");return false;}
}
static bool LoadStudioProject(const std::string& path) {
    try {
        if(path.empty()||path.find('\0')!=std::string::npos)throw std::runtime_error("Invalid project path");
        auto data=ReadAssetFile(path,32*1024*1024);
        CheckJsonBudget(data.begin(),data.end());
        size_t nodes=0;
        nlohmann::json root=nlohmann::json::parse(data.begin(),data.end(),[&](int depth,nlohmann::json::parse_event_t,nlohmann::json&){if(depth>128||++nodes>4000000)throw std::runtime_error("Project structure exceeds parsing budget");return true;});
        const int schema=ReadInteger(root.at("schema"),1,3);
        const auto& saved=root.at("objects");if(!saved.is_array()||saved.size()>10000)throw std::runtime_error("Invalid object count");
        std::vector<GameObject> objects;objects.reserve(saved.size());size_t totalVertices=0;
        for(const auto& j:saved) {
            std::string name=j.at("name").get<std::string>();if(name.size()>255||name.find('\0')!=std::string::npos)throw std::runtime_error("Object name too long");
            int type=ReadInteger(j.at("type"),0,4);if(type<0||type>4)throw std::runtime_error("Unknown primitive");
            GameObject o(name,MeshType(type),false);o.Id=objects.size()+1;
            ReadVector(j.at("position"),o.Position);ReadVector(j.at("rotation"),o.Rotation);ReadVector(j.at("scale"),o.Scale);ValidateTransform(o.Position,o.Rotation,o.Scale);ReadVector(j.at("color"),o.Color,true);ReadVector(j.at("velocity"),o.Velocity);
            for(float v:o.Velocity)if(std::abs(v)>10000)throw std::runtime_error("Velocity outside allowed range");
            o.Metallic=ReadRange(j.at("metallic"),0,1);o.Roughness=ReadRange(j.at("roughness"),0,1);
            o.Visible=j.at("visible").get<bool>();o.Wireframe=j.at("wireframe").get<bool>();o.Dynamic=j.at("dynamic").get<bool>();o.Character=j.at("character").get<bool>();
            o.Mass=ReadRange(j.at("mass"),.01f,10000);o.Restitution=ReadRange(j.at("bounce"),0,1);o.Friction=ReadRange(j.at("friction"),0,20);o.MoveSpeed=ReadRange(j.at("speed"),0,100);o.JumpSpeed=ReadRange(j.at("jump"),0,100);
            if(!j.at("vertices").is_array()||j.at("vertices").size()>MAX_EDIT_VERTICES*3||!j.at("indices").is_array()||j.at("indices").size()>MAX_EDIT_INDICES)throw std::runtime_error("Mesh exceeds limits");
            o.Vertices=j.at("vertices").get<std::vector<float>>();o.Indices=ReadUnsignedArray<uint32_t>(j.at("indices"),MAX_EDIT_INDICES);o.VertexColors=j.at("vertex_colors").get<std::vector<float>>();
            if(o.Vertices.size()%3||o.Indices.size()%3||(!o.Vertices.empty()&&o.Indices.empty())||(o.Vertices.empty()&&!o.Indices.empty())||(!o.VertexColors.empty()&&o.VertexColors.size()!=o.Vertices.size()))throw std::runtime_error("Malformed mesh");
            for(float v:o.Vertices)if(!std::isfinite(v))throw std::runtime_error("Invalid vertex");
            for(float v:o.VertexColors)if(!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Invalid vertex color");
            for(uint32_t i:o.Indices)if(i>=o.Vertices.size()/3)throw std::runtime_error("Mesh index outside vertex range");
            const auto& properties=j.at("properties");if(!properties.is_object()||properties.size()>256)throw std::runtime_error("Invalid properties");
            for(auto it=properties.begin();it!=properties.end();++it) {
                ObjectProperty v;int kind=ReadInteger(it.value().at("kind"),0,2);if(kind<0||kind>2)throw std::runtime_error("Invalid property kind");
                v.Type=ObjectProperty::Kind(kind);v.NumberValue=it.value().at("number").get<double>();v.BoolValue=it.value().at("boolean").get<bool>();v.TextValue=it.value().at("text").get<std::string>();
                if(!SetProperty(o,it.key(),v))throw std::runtime_error("Invalid property value");
            }
            if(schema>=2)ReadModelFields(j,o,path);
            totalVertices+=o.Vertices.size()/3;if(totalVertices>1000000)throw std::runtime_error("Project mesh budget exceeded");
            for(const auto& previous:objects)if(previous.Id==o.Id)throw std::runtime_error("Duplicate stable object ID");
            objects.push_back(std::move(o));
        }
        auto scripts=std::unique_ptr<ScriptDocuments>(new ScriptDocuments());
        const auto& docs=root.at("scripts");if(!docs.is_array()||docs.size()>kScriptSlots)throw std::runtime_error("Invalid script count");
        for(size_t i=0;i<docs.size();++i){auto name=docs[i].at("name").get<std::string>(),code=docs[i].at("code").get<std::string>();if(name.size()>63||code.size()>65535||name.find('\0')!=std::string::npos||code.find('\0')!=std::string::npos)throw std::runtime_error("Invalid script document");if(root.contains("project")&&path.substr(path.find_last_of("/\\")==std::string::npos?0:path.find_last_of("/\\")+1)=="project.lunoryx"&&docs[i].contains("file")){auto file=docs[i].at("file").get<std::string>();if(file.compare(0,8,"scripts/")!=0)throw std::runtime_error("Script file must stay in scripts/");auto source=ProjectAssetPath(file,path);if(AssetExists(source)){auto bytes=ReadAssetFile(source,65535);code.assign(bytes.begin(),bytes.end());if(code.find('\0')!=std::string::npos)throw std::runtime_error("Script file contains NUL");}}auto& doc=(*scripts)[i];doc.Allocated=true;doc.Language=docs[i].contains("language")?ReadInteger(docs[i].at("language"),0,1):0;doc.Module=docs[i].value("module",false);doc.Open=docs[i].value("open",true);std::snprintf(doc.Name,sizeof(doc.Name),"%s",name.c_str());code=CheckedScriptText(std::move(code));std::snprintf(doc.Code,sizeof(doc.Code),"%s",code.c_str());}
        CheckUniqueScriptNames(*scripts);
        float top[3],bottom[3];ReadVector(root.at("sky_top"),top,true);ReadVector(root.at("sky_bottom"),bottom,true);
        int preset=ReadInteger(root.at("sky_preset"),0,2);if(preset<0||preset>2)throw std::runtime_error("Unknown sky preset");
        float gravity=ReadRange(root.at("gravity"),-30,0);bool ground=root.at("ground").get<bool>();bool continuous=root.value("continuous_collision",true);
        std::string projectName=root.contains("project")?root["project"].value("name",std::string("Project")):std::string();if(projectName.size()>128||projectName.find('\0')!=std::string::npos)throw std::runtime_error("Invalid project name");
        std::vector<StudioSound> sounds;
        if(root.contains("sounds")){
            const auto& savedSounds=root.at("sounds");if(!savedSounds.is_array()||savedSounds.size()>64)throw std::runtime_error("Invalid sound count");
            for(const auto& j:savedSounds){StudioSound sound(false);if(j.contains("data"))sound.Data=ReadBinary(j.at("data"));if(sound.Data.size()>16*1024*1024)throw std::runtime_error("Embedded audio exceeds budget");sound.Name=j.at("name").get<std::string>();sound.Path=j.at("path").get<std::string>();
                if(sound.Name.size()>255||sound.Path.size()>511||sound.Name.find('\0')!=std::string::npos||sound.Path.find('\0')!=std::string::npos)throw std::runtime_error("Invalid audio path or name");
                ReadVector(j.at("position"),sound.Position);sound.Volume=ReadRange(j.at("volume"),0,2);sound.Pitch=ReadRange(j.at("pitch"),.1f,4);sound.Loop=j.at("loop").get<bool>();sound.Spatial=j.at("spatial").get<bool>();sound.Autoplay=j.at("autoplay").get<bool>();sound.Path=ProjectAssetPath(sound.Path,path);sounds.push_back(sound);}
        }
        StudioGameSettings game;
        if(root.contains("game"))game=ReadGameSettings(root["game"],path);
        // Commit only after the complete document has passed validation. Never execute loaded scripts.
        StopManagedScripts();
        for(const auto& sound:g_Sounds){studio_audio_unload(sound.Id);}
        for(const auto& o:objects)ReserveObjectId(o.Id);
        for(auto& sound:sounds)sound.Id=NewObjectId();
        g_Sounds.swap(sounds);g_SelectedSound=-1;
        g_Game=game;
        StopAnimationPreview();ResetAnimationPlayers();g_ActorSpawns.clear();g_DebugPaused=g_DebugStep=false;
        g_Objects.swap(objects);g_Studio.Scripts.swap(*scripts);std::copy(top,top+3,g_SkyTop);std::copy(bottom,bottom+3,g_SkyBottom);g_SkyPreset=preset;
        g_Physics.Continuous=continuous;g_Physics.DroppedTime=0;g_Physics.Gravity=gravity;g_Physics.Ground=ground;g_Physics.Accumulator=0;g_PhysicsRunning=false;g_SimulationRunning=false;g_PlaySnapshot.clear();g_SelectedObjectIndex=-1;
        g_Studio.RequestDocument=-2;g_Studio.ActiveScript=0;g_Studio.HasSculptUndo=false;g_ObjectHistory.Clear();g_ObjectHistory.Capture(g_Objects);
        if(g_LuaState)ClearUpdateCallback(g_LuaState);
        g_CSharpTrusted=false;
        g_ProjectDirectory=root.contains("project")?PathDirectory(AbsoluteAssetPath(path)):std::string();g_ProjectName=projectName;g_ShowProjectPicker=false;
        g_OpenedProject=AbsoluteAssetPath(path);std::snprintf(g_ProjectPath,sizeof(g_ProjectPath),"%s",path.c_str());g_SavedDocumentHash=CompleteDocumentHash();
        AppendConsole("Loaded project without executing scripts: "+path+"\n");return true;
    } catch(const std::exception& error){AppendConsole(std::string("Project load failed: ")+error.what()+"\n");return false;}
}

static uint64_t CompleteDocumentHash(){
    uint64_t h=StudioDocumentHash();h^=uint64_t(g_Game.StartupScript);h^=g_Game.CameraTarget;for(char c:g_Game.Title)h=(h^static_cast<unsigned char>(c))*1099511628211ULL;
    for(const auto& doc:g_Studio.Scripts)if(doc.Allocated){for(const char* c=doc.Name;*c;++c)h=(h^static_cast<unsigned char>(*c))*1099511628211ULL;for(const char* c=doc.Code;*c;++c)h=(h^static_cast<unsigned char>(*c))*1099511628211ULL;}
    return h;
}
static int g_PendingProjectAction=0;
static std::string g_PendingProjectPath;
static bool g_ExitRequested=false,g_OpenSaveAs=false;
static void PerformProjectAction(){int action=g_PendingProjectAction;g_PendingProjectAction=0;if(action==1)g_ExitRequested=true;else if(action==2)LoadStudioProject(g_PendingProjectPath);else if(action==3){g_Objects.clear();g_SelectedObjectIndex=-1;}else if(action==4)CreateSampleGame();else if(action==5)g_ShowProjectPicker=true;}
static void RequestProjectAction(int action,const std::string& path=""){if(g_PhysicsRunning)StopSimulation();g_PendingProjectAction=action;g_PendingProjectPath=path;if(!ProjectDirty())PerformProjectAction();}
static bool SaveCurrentProject(){if(!SyncProjectFiles())return false;return SaveStudioProject(g_OpenedProject.empty()?std::string(g_ProjectPath):g_OpenedProject);}
static void RenderProjectDialogs(){
    if(g_PendingProjectAction)ImGui::OpenPopup("Unsaved project");
    if(ImGui::BeginPopupModal("Unsaved project",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted("Save changes before continuing?");
        if(ImGui::Button("Save")){if(SaveCurrentProject()){PerformProjectAction();ImGui::CloseCurrentPopup();}}
        ImGui::SameLine();if(ImGui::Button("Discard")){PerformProjectAction();ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Cancel")){g_PendingProjectAction=0;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }
    if(g_OpenSaveAs){ImGui::OpenPopup("Save Project As");g_OpenSaveAs=false;}
    if(ImGui::BeginPopupModal("Save Project As",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::InputText("Path",g_ProjectPath,sizeof(g_ProjectPath));
        if(ImGui::Button("Save")&&SaveStudioProject(g_ProjectPath))ImGui::CloseCurrentPopup();
        ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
}
