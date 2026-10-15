#include "StudioSegmentEngine.h"
#include <JuceHeader.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

static void require(bool c,const char* label) {if(!c)throw std::runtime_error(label);}
int main(){
 try{
  for(double sr:{44100.0,48000.0,96000.0}){
   StudioSegmentEngine engine;
   engine.prepare(sr,256,2);
   require(engine.getLatencySamples()>int(sr*.34)&&engine.getLatencySamples()<int(sr*.36),
           "Expected fixed FL host/PDC latency");
   // A zero correction is already represented by the current read head:
   // reapplying it must not schedule destructive needless fades.
   require(engine.addAnchor((int64_t)(sr*.5),0,.92f),"Accept coincident hit");
   require(engine.getArtifactRisk()==0.0f,"No artifact risk on unchanged timing");
   bool anchorAccepted=false;
   double total=0,peak=0;
   const int blocks=(int)(sr*1.5/256);
   for(int b=0;b<blocks;++b){
    juce::AudioBuffer<float> audio(2,256);
    for(int i=0;i<256;++i){
     const double t=(b*256+i)/sr;
     const float x=(float)(.2*std::sin(2.0*juce::MathConstants<double>::pi*220.0*t));
     audio.setSample(0,i,x);
     audio.setSample(1,i,-x); // antiphase must not trick the quiet-spot finder
    }
    engine.pushInput(audio);
    if(!anchorAccepted && (b+1)*256>sr*.75){
     anchorAccepted=engine.addAnchor((int64_t)(sr*.75),-(int)(sr*.035),.95f);
    }
    engine.pullOutput(audio);
    for(int i=0;i<256;++i){
     const auto l=audio.getSample(0,i),r=audio.getSample(1,i);
     require(std::isfinite(l)&&std::isfinite(r),"Finite segment output");
     peak=std::max({peak,double(std::abs(l)),double(std::abs(r))});
     total+=double(l)*l+double(r)*r;
    }
   }
   require(anchorAccepted,"Accepted realistic negative timing correction");
   require(total>0.1 && peak<0.5,"No unbounded splice peaks or silence");
   std::cout<<"PASS StudioSegmentEngine "<<sr<<" Hz, peak "<<peak<<"\n";
  }
  return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
