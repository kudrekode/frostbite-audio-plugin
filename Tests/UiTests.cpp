#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void check(bool ok,const juce::String& why) {
    if(!ok) throw std::runtime_error(why.toStdString());
}
template<class T> T* find(juce::Component& c,const juce::String& title) {
    if(auto* item=dynamic_cast<T*>(&c)) {
        if constexpr(std::is_same_v<T,juce::Slider>) { if(item->getTitle()==title) return item; }
        else if constexpr(std::is_same_v<T,juce::TextButton>) { if(item->getButtonText()==title) return item; }
        else if constexpr(std::is_same_v<T,juce::ComboBox>) { if(item->getTitle()==title) return item; }
    }
    for(auto* child:c.getChildren()) if(auto* item=find<T>(*child,title)) return item;
    return nullptr;
}
juce::Slider& slider(juce::Component& editor,size_t i) {
    auto* s=find<juce::Slider>(editor,frostbite::parameterInfo[i].name);
    check(s!=nullptr,"Missing slider: "+juce::String(frostbite::parameterInfo[i].id));
    return *s;
}
juce::Label& readout(juce::Slider& s) {
    for(auto* c:s.getChildren()) if(auto* label=dynamic_cast<juce::Label*>(c)) return *label;
    throw std::runtime_error("Missing editable slider readout");
}
void pump() { juce::MessageManager::getInstance()->runDispatchLoopUntil(60); }
struct ThreadAudit final : juce::Slider::Listener {
    explicit ThreadAudit(juce::Slider& s):slider(s) { slider.addListener(this); }
    ~ThreadAudit() override { slider.removeListener(this); }
    void sliderValueChanged(juce::Slider*) override {
        if(!juce::MessageManager::getInstance()->isThisTheMessageThread()) guiOnly=false;
    }
    juce::Slider& slider;
    std::atomic<bool> guiOnly{true};
};
struct PaintAudit final : juce::ComponentListener {
    explicit PaintAudit(juce::Component& c):component(c) { component.addComponentListener(this); }
    ~PaintAudit() override { component.removeComponentListener(this); }
    void componentPainted(juce::Component&,const juce::ComponentPaintDiagnostics&) override { ++count; }
    juce::Component& component;
    int count=0;
};
void hostSet(FrostbiteAudioProcessor& p,size_t i,float normalised) {
    // Emulate a host callback on a non-message thread. The attachment must marshal to the GUI.
    std::thread worker([&] { p.state.getParameter(frostbite::parameterInfo[i].id)->setValueNotifyingHost(normalised); });
    worker.join(); pump();
}
void synchronised(FrostbiteAudioProcessor& p,juce::Component& editor) {
    for(size_t i=0;i<frostbite::Count;++i) {
        if(i==frostbite::Capture || i==frostbite::Bypass || i==frostbite::EchoSync || i==frostbite::EchoDivision || i==frostbite::Character || i>=frostbite::Voicing) continue;
        auto& s=slider(editor,i);
        auto* param=p.state.getParameter(frostbite::parameterInfo[i].id);
        check(std::abs(s.getValue()-param->convertFrom0to1(param->getValue()))<.002,
              "Parameter/control mismatch: "+param->paramID);
        check(readout(s).getText()==param->getText(param->getValue(),0),"Stale readout: "+param->paramID);
        check(std::abs(s.valueToProportionOfLength(s.getValue())-param->getValue())<.0001,
              "Normalisation/skew mismatch: "+param->paramID);
    }
}
juce::Image snapshot(juce::Component& c) { return c.createComponentSnapshot(c.getLocalBounds(),true,1.0f); }
int differences(const juce::Image& a,const juce::Image& b,juce::Rectangle<int> area) {
    int count=0;
    for(int y=area.getY();y<area.getBottom();++y) for(int x=area.getX();x<area.getRight();++x)
        if(a.getPixelAt(x,y)!=b.getPixelAt(x,y)) ++count;
    return count;
}
bool accentNear(const juce::Image& im,juce::Point<float> point,juce::Colour accent) {
    for(int y=juce::roundToInt(point.y)-2;y<=juce::roundToInt(point.y)+2;++y)
        for(int x=juce::roundToInt(point.x)-2;x<=juce::roundToInt(point.x)+2;++x) {
            const auto pixel=im.getPixelAt(x,y);
            if(std::abs(int(pixel.getRed())-int(accent.getRed()))<20
               && std::abs(int(pixel.getGreen())-int(accent.getGreen()))<20
               && std::abs(int(pixel.getBlue())-int(accent.getBlue()))<20) return true;
        }
    return false;
}
void verifyIndicator(juce::Slider& s,const juce::Image& im,float normalised) {
    auto bounds=s.getLookAndFeel().getSliderLayout(s).sliderBounds.toFloat().reduced(9);
    const auto centre=bounds.getCentre();
    const auto radius=std::min(bounds.getWidth(),bounds.getHeight())*.5f;
    const auto rotary=s.getRotaryParameters();
    const auto angle=rotary.startAngleRadians+normalised*(rotary.endAngleRadians-rotary.startAngleRadians);
    const auto direction=juce::Point<float>(std::sin(angle),-std::cos(angle));
    check(accentNear(im,centre+direction*radius*.56f,s.findColour(juce::Slider::rotarySliderFillColourId)),
          "Rendered indicator at wrong angle: "+s.getTitle());
    if(normalised>.1f) {
        const auto halfAngle=rotary.startAngleRadians+normalised*.5f*(rotary.endAngleRadians-rotary.startAngleRadians);
        check(accentNear(im,centre+juce::Point<float>(std::sin(halfAngle),-std::cos(halfAngle))*radius,
                         (s.getComponentID()=="damage"?s.findColour(juce::Slider::rotarySliderFillColourId).interpolatedWith(juce::Colour(0xffff6347),normalised*normalised):s.findColour(juce::Slider::rotarySliderFillColourId))),"Missing rendered active arc: "+s.getTitle());
    } else if(normalised==0) {
        const auto halfAngle=(rotary.startAngleRadians+rotary.endAngleRadians)*.5f;
        check(!accentNear(im,centre+juce::Point<float>(std::sin(halfAngle),-std::cos(halfAngle))*radius,
                          s.findColour(juce::Slider::rotarySliderFillColourId)),"Active arc visible at minimum: "+s.getTitle());
    }
}
void saveImage(const juce::Image& im,const juce::File& file) {
    juce::FileOutputStream stream(file); juce::PNGImageFormat png;
    check(stream.openedOk() && stream.setPosition(0) && stream.truncate().wasOk() && png.writeImageToStream(im,stream),
          "Cannot save screenshot: "+file.getFullPathName());
}
void click(juce::Component& editor,const char* name) {
    auto* b=find<juce::TextButton>(editor,name); check(b!=nullptr,"Missing button: "+juce::String(name)); b->onClick();
}
juce::MouseEvent mouse(juce::Slider& s,juce::Point<float> position,juce::Point<float> start,int clicks=1) {
    return {juce::Desktop::getInstance().getMainMouseSource(),position,juce::ModifierKeys::leftButtonModifier,
            1,0,0,0,0,&s,&s,juce::Time::getCurrentTime(),start,juce::Time::getCurrentTime(),clicks,true};
}
void testPresetPopup(FrostbiteAudioProcessor& p,juce::Component& editor,const juce::File& dir) {
    auto* combo=find<juce::ComboBox>(editor,"Factory presets");
    check(combo!=nullptr,"Missing preset selector");
    p.setCurrentProgram(0); pump();
    check(combo->getSelectedId()==1,"Factory preset selection lost during metadata update");
    combo->grabKeyboardFocus();
    check(combo->hasKeyboardFocus(true),"Preset selector cannot receive keyboard focus");
    saveImage(snapshot(*combo),dir.getChildFile("preset-selector-focused.png"));
    combo->showPopup();
    juce::Component::SafePointer<juce::Component> menu(juce::Component::getCurrentlyModalComponent());
    check(menu!=nullptr && menu->getName()=="menu","Actual preset popup was not created");
    check(&menu->getLookAndFeel()==&combo->getLookAndFeel(),"Popup lost FROSTBITE LookAndFeel");
    auto* peer=menu->getPeer();
    check(peer!=nullptr,"Actual popup has no native peer");
   #if JUCE_WINDOWS
    check(peer->getAvailableRenderingEngines()[peer->getCurrentRenderingEngine()]=="Software Renderer",
          "Popup must use the same reliable software renderer as the editor");
   #endif
    check(menu->getNumChildComponents()==p.getNumPrograms(),"Factory menu must contain all eight individual rows");
    const auto highlight=combo->getLookAndFeel().findColour(juce::PopupMenu::highlightedBackgroundColourId);
    const auto highlightText=combo->getLookAndFeel().findColour(juce::PopupMenu::highlightedTextColourId);
    check(highlightText.getPerceivedBrightness()>highlight.getPerceivedBrightness()+.45f,"Popup hover text lacks contrast");
    auto highlighted=[&](const juce::Image& image,int row) {
        const auto bounds=menu->getChildComponent(row)->getBounds();
        return image.getPixelAt(bounds.getX()+6,bounds.getCentreY())==highlight;
    };
    // Exercise JUCE's real menu mouse handler. No OS cursor movement or fabricated row painting.
    for(int row=0;row<p.getNumPrograms();++row) {
        auto* item=menu->getChildComponent(row);
        check(item->getHeight()>=30,"Preset menu row is too small to read");
        const auto position=item->getBounds().getCentre().toFloat();
        const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(),position,{},
            1,0,0,0,0,menu.getComponent(),menu.getComponent(),juce::Time::getCurrentTime(),position,juce::Time::getCurrentTime(),0,false);
        menu->mouseMove(event);
        const auto im=snapshot(*menu);
        for(int candidate=0;candidate<p.getNumPrograms();++candidate)
            check(highlighted(im,candidate)==(candidate==row),"Popup mouse tracking highlights the wrong preset row");
        saveImage(im,dir.getChildFile("preset-popup-hover-"+juce::String(row+1)+".png"));
    }
    // Move up from the last hovered row. Mouse and keyboard must share the visible highlight.
    check(menu->keyPressed(juce::KeyPress(juce::KeyPress::upKey)),"Popup does not accept keyboard navigation");
    const auto keyed=snapshot(*menu);
    check(highlighted(keyed,p.getNumPrograms()-2) && !highlighted(keyed,p.getNumPrograms()-1),"Keyboard highlight failed to move between rows");
    saveImage(keyed,dir.getChildFile("preset-popup-keyboard.png"));
    // Selected row retains a tick even when another row is hovered.
    auto tickArea=menu->getChildComponent(0)->getBounds().withWidth(26).reduced(5);
    int selectedPixels=0;
    for(int y=tickArea.getY();y<tickArea.getBottom();++y) for(int x=tickArea.getX();x<tickArea.getRight();++x)
        if(keyed.getPixelAt(x,y).getPerceivedBrightness()>.6f) ++selectedPixels;
    check(selectedPixels>10,"Selected factory preset has no visible tick");
    check(menu->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)),"Popup cannot select with Return");
    // JUCE closes the modal menu, then posts a separate ComboBox change notification.
    // Native paints can consume one dispatch slice before both callbacks have run.
    for(int attempt=0;attempt<10 && p.getCurrentProgram()!=p.getNumPrograms()-2;++attempt) pump();
    check(p.getCurrentProgram()==p.getNumPrograms()-2 && combo->getSelectedId()==p.getNumPrograms()-1,"Keyboard popup selection did not load the selected preset: program="
          +juce::String(p.getCurrentProgram())+", selection="+juce::String(combo->getSelectedId()));
    combo->showPopup();
    menu=juce::Component::getCurrentlyModalComponent();
    check(menu!=nullptr && menu->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Popup cannot dismiss with Escape");
    pump();
}
void testUi(const juce::File& dir) {
    FrostbiteAudioProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    synchronised(p,*editor);
    click(*editor,"DETAILS");
    for(auto tab:{"FROZEN","ECHO","ABYSS","CHARACTER"}) click(*editor,tab);
    click(*editor,"DETAILS");
    juce::String matrix="Parameter,Initial,Host min/mid/max,Rendered arc/indicator/readout,Mouse drag,Double-click reset,Numeric entry\n";
    for(size_t i=0;i<frostbite::Count;++i) {
        if(i==frostbite::Capture || i==frostbite::Bypass || i==frostbite::EchoSync || i==frostbite::EchoDivision || i==frostbite::Character || i>=frostbite::Voicing) continue;
        auto& s=slider(*editor,i); auto* param=p.state.getParameter(frostbite::parameterInfo[i].id);
        ThreadAudit audit(s);
        std::array<juce::Image,3> images;
        int n=0;
        for(float position:{0.0f,.5f,1.0f}) {
            hostSet(p,i,position); synchronised(p,*editor);
            check(std::abs(param->getValueForText(param->getText(param->getValue(),0))-param->getValue())<.0001f,
                  "Formatted value does not round-trip: "+param->paramID);
            images[size_t(n)]=snapshot(s); verifyIndicator(s,images[size_t(n)],param->getValue()); ++n;
        }
        auto box=readout(s).getBounds();
        for(int j=1;j<3;++j) {
            check(differences(images[size_t(j-1)],images[size_t(j)],box)>5,"Readout pixels static: "+param->paramID);
            check(differences(images[size_t(j-1)],images[size_t(j)],s.getLocalBounds().withBottom(box.getY()))>30,
                  "Rotary pixels static: "+param->paramID);
        }
        // User drag follows the real JUCE mouse handler and attachment, with no manual parameter sync.
        hostSet(p,i,.25f);
        const auto start=s.getLocalBounds().getCentre().toFloat();
        s.mouseDown(mouse(s,start,start)); s.mouseDrag(mouse(s,start.translated(0,-30),start)); s.mouseUp(mouse(s,start.translated(0,-30),start));
        check(param->getValue()>.25f,"Mouse drag did not change parameter: "+param->paramID);
        synchronised(p,*editor);
        s.mouseDoubleClick(mouse(s,start,start,2));
        check(std::abs(param->getValue()-param->getDefaultValue())<.002f,"Double-click reset: "+param->paramID);
        synchronised(p,*editor);
        check(audit.guiOnly.load(),"Control updated from a non-GUI thread: "+param->paramID);
        matrix+=param->paramID+",PASS,PASS,PASS,PASS,PASS,PASS\n";
        // Exercise the editable label's text->slider->parameter path.
        readout(s).setText(param->getText(.75f,0),juce::sendNotificationSync);
        check(std::abs(param->getValue()-.75f)<.025f,"Numeric entry: "+param->paramID);
        synchronised(p,*editor);
    }
    // Precision must represent the actual 0.01 Hz step, including the 0.03 Hz minimum.
    hostSet(p,frostbite::MotionRate,0);
    check(readout(slider(*editor,frostbite::MotionRate)).getText()=="0.03 Hz","Motion rate precision loses actual value");

    for(float position:{0.0f,.5f,1.0f}) {
        for(size_t i=0;i<frostbite::Count;++i) hostSet(p,i,position);
        const auto suffix=position==0?"min":position==1?"max":"mid";
        const auto composite=snapshot(*editor);
        saveImage(composite,dir.getChildFile(juce::String("macros-")+suffix+".png"));
        for(size_t i=0;i<=frostbite::Output;++i) {
            auto& s=slider(*editor,i);
            verifyIndicator(s,composite.getClippedImage(editor->getLocalArea(&s,s.getLocalBounds())),
                            p.state.getParameter(frostbite::parameterInfo[i].id)->getValue());
        }
        click(*editor,"DETAILS");
        for(auto tab:{"FROZEN","ECHO","ABYSS","CHARACTER"}) {
            click(*editor,tab);
            saveImage(snapshot(*editor),dir.getChildFile(juce::String(tab)+"-"+suffix+".png"));
        }
        click(*editor,"DETAILS");
    }
    for(size_t i:{size_t(frostbite::Capture),size_t(frostbite::Bypass),size_t(frostbite::EchoSync)}) {
        const char* title=i==frostbite::Capture?"CAPTURE":i==frostbite::Bypass?"BYPASS":"TEMPO SYNC";
        // Capture can be relabelled by its state callback/timer.
        auto* button=find<juce::TextButton>(*editor,title);
        if(!button && i==frostbite::Capture) button=find<juce::TextButton>(*editor,"RELEASE");
        check(button!=nullptr,"Missing attached button");
        for(float value:{0.0f,1.0f}) { hostSet(p,i,value); check(button->getToggleState()==(value>0),"Host/button sync"); }
        button->triggerClick(); pump(); check(p.values()[i]==0,"Button/parameter sync");
    }
    // Validate both choice attachments, identified by their item lists.
    auto* surface=editor->getChildComponent(0);
    for(auto* child:surface->getChildren()) if(auto* combo=dynamic_cast<juce::ComboBox*>(child)) {
        if(combo->getNumItems()!=9 && combo->getNumItems()!=2) continue;
        size_t i=0; for(;i<frostbite::Count;++i) if(combo->getComponentID()==frostbite::parameterInfo[i].id) break;
        check(i<frostbite::Count,"choice missing parameter identity");
        hostSet(p,i,0); check(combo->getSelectedItemIndex()==0,"Host/choice minimum");
        hostSet(p,i,1); check(combo->getSelectedItemIndex()==combo->getNumItems()-1,"Host/choice maximum");
        combo->setSelectedItemIndex(0,juce::sendNotificationSync); check(p.values()[i]==0,"Choice/parameter sync");
    }
    for(int i=0;i<p.getNumPrograms();++i) { p.setCurrentProgram(i); synchronised(p,*editor); }
    const auto preset=dir.getChildFile("ui-roundtrip.frostbite");
    check(p.savePreset(preset).wasOk(),"UI preset save");
    juce::MemoryBlock state; p.getStateInformation(state);
    p.restoreDefaults(); synchronised(p,*editor);
    check(p.loadPreset(preset).wasOk(),"UI preset load"); synchronised(p,*editor);
    p.restoreDefaults();
    std::thread restore([&] { p.setStateInformation(state.getData(),int(state.getSize())); }); restore.join(); pump();
    synchronised(p,*editor);
    editor.reset();
    hostSet(p,frostbite::Freeze,.731f); // No editor exists; reopening must read current APVTS state.
    editor.reset(p.createEditor()); synchronised(p,*editor);
    // Destruction while async attachment callbacks are pending must cancel them safely.
    std::thread pending([&] { p.state.getParameter("freeze")->setValueNotifyingHost(.123f); }); pending.join();
    editor.reset(); pump();
    editor.reset(p.createEditor()); synchronised(p,*editor);
    for(auto size:{juce::Point<int>(800,560),juce::Point<int>(1500,1050),juce::Point<int>(1600,1120)}) {
        editor->setSize(size.x,size.y);
        saveImage(snapshot(*editor),dir.getChildFile("scaled-"+juce::String(size.x)+".png"));
    }
   #if JUCE_WINDOWS
    // Model the VST3 wrapper: the editor is parented before its native peer exists.
    editor->setSize(1000,700);
    juce::Component wrapper;
    wrapper.setBounds(32,32,1000,700);
    wrapper.addAndMakeVisible(*editor);
    for(int attach=0;attach<2;++attach) {
        wrapper.addToDesktop(juce::ComponentPeer::windowIsTemporary
                             |juce::ComponentPeer::windowIgnoresMouseClicks
                             |juce::ComponentPeer::windowIgnoresKeyPresses);
        wrapper.setAlwaysOnTop(true);
        wrapper.setVisible(true);
        auto* peer=editor->getPeer();check(peer!=nullptr,"Native editor peer missing");
        check(peer->getAvailableRenderingEngines()[peer->getCurrentRenderingEngine()]=="Software Renderer",
              "Windows host peer must select the software renderer on attachment/recreation");
        peer->performAnyPendingRepaintsNow();
        auto& control=slider(*editor,frostbite::Freeze);
        PaintAudit paints(control);
        for(double value:{.2,.8}) {
            const auto before=paints.count;
            control.setValue(value,juce::sendNotificationSync);
            // Deliberately no dispatch-loop pump, timer, snapshot or explicit peer flush here.
            // The actual value callback must service native dirty regions by itself.
            check(paints.count>before,"Native control paint deferred after a synchronous value change");
        }
        if(attach==0) testPresetPopup(p,*editor,dir);
        wrapper.setVisible(false);
        wrapper.removeFromDesktop();
    }
    wrapper.removeChildComponent(editor.get());
   #endif
    check(dir.getChildFile("control-matrix.csv").replaceWithText(matrix),"Cannot write control matrix");
}
}
int main() {
    juce::ScopedJuceInitialiser_GUI juce;
    try {
        auto dir=juce::File::getCurrentWorkingDirectory().getChildFile("qa/ui");
        check(dir.createDirectory().wasOk(),"UI evidence directory");
        testUi(dir);
        std::cout<<"PASS UI: 29 rotary controls, 3 buttons, 5 choices; off-thread host updates; rendered min/mid/max arcs, indicators and text; mouse drag, reset, numeric entry; 18 presets, preset/state restore, reopen, pending attachment destruction, scaled snapshots; native peer recreation and immediate paints without timer dispatch; actual popup per-row mouse hover, selected tick, keyboard focus/navigation/selection/dismissal\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL UI "<<e.what()<<"\n"; return 1; }
}
