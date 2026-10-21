#include "PluginEditor.h"

namespace {
const juce::Colour ink{0xff090f14},panel{0xff101a22},ice{0xffb9e7f4},steel{0xff6e8998},white{0xffe2edf0},amber{0xffd5a279};
juce::Font font(float height,bool bold=false) {
    return juce::Font(juce::FontOptions("Bahnschrift",height,bold?juce::Font::bold:juce::Font::plain));
}
void flushNativeRepaints(juce::Component& component) {
   #if JUCE_WINDOWS
    // JUCE 9's software peer queues dirty rectangles until its DXGI vblank callback.
    // Some host/display combinations stop delivering that callback. Flush existing
    // invalidations on the GUI thread; this does not request a full-window repaint.
    if(juce::MessageManager::getInstance()->isThisTheMessageThread())
        if(auto* peer=component.getPeer()) peer->performAnyPendingRepaintsNow();
   #else
    juce::ignoreUnused(component);
   #endif
}
void text(juce::Graphics& g,juce::String s,juce::Rectangle<float> r,float size,juce::Colour colour,
          juce::Justification justify=juce::Justification::centredLeft) {
    g.setColour(colour); g.setFont(font(size)); g.drawText(s,r,justify);
}
}
FrostLookAndFeel::FrostLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId,white);
    setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId,steel.withAlpha(.3f));
    setColour(juce::ComboBox::textColourId,white);
    setColour(juce::PopupMenu::backgroundColourId,panel);
    setColour(juce::PopupMenu::textColourId,white);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff26424e));
    setColour(juce::PopupMenu::highlightedTextColourId,ice);
    setColour(juce::TooltipWindow::backgroundColourId,panel);
    setColour(juce::TooltipWindow::textColourId,white);
    setColour(juce::TooltipWindow::outlineColourId,steel);
}
void FrostLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float start,float end,juce::Slider& s) {
    const auto area=juce::Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(9);
    const auto centre=area.getCentre(); const float radius=std::min(area.getWidth(),area.getHeight())*.5f;
    const auto accent=s.findColour(juce::Slider::rotarySliderFillColourId);
    const float position=juce::jlimit(0.0f,1.0f,pos);
    const float angle=start+position*(end-start);
    juce::Path track,active;
    track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
    if(position>0) active.addCentredArc(centre.x,centre.y,radius,radius,0,start,angle,true);
    g.setColour(juce::Colour(0xff263641)); g.strokePath(track,juce::PathStrokeType(2));
    // A value-driven halo stays inside this slider's existing repaint region. No animation.
    const float intensity=position*position;
    const auto glow=s.getComponentID()=="damage"?juce::Colour(0xffe17b54):accent;
    g.setColour(glow.withAlpha(.06f+.16f*intensity));
    g.strokePath(active,juce::PathStrokeType(6+4*intensity));
    g.setColour(accent); g.strokePath(active,juce::PathStrokeType(3));
    for(int i=0;i<=20;++i) {
        const float a=start+(end-start)*float(i)/20;
        const auto p1=centre+juce::Point<float>(std::sin(a),-std::cos(a))*(radius+5);
        const auto p2=centre+juce::Point<float>(std::sin(a),-std::cos(a))*(radius+7);
        g.setColour(steel.withAlpha(i%5==0?.55f:.18f)); g.drawLine({p1,p2},1);
    }
    const auto dial=juce::Rectangle<float>(radius*1.57f,radius*1.57f).withCentre(centre);
    juce::DropShadow(juce::Colours::black.withAlpha(.7f),5,{0,3}).drawForRectangle(g,dial.toNearestInt());
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff31434f),dial.getTopLeft(),
        juce::Colour(0xff111b22),dial.getBottomRight(),false));
    g.fillEllipse(dial);
    g.setColour(juce::Colour(0xff415460)); g.drawEllipse(dial.reduced(.5f),.7f);
    g.setColour(juce::Colour(0xff15232d)); g.drawEllipse(dial.reduced(4),1);
    const auto direction=juce::Point<float>(std::sin(angle),-std::cos(angle));
    g.setColour(accent);
    g.drawLine({centre+direction*radius*.40f,centre+direction*radius*.70f},3);
    g.fillEllipse(juce::Rectangle<float>(4,4).withCentre(centre+direction*radius*.70f));
    if(s.hasKeyboardFocus(false)) { g.setColour(ice.withAlpha(.8f)); g.drawEllipse(dial.expanded(4),1); }
}
void FrostLookAndFeel::drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down) {
    auto r=b.getLocalBounds().toFloat().reduced(.5f);
    const bool active=b.getToggleState();
    g.setColour(active?ice.withAlpha(.16f):juce::Colour(over?0xff233541:0xff141f28)); g.fillRoundedRectangle(r,4);
    g.setColour(active?ice.withAlpha(.7f):steel.withAlpha(down?.7f:.25f)); g.drawRoundedRectangle(r,4,1);
}
void FrostLookAndFeel::drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool) {
    text(g,b.getButtonText(),b.getLocalBounds().toFloat(),11,b.getToggleState()?ice:steel,juce::Justification::centred);
}
void FrostLookAndFeel::drawComboBox(juce::Graphics& g,int w,int h,bool down,int,int,int,int,juce::ComboBox& box) {
    const bool focused=box.hasKeyboardFocus(true);
    g.setColour(down?juce::Colour(0xff26424e):panel); g.fillRoundedRectangle(0,0,float(w),float(h),4);
    g.setColour(focused?ice:steel.withAlpha(.3f)); g.drawRoundedRectangle(.5f,.5f,float(w-1),float(h-1),4,focused?1.5f:1.0f);
    juce::Path p; p.startNewSubPath(float(w-20),float(h/2-2)); p.lineTo(float(w-15),float(h/2+3)); p.lineTo(float(w-10),float(h/2-2));
    g.setColour(steel); g.strokePath(p,juce::PathStrokeType(1));
}
void FrostLookAndFeel::drawPopupMenuItem(juce::Graphics& g,const juce::Rectangle<int>& area,
    bool separator,bool enabled,bool highlighted,bool ticked,bool submenu,const juce::String& label,
    const juce::String& shortcut,const juce::Drawable* icon,const juce::Colour* colour) {
    if(!separator && enabled && ticked && !highlighted) {
        g.setColour(ice.withAlpha(.07f)); g.fillRoundedRectangle(area.toFloat().reduced(1),3);
    }
    // Keep JUCE's tick, icon, submenu and accessibility conventions; explicitly style every row.
    juce::LookAndFeel_V4::drawPopupMenuItem(g,area,separator,enabled,highlighted,ticked,submenu,label,shortcut,icon,colour);
    if(!separator && enabled && highlighted) {
        g.setColour(ice); g.fillRect(area.getX()+1,area.getY()+4,2,juce::jmax(0,area.getHeight()-8));
    }
}
juce::Font FrostLookAndFeel::getPopupMenuFont() { return font(14); }
juce::PopupMenu::Options FrostLookAndFeel::getOptionsForComboBoxPopupMenu(juce::ComboBox& box,juce::Label& label) {
    return juce::LookAndFeel_V4::getOptionsForComboBoxPopupMenu(box,label).withStandardItemHeight(32);
}
void FrostLookAndFeel::preparePopupMenuWindow(juce::Component& window) {
   #if JUCE_WINDOWS
    // Popups have separate native peers, so the editor's paint workaround does not reach them.
    if(auto* peer=window.getPeer()) {
        const auto software=peer->getAvailableRenderingEngines().indexOf("Software Renderer");
        if(software>=0) peer->setCurrentRenderingEngine(software);
    }
    popupWindows.emplace_back(&window);
    startTimerHz(60);
   #else
    juce::ignoreUnused(window);
   #endif
}
void FrostLookAndFeel::timerCallback() {
    std::erase_if(popupWindows,[](const auto& window) { return window==nullptr; });
    // Only service JUCE's existing dirty menu rows. JUCE retains mouse/keyboard tracking.
    for(auto& window:popupWindows) flushNativeRepaints(*window);
    if(popupWindows.empty()) stopTimer();
}
juce::Font FrostLookAndFeel::getComboBoxFont(juce::ComboBox&) { return font(13); }
juce::Font FrostLookAndFeel::getLabelFont(juce::Label&) { return font(12); }

