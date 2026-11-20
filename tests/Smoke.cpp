// Rauchtest: Plugin instanziieren, 10 Takte intern spielen, Slots + MIDI-Export + Zustand pruefen, Editor rendern
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<g8::Grids8Processor> p (dynamic_cast<g8::Grids8Processor*> (createPluginFilter()));
    const double sr = 48000; const int bs = 512;
    p->setPlayConfigDetails (0, 2, sr, bs);
    p->prepareToPlay (sr, bs);
    p->internalPlay = true;
    juce::AudioBuffer<float> buf (2, bs); juce::MidiBuffer midi;
    int notes = 0; float peak = 0;
    const int blocks = (int) (sr * 60.0 / 122 * 4 * 10 / bs) + 4; // ~10 Takte
    for (int b = 0; b < blocks; ++b)
    {
        p->processBlock (buf, midi);
        for (const auto m : midi) if (m.getMessage().isNoteOn()) ++notes;
        peak = std::max (peak, buf.getMagnitude (0, bs));
        if (b % 8 == 0) p->pumpMessageThreadWork();
    }
    p->pumpMessageThreadWork();
    std::cout << "notes " << notes << " peak " << peak << " history " << p->history.size() << "\n";
    for (int i = 0; i < 8; ++i) std::cout << "slot" << i + 1 << ": " << p->slotMeta (i) << "\n";
    auto f = p->writeSlotMidi (3, false); auto f2 = p->writeSlotMidi (4, true);
    std::cout << "midi " << f.getFullPathName() << " " << f.getSize() << " / " << f2.getSize() << "\n";
    juce::MidiFile mf; juce::FileInputStream in (f); mf.readFrom (in);
    std::cout << "tracks " << mf.getNumTracks() << " lastTick " << mf.getLastTimestamp() << "\n";
    p->toggleHold (1);
    juce::MemoryBlock st; p->getStateInformation (st);
    std::unique_ptr<g8::Grids8Processor> q (dynamic_cast<g8::Grids8Processor*> (createPluginFilter()));
    q->setStateInformation (st.getData(), (int) st.getSize());
    std::cout << "restored: " << q->slotMeta (1) << " | state bytes " << st.getSize() << "\n";
    if (argc > 1)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        ed->setSize (g8::Content::W, g8::Content::H);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out (argv[1]); out.deleteFile();
        juce::FileOutputStream os (out); juce::PNGImageFormat png; png.writeImageToStream (img, os);
        std::cout << "snapshot " << out.getFullPathName() << "\n";
    }
    return 0;
}
