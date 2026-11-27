#define DOP_AUDIT_NO_MAIN
#include "audit_learning.cpp"
#include "../../Source/PluginProcessor.h"
struct DoppelgangerProcessorTestAccess {
 static void tick(DoppelgangerProcessor&p){p.timerCallback();}
 static void apply(DoppelgangerProcessor&p){p.applyAutopilotSettings();const juce::ScopedLock sl(p.stateLock);p.redesign();}
};
int main(int argc,char**argv){
 if(argc!=2)return 1;juce::ScopedJuceInitialiser_GUI init;
 std::ifstream manifest(argv[1]);std::string line;std::cout<<std::setprecision(9);
 while(std::getline(manifest,line)){
  if(line.empty()||line[0]=='#')continue;
  std::istringstream in(line);std::string id,path,mode;double fs,start,duration,gain;int chunk;
  if(!(in>>id>>fs>>std::quoted(path)>>start>>duration>>mode>>gain>>chunk))return 2;
  std::ifstream f(path,std::ios::binary);if(!f)return 3;f.seekg(0,std::ios::end);
  size_t total=(size_t)f.tellg()/sizeof(float),first=(size_t)std::llround(start*fs),count=std::min(total-first,(size_t)std::llround(duration*fs));
  std::vector<float>x(count);f.seekg((std::streamoff)(first*sizeof(float)));f.read((char*)x.data(),(std::streamsize)(count*sizeof(float)));
  auto p=std::make_unique<DoppelgangerProcessor>();auto layout=p->getBusesLayout();layout.inputBuses.set(0,juce::AudioChannelSet::mono());layout.outputBuses.set(0,juce::AudioChannelSet::mono());if(!p->setBusesLayout(layout))return 4;
  p->prepareToPlay(fs,chunk);p->startLearning();juce::MidiBuffer midi;juce::AudioBuffer<float>b(1,chunk);
  double nextTick=.033,used=0;bool stopped=false;
  for(size_t off=0;off<x.size();off+=(size_t)chunk){int n=(int)std::min((size_t)chunk,x.size()-off);b.setSize(1,n,false,false,true);
   for(int i=0;i<n;++i)b.setSample(0,i,(float)(x[off+(size_t)i]*gain));p->processBlock(b,midi);used=(off+(size_t)n)/fs;
   if(used>=nextTick){nextTick=used+.033;DoppelgangerProcessorTestAccess::tick(*p);if(!p->isLearning()){stopped=true;break;}}
  }
  if(p->isLearning())p->finishLearning();DoppelgangerProcessorTestAccess::apply(*p);auto info=p->getDisplayInfo();
  std::cout<<"{\"id\":\""<<id<<"\",\"used_seconds\":"<<used<<",\"stopped\":"<<(stopped?"true":"false")<<",\"speech_seconds\":"<<info.profile.speechSeconds<<",\"pitch\":"<<info.profile.pitchHz<<",\"register\":"<<info.effectiveRegister<<",\"profile\":";array(info.profile.curveDb);std::cout<<",\"correction\":";array(info.correction);std::cout<<"}"<<std::endl;
 }
}