FrostKnob::FrostKnob(FrostbiteAudioProcessor& p,size_t i):index(i) {
    const auto& info=frostbite::parameterInfo[i];
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi*1.23f,juce::MathConstants<float>::pi*2.77f,true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,100,21);
    slider.setColour(juce::Slider::textBoxTextColourId,white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::rotarySliderFillColourId,i==frostbite::Damage?amber:ice);
    slider.setDoubleClickReturnValue(true,frostbite::defaults()[i]);
    slider.setVelocityModeParameters(.35,1,.04,true,juce::ModifierKeys::shiftModifier);
    slider.setMouseDragSensitivity(180);
    slider.setWantsKeyboardFocus(true);
    slider.setTitle(info.name);
    slider.setComponentID(info.id);
    slider.setTooltip(juce::String(info.name)+". Double-click to reset. Hold Shift for fine adjustment. Arrow keys adjust.");
    addAndMakeVisible(slider);
    // JUCE owns value/readout repainting. This callback only invalidates our engine-state title.
    slider.onValueChange=[this] { repaint(0,0,getWidth(),27); flushNativeRepaints(*this); };
    attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,info.id,slider);
}
void FrostKnob::resized() { slider.setBounds(getLocalBounds().withTrimmedTop(23).reduced(4,0)); }
void FrostKnob::focusOfChildComponentChanged(FocusChangeType) { repaint(0,0,getWidth(),27); }
void FrostKnob::paint(juce::Graphics& g) {
    const auto accent=index==frostbite::Damage?amber:ice;
    const bool selected=slider.hasKeyboardFocus(true);
    text(g,juce::String(frostbite::parameterInfo[index].name).toUpperCase(),{0,0,float(getWidth()),21},11,
        selected?accent:index==frostbite::Damage?amber:steel,juce::Justification::centred);
    if(index<=frostbite::Motion) {
        g.setColour(slider.getValue()>slider.getMinimum()?accent:steel.withAlpha(.25f));
        g.fillEllipse(float(getWidth())*.5f-2,21,4,4);
    }
}

