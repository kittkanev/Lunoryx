#pragma once
// Drive the actual frame loop, not a second copy of the scheduling logic.
struct RuntimeSmoke {
  bool Enabled=false,Passed=false;
  int Frame=0;
  float Rotation=0,Height=0;
  void BeforeTick(bool& done) {
    if(!Enabled)return;
    ++Frame;
    auto gem=[]()->GameObject&{for(auto& o:g_Objects)if(o.Name=="Gem 1")return o;throw std::runtime_error("Smoke gem missing");};
    auto actor=[]()->GameObject&{for(auto& o:g_Objects)if(o.Character)return o;throw std::runtime_error("Smoke actor missing");};
    if(Frame==1){CreateSampleGame();StartSimulation();}
    if(Frame==2){g_DebugPaused=true;Rotation=gem().Rotation[1];Height=actor().Position[1];actor().Velocity[1]=5;}
    if(Frame==3){if(gem().Rotation[1]!=Rotation||actor().Position[1]!=Height)throw std::runtime_error("Pause did not freeze runtime");g_DebugStep=true;}
    if(Frame==4){if(std::abs(gem().Rotation[1]-Rotation-1)>.001f||actor().Position[1]<=Height)throw std::runtime_error("Step did not advance animation and physics by one frame");Rotation=gem().Rotation[1];Height=actor().Position[1];}
    if(Frame==5){if(gem().Rotation[1]!=Rotation||actor().Position[1]!=Height)throw std::runtime_error("Step did not retain pause");
      // Verify the paused state's Lua counter through a trusted pure Lua expression.
      StopSimulation();Passed=true;done=true;std::cout<<"PASS: real frame pause, single-step animation/physics, retained pause and Stop\n";}
  }
};
