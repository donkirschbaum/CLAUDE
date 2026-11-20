#include "../Source/PluginProcessor.h"
#include <cstdio>
int main(){
  juce::ScopedJuceInitialiser_GUI init;
  FundstueckProcessor p; p.setRateAndBufferSizeDetails(48000,512); p.prepareToPlay(48000,512);
  juce::AudioBuffer<float> b(2,512); juce::MidiBuffer m;
  p.processBlock(b,m); // take slice set
  printf("slices=%d status=%s\n",(int)p.msgSet->slices.size(),p.statusText.toRawUTF8());
  for(int l=0;l<5;l++){printf("lane%d slice=%d steps:",l,p.laneSlice[l].load());for(int s=0;s<16;s++)printf("%d ",p.steps[l][s].load());printf("\n");}
  p.internalPlay=true; double sum=0; float pk=0; int steps=0,last=-1;
  for(int i=0;i<94*4;i++){ b.clear(); p.processBlock(b,m); for(int n=0;n<512;n++){float v=b.getSample(0,n);sum+=v*v;pk=std::max(pk,std::abs(v));}
    int st=p.currentStep.load(); if(st!=last){steps++;last=st;} }
  printf("4s internal: rms=%.4f peak=%.3f stepChanges=%d\n",std::sqrt(sum/(94*4*512)),pk,steps);
  juce::MemoryBlock mb; p.getStateInformation(mb); printf("state bytes=%d\n",(int)mb.getSize());
  FundstueckProcessor q; q.setStateInformation(mb.getData(),(int)mb.getSize());
  bool same=true; for(int l=0;l<5;l++)for(int s=0;s<16;s++) same&=p.steps[l][s].load()==q.steps[l][s].load();
  printf("restore same=%d slices=%d\n",same,(int)q.msgSet->slices.size());
  // MIDI note test
  FundstueckProcessor r; r.prepareToPlay(48000,512); r.processBlock(b,m);
  juce::MidiBuffer mm; mm.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),10); b.clear(); r.processBlock(b,mm);
  float pk2=0; for(int i=0;i<20;i++){ for(int n=0;n<512;n++) pk2=std::max(pk2,std::abs(b.getSample(0,n))); b.clear(); juce::MidiBuffer e; r.processBlock(b,e);} 
  printf("midi note peak=%.3f\n",pk2);
  // Aufnahme ueber Eingang: 2 s Klopf-Impulse
  FundstueckProcessor rc; rc.setRateAndBufferSizeDetails(48000,512); rc.prepareToPlay(48000,512);
  printf("buses in=%d totalIn=%d out=%d\n", rc.getBusCount(true), rc.getTotalNumInputChannels(), rc.getTotalNumOutputChannels());
  juce::AudioBuffer<float> io(std::max(rc.getTotalNumInputChannels(),2),512); juce::MidiBuffer em;
  rc.startRecording(); int t=0;
  for(int blk=0;blk<190;blk++){ io.clear(); for(int n=0;n<512;n++,t++){ int ph=t%12000; float v= ph<2400? std::sin(t*0.05f)*std::exp(-ph/600.f):0.f; io.setSample(2,n,v); io.setSample(3,n,v);} rc.processBlock(io,em);} // sidechain channels 2/3
  printf("rec pos=%d level=%.3f\n", rc.recPos.load(), rc.inLevel.load());
  rc.stopRecording(); printf("after rec: %s slices=%d\n", rc.statusText.toRawUTF8(), (int)rc.msgSet->slices.size());
  // Sync: ohne Host-Playhead -> sync an => kein Lauf
  rc.apvts.getParameter("sync")->setValueNotifyingHost(1.f); rc.internalPlay=true; int changes=0,lastS=-5;
  for(int i=0;i<100;i++){ io.clear(); rc.processBlock(io,em); if(rc.currentStep.load()!=lastS){changes++;lastS=rc.currentStep.load();} }
  printf("sync on, no host: syncOn=%d stepChanges=%d (erwartet 0-1)\n", (int)rc.syncOn.load(), changes);
  return 0;
}
