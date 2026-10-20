#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Development host only. Decoding and device changes happen on the message
// thread with the audio source stopped. The plugin callback stays allocation-free.
class AuditionPanel final : public juce::AudioAppComponent {
public:
    AuditionPanel() {
        editor.reset(processor.createEditor()); addAndMakeVisible(*editor);
        for(auto* button:{&load,&play,&stop,&devices}) addAndMakeVisible(button);
        addAndMakeVisible(filename);
        filename.setText("Load a WAV, AIFF or FLAC to audition",juce::dontSendNotification);
        filename.setColour(juce::Label::textColourId,juce::Colour(0xffb9e7f4));
        load.onClick=[this] {
            chooser=std::make_unique<juce::FileChooser>("Load audio",juce::File{},"*.wav;*.aiff;*.flac");
            juce::Component::SafePointer<AuditionPanel> safe(this);
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
                [safe](const juce::FileChooser& fc) {if(safe && fc.getResult()!=juce::File{}) safe->loadFile(fc.getResult());});
        };
        play.onClick=[this] {restart.store(true);playing.store(true);};
        stop.onClick=[this] {playing.store(false);};
        devices.onClick=[this] {
            if(settingsWindow) {settingsWindow->toFront(true);return;}
            juce::DialogWindow::LaunchOptions options;
            options.dialogTitle="FROSTBITE audio output";options.dialogBackgroundColour=juce::Colour(0xff101a22);
            options.content.setOwned(new juce::AudioDeviceSelectorComponent(deviceManager,0,0,2,2,false,false,true,false));
            options.content->setSize(500,390);options.escapeKeyTriggersCloseButton=true;
            options.useNativeTitleBar=true;options.resizable=false;settingsWindow=options.launchAsync();
        };
        setSize(1000,758);setAudioChannels(0,2);
    }
    ~AuditionPanel() override {if(settingsWindow) delete settingsWindow.getComponent();shutdownAudio();editor.reset();}
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff101a22));}
    void resized() override {
        load.setBounds(14,12,100,32);play.setBounds(124,12,70,32);stop.setBounds(204,12,70,32);
        devices.setBounds(284,12,100,32);filename.setBounds(404,12,getWidth()-418,32);
        editor->setBounds(0,58,getWidth(),getHeight()-58);
    }
    void prepareToPlay(int block,double rate) override {
        deviceRate=rate;processor.setRateAndBufferSizeDetails(rate,block);processor.prepareToPlay(rate,block);
    }
    void releaseResources() override {processor.releaseResources();}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
        info.clearActiveBufferRegion();
        if(restart.exchange(false)) cursor=0;
        if(info.buffer->getNumChannels()<2) return;
        if(playing.load() && source.getNumSamples()>1) {
            for(int n=0;n<info.numSamples;++n) {
                const int a=int(cursor);
                if(a>=source.getNumSamples()-1) {playing.store(false);break;}
                const float f=float(cursor-a);
                for(int c=0;c<2;++c) {
                    const auto* samples=source.getReadPointer(c);
                    info.buffer->setSample(c,info.startSample+n,samples[a]+f*(samples[a+1]-samples[a]));
                }
                cursor+=sourceRate/deviceRate;
            }
        }
        float* channels[]{info.buffer->getWritePointer(0,info.startSample),info.buffer->getWritePointer(1,info.startSample)};
        juce::AudioBuffer<float> view(channels,2,info.numSamples);
        processor.processBlock(view,midi);
    }
    bool loadFile(const juce::File& f) {
        juce::AudioFormatManager formats;formats.registerBasicFormats();
        auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(f));
        if(!reader || reader->lengthInSamples>reader->sampleRate*600) {
            filename.setText("Unsupported file or longer than 10 minutes",juce::dontSendNotification);return false;
        }
        playing.store(false);shutdownAudio();
        source.setSize(2,int(reader->lengthInSamples));source.clear();
        reader->read(&source,0,source.getNumSamples(),0,true,true);
        if(reader->numChannels==1) source.copyFrom(1,0,source,0,0,source.getNumSamples());
        sourceRate=reader->sampleRate;cursor=0;
        filename.setText(f.getFileName(),juce::dontSendNotification);
        setAudioChannels(0,2);return true;
    }
private:
    FrostbiteAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::TextButton load{"LOAD AUDIO"},play{"PLAY"},stop{"STOP"},devices{"AUDIO"};
    juce::Label filename;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;
    juce::AudioBuffer<float> source;
    juce::MidiBuffer midi;
    std::atomic<bool> playing{false},restart{false};
    double cursor=0,deviceRate=48000,sourceRate=48000;
};
class AuditionWindow final : public juce::DocumentWindow {
public:
    AuditionWindow():DocumentWindow("FROSTBITE / File Audition",juce::Colour(0xff101a22),allButtons) {
        setUsingNativeTitleBar(true);setContentOwned(new AuditionPanel(),true);
        setResizable(false,false);centreWithSize(1000,758);setVisible(true);
    }
    void closeButtonPressed() override {setVisible(false);juce::MessageManager::getInstance()->stopDispatchLoop();}
};