FrostbiteAudioProcessorEditor::FrostbiteAudioProcessorEditor(FrostbiteAudioProcessor& p):AudioProcessorEditor(p),processor(p) {
    // The editor fills its entire background, including for partial native-window repaints.
    setOpaque(true);
    setLookAndFeel(&look); addAndMakeVisible(surface);
    for(size_t i=0;i<frostbite::Count;++i) {
        if(i==frostbite::Capture || i==frostbite::Bypass || i==frostbite::EchoSync || i==frostbite::EchoDivision || i==frostbite::Character) continue;
        knobs[i]=std::make_unique<FrostKnob>(p,i); surface.addAndMakeVisible(*knobs[i]);
    }
    for(auto* b:{&capture,&bypass,&advanced,&previous,&next,&save,&load,&init,&sync}) surface.addAndMakeVisible(b);
    capture.setClickingTogglesState(true); bypass.setClickingTogglesState(true); sync.setClickingTogglesState(true);
    capture.setTooltip("Capture the preceding 1.5 seconds once. Release fades the held grains. Play audio before capturing.");
    bypass.setTooltip("Crossfade to unprocessed input. Effects continue internally, preserving tails when re-enabled.");
    advanced.setTooltip("Show the sound engine controls.");
    capture.onStateChange=[this] {
        const juce::String label=capture.getToggleState()?"RELEASE":"CAPTURE";
        if(capture.getButtonText()!=label) {
            capture.setButtonText(label); repaint(getLocalArea(&surface,juce::Rectangle<int>(28,98,944,254)));
            flushNativeRepaints(*this);
        }
    };
    bypass.onStateChange=[this] { repaint(getLocalArea(&surface,juce::Rectangle<int>(66,58,294,16))); flushNativeRepaints(*this); };
    sync.onStateChange=[this] { flushNativeRepaints(*this); };
    captureAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"capture",capture);
    bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"bypass",bypass);
    syncAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"echo_sync",sync);
    for(int i=0;i<8;++i) presets.addItem(p.getProgramName(i),i+1);
    presets.setTitle("Factory presets"); presets.setComponentID("factory_presets");
    presets.setWantsKeyboardFocus(true);
    presets.setText(p.presetName(),juce::dontSendNotification);
    presets.onChange=[this] { if(presets.getSelectedId()>0) processor.setCurrentProgram(presets.getSelectedId()-1); };
    presets.setTooltip("Factory presets store settings. Play and capture your own source audio.");
    surface.addAndMakeVisible(presets);
    previous.onClick=[this]{processor.setCurrentProgram((processor.getCurrentProgram()+7)%8);};
    next.onClick=[this]{processor.setCurrentProgram((processor.getCurrentProgram()+1)%8);};
    save.onClick=[this]{choosePreset(true);}; load.onClick=[this]{choosePreset(false);};
    init.onClick=[this]{processor.restoreDefaults();};
    advanced.onClick=[this]{details=!details; advanced.setToggleState(details,juce::dontSendNotification); layoutSurface(); repaint(); flushNativeRepaints(*this);};
    const juce::StringArray labels{"FROZEN","ECHO","ABYSS","CHARACTER"};
    for(int i=0;i<4;++i) {
        tabs[size_t(i)].setButtonText(labels[i]); surface.addAndMakeVisible(tabs[size_t(i)]);
        tabs[size_t(i)].onClick=[this,i]{page=i; layoutSurface(); repaint(); flushNativeRepaints(*this);};
    }
    division.addItemList({"1/16","1/8","1/8 T","1/4","1/8 D","1/4 T","1/2","1/4 D","1/1"},1);
    character.addItemList({"WARM","BROKEN"},1);
    surface.addAndMakeVisible(division); surface.addAndMakeVisible(character);
    divisionAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"echo_division",division);
    characterAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"character",character);
    division.onChange=[this] { flushNativeRepaints(*this); };
    character.onChange=[this] { flushNativeRepaints(*this); };
    setResizable(true,true); setResizeLimits(800,560,1600,1120);
    getConstrainer()->setFixedAspectRatio(1000.0/700.0);
    setSize(1000,700); startTimerHz(24);
}
FrostbiteAudioProcessorEditor::~FrostbiteAudioProcessorEditor() {
    stopTimer(); presets.hidePopup(); division.hidePopup(); character.hidePopup(); setLookAndFeel(nullptr);
}
void FrostbiteAudioProcessorEditor::parentHierarchyChanged() {
   #if JUCE_WINDOWS
    // Studio One can present a stale Direct2D surface while slider/host values keep
    // changing. Select JUCE's GDI software backend when the host peer is attached.
    if(auto* peer=getPeer()) {
        const auto software=peer->getAvailableRenderingEngines().indexOf("Software Renderer");
        if(software>=0 && peer->getCurrentRenderingEngine()!=software) peer->setCurrentRenderingEngine(software);
    }
   #endif
}
void FrostbiteAudioProcessorEditor::resized() {
    const float scale=std::min(float(getWidth())/1000,float(getHeight())/700);
    surface.setTransform(juce::AffineTransform::scale(scale)); surface.setBounds(0,0,1000,700); layoutSurface();
}
void FrostbiteAudioProcessorEditor::layoutSurface() {
    presets.setBounds(427,27,232,34); previous.setBounds(389,27,30,34); next.setBounds(667,27,30,34);
    save.setBounds(711,27,51,34); load.setBounds(770,27,51,34); init.setBounds(829,27,51,34); bypass.setBounds(893,27,79,34);
    capture.setBounds(795,108,154,38); advanced.setBounds(845,317,104,27);
    for(size_t i=0;i<frostbite::Count;++i) if(knobs[i]) knobs[i]->setVisible(i<=frostbite::Output);
    for(int i=0;i<6;++i) knobs[size_t(i)]->setBounds(29+i*158,378,152,148);
    knobs[frostbite::Mix]->setBounds(716,550,116,123); knobs[frostbite::Output]->setBounds(850,550,116,123);
    for(int i=0;i<4;++i) { tabs[size_t(i)].setVisible(details); tabs[size_t(i)].setBounds(48+i*124,109,116,29); tabs[size_t(i)].setToggleState(page==i,juce::dontSendNotification); }
    division.setVisible(details && page==1); sync.setVisible(details && page==1); character.setVisible(details && page==3);
    division.setBounds(804,179,137,31); sync.setBounds(804,221,137,29); character.setBounds(804,197,137,33);
    if(!details) return;
    using namespace frostbite;
    std::vector<size_t> ids;
    if(page==0) ids={Texture,Drift,FreezeLevel,Input};
    if(page==1) ids={EchoMs,Feedback,EchoTone,Width,EchoLevel};
    if(page==2) ids={Size,Decay,Damping,Diffusion,ReverbMod,Predelay,ReverbLevel};
    if(page==3) ids={Drive,DamageTone,DamageBlend,MotionRate,MotionDepth};
    int w=page==2?126:142;
    for(size_t i=0;i<ids.size();++i) { knobs[ids[i]]->setVisible(true); knobs[ids[i]]->setBounds(45+int(i)*w,157,w-8,140); }
}
void FrostbiteAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(ink);
    g.addTransform(juce::AffineTransform::scale(float(getWidth())/1000,float(getHeight())/700));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1a2a34),500,0,ink,500,700,false));
    g.fillRect(0,0,1000,700);
    for(int y=0;y<700;y+=4) { g.setColour(juce::Colours::black.withAlpha(.035f)); g.drawHorizontalLine(y,0,1000); }
    // Original crystalline mark, drawn entirely with vectors.
    juce::Path mark; mark.startNewSubPath(28,29); mark.lineTo(53,29); mark.lineTo(42,43); mark.lineTo(51,43); mark.lineTo(30,64); mark.lineTo(36,46); mark.lineTo(28,46); mark.closeSubPath();
    g.setColour(ice); g.fillPath(mark);
    g.setFont(font(27,true)); g.setColour(white); g.drawText("F R O S T B I T E",66,24,294,34,juce::Justification::centredLeft);
    text(g,bypass.getToggleState()?"BYPASSED / DRY SIGNAL":"ATMOSPHERIC AUDIO ENGINE",{68,58,260,16},9,
         bypass.getToggleState()?amber:steel);
    g.setColour(steel.withAlpha(.18f)); g.drawHorizontalLine(86,28,972);
    auto display=juce::Rectangle<float>(28,98,944,254);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff142733),500,98,juce::Colour(0xff0a151e),500,352,false));
    g.fillRoundedRectangle(display,7);
    g.setColour(steel.withAlpha(.2f)); g.drawRoundedRectangle(display,7,1);
    if(!details) {
        text(g,"01  /  CRYOGENIC MEMORY",{49,112,330,20},10,steel);
        text(g,capture.getToggleState()?"A MOMENT, SUSPENDED.":"HOLD THE MOMENT.",{49,141,630,40},24,white);
        text(g,capture.getToggleState()?(processor.captured.load()?"RECORDED AUDIO  /  GRANULAR HOLD":"NO AUDIO CAPTURED  /  RELEASE, PLAY, RECAPTURE"):
             "Play something. Capture it. Let the world fall away.",{50,184,655,22},12,steel);
        // The cloud envelope and every waveform point are driven by processed audio.
        const bool held=capture.getToggleState() && processor.captured.load();
        for(int layer=7;layer>=0;--layer) {
            juce::Path cloud;
            const float base=270+float(layer)*3;
            cloud.startNewSubPath(48,base);
            for(int j=0;j<128;++j) {
                const float x=48+float(j)*904/127;
                const float envelope=std::sin(float(j)/127*juce::MathConstants<float>::pi);
                const float y=base-wave[size_t(j)]*(44+float(layer)*4)*envelope-(held?6.0f:0.0f)*envelope;
                cloud.lineTo(x,y);
            }
            g.setColour(ice.withAlpha(.04f+float(7-layer)*.015f)); g.strokePath(cloud,juce::PathStrokeType(layer==0?1.5f:.8f));
        }
        g.setColour(steel.withAlpha(.1f));
        for(int j=0;j<17;++j) { const float x=48+float(j)*56.5f; g.drawLine(x,305,500+(x-500)*.6f,241,.6f); }
        text(g,held?"FROZEN / 1.5 s MEMORY":"LIVE / STEREO INPUT",{49,318,500,18},9,held?ice:steel);
    } else {
        text(g,"ENGINE PARAMETERS  /  AUTOMATABLE",{49,316,740,20},9,steel);
    }
    const char* captions[]{"SUSTAIN","FRACTURE","HORIZON","REFLECTION","IMMENSITY","INSTABILITY"};
    for(int i=0;i<6;++i) {
        const float x=28+float(i)*158;
        g.setColour(panel.withAlpha(.6f)); g.fillRoundedRectangle(x,366,154,178,5);
        g.setColour(steel.withAlpha(.13f)); g.drawRoundedRectangle(x,366,154,178,5,1);
        text(g,captions[i],{x,530,154,11},8,i==1?amber.withAlpha(.7f):steel.withAlpha(.7f),juce::Justification::centred);
    }
    text(g,"BEAUTIFUL  /  DESTROYED",{47,568,600,20},12,ice);
    text(g,"Frozen textures. Damaged echoes. Infinite distances.",{47,594,650,24},13,steel);
    text(g,"IN   >   DAMAGE   >   FROZEN + ECHO   >   ABYSS   >   DISTANCE",{47,635,650,18},9,steel.withAlpha(.6f));
    g.setColour(steel.withAlpha(.16f)); g.drawHorizontalLine(679,28,972);
    text(g,"FROSTBITE AUDIO  /  1.0.0",{29,681,300,16},8,steel.withAlpha(.65f));
    text(g,processor.presetName(),{357,681,360,16},9,steel,juce::Justification::centred);
    auto meter=[&](float x,float v,const char* label) {
        text(g,label,{x-25,682,23,14},8,steel);
        g.setColour(steel.withAlpha(.2f)); g.fillRect(x,687.0f,65.0f,3.0f);
        g.setColour(v>.98f?amber:ice); g.fillRect(x,687.0f,65*juce::jlimit(0.0f,1.0f,(juce::Decibels::gainToDecibels(v,-60.0f)+60)/60),3.0f);
    };
    meter(788,input,"IN"); meter(902,output,"OUT");
}
void FrostbiteAudioProcessorEditor::timerCallback() {
    if(!isShowing()) return;
    input=std::max(processor.inputMeter.load(std::memory_order_relaxed),input*.88f);
    output=std::max(processor.outputMeter.load(std::memory_order_relaxed),output*.88f);
    const auto head=processor.scopeHead.load(std::memory_order_acquire);
    for(unsigned i=0;i<128;++i) wave[i]=processor.scope[(head+i)%128].load(std::memory_order_relaxed);
    const auto r=processor.presetRevision.load();
    if(r!=revision) { revision=r; presets.setText(processor.presetName(),juce::dontSendNotification); }
    // Only audio visualisation, meters and preset metadata need polling. Knobs/readouts
    // repaint through JUCE SliderAttachment, including when this timer is not running.
    auto invalidate=[this](juce::Rectangle<int> logical) {
        repaint(getLocalArea(&surface,logical));
    };
    if(!details) invalidate({28,98,944,254});
    invalidate({28,679,944,21});
    flushNativeRepaints(*this);
}
void FrostbiteAudioProcessorEditor::choosePreset(bool saving) {
    const auto folder=processor.presetDirectory();
    chooser=std::make_unique<juce::FileChooser>(saving?"Save FROSTBITE preset":"Load FROSTBITE preset",
        saving?folder.getChildFile("My Frozen World.frostbite"):folder,"*.frostbite");
    const auto flags=saving?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting:
        juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<FrostbiteAudioProcessorEditor> safe(this);
    chooser->launchAsync(flags,[safe,saving](const juce::FileChooser& fc) {
        if(safe==nullptr) return;
        auto f=fc.getResult(); if(f==juce::File{}) return;
        if(saving) f=f.withFileExtension(".frostbite");
        safe->report(saving?safe->processor.savePreset(f):safe->processor.loadPreset(f));
    });
}
void FrostbiteAudioProcessorEditor::report(juce::Result r) {
    if(r.failed()) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"FROSTBITE",r.getErrorMessage());
}
