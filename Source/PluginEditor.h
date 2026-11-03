#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
/** shared palette + knob drawing */
struct GyrdiLookAndFeel : public juce::LookAndFeel_V4
{
    static inline const juce::Colour bg     { 0xff15121a };
    static inline const juce::Colour panel  { 0xff201a26 };
    static inline const juce::Colour brass  { 0xffd9a441 };
    static inline const juce::Colour ink    { 0xffefe6d6 };
    static inline const juce::Colour dim    { 0xff8a7f92 };

    GyrdiLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

//==============================================================================
/**
    The floating instrument. Every part is clickable:
    body / wheel / crank+handler / pads / pegbox (strings) / familiar / cover open a preset list,
    individual strings toggle on & off, and the 24 keys (+ the 25th "slam" key) play.
*/
class InstrumentView : public juce::Component, private juce::Timer
{
public:
    explicit InstrumentView (HyrdiGyrdiProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    static constexpr float W = 680.0f, H = 720.0f;   // design-space size

private:
    void timerCallback() override;
    juce::Point<float> toDesign (juce::Point<float> p) const;
    int  choice (const char* id) const;
    float value (const char* id) const;
    void setParam (const char* id, float plainValue);
    void showChoiceMenu (const char* id, const juce::StringArray& names, const juce::String& title);
    void showBodyMenu();
    void showCoverMenu();
    juce::String hitTestPart (juce::Point<float> designPoint, int& index) const;

    void drawBackground (juce::Graphics&, int body);
    void drawBody (juce::Graphics&, int body, int decor);
    void drawWheel (juce::Graphics&, int wheel);
    void drawStrings (juce::Graphics&);
    void drawPads (juce::Graphics&, int pads);
    void drawKeybox (juce::Graphics&);
    void drawFamiliar (juce::Graphics&, int fam, float exciter);
    void drawCrank (juce::Graphics&, int handler);
    void drawCover (juce::Graphics&);
    void drawCaption (juce::Graphics&, const juce::String& text, float x, float y, juce::Justification j = juce::Justification::centred);

    HyrdiGyrdiProcessor& proc;
    float time = 0.0f, bob = 0.0f;
    float crankAngle = 0, wheelSpeed = 0, famGlow = 0, padHeat = 0, lidOpen = 1, slamFlash = 0, bowDir = 1;
    int heldNote = -1, clickedNote = -1;
    juce::String hoverPart;
    juce::Random rnd;
};

//==============================================================================
/** small chevron tab that shows / hides the controls pane */
struct PaneToggle : public juce::Button
{
    PaneToggle() : juce::Button ("toggleControls") {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    bool paneOpen = true;
};

//==============================================================================
class HyrdiGyrdiEditor : public juce::AudioProcessorEditor
{
public:
    explicit HyrdiGyrdiEditor (HyrdiGyrdiProcessor&);
    ~HyrdiGyrdiEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    struct Section { juce::String title; std::vector<Knob*> knobs; };

    Knob* addKnob (const char* paramId, const juce::String& shortName);
    Toggle* addToggle (const char* paramId, const juce::String& text);
    void setControlsVisible (bool shouldShow);

    static constexpr int instrumentWidth = 680, controlsWidth = 420, editorHeight = 720;
    static inline const juce::Identifier controlsVisibleId { "controlsVisible" };  // saved with the session

    HyrdiGyrdiProcessor& proc;
    GyrdiLookAndFeel lnf;
    InstrumentView instrument;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::vector<Section> sections;
    juce::ComboBox crankMode;
    PaneToggle paneToggle;
    juce::TooltipWindow tooltips { this, 600 };
    bool controlsVisible = true;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> crankModeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HyrdiGyrdiEditor)
};
