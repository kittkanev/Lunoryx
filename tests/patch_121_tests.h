#pragma once
static void CheckPatch121(){
  float target[3]={3,2,-4},pitch=25,yaw=-35,distance=10;
  auto eye=StudioCameraEye(distance,pitch,yaw,target);StudioFreeLook(distance,pitch,yaw,target,73,-12);auto same=StudioCameraEye(distance,pitch,yaw,target);
  assert(std::abs(same.x-eye.x)<.0001f&&std::abs(same.y-eye.y)<.0001f&&std::abs(same.z-eye.z)<.0001f);
  StudioFly(target,pitch,yaw,{0,0,-1},5,.1f);auto moved=StudioCameraEye(distance,pitch,yaw,target);assert(std::abs(std::sqrt(Dot(Subtract(moved,same),Subtract(moved,same)))-.5)<.0001);
  StudioFreeLook(distance,pitch,yaw,target,100000,1000);assert(pitch==89&&std::abs(yaw)<=180);
  auto start=StudioCameraEye(distance,pitch,yaw,target);StudioFly(target,pitch,yaw,{0,1,0},5,.1f);auto up=StudioCameraEye(distance,pitch,yaw,target);assert(std::abs(up.x-start.x)<.0001f&&std::abs(up.z-start.z)<.0001f&&std::abs(up.y-start.y-.5f)<.0001f);
  auto before=StudioCameraEye(distance,pitch,yaw,target);StudioFly(target,pitch,yaw,{1,1,-1},5,.1f);auto after=StudioCameraEye(distance,pitch,yaw,target);assert(std::abs(std::sqrt(Dot(Subtract(after,before),Subtract(after,before)))-.5)<.0001);
  assert(StudioSelectionContains(10,20,30,40,0,0,false,1));assert(!StudioSelectionContains(40,20,0,0,30,30,false,1));assert(StudioSelectionContains(4,3,0,0,0,0,true,5));assert(!StudioSelectionContains(5,3,0,0,0,0,true,5));
  assert(StudioSegmentDistance2(5,2,0,0,10,0)==4);
  auto matrix=StudioIdentity();auto p=StudioProjectPoint(matrix,{0,0,0},0,0,100,100);assert(p.Valid&&p.X==50&&p.Y==50);p=StudioProjectPoint(matrix,{0,0,2},0,0,100,100);assert(!p.Valid);
  g_PhysicsRunning=false;g_ModelBusy=false;g_AnimationPreview=false;g_Modeling.Edit=false;g_Objects.clear();g_Objects.emplace_back("Selected");g_SelectedObjectIndex=0;g_Objects[0].Selected=true;
  ToggleModelEdit();assert(g_ModelBusy);g_ModelJob.wait();PollModelJob();assert(g_Modeling.Edit&&!g_ModelBusy);
  for(int mode=0;mode<3;++mode){g_Modeling.Mode=mode;SelectAllViewport(false);assert(g_Modeling.Selected.size()==size_t(mode==0?8:(mode==1?12:6)));SelectAllViewport(true);assert(g_Modeling.Selected.empty());}
  g_SelectedObjectIndex=-1;ToggleModelEdit();assert(!g_Modeling.Edit);g_SelectedObjectIndex=0;g_Objects.emplace_back("Second");SelectAllViewport(false);assert(g_Objects[0].Selected&&g_Objects[1].Selected);SelectAllViewport(true);assert(!g_Objects[0].Selected&&!g_Objects[1].Selected&&g_SelectedObjectIndex==-1);
  std::cout<<"PASS: free camera eye invariance, six-axis normalized flight, box/circle geometry and Object/Edit vertex-edge-face modes\n";
}
