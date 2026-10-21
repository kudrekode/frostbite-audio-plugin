#pragma once
#include "PluginProcessor.h"

class FrostLookAndFeel final : public juce::LookAndFeel_V4,private juce::Timer {
public:
    FrostLookAndFeel();
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override;
    void drawComboBox(juce::Graphics&,int,int,bool,int,int,int,int,juce::ComboBox&) override;
    void drawPopupMenuItem(juce::Graphics&,const juce::Rectangle<int>&,bool,bool,bool,bool,bool,
                           const juce::String&,const juce::String&,const juce::Drawable*,const juce::Colour*) override;
    juce::Font getPopupMenuFont() override;
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(juce::ComboBox&,juce::Label&) override;
    void preparePopupMenuWindow(juce::Component&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
private:
    void timerCallback() override;
    std::vector<juce::Component::SafePointer<juce::Component>> popupWindows;
};

class FrostKnob final : public juce::Component {
public:
    FrostKnob(FrostbiteAudioProcessor&,size_t);
    void resized() override;
    void paint(juce::Graphics&) override;
    juce::Slider slider;
private:
    void focusOfChildComponentChanged(FocusChangeType) override;
    size_t index;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class FrostbiteAudioProcessorEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit FrostbiteAudioProcessorEditor(FrostbiteAudioProcessor&);
    ~FrostbiteAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
private:
    FrostbiteAudioProcessor& processor;
    FrostLookAndFeel look;
    juce::Component surface;
    std::array<std::unique_ptr<FrostKnob>,frostbite::Count> knobs;
    juce::TextButton capture{"CAPTURE"},bypass{"BYPASS"},advanced{"DETAILS"},previous{"<"},next{">"},save{"SAVE"},load{"LOAD"},init{"INIT"};
    juce::TextButton sync{"TEMPO SYNC"};
    std::array<juce::TextButton,4> tabs;
    juce::ComboBox presets,division,character,voicing,freezeMode,freezeRouting;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> captureAttachment,bypassAttachment,syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionAttachment,characterAttachment,voicingAttachment,freezeModeAttachment,freezeRoutingAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips{this,600};
    bool details=false;
    int page=0;
    unsigned revision=0;
    float input=0,output=0;
    std::array<float,128> wave{};
    void timerCallback() override;
    void layoutSurface();
    void choosePreset(bool saving);
    void report(juce::Result);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrostbiteAudioProcessorEditor)
};
