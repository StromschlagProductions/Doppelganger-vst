#include "../../tests/SpeechSynth.h"
#include <fstream>
#include <iostream>
#include <string>
#include <iomanip>
int main(int argc,char**argv){if(argc!=2)return 1;std::string root=argv[1];std::ofstream jobs(root+"/controls.tsv");
 for(double f0:{120.,157.,164.,210.,222.,230.,260.})for(double rate:{.5,3.}){
  SpeechOptions o;o.f0=f0;o.sibPerSecond=rate;o.sibDb=-12;o.seed=3;
  auto x=synthSpeech(48000,60,o);std::string id="synth_f"+std::to_string((int)f0)+"_s"+std::to_string((int)(rate*10));
  std::string path=root+"/"+id+".f32";std::ofstream f(path,std::ios::binary);f.write((char*)x.data(),x.size()*sizeof(float));
  jobs<<id<<"\t48000\t"<<std::quoted(path)<<"\t0\t60\tauto\t1\t512\n";
 }
}
