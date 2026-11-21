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
  // Polymeter + Export
  FundstueckProcessor ex; ex.setRateAndBufferSizeDetails(48000,512); ex.prepareToPlay(48000,512);
  { juce::AudioBuffer<float> b2(4,512); juce::MidiBuffer e2; ex.processBlock(b2,e2); }
  printf("loopSteps default=%d\n", ex.loopSteps());
  ex.apvts.getParameter("perc_poly")->setValueNotifyingHost(1.f);
  auto* stp = ex.apvts.getParameter("perc_steps"); stp->setValueNotifyingHost(stp->convertTo0to1(3.f));
  for(int s2=0;s2<16;s2++) ex.steps[3][s2] = (s2==0);
  printf("perc len=%d loopSteps=%d (erwartet 48)\n", ex.laneLength(3), ex.loopSteps());
  ex.setTrim(0, .1f, .5f); printf("trim0 %.2f..%.2f\n", ex.getTrimStart(0), ex.getTrimEnd(0));
  juce::String info; auto t0=juce::Time::getMillisecondCounterHiRes(); auto f = ex.exportLoop(info);
  printf("export %s exists=%d size=%lld ms=%.0f\n  %s\n", f.getFileName().toRawUTF8(), (int)f.existsAsFile(), (long long)f.getSize(), juce::Time::getMillisecondCounterHiRes()-t0, info.toRawUTF8());
  { juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f));
    if(rd){ printf("  wav: %.0f Hz, %d ch, %.3f s (erwartet %.3f s)\n", rd->sampleRate,(int)rd->numChannels, rd->lengthInSamples/rd->sampleRate, 48*60.0/90/4);
      juce::AudioBuffer<float> wb(2,(int)rd->lengthInSamples); rd->read(&wb,0,(int)rd->lengthInSamples,0,true,true); printf("  peak=%.3f rms=%.3f\n", wb.getMagnitude(0,wb.getNumSamples()), wb.getRMSLevel(0,0,wb.getNumSamples())); } }
  // Solo: nur Kick
  ex.apvts.getParameter("kick_solo")->setValueNotifyingHost(1.f); auto f2=ex.exportLoop(info); printf("solo export: %s\n", info.toRawUTF8());
  f.deleteFile(); f2.deleteFile();
  // Undo / Redo / Tonart
  FundstueckProcessor u; u.prepareToPlay(48000,512);
  auto mel=[&]{ juce::String t; for(int s2=0;s2<16;s2++){int v=u.steps[4][s2].load(); t<< (v<0? juce::String("."):FundstueckProcessor::noteName(v)) << " ";} return t; };
  printf("root=%d scale=%s\n mel A-moll: %s\n", u.root, fs::kScales[u.scale].name, mel().toRawUTF8());
  u.pushUndo(); int o=u.root; u.root=0; u.remapMelody(o,u.scale); printf(" nach C:     %s\n", mel().toRawUTF8());
  u.pushUndo(); int os=u.scale; u.scale=3; u.remapMelody(u.root,os); printf(" C-Dur:      %s\n", mel().toRawUTF8());
  u.pushUndo(); u.clearPattern(); printf(" geleert:    %s\n", mel().toRawUTF8());
  u.undo(); printf(" undo:       %s\n", mel().toRawUTF8());
  u.undo(); u.undo(); printf(" undo x3:    %s root=%d scale=%d\n", mel().toRawUTF8(), u.root, u.scale);
  u.redo(); printf(" redo:       %s root=%d\n", mel().toRawUTF8(), u.root);
  int before=(int)u.msgSet->slices.size(); u.pushUndo(); u.sens=0.95f; u.reanalyze(FundstueckProcessor::Mode::Resens);
  int after=(int)u.msgSet->slices.size(); u.undo(); printf(" sens: %d -> %d -> undo %d (sens=%.2f)\n", before, after, (int)u.msgSet->slices.size(), u.sens);
  return 0;
}
