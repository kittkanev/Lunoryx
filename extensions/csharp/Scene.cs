using System.Text.Json.Nodes;
namespace Luaryx;
public readonly record struct Vector3(float X,float Y,float Z) {
    public static Vector3 Zero => new(0,0,0);
    public static Vector3 operator +(Vector3 a,Vector3 b)=>new(a.X+b.X,a.Y+b.Y,a.Z+b.Z);
    public static Vector3 operator *(Vector3 a,float b)=>new(a.X*b,a.Y*b,a.Z*b);
    internal JsonArray Json()=>new(X,Y,Z);
    internal static Vector3 Read(JsonNode? n)=>new(n![0]!.GetValue<float>(),n[1]!.GetValue<float>(),n[2]!.GetValue<float>());
}
public abstract class Script {
    public virtual void OnStart(){} public virtual void OnUpdate(float deltaTime){}
    public virtual void OnMessage(string name,JsonNode? value){}
    public virtual void OnTrigger(string kind,ulong zone,ulong actor){}
    public virtual void OnStop(){}
}
public sealed class Entity {
    public ulong Id {get;} internal JsonObject Data;internal bool Alive=true;
    internal Entity(JsonObject data){Data=data;Id=data["id"]!.GetValue<ulong>();}
    public string Name=>Data["name"]!.GetValue<string>();
    private void Check(){if(!Alive)throw new InvalidOperationException($"Entity {Id} no longer exists");}
    private Vector3 Get(string key){Check();return Vector3.Read(Data[key]);}
    private void Set(string key,Vector3 value){Check();var array=value.Json();Data[key]=array;Scene.Command(new(){["op"]="transform",["id"]=Id,[key]=array.DeepClone()});}
    public Vector3 Position{get=>Get("position");set=>Set("position",value);}
    public Vector3 Rotation{get=>Get("rotation");set=>Set("rotation",value);}
    public Vector3 Scale{get=>Get("scale");set=>Set("scale",value);}
    public Vector3 Velocity{get=>Get("velocity");set=>Set("velocity",value);}
    public void SetProperty(string name,JsonNode? value){Check();var properties=Data["properties"]!.AsObject();properties[name]=value?.DeepClone();Scene.Command(new(){["op"]="property",["id"]=Id,["name"]=name,["value"]=value?.DeepClone()});}
    public JsonNode? GetProperty(string name){Check();return Data["properties"]?[name]?.DeepClone();}
    public void Destroy(){Check();Alive=false;Scene.Command(new(){["op"]="remove",["id"]=Id});}
}
public static class Scene {
    private static readonly Dictionary<ulong,Entity> Entities=[];
    internal static readonly JsonArray Commands=[];
    internal static readonly JsonArray Messages=[];
    internal static readonly List<string> Logs=[];
    private static ulong NextId;
    internal static void Refresh(JsonObject request){Commands.Clear();Messages.Clear();Logs.Clear();NextId=request["nextId"]!.GetValue<ulong>();var present=new HashSet<ulong>();foreach(var value in request["scene"]!.AsArray()){var data=(JsonObject)value!.DeepClone();var id=data["id"]!.GetValue<ulong>();present.Add(id);if(Entities.TryGetValue(id,out var entity)){entity.Data=data;entity.Alive=true;}else Entities.Add(id,new Entity(data));}foreach(var id in Entities.Keys.Where(id=>!present.Contains(id)).ToArray()){Entities[id].Alive=false;Entities.Remove(id);}}

    internal static void Command(JsonObject command){if(Commands.Count>=4096)throw new InvalidOperationException("Scene command budget exceeded");Commands.Add(command);}
    public static Entity Get(ulong id)=>Entities.TryGetValue(id,out var entity)&&entity.Alive?entity:throw new KeyNotFoundException($"Entity {id} no longer exists");
    public static Entity? Find(string name){var matches=Entities.Values.Where(x=>x.Alive&&x.Name==name).ToArray();return matches.Length>1?throw new InvalidOperationException("Ambiguous entity name; use a stable ID"):matches.FirstOrDefault();}
    public static IReadOnlyList<Entity> All=>Entities.Values.Where(x=>x.Alive).ToArray();
    public static Entity Create(string name,string primitive="cube"){
        var data=new JsonObject{["id"]=++NextId,["name"]=name,["type"]=primitive,["position"]=new Vector3().Json(),["rotation"]=new Vector3().Json(),["scale"]=new Vector3(1,1,1).Json(),["velocity"]=new Vector3().Json(),["properties"]=new JsonObject()};
        var entity=new Entity(data);Entities.Add(entity.Id,entity);Command(new(){["op"]="add",["id"]=entity.Id,["name"]=name,["type"]=primitive});return entity;
    }
    public static void Emit(string name,JsonNode? value=null){if(Messages.Count>=256||name.Length>128)throw new InvalidOperationException("Message budget exceeded");Messages.Add(new JsonObject{["name"]=name,["value"]=value?.DeepClone()});}
    public static void Log(object? value){var text=value?.ToString()??"null";if(text.Length>4096)text=text[..4096];if(Logs.Count<128)Logs.Add(text);}
}
