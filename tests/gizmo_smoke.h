#pragma once
// Exercises the production ImGui/ImGuizmo input path inside the running editor.
struct GizmoSmokeTest {
  bool Enabled = false, Done = false, Passed = false;
  int Stage = 0, Frames = 0, Attempts = 0;
  float X = 0, Y = 0,DragX=100,DragY=0,Sign=1;
  int Axis=0;
  void Initialize() {
    if (!Enabled)
      return;
    const char* axis=std::getenv("LUNORYX_GIZMO_AXIS");Axis=axis&&axis[0]=='y'?1:(axis&&axis[0]=='z'?2:0);Sign=std::getenv("LUNORYX_GIZMO_NEGATIVE")?-1.f:1.f;
    g_Objects.clear();
    g_Objects.emplace_back("Drag test");
    g_Objects[0].Position[1] = .5f;
    g_Objects[0].Selected = true;
    g_Objects.emplace_back("Multi-selection");
    g_Objects[1].Position[1] = .5f;
    g_Objects[1].Position[2] = 2;
    g_Objects[1].Selected = true;
    g_SelectedObjectIndex = 0;
    g_SimulationRunning = false;
    g_ObjectHistory.Clear();
    g_ObjectHistory.Capture(g_Objects);
  }
  void Input(float distance, float pitch, float yaw, const float target[3]) {
    if (!Enabled || Done || g_Studio.SceneSize.x < 10)
      return;
    auto &io = ImGui::GetIO();
    io.ConfigInputTrickleEventQueue = false;
    if (Stage == 0) {
      StudioMatrix view, projection;
      StudioCameraMatrices(distance, pitch, yaw, target,
                           g_Studio.SceneSize.x / g_Studio.SceneSize.y,
                           g_Preferences.Fov, g_Preferences.FarClip, view,
                           projection);
      StudioMatrix vp = StudioMultiply(projection, view);
      float p[4] = {0, .5f, 0, 1}, clip[4] = {};p[Axis]+=Sign*(.08f+Attempts*.06f);
      for (int row = 0; row < 4; row++)
        for (int k = 0; k < 4; k++)
          clip[row] += vp.M[k * 4 + row] * p[k];
      X = g_Studio.SceneMin.x +
          (clip[0] / clip[3] + 1) * .5f * g_Studio.SceneSize.x;
      Y = g_Studio.SceneMin.y +
          (1 - clip[1] / clip[3]) * .5f * g_Studio.SceneSize.y;
      float c0[4]={},c1[4]={};for(int row=0;row<4;++row){c0[row]=vp.M[4+row]*.5f+vp.M[12+row];c1[row]=c0[row]+vp.M[Axis*4+row]*Sign;}
      DragX=(c1[0]/c1[3]-c0[0]/c0[3])*g_Studio.SceneSize.x;DragY=-(c1[1]/c1[3]-c0[1]/c0[3])*g_Studio.SceneSize.y;
      float n=std::sqrt(DragX*DragX+DragY*DragY);DragX=DragX/n*100;DragY=DragY/n*100;
      io.AddMousePosEvent(X, Y);
    } else {
      float f=Stage==2?float(Frames+1)*.25f:(Stage==3?1.f:0.f);io.AddMousePosEvent(X+DragX*f,Y+DragY*f);
      io.AddMouseButtonEvent(0, Stage == 1 || Stage == 2);
    }
  }
  void Check() {
    if (!Enabled || Done)
      return;
    Frames++;
    if (Stage == 0) {
      if (Frames > 5 &&
          ImGuizmo::GetHoveredHandleType() == ImGuizmo::MT_MOVE_X+Axis) {
        Stage = 1;
        Frames = 0;
      } else if (Frames > 5) {
        Attempts++;
        if (Attempts > 40) {
          Done = true;
          std::cerr << "FAIL: could not acquire the X gizmo handle\n";
        }
      }
    } else if (Stage == 1 && Frames >= 3) {
      Stage = 2;
      Frames = 0;
    } else if (Stage == 2 && Frames >= 4) {
      Stage = 3;
      Frames = 0;
    } else if (Stage == 3 && Frames >= 3) {
      Done = true;
      float delta[3]={g_Objects[0].Position[0],g_Objects[0].Position[1]-.5f,g_Objects[0].Position[2]};
      Passed=delta[Axis]*Sign>.1f;
      for(int k=0;k<3;++k){if(k!=Axis)Passed=Passed&&std::abs(delta[k])<.01f;float second=g_Objects[1].Position[k]-(k==1?.5f:(k==2?2.f:0.f));Passed=Passed&&std::abs(second-delta[k])<.01f;}
      Passed=Passed&&g_ObjectHistory.Undo(g_Objects)&&std::abs(g_Objects[0].Position[Axis]-(Axis==1?.5f:0.f))<.001f&&g_ObjectHistory.Redo(g_Objects)&&std::abs(g_Objects[0].Position[Axis]-(Axis==1?.5f:0.f)-delta[Axis])<.001f;
      if (!Passed)
        std::cerr << "Position " << g_Objects[0].Position[0] << ","
                  << g_Objects[0].Position[1] << "," << g_Objects[0].Position[2]
                  << " History " << g_ObjectHistory.Cursor << "/"
                  << g_ObjectHistory.States.size() << "\n";
      std::cout<<"Axis "<<"XYZ"[Axis]<<(Sign>0?"+":"-")<<": ";
      std::cout << (Passed ? "PASS: original editor mouse X-axis drag, "
                             "multi-selection and undo/redo\n"
                           : "FAIL: gizmo drag or undo/redo\n");
    }
  }
};
