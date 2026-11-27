#include "../../Source/DoppelgangerDSP.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>

// Diagnostic runner using the existing DSP unchanged. Auto-stop follows processor thresholds and an
// audio-time 33 ms timer model; real GUI scheduling/queues are checked separately by processor integration.
struct Result {
 dop::LearnedProfile p;
 double used=0; bool stopped=false;
};
Result analyse(std::vector<float> input, double fs, const std::string& mode, double gain, int chunk){
 dop::Learner learner; learner.prepare(fs);
 dop::Engine engine; engine.prepare(fs); dop::EngineParams ep;
 ep.deessOn=false;ep.leveler=0;engine.setParams(ep);
 dop::ConvergenceTracker tracker;float nextCheck=17.f;double nextTick=.033;
 std::vector<float> tap((size_t)chunk);std::vector<double> pw((size_t)chunk);
 Result r;
 for(size_t off=0;off<input.size();off+=(size_t)chunk){
  int n=(int)std::min((size_t)chunk,input.size()-off);
  for(int i=0;i<n;++i)input[off+(size_t)i]*=(float)gain;
  float* ptr[]={input.data()+off};engine.process(ptr,1,n,tap.data(),pw.data());
  learner.push(tap.data(),n,pw.data()); r.used=(off+(size_t)n)/fs;
  if(mode=="auto"&&r.used>=nextTick){
   nextTick=r.used+.033;
   float sec=learner.speechSeconds();bool enough=sec>=30.f;
   if(!enough&&sec>=17.f&&sec>=nextCheck){
    nextCheck=sec+1.f;enough=tracker.update(learner.finish(),learner.recentLoudnessShift(6.0));
   }
   if(enough){r.stopped=true;break;}
  }
 }
 r.p=learner.finish();return r;
}
void array(const dop::GridCurve& a){std::cout<<'[';for(size_t i=0;i<a.size();++i){if(i)std::cout<<',';std::cout<<a[i];}std::cout<<']';}
void design(const dop::LearnedProfile&p,int reg,double fs){
 float lc=dop::autopilot::suggestLowcut(p.pitchHz,80);
 float amt=dop::autopilot::suggestAmount(p.curveDb,reg,lc,20,12000);
 auto c=dop::correctionCurve(p.curveDb,reg,amt,lc,20,12000);
 auto b=dop::fitBank(c,fs);
 dop::GridCurve response{};
 for(size_t i=0;i<response.size();++i)for(const auto& coeff:b.coeffs)
  response[i]+=(float)dop::magnitudeDb(coeff,fs,dop::kRefGridHz[i]);
 std::cout<<"{\"register\":"<<reg<<",\"amount\":"<<amt*100<<",\"lowcut\":"<<lc<<",\"fit_error\":"<<b.maxErrorDb<<",\"correction\":";
 array(c);std::cout<<",\"response\":";array(response);std::cout<<'}';
}
#ifndef DOP_AUDIT_NO_MAIN
int main(int argc,char**argv){
 if(argc!=2)return 1;
 std::ifstream manifest(argv[1]);if(!manifest)return 2;
 std::string line;std::cout<<std::setprecision(9);
 while(std::getline(manifest,line)){
  if(line.empty()||line[0]=='#')continue;
  std::istringstream in(line);std::string id,path,mode;double fs,start,duration,gain;int chunk;
  if(!(in>>id>>fs>>std::quoted(path)>>start>>duration>>mode>>gain>>chunk)||chunk<1||chunk>32768||!std::isfinite(fs)||fs<8000||fs>768000||!std::isfinite(start)||start<0||!std::isfinite(duration)||duration<=0||duration>900||!std::isfinite(gain)||(mode!="auto"&&mode!="fixed"))return 3;
  std::ifstream f(path,std::ios::binary);if(!f)return 4;
  f.seekg(0,std::ios::end);size_t total=(size_t)f.tellg()/sizeof(float),first=(size_t)std::llround(start*fs);
  if(first>=total)return 5;
  size_t count=std::min(total-first,(size_t)std::llround(duration*fs));
  std::vector<float>x(count);f.seekg((std::streamoff)(first*sizeof(float)));f.read((char*)x.data(),(std::streamsize)(count*sizeof(float)));if(!f)return 6;
  auto r=analyse(std::move(x),fs,mode,gain,chunk);const auto&p=r.p;
  std::cout<<"{\"id\":\""<<id<<"\",\"fs\":"<<fs<<",\"used_seconds\":"<<r.used<<",\"stopped\":"<<(r.stopped?"true":"false")
   <<",\"valid\":"<<(p.valid&&p.speechSeconds>=4?"true":"false")<<",\"speech_seconds\":"<<p.speechSeconds<<",\"pitch\":"<<p.pitchHz
   <<",\"register\":"<<dop::registerForPitch(p.pitchHz)<<",\"lufs\":"<<p.loudnessLufs<<",\"profile\":";array(p.curveDb);
  std::cout<<",\"designs\":[";for(int reg=0;reg<3;++reg){if(reg)std::cout<<',';design(p,reg,fs);}std::cout<<"]}"<<std::endl;
 }
 return 0;
}

#endif
