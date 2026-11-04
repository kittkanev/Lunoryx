#pragma once
namespace GeometryNodes {
inline const char *Name(StudioGeometryNode::Kind kind) {
  static const char *names[] = {"Box",   "Sphere", "Cylinder", "Transform",
                                "Noise", "Array",  "Mirror",   "Output"};
  return int(kind) >= 0 && int(kind) <= 7 ? names[int(kind)] : "Invalid";
}
inline std::vector<StudioGeometryNode> Default() {
  StudioGeometryNode source, transform, output;
  source.Id = 1;
  source.X = 30;
  source.Y = 50;
  transform.Id = 2;
  transform.Input = 1;
  transform.Type = StudioGeometryNode::Transform;
  transform.Vector[0] = transform.Vector[1] = transform.Vector[2] = 0;
  transform.X = 280;
  transform.Y = 50;
  output.Id = 3;
  output.Input = 2;
  output.Type = StudioGeometryNode::Output;
  output.X = 530;
  output.Y = 50;
  return {source, transform, output};
}
inline void Validate(const std::vector<StudioGeometryNode> &nodes) {
  if (nodes.empty() || nodes.size() > 32)
    throw std::runtime_error("Geometry graph requires 1..32 nodes");
  std::map<uint32_t, size_t> ids;
  size_t outputs = 0;
  for (size_t i = 0; i < nodes.size(); ++i) {
    const auto &n = nodes[i];
    if (n.Id == 0 || n.Id > 1000000 || !ids.emplace(n.Id, i).second ||
        int(n.Type) < 0 || int(n.Type) > 7)
      throw std::runtime_error("Invalid or duplicate geometry node ID/type");
    outputs += n.Type == StudioGeometryNode::Output;
    for (float v : n.Vector)
      if (!std::isfinite(v) || std::abs(v) > 10000)
        throw std::runtime_error("Invalid node vector");
    if (!std::isfinite(n.Amount) || std::abs(n.Amount) > 1000 ||
        !std::isfinite(n.Frequency) || n.Frequency < .001f ||
        n.Frequency > 1000 || !std::isfinite(n.Angle) ||
        std::abs(n.Angle) > 36000 || n.Count < 2 || n.Count > 32 ||
        n.Axis < 0 || n.Axis > 2 || !std::isfinite(n.X) ||
        !std::isfinite(n.Y) || std::abs(n.X) > 16384 || std::abs(n.Y) > 16384)
      throw std::runtime_error("Geometry node parameter outside limits");
    if (n.Type == StudioGeometryNode::Transform && std::abs(n.Amount) < .0001f)
      throw std::runtime_error("Node scale must be nonzero");
    if (n.Type <= StudioGeometryNode::Cylinder)
      for (float v : n.Vector)
        if (v < .0001f)
          throw std::runtime_error("Generator dimensions must be positive");
    if (n.Type <= StudioGeometryNode::Cylinder && n.Input)
      throw std::runtime_error("Generator cannot have a geometry input");
  }
  if (outputs != 1)
    throw std::runtime_error("Geometry graph needs exactly one Output");
  // A graph is small and has one input per node. Iterative walks also avoid
  // recursive callbacks crossing MinGW's DLL-visibility boundaries.
  for (size_t start = 0; start < nodes.size(); ++start) {
    std::set<size_t> path;
    size_t i = start;
    while (nodes[i].Type > StudioGeometryNode::Cylinder) {
      if (!path.insert(i).second)
        throw std::runtime_error("Geometry graph contains a cycle");
      auto found = ids.find(nodes[i].Input);
      if (found == ids.end())
        throw std::runtime_error("Connect every geometry input");
      if (nodes[found->second].Type == StudioGeometryNode::Output)
        throw std::runtime_error("Output cannot feed another node");
      i = found->second;
    }
  }
}
inline StudioMesh Evaluate(const std::vector<StudioGeometryNode> &nodes) {
  Validate(nodes);
  std::map<uint32_t, size_t> ids;
  size_t output = 0;
  for (size_t i = 0; i < nodes.size(); ++i) {
    ids[nodes[i].Id] = i;
    if (nodes[i].Type == StudioGeometryNode::Output)
      output = i;
  }
  // Single geometry input per node means the reachable result is a bounded
  // chain. Evaluate iteratively, keeping only one mesh live at a time.
  std::vector<size_t> chain;
  size_t i = output;
  while (true) {
    chain.push_back(i);
    if (nodes[i].Type <= StudioGeometryNode::Cylinder)
      break;
    i = ids.at(nodes[i].Input);
  }
  StudioMesh mesh;
  for (auto at = chain.rbegin(); at != chain.rend(); ++at) {
    const auto &n = nodes[*at];
    if (n.Type <= StudioGeometryNode::Cylinder) {
      MeshType type = n.Type == StudioGeometryNode::Box ? MeshType::Cube
                      : n.Type == StudioGeometryNode::Sphere
                          ? MeshType::Sphere
                          : MeshType::Cylinder;
      GameObject source("Node source", type);
      mesh = Model::FromObject(source);
      Model::Polygonize(mesh);
      for (size_t v = 0; v < mesh.Positions.size(); ++v)
        mesh.Positions[v] *= n.Vector[v % 3];
    } else if (n.Type == StudioGeometryNode::Transform) {
      float angle = n.Angle * PI / 180, c = std::cos(angle),
            s = std::sin(angle);
      for (size_t v = 0; v < mesh.Positions.size(); v += 3) {
        float x = mesh.Positions[v] * n.Amount,
              z = mesh.Positions[v + 2] * n.Amount;
        mesh.Positions[v] = c * x + s * z + n.Vector[0];
        mesh.Positions[v + 1] = mesh.Positions[v + 1] * n.Amount + n.Vector[1];
        mesh.Positions[v + 2] = -s * x + c * z + n.Vector[2];
      }
      if (n.Amount < 0)
        for (auto &face : mesh.Faces)
          std::reverse(face.begin(), face.end());
    } else if (n.Type == StudioGeometryNode::Noise) {
      Model::Triangulate(mesh);
      for (size_t v = 0; v < mesh.Positions.size(); v += 3) {
        float noise = std::sin((mesh.Positions[v] * 1.71f +
                                mesh.Positions[v + 1] * 2.93f +
                                mesh.Positions[v + 2] * 3.31f) *
                               n.Frequency) *
                      n.Amount;
        for (int k = 0; k < 3; ++k)
          mesh.Positions[v + k] += mesh.Normals[v + k] * noise;
      }
    } else if (n.Type == StudioGeometryNode::Array ||
               n.Type == StudioGeometryNode::Mirror) {
      StudioModifier m;
      m.Type = n.Type == StudioGeometryNode::Array ? StudioModifier::Array
                                                   : StudioModifier::Mirror;
      m.Count = n.Count;
      m.Axis = n.Axis;
      std::copy(n.Vector, n.Vector + 3, m.Offset);
      Model::Replicate(mesh, m);
    }
    Model::Check(mesh);
    Model::Triangulate(mesh);
  }
  return mesh;
}
inline void Apply(GameObject &object,
                  const std::vector<StudioGeometryNode> &nodes) {
  if (!object.Modifiers.empty())
    throw std::runtime_error("Apply modifiers before using geometry nodes");
  auto mesh = Evaluate(nodes); // Commit only a fully validated result.
  Model::ToObject(object, mesh);
  object.GeometryNodes = nodes;
}
} // namespace GeometryNodes
