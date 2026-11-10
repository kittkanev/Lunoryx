#pragma once
static bool g_InModelWorker=false;
static nlohmann::json SaveWorkerObject(const GameObject& object){
  nlohmann::json modifiers=nlohmann::json::array();for(const auto& m:object.Modifiers)modifiers.push_back({{"type",int(m.Type)},{"enabled",m.Enabled},{"axis",m.Axis},{"count",m.Count},{"levels",m.Levels},{"amount",m.Amount},{"offset",SaveVector(m.Offset)}});
  auto result=nlohmann::json{{"name",object.Name},{"type",int(object.Type)},{"position",SaveVector(object.Position)},{"rotation",SaveVector(object.Rotation)},{"scale",SaveVector(object.Scale)},{"color",SaveVector(object.Color)},{"metallic",object.Metallic},{"roughness",object.Roughness},{"mesh",SaveModelMesh(Model::FromObject(object))},{"modifiers",modifiers},{"procedural",!object.GeometryNodes.empty()}};
  if(!object.Modifiers.empty())result["base_mesh"]=SaveModelMesh(object.BaseMesh);
  return result;
}
static GameObject ReadWorkerObject(const nlohmann::json& value){GameObject object(value.at("name").get<std::string>(),MeshType(ReadInteger(value.at("type"),0,4)),false);ReadVector(value.at("position"),object.Position);ReadVector(value.at("rotation"),object.Rotation);ReadVector(value.at("scale"),object.Scale);ReadVector(value.at("color"),object.Color,true);ValidateTransform(object.Position,object.Rotation,object.Scale);object.Metallic=ReadRange(value.at("metallic"),0,1);object.Roughness=ReadRange(value.at("roughness"),0,1);Model::ToObject(object,ReadModelMesh(value.at("mesh")));
  for(const auto& m:value.at("modifiers")){if(object.Modifiers.size()>=16)throw std::runtime_error("Worker modifier budget exceeded");StudioModifier modifier;modifier.Type=StudioModifier::Kind(ReadInteger(m.at("type"),0,3));modifier.Enabled=m.at("enabled").get<bool>();modifier.Axis=ReadInteger(m.at("axis"),0,2);modifier.Count=ReadInteger(m.at("count"),2,64);modifier.Levels=ReadInteger(m.at("levels"),1,4);modifier.Amount=ReadRange(m.at("amount"),.00001f,1000);ReadVector(m.at("offset"),modifier.Offset);Model::ValidateModifier(modifier);object.Modifiers.push_back(modifier);}if(!object.Modifiers.empty())object.BaseMesh=ReadModelMesh(value.at("base_mesh"));if(value.value("procedural",false))object.GeometryNodes.push_back(StudioGeometryNode());return object;
}
static int RunNativeModelWorker(){
  g_InModelWorker=true;
#ifdef _WIN32
  _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
  try{std::string line;char block[8192];while(std::fgets(block,sizeof(block),stdin)){size_t n=std::strlen(block);if(line.size()+n>64*1024*1024)throw std::runtime_error("Geometry worker input exceeds budget");line.append(block,n);if(n&&block[n-1]=='\n')break;}CheckJsonBudget(line.begin(),line.end());auto request=nlohmann::json::parse(line);std::vector<GameObject> objects;for(const auto& value:request.at("objects")){if(objects.size()>=2)throw std::runtime_error("Geometry worker operand budget exceeded");objects.push_back(ReadWorkerObject(value));}int index=ModelCommandLocal(objects,0,request.at("action").get<std::string>(),request.at("options"));std::cout<<nlohmann::json({{"ok",true},{"object",SaveWorkerObject(objects.at(size_t(index)))}}).dump()<<'\n'<<std::flush;return 0;}
  catch(const std::exception& error){std::cout<<nlohmann::json({{"ok",false},{"error",error.what()}}).dump()<<'\n'<<std::flush;return 1;}
}
static int ModelCommand(std::vector<GameObject>& objects,int index,const std::string& action,const nlohmann::json& options){
  if(!g_ModelWorkerReady||g_InModelWorker)return ModelCommandLocal(objects,index,action,options);
  if(index<0||size_t(index)>=objects.size())throw std::runtime_error("Invalid mesh object");
  auto operands=nlohmann::json::array({SaveWorkerObject(objects[size_t(index)])});auto args=options;
  if(action=="boolean"){int other=options.at("other").get<int>();if(other<0||size_t(other)>=objects.size()||other==index)throw std::runtime_error("Invalid boolean operand");operands.push_back(SaveWorkerObject(objects[size_t(other)]));args["other"]=1;}
  WorkerProcess worker;worker.Start(GamePackage::ExecutablePath(),{"--model-worker"});auto reply=worker.Exchange(nlohmann::json({{"action",action},{"options",args},{"objects",operands}}).dump(),3000);auto result=nlohmann::json::parse(reply);if(!result.value("ok",false))throw std::runtime_error(result.value("error",std::string("Geometry worker failed")));
  auto geometry=ReadWorkerObject(result.at("object"));CheckSceneMeshBudget(objects,geometry.Vertices.size()/3,action=="boolean"?-1:index);
  if(action=="boolean"){CheckObjectLimit(objects.size());geometry.Id=NewObjectId();objects.push_back(std::move(geometry));return int(objects.size())-1;}
  auto& original=objects[size_t(index)];original.Vertices.swap(geometry.Vertices);original.Indices.swap(geometry.Indices);original.Faces.swap(geometry.Faces);original.VertexColors.swap(geometry.VertexColors);original.UV.swap(geometry.UV);original.Normals.swap(geometry.Normals);original.FaceMaterials.swap(geometry.FaceMaterials);original.Seams.swap(geometry.Seams);original.Modifiers.swap(geometry.Modifiers);original.BaseMesh=std::move(geometry.BaseMesh);return index;
}
