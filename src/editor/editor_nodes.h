#pragma once
static bool g_ReportNodes = false;
static uint64_t g_NodeObject = 0;
static std::vector<StudioGeometryNode> g_NodeDraft, g_NodeCommitted;
static uint32_t g_NodeLink = 0, g_NodeSelected = 0;
static ImVec2 g_NodePan(20, 30);
static std::string g_NodeError;
static bool SameGeometryGraph(const std::vector<StudioGeometryNode> &a,
                              const std::vector<StudioGeometryNode> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    const auto &x = a[i];
    const auto &y = b[i];
    if (x.Id != y.Id || x.Input != y.Input || x.Type != y.Type ||
        x.Amount != y.Amount || x.Frequency != y.Frequency ||
        x.Angle != y.Angle || x.Count != y.Count || x.Axis != y.Axis ||
        x.X != y.X || x.Y != y.Y ||
        !std::equal(x.Vector, x.Vector + 3, y.Vector))
      return false;
  }
  return true;
}
static void CreateNodeObject() {
  if (g_PhysicsRunning || g_ModelBusy || g_Objects.size() >= 10000)
    return;
  GameObject object("Procedural Geometry");
  GeometryNodes::Apply(object, GeometryNodes::Default());
  object.Position[1] = 1;
  object.Color[0] = .15f;
  object.Color[1] = .7f;
  object.Color[2] = .78f;
  g_Objects.push_back(std::move(object));
  SelectObject(int(g_Objects.size()) - 1);
  g_NodeObject = 0;
  g_ShowNodes = true;
}
static void RenderGeometryNodes() {
  if (!g_ShowNodes)
    return;
  if (g_ReportNodes)
    ImGui::SetNextWindowPos(ImVec2(10, 430), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(g_ReportNodes ? ImVec2(1000, 360) : ImVec2(930, 470),
                           ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Geometry Nodes", &g_ShowNodes)) {
    ImGui::BeginDisabled(g_PhysicsRunning || g_ModelBusy);
    if (ImGui::Button("New node mesh"))
      CreateNodeObject();
    int selected = g_SelectedObjectIndex;
    if (selected < 0 || size_t(selected) >= g_Objects.size() ||
        g_Objects[size_t(selected)].GeometryNodes.empty()) {
      ImGui::TextWrapped(
          "Create a procedural mesh to start. Preview evaluates its geometry "
          "graph. Bake keeps the result for sculpting, UVs and modifiers.");
      ImGui::EndDisabled();
      ImGui::End();
      return;
    }
    auto &object = g_Objects[size_t(selected)];
    if (g_NodeObject != object.Id ||
        !SameGeometryGraph(g_NodeCommitted, object.GeometryNodes)) {
      g_NodeObject = object.Id;
      g_NodeDraft = g_NodeCommitted = object.GeometryNodes;
      g_NodeLink = 0;
      g_NodeError.clear();
      g_NodePan = ImVec2(20, 30);
    }
    ImGui::SameLine();
    if (ImGui::Button("Preview / Commit")) {
      try {
        GeometryNodes::Validate(g_NodeDraft);
        auto draft = g_NodeDraft;
        uint64_t target = object.Id;
        QueueModelJob([draft, target](std::vector<GameObject> &scene) {
          auto it = std::find_if(
              scene.begin(), scene.end(),
              [target](const GameObject &o) { return o.Id == target; });
          if (it == scene.end())
            throw std::runtime_error("Node object no longer exists");
          GeometryNodes::Apply(*it, draft);
          size_t vertices = 0;
          for (const auto &o : scene)
            vertices += o.Vertices.size() / 3;
          if (vertices > 1000000)
            throw std::runtime_error("Project mesh budget exceeded");
        });
        g_NodeError.clear();
      } catch (const std::exception &e) {
        g_NodeError = e.what();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert draft")) {
      g_NodeDraft = g_NodeCommitted;
      g_NodeLink = 0;
      g_NodeError.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Bake for mesh / sculpt")) {
      g_ObjectHistory.Capture(g_Objects);
      object.GeometryNodes.clear();
      g_ObjectHistory.Capture(g_Objects);
      g_NodeObject = 0;
      ImGui::EndDisabled();
      ImGui::End();
      return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete node") && g_NodeSelected) {
      auto it = std::find_if(
          g_NodeDraft.begin(), g_NodeDraft.end(),
          [](const StudioGeometryNode &n) { return n.Id == g_NodeSelected; });
      if (it != g_NodeDraft.end() && it->Type != StudioGeometryNode::Output) {
        uint32_t previous = it->Input;
        for (auto &n : g_NodeDraft)
          if (n.Input == it->Id)
            n.Input = previous;
        g_NodeDraft.erase(it);
        g_NodeSelected = 0;
        g_NodeLink = 0;
      }
    }
    static int type = 3;
    const char *names[] = {"Box",   "Sphere", "Cylinder", "Transform",
                           "Noise", "Array",  "Mirror"};
    ImGui::SetNextItemWidth(150);
    ImGui::Combo("Add type", &type, names, 7);
    ImGui::SameLine();
    if (ImGui::Button("Add node") && g_NodeDraft.size() < 32) {
      StudioGeometryNode node;
      node.Type = StudioGeometryNode::Kind(type);
      node.Id = 1;
      for (const auto &n : g_NodeDraft)
        node.Id = std::max(node.Id, n.Id + 1);
      node.X = float(g_NodeDraft.size()) * 210;
      node.Y = 70;
      if (type > 2)
        node.Vector[0] = node.Vector[1] = node.Vector[2] = 0;
      if (type == 4)
        node.Amount = .15f;
      if (type == 5) {
        node.Vector[0] = 1.5f;
        node.Count = 3;
      }
      for (auto &n : g_NodeDraft)
        if (n.Type == StudioGeometryNode::Output) {
          node.Input = type > 2 ? n.Input : 0;
          if (type > 2)
            n.Input = node.Id;
          break;
        }
      g_NodeDraft.push_back(node);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(SameGeometryGraph(g_NodeDraft, g_NodeCommitted)
                               ? "Committed"
                               : "Draft: press Preview to save changes");
    if (!g_NodeError.empty())
      ImGui::TextColored(ImVec4(1, .5f, .35f, 1), "%s", g_NodeError.c_str());
    ImGui::TextUnformatted(
        "Drag node headers. Click an output pin, then an input pin to connect. "
        "MMB pans. One geometry input per node.");
    ImGui::BeginChild("Node canvas", ImVec2(0, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 origin = ImGui::GetCursorScreenPos(),
                 avail = ImGui::GetContentRegionAvail();
    auto *draw = ImGui::GetWindowDrawList();
    auto &io = ImGui::GetIO();
    if (ImGui::IsWindowHovered() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
      g_NodePan.x += io.MouseDelta.x;
      g_NodePan.y += io.MouseDelta.y;
    }
    for (float x = std::fmod(g_NodePan.x, 24.f); x < avail.x; x += 24)
      draw->AddLine(ImVec2(origin.x + x, origin.y),
                    ImVec2(origin.x + x, origin.y + avail.y),
                    IM_COL32(34, 45, 61, 255));
    for (float y = std::fmod(g_NodePan.y, 24.f); y < avail.y; y += 24)
      draw->AddLine(ImVec2(origin.x, origin.y + y),
                    ImVec2(origin.x + avail.x, origin.y + y),
                    IM_COL32(34, 45, 61, 255));
    auto position = [&](const StudioGeometryNode &n) {
      return ImVec2(origin.x + g_NodePan.x + n.X, origin.y + g_NodePan.y + n.Y);
    };
    for (const auto &n : g_NodeDraft)
      if (n.Input) {
        for (const auto &source : g_NodeDraft)
          if (source.Id == n.Input) {
            auto a = position(source), b = position(n);
            a.x += 190;
            a.y += 20;
            b.y += 20;
            draw->AddBezierCubic(a, ImVec2(a.x + 60, a.y),
                                 ImVec2(b.x - 60, b.y), b,
                                 IM_COL32(75, 205, 220, 255), 2);
          }
      }
    for (auto &n : g_NodeDraft) {
      ImGui::PushID(int(n.Id));
      auto p = position(n);
      draw->AddRectFilled(p, ImVec2(p.x + 190, p.y + 145),
                          IM_COL32(19, 28, 42, 255), 5);
      draw->AddRectFilled(p, ImVec2(p.x + 190, p.y + 27),
                          n.Type == StudioGeometryNode::Output
                              ? IM_COL32(79, 66, 111, 255)
                              : IM_COL32(26, 82, 95, 255),
                          5);
      draw->AddRect(p, ImVec2(p.x + 190, p.y + 145),
                    IM_COL32(62, 106, 123, 255), 5);
      ImGui::SetCursorScreenPos(ImVec2(p.x + 10, p.y + 3));
      ImGui::InvisibleButton("Move node", ImVec2(170, 24));
      if (ImGui::IsItemClicked())
        g_NodeSelected = n.Id;
      if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        n.X = std::max(-16000.f, std::min(16000.f, n.X + io.MouseDelta.x));
        n.Y = std::max(-16000.f, std::min(16000.f, n.Y + io.MouseDelta.y));
      }
      draw->AddText(ImVec2(p.x + 12, p.y + 6), IM_COL32(225, 240, 245, 255),
                    GeometryNodes::Name(n.Type));
      ImGui::SetCursorScreenPos(ImVec2(p.x + 10, p.y + 35));
      ImGui::BeginGroup();
      ImGui::PushItemWidth(115);
      if (n.Type <= StudioGeometryNode::Cylinder)
        ImGui::DragFloat3("##Dimensions", n.Vector, .02f, .001f, 1000, "%.2f");
      if (n.Type == StudioGeometryNode::Transform) {
        ImGui::DragFloat3("##Offset", n.Vector, .02f, -1000, 1000, "%.2f");
        ImGui::DragFloat("Scale", &n.Amount, .01f, .001f, 100);
        ImGui::DragFloat("Yaw", &n.Angle, 1, -360, 360);
      }
      if (n.Type == StudioGeometryNode::Noise) {
        ImGui::DragFloat("Strength", &n.Amount, .01f, -10, 10);
        ImGui::DragFloat("Frequency", &n.Frequency, .02f, .001f, 100);
      }
      if (n.Type == StudioGeometryNode::Array) {
        ImGui::SliderInt("Count", &n.Count, 2, 32);
        ImGui::DragFloat3("##Offset", n.Vector, .02f, -1000, 1000, "%.2f");
      }
      if (n.Type == StudioGeometryNode::Mirror) {
        const char *axes[] = {"X", "Y", "Z"};
        ImGui::Combo("Axis", &n.Axis, axes, 3);
      }
      if (n.Type == StudioGeometryNode::Output)
        ImGui::TextWrapped("Geometry result\nPreview to update mesh.");
      ImGui::PopItemWidth();
      ImGui::EndGroup();
      if (n.Type > StudioGeometryNode::Cylinder) {
        draw->AddCircleFilled(ImVec2(p.x, p.y + 20), 6,
                              IM_COL32(80, 208, 220, 255));
        ImGui::SetCursorScreenPos(ImVec2(p.x - 8, p.y + 12));
        ImGui::InvisibleButton("Geometry input", ImVec2(16, 16));
        if (ImGui::IsItemClicked() && g_NodeLink && g_NodeLink != n.Id) {
          n.Input = g_NodeLink;
          g_NodeLink = 0;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1))
          n.Input = 0;
      }
      if (n.Type != StudioGeometryNode::Output) {
        draw->AddCircleFilled(ImVec2(p.x + 190, p.y + 20), 6,
                              g_NodeLink == n.Id ? IM_COL32(255, 190, 80, 255)
                                                 : IM_COL32(80, 208, 220, 255));
        ImGui::SetCursorScreenPos(ImVec2(p.x + 182, p.y + 12));
        ImGui::InvisibleButton("Geometry output", ImVec2(16, 16));
        if (ImGui::IsItemClicked())
          g_NodeLink = n.Id;
      }
      ImGui::PopID();
    }
    if (g_NodeLink)
      for (const auto &n : g_NodeDraft)
        if (n.Id == g_NodeLink) {
          auto p = position(n);
          p.x += 190;
          p.y += 20;
          draw->AddBezierCubic(p, ImVec2(p.x + 60, p.y),
                               ImVec2(io.MousePos.x - 60, io.MousePos.y),
                               io.MousePos, IM_COL32(255, 190, 80, 255), 2);
        }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(avail);
    ImGui::EndChild();
    ImGui::EndDisabled();
  }
  ImGui::End();
}
