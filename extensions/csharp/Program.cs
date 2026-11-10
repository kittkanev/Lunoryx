using System.Reflection;
using System.Reflection.PortableExecutable;
using System.Runtime.Loader;
using System.Text;
using System.Text.Json.Nodes;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Luaryx;

Console.InputEncoding=new UTF8Encoding(false);Console.OutputEncoding=new UTF8Encoding(false);
var protocol=Console.Out;Console.SetOut(Console.Error);
List<Script> scripts=[];AssemblyLoadContext? context=null;
var references=(((string?)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES"))??"").Split(Path.PathSeparator,StringSplitOptions.RemoveEmptyEntries).Concat(Directory.GetFiles(AppContext.BaseDirectory,"*.dll")).Distinct().Where(path=>Path.GetFileName(path).StartsWith("System.")||Path.GetFileName(path) is "netstandard.dll" or "mscorlib.dll" or "Luaryx.Worker.dll" or "Microsoft.CSharp.dll").Where(IsManagedAssembly).Select(path=>MetadataReference.CreateFromFile(path)).ToList();
if(!references.Any(x=>x.Display==typeof(object).Assembly.Location))references.Add(MetadataReference.CreateFromFile(typeof(object).Assembly.Location));
while(true){string? line=Console.ReadLine();if(line is null)break;if(line.Length>8*1024*1024){protocol.WriteLine("{\"ok\":false,\"error\":\"Request exceeds 8 MiB\"}");continue;}
    try{var request=JsonNode.Parse(line)!.AsObject();var action=request["action"]!.GetValue<string>();Scene.Refresh(request);
        if(action=="compile"){
            foreach(var old in scripts)old.OnStop();scripts.Clear();context?.Unload();context=null;
            var sources=request["scripts"]!.AsArray();var trees=sources.Select(source=>CSharpSyntaxTree.ParseText(source!["code"]!.GetValue<string>(),new CSharpParseOptions(LanguageVersion.CSharp13),source["name"]!.GetValue<string>(),Encoding.UTF8)).ToArray();
            var compilation=CSharpCompilation.Create("LuaryxProject_"+Guid.NewGuid().ToString("N"),trees,references,new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary,optimizationLevel:OptimizationLevel.Release,allowUnsafe:false));
            using var bytes=new MemoryStream();var result=compilation.Emit(bytes);
            if(!result.Success){var diagnostics=new JsonArray();foreach(var diagnostic in result.Diagnostics.Where(x=>x.Severity==DiagnosticSeverity.Error).Take(64)){var location=diagnostic.Location.GetLineSpan();diagnostics.Add(new JsonObject{["file"]=location.Path??"",["line"]=location.StartLinePosition.Line+1,["column"]=location.StartLinePosition.Character+1,["message"]=diagnostic.GetMessage()});}protocol.WriteLine(new JsonObject{["ok"]=false,["error"]="C# compilation failed",["diagnostics"]=diagnostics}.ToJsonString());continue;}
            var entryTypes=new HashSet<string>();
            for(int i=0;i<trees.Length;i++)if(!(sources[i]?["module"]?.GetValue<bool>()??false)){
                var semantic=compilation.GetSemanticModel(trees[i]);
                foreach(var declaration in trees[i].GetRoot().DescendantNodes().OfType<ClassDeclarationSyntax>()){
                    var symbol=semantic.GetDeclaredSymbol(declaration);
                    if(symbol is not null)entryTypes.Add(symbol.ToDisplayString());
                }
            }
            context=new AssemblyLoadContext("Luaryx project",isCollectible:true);bytes.Position=0;var assembly=context.LoadFromStream(bytes);
            foreach(var type in assembly.GetTypes().Where(type=>!type.IsAbstract&&entryTypes.Contains(type.FullName??type.Name)&&typeof(Script).IsAssignableFrom(type))){var instance=(Script?)Activator.CreateInstance(type)??throw new InvalidOperationException("Script constructor failed");scripts.Add(instance);instance.OnStart();}
        }else if(action=="update"){
            foreach(var item in request["messages"]?.AsArray()??[]){foreach(var script in scripts)script.OnMessage(item!["name"]!.GetValue<string>(),item["value"]?.DeepClone());}
            foreach(var item in request["triggers"]?.AsArray()??[]){foreach(var script in scripts)script.OnTrigger(item!["event"]!.GetValue<string>(),item["zone"]!.GetValue<ulong>(),item["actor"]!.GetValue<ulong>());}
            float delta=request["dt"]!.GetValue<float>();foreach(var script in scripts)script.OnUpdate(delta);
        }else if(action=="stop"){foreach(var script in scripts)script.OnStop();scripts.Clear();context?.Unload();context=null;}
        else throw new InvalidOperationException("Unknown worker request");
        var response=new JsonObject{["ok"]=true,["commands"]=Scene.Commands.DeepClone(),["messages"]=Scene.Messages.DeepClone(),["logs"]=new JsonArray(Scene.Logs.Select(x=>(JsonNode?)JsonValue.Create(x)).ToArray())};protocol.WriteLine(response.ToJsonString());
    }catch(Exception error){protocol.WriteLine(new JsonObject{["ok"]=false,["error"]=(error.InnerException??error).Message}.ToJsonString());}
}

static bool IsManagedAssembly(string path){
    try{using var input=File.OpenRead(path);using var image=new PEReader(input);return image.HasMetadata;}
    catch(BadImageFormatException){return false;}
}
