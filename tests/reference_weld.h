#pragma once
// Frozen complete Weld operation from 1.0.1 (216242f). Shared Compact and
// Triangulate code is unchanged for these fixtures. Test/benchmark use only.
namespace Model {
inline void ReferenceWeld(StudioMesh &m, const std::set<uint32_t> &selected,
                 float tolerance) {
  if (selected.size() < 2 || !std::isfinite(tolerance) || tolerance <= 0)
    throw std::runtime_error(
        "Select at least two vertices and a positive weld distance");
  std::map<uint32_t, uint32_t> remap;
  for (auto v : selected) {
    if (v >= m.Positions.size() / 3)
      throw std::runtime_error("Invalid vertex selection");
    remap[v] = v;
    for (const auto &r : remap)
      if (r.first != v) {
        Vec3 d = Subtract(Point(m, v), Point(m, r.first));
        if (Dot(d, d) <= double(tolerance) * tolerance) {
          remap[v] = r.second;
          break;
        }
      }
  }
  std::vector<std::vector<uint32_t>> faces;
  std::vector<uint32_t> materials;
  size_t faceIndex = 0;
  for (auto f : m.Faces) {
    for (auto &v : f)
      if (remap.count(v))
        v = remap[v];
    std::vector<uint32_t> clean;
    for (auto v : f)
      if (clean.empty() || clean.back() != v)
        clean.push_back(v);
    if (clean.size() > 1 && clean.front() == clean.back())
      clean.pop_back();
    std::set<uint32_t> unique(clean.begin(), clean.end());
    if (unique.size() >= 3 && unique.size() == clean.size()) {
      faces.push_back(clean);
      if (!m.FaceMaterials.empty())
        materials.push_back(m.FaceMaterials[faceIndex]);
    }
    ++faceIndex;
  }
  if (faces.empty())
    throw std::runtime_error("Weld would collapse the mesh");
  m.Faces = std::move(faces);
  m.FaceMaterials = std::move(materials);
  m.Seams.clear();
  Compact(m);
}
}
