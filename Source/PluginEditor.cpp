#include "PluginEditor.h"

using namespace juce;

namespace
{
const String productName = String (CharPointer_UTF8 ("h\xc3\xbf" "rdiGYRDI"));
const String makerName   = String (CharPointer_UTF8 ("v\xc3\xb6" "luntyr"));   // branding glyph; metadata uses plain "voluntyr"

// design-space geometry for the instrument (680 x 720)
const Rectangle<float> bodyRect   { 40.0f, 270.0f, 380.0f, 310.0f };
const Rectangle<float> keyboxRect { 330.0f, 372.0f, 300.0f, 96.0f };
const Rectangle<float> pegboxRect { 628.0f, 350.0f, 44.0f, 140.0f };
const Point<float>     wheelCentre { 140.0f, 425.0f };
constexpr float        wheelRadius = 70.0f;
constexpr float        crankLength = 105.0f;
const Point<float>     familiarPos { 262.0f, 500.0f };
constexpr float        stringStartX = 32.0f, stringEndX = 640.0f;
const float            stringY[7] = { 410.0f, 418.0f, 392.0f, 400.0f, 432.0f, 442.0f, 454.0f };
constexpr float        keyX0 = 338.0f, keyW = 11.8f, keyY = 468.0f, keyH = 26.0f;

Colour bodyColour (int b)
{
    static const Colour c[4] = { Colour (0xff8a5a33), Colour (0xff7d8794), Colour (0xff5e7a4f), Colour (0xffd0663a) };
    return c[jlimit (0, 3, b)];
}
Colour stringColour (int s)
{
    // gut, steel, nylon, fiber-light, hemp
    static const Colour c[5] = { Colour (0xffe8d3a8), Colour (0xffd8e2ea), Colour (0xfff2f0e6), Colour (0xffbff4ff), Colour (0xffb59a6a) };
    return c[jlimit (0, 4, s)];
}
} // namespace

//==============================================================================
GyrdiLookAndFeel::GyrdiLookAndFeel()
{
    setColour (Slider::textBoxTextColourId, ink);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Label::textColourId, ink);
    setColour (ComboBox::backgroundColourId, panel.brighter (0.1f));
    setColour (ComboBox::textColourId, ink);
    setColour (ComboBox::outlineColourId, brass.withAlpha (0.4f));
    setColour (ComboBox::arrowColourId, brass);
    setColour (PopupMenu::backgroundColourId, Colour (0xff1b1620));
    setColour (PopupMenu::textColourId, ink);
    setColour (PopupMenu::highlightedBackgroundColourId, brass.withAlpha (0.35f));
    setColour (PopupMenu::headerTextColourId, brass);
    setColour (ToggleButton::textColourId, ink);
    setColour (TooltipWindow::backgroundColourId, panel);
}

void GyrdiLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);

    g.setColour (Colour (0xff0e0b11));
    g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c));

    Path track;
    track.addCentredArc (c.x, c.y, r - 3, r - 3, 0, start, end, true);
    g.setColour (dim.withAlpha (0.35f));
    g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // bipolar params (direction, EQ, transpose…) draw from the centre
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    Path val;
    val.addCentredArc (c.x, c.y, r - 3, r - 3, 0, jmin (from, angle), jmax (from, angle), true);
    g.setColour (brass);
    g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    Path needle;
    needle.addRoundedRectangle (-1.5f, -(r - 7), 3.0f, r * 0.55f, 1.5f);
    g.setColour (ink);
    g.fillPath (needle, AffineTransform::rotation (angle).translated (c));
}

void GyrdiLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (b.getToggleState() ? brass.withAlpha (0.85f) : panel.brighter (highlighted ? 0.25f : 0.12f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (brass.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (b.getToggleState() ? bg : ink);
    g.setFont (FontOptions (12.0f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), Justification::centred, 2);
}

//==============================================================================
InstrumentView::InstrumentView (HyrdiGyrdiProcessor& p) : proc (p)
{
    setRepaintsOnMouseActivity (false);
    startTimerHz (30);
}

int InstrumentView::choice (const char* id) const { return (int) proc.apvts.getRawParameterValue (id)->load(); }
float InstrumentView::value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

void InstrumentView::setParam (const char* id, float plain)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void InstrumentView::timerCallback()
{
    const auto& t = proc.telemetry();
    time += 1.0f / 30.0f;
    bob = 5.0f * std::sin (time * 0.9f);
    crankAngle = t.crankAngle.load();
    wheelSpeed = t.wheelSpeed.load();
    famGlow    = jmax (t.familiarGlow.load(), famGlow * 0.85f);
    padHeat    = t.padHeat.load();
    lidOpen    = t.lidOpen.load();
    slamFlash  = jmax (t.slamFlash.load(), slamFlash * 0.85f);
    bowDir     = t.bowDirection.load();
    heldNote   = t.heldNote.load();
    repaint();
}

Point<float> InstrumentView::toDesign (Point<float> p) const
{
    return { p.x * W / (float) getWidth(), p.y * H / (float) getHeight() - bob };
}

//==============================================================================
void InstrumentView::paint (Graphics& g)
{
    const int body = choice (hg::ids::body);
    g.addTransform (AffineTransform::scale ((float) getWidth() / W, (float) getHeight() / H));
    drawBackground (g, body);

    // title
    g.setColour (GyrdiLookAndFeel::ink);
    g.setFont (FontOptions (40.0f, Font::bold));
    g.drawText (productName, 28, 22, 500, 46, Justification::left);
    g.setColour (GyrdiLookAndFeel::dim);
    g.setFont (FontOptions (13.0f));
    g.drawText ("by " + makerName + "   |   click any part of the instrument to change it", 30, 66, 500, 18, Justification::left);

    {
        Graphics::ScopedSaveState s (g);
        g.addTransform (AffineTransform::translation (0.0f, bob));
        drawBody (g, body, choice (hg::ids::decor));
        drawWheel (g, choice (hg::ids::wheel));
        drawKeybox (g);
        drawStrings (g);
        drawPads (g, choice (hg::ids::pads));
        drawCover (g);
        drawFamiliar (g, choice (hg::ids::familiar), value (hg::ids::exciter));
        drawCrank (g, choice (hg::ids::handler));

        // part captions
        g.setFont (FontOptions (12.0f));
        drawCaption (g, hg::names::body[body] + "  /  " + hg::names::build[choice (hg::ids::build)], bodyRect.getCentreX(), bodyRect.getBottom() + 14);
        drawCaption (g, "wheel: " + hg::names::wheel[choice (hg::ids::wheel)], wheelCentre.x, wheelCentre.y - wheelRadius - 40);
        drawCaption (g, "pads: " + hg::names::pads[choice (hg::ids::pads)], wheelCentre.x + 10, wheelCentre.y - wheelRadius - 24);
        drawCaption (g, "strings: " + hg::names::strings[choice (hg::ids::strings)], pegboxRect.getCentreX() - 20, pegboxRect.getY() - 14);
        drawCaption (g, hg::names::familiar[choice (hg::ids::familiar)] + (value (hg::ids::exciter) < 0.02f ? " (asleep)" : ""),
                     familiarPos.x, familiarPos.y + 38);
    }

    if (hoverPart.isNotEmpty())
    {
        g.setColour (GyrdiLookAndFeel::brass);
        g.setFont (FontOptions (14.0f));
        g.drawText (hoverPart, 0, (int) H - 34, (int) W, 22, Justification::centred);
    }

    if (slamFlash > 0.02f)
    {
        g.setColour (Colours::white.withAlpha (0.12f * slamFlash));
        g.fillRect (0.0f, 0.0f, W, H);
    }
}

void InstrumentView::drawBackground (Graphics& g, int body)
{
    static const Colour top[4]    = { Colour (0xff1a2a1f), Colour (0xff1c2330), Colour (0xff1d1430), Colour (0xff2a1d14) };
    static const Colour bottom[4] = { Colour (0xff4a2f18), Colour (0xff3a3f48), Colour (0xff0f3a3a), Colour (0xff17363a) };
    g.setGradientFill (ColourGradient (top[body], 0, 0, bottom[body], 0, H, false));
    g.fillAll();

    // drifting motes: sawdust, sparks, spores or confetti depending on the body
    static const Colour mote[4] = { Colour (0xffe9c38a), Colour (0xffffb35c), Colour (0xff8affd6), Colour (0xffff7ad1) };
    Random r (1234 + body);
    for (int i = 0; i < 40; ++i)
    {
        const float x = std::fmod (r.nextFloat() * W + time * (6.0f + 10.0f * r.nextFloat()), W);
        const float y = std::fmod (r.nextFloat() * H - time * (4.0f + 8.0f * r.nextFloat()) + H * 4, H);
        const float a = 0.15f + 0.25f * (0.5f + 0.5f * std::sin (time * 1.3f + (float) i));
        g.setColour (mote[body].withAlpha (a));
        g.fillEllipse (x, y, 2.5f, 2.5f);
    }
    // soft shadow under the floating instrument
    g.setColour (Colours::black.withAlpha (0.25f - 0.01f * bob));
    g.fillEllipse (90.0f - bob, 640.0f, 520.0f + 2 * bob, 30.0f);
}

void InstrumentView::drawBody (Graphics& g, int body, int decor)
{
    const Colour c = bodyColour (body);
    Path p;
    p.addEllipse (bodyRect);
    Path neck;
    neck.addRoundedRectangle (keyboxRect.expanded (6.0f, 10.0f).withLeft (300.0f), 14.0f);
    p.addPath (neck);

    g.setGradientFill (ColourGradient (c.brighter (0.35f), bodyRect.getX() + 80, bodyRect.getY() + 40,
                                       c.darker (0.6f), bodyRect.getRight(), bodyRect.getBottom(), true));
    g.fillPath (p);
    g.setColour (c.darker (0.8f));
    g.strokePath (p, PathStrokeType (3.0f));

    // material hints
    g.setColour (c.darker (0.3f).withAlpha (0.5f));
    if (body == 0)       for (int i = 0; i < 8; ++i) g.drawEllipse (bodyRect.reduced (14.0f + 15.0f * i, 18.0f + 12.0f * i).translated (12, 0), 1.0f); // grain
    else if (body == 1)  for (int i = 0; i < 14; ++i) g.fillEllipse (bodyRect.getX() + 40 + (i % 7) * 46.0f, bodyRect.getY() + (i < 7 ? 50.0f : 260.0f), 6, 6); // rivets
    else if (body == 2)  for (int i = 0; i < 6; ++i) g.drawEllipse (bodyRect.getX() + 60 + i * 50.0f, bodyRect.getBottom() - 90 - (i % 2) * 30.0f, 26, 18, 1.5f); // shell cells
    else                 for (int i = 0; i < 5; ++i) g.fillRect (bodyRect.getX() + 70 + i * 55.0f, bodyRect.getY() + 60 + (i % 3) * 70.0f, 34.0f, 22.0f); // patches

    // sound hole / rosette
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (215.0f, 500.0f, 34.0f, 34.0f);

    // decorations: purely visual
    g.setColour (GyrdiLookAndFeel::brass.withAlpha (0.75f));
    if (decor == 1) // sigils
    {
        for (int k = 0; k < 2; ++k)
        {
            const Point<float> ctr (k == 0 ? 110.0f : 330.0f, k == 0 ? 330.0f : 540.0f);
            Path star; star.addStar (ctr, 5, 9.0f, 22.0f, time * 0.2f);
            g.strokePath (star, PathStrokeType (1.5f));
            g.drawEllipse (Rectangle<float> (50, 50).withCentre (ctr), 1.2f);
        }
    }
    else if (decor == 2) // runes: little angular glyphs along the rim
    {
        for (int i = 0; i < 9; ++i)
        {
            const float a = -2.4f + i * 0.28f;
            const Point<float> ctr = bodyRect.getCentre() + Point<float> (std::cos (a) * 165.0f, std::sin (a) * 130.0f);
            Path r; r.startNewSubPath (ctr.translated (0, -8)); r.lineTo (ctr.translated (0, 8));
            r.startNewSubPath (ctr.translated (0, -3)); r.lineTo (ctr.translated ((i % 2 ? 6.0f : -6.0f), -8));
            if (i % 3 == 0) { r.startNewSubPath (ctr.translated (0, 2)); r.lineTo (ctr.translated (6, 7)); }
            g.strokePath (r, PathStrokeType (1.8f));
        }
    }
    else if (decor == 3) // stickers
    {
        static const Colour sc[4] = { Colour (0xffff5c8a), Colour (0xff5cd3ff), Colour (0xfffff15c), Colour (0xff8aff5c) };
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (sc[i % 4]);
            const float x = 80.0f + (i * 53) % 260, y = 300.0f + (i * 97) % 230;
            if (i % 2) g.fillEllipse (x, y, 22, 22);
            else { Path s; s.addStar ({ x + 11, y + 11 }, 5, 6, 13); g.fillPath (s); }
        }
    }
}

void InstrumentView::drawWheel (Graphics& g, int wheel)
{
    static const Colour rim[4] = { Colour (0xffb98a52), Colour (0xff6e5e5a), Colour (0xffd9e3ea), Colour (0xff2c3a33) };
    const auto r = Rectangle<float> (wheelRadius * 2, wheelRadius * 2).withCentre (wheelCentre);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (r.translated (4, 6));
    g.setGradientFill (ColourGradient (rim[wheel].brighter (0.3f), r.getX(), r.getY(), rim[wheel].darker (0.5f), r.getRight(), r.getBottom(), false));
    g.fillEllipse (r);
    if (wheel == 1) { g.setColour (Colour (0xff7a0f12).withAlpha (0.7f)); g.drawEllipse (r.reduced (3), 5.0f); } // blood
    if (wheel == 3) { g.setColour (Colours::white.withAlpha (0.35f)); for (int i = 0; i < 5; ++i) g.drawLine (r.getX() + 25 + i * 18, r.getY() + 40, r.getX() + 40 + i * 14, r.getBottom() - 40, 1.0f); } // chalk marks

    // spokes rotate with the crank
    g.setColour (rim[wheel].darker (0.8f));
    for (int i = 0; i < 6; ++i)
    {
        const float a = crankAngle + i * MathConstants<float>::twoPi / 6.0f;
        g.drawLine (Line<float> (wheelCentre, wheelCentre + Point<float> (std::cos (a), std::sin (a)) * (wheelRadius - 6)), 3.0f);
    }
    g.drawEllipse (r, 3.0f);
    // motion glint
    if (wheelSpeed > 0.05f)
    {
        Path arc;
        arc.addCentredArc (wheelCentre.x, wheelCentre.y, wheelRadius - 3, wheelRadius - 3, 0, crankAngle, crankAngle + 1.2f * wheelSpeed, true);
        g.setColour (Colours::white.withAlpha (0.4f * wheelSpeed));
        g.strokePath (arc, PathStrokeType (3.0f));
    }
}

void InstrumentView::drawKeybox (Graphics& g)
{
    g.setColour (Colour (0xff2a1f18));
    g.fillRoundedRectangle (keyboxRect, 6.0f);
    g.setColour (GyrdiLookAndFeel::brass.withAlpha (0.4f));
    g.drawRoundedRectangle (keyboxRect, 6.0f, 1.5f);

    // pegbox
    g.setColour (Colour (0xff3b2a1e));
    g.fillRoundedRectangle (pegboxRect, 10.0f);
    g.setColour (GyrdiLookAndFeel::brass);
    for (int i = 0; i < 7; ++i)
        if (hg::slotExists (choice (hg::ids::build), i))
            g.fillEllipse (pegboxRect.getX() + (i % 2 ? 26.0f : 8.0f), pegboxRect.getY() + 12 + i * 17.0f, 10, 10);

    // keys: 24 playable + the 25th "slam" key
    const int base = (int) value (hg::ids::keyBase);
    for (int k = 0; k < 25; ++k)
    {
        const int pc = (base + k) % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        auto key = Rectangle<float> (keyX0 + k * keyW, keyY, keyW - 1.5f, black ? keyH - 8 : keyH);
        Colour c = black ? Colour (0xff2d2420) : Colour (0xffe9dcc2);
        if (k == 24) c = Colour (0xff9c2b2b);
        if (heldNote == base + k) c = GyrdiLookAndFeel::brass;
        g.setColour (c);
        g.fillRoundedRectangle (key, 2.0f);
    }
}

void InstrumentView::drawStrings (Graphics& g)
{
    const int build = choice (hg::ids::build);
    const Colour sc = stringColour (choice (hg::ids::strings));
    for (int i = 0; i < 7; ++i)
    {
        if (! hg::slotExists (build, i)) continue;
        const bool on = value (hg::ids::stringOn[i]) > 0.5f;
        if (! on)
        {
            Path line; line.startNewSubPath (stringStartX, stringY[i]); line.lineTo (stringEndX, stringY[i]);
            Path dashed; const float dashes[] = { 4.0f, 5.0f };
            PathStrokeType (1.0f).createDashedStroke (dashed, line, dashes, 2);
            g.setColour (sc.withAlpha (0.25f));
            g.fillPath (dashed);
            continue;
        }
        // vibrating string: amplitude follows wheel speed (sympathy shimmers instead)
        const bool sym = i == hg::Sym;
        const float amp = (sym ? 0.6f : 1.6f) * wheelSpeed;
        Path s;
        s.startNewSubPath (stringStartX, stringY[i]);
        for (float x = stringStartX; x <= stringEndX; x += 8.0f)
        {
            const float env = std::sin (MathConstants<float>::pi * (x - stringStartX) / (stringEndX - stringStartX));
            s.lineTo (x, stringY[i] + amp * env * std::sin (x * 0.15f + time * 40.0f + (float) i));
        }
        g.setColour (sc.withAlpha (sym ? 0.55f : 0.9f));
        g.strokePath (s, PathStrokeType (i >= hg::Bass1 && ! sym ? 2.0f : 1.3f));
    }
}

void InstrumentView::drawPads (Graphics& g, int pads)
{
    static const Colour pc[4]   = { Colour (0xfff4efe6), Colour (0xffe6dcc0), Colour (0xffa8f0ff), Colour (0xffb8c2cc) };
    static const Colour heat[4] = { Colour (0xffff8a3d), Colour (0xffffe9b0), Colour (0xff5cf2ff), Colour (0xffffffff) };
    const int build = choice (hg::ids::build);
    for (int i = 0; i < 7; ++i)
    {
        if (i == hg::Sym || ! hg::slotExists (build, i)) continue;
        auto r = Rectangle<float> (wheelCentre.x - 10, stringY[i] - 3.5f, 20.0f, 7.0f);
        if (padHeat > 0.05f)
        {
            g.setColour (heat[pads].withAlpha (0.5f * padHeat));
            g.fillEllipse (r.expanded (6.0f * padHeat, 5.0f * padHeat));
        }
        g.setColour (pc[pads].interpolatedWith (heat[pads], padHeat * 0.6f));
        g.fillRoundedRectangle (r, 3.0f);
    }
    // titanium throws sparks, glass glows, cotton smoulders, bone… dusts
    if (padHeat > 0.3f)
    {
        g.setColour (heat[pads].withAlpha (padHeat));
        for (int i = 0; i < (int) (padHeat * 14); ++i)
        {
            const float a = rnd.nextFloat() * MathConstants<float>::twoPi;
            const float len = (pads == 3 ? 18.0f : 6.0f) * rnd.nextFloat();
            const Point<float> o (wheelCentre.x + rnd.nextFloat() * 20 - 10, 390.0f + rnd.nextFloat() * 60);
            if (pads == 3) g.drawLine (Line<float> (o, o + Point<float> (std::cos (a), std::sin (a)) * len), 1.2f);
            else           g.fillEllipse (o.x, o.y - len * 3, 2.5f, 2.5f);
        }
    }
}

void InstrumentView::drawCover (Graphics& g)
{
    // the lid hinges at the top of the keybox; open = flipped up, closed = covering the keys
    const float o = lidOpen;
    const float top = keyboxRect.getY() - 70.0f * o;
    const float bottom = keyboxRect.getBottom() + 28.0f - (keyboxRect.getHeight() + 28.0f) * o;
    auto lid = Rectangle<float> (keyboxRect.getX() - 4, jmin (top, bottom), keyboxRect.getWidth() + 8, std::abs (bottom - top) + 4);
    const Colour c = bodyColour (choice (hg::ids::body)).darker (0.2f);
    g.setColour (c.withAlpha (0.97f));
    g.fillRoundedRectangle (lid, 6.0f);
    g.setColour (GyrdiLookAndFeel::brass.withAlpha (0.7f));
    g.drawRoundedRectangle (lid, 6.0f, 1.5f);

    // the name plate shows the current "preset" (handler + familiar)
    if (lid.getHeight() > 22.0f)
    {
        auto plate = lid.withSizeKeepingCentre (220.0f, jmin (24.0f, lid.getHeight() - 8));
        g.setColour (Colour (0xff1a1410));
        g.fillRoundedRectangle (plate, 4.0f);
        g.setColour (GyrdiLookAndFeel::brass);
        g.setFont (FontOptions (12.0f));
        g.drawText (hg::names::handler[choice (hg::ids::handler)] + " & " + hg::names::familiar[choice (hg::ids::familiar)],
                    plate.toNearestInt(), Justification::centred);
    }
}

void InstrumentView::drawFamiliar (Graphics& g, int fam, float exciter)
{
    // shiba-soul, gremlin, troll, fairy, bees-nest
    static const Colour fc[5] = { Colour (0xffe8a24a), Colour (0xff6fbf4a), Colour (0xff6f8a86), Colour (0xffd8ecff), Colour (0xfff2c230) };
    fam = jlimit (0, 4, fam);
    const bool asleep = exciter < 0.02f;
    const float size = 20.0f * (1.0f + 0.35f * famGlow);
    const auto ctr = familiarPos.translated (0.0f, asleep ? 0.0f : -3.0f * famGlow);

    // the bridge it lives in
    g.setColour (Colour (0xff3a2a1c));
    g.fillRoundedRectangle (familiarPos.x - 34, stringY[hg::Fam1] - 10, 10, 75, 3);

    if (famGlow > 0.02f)
    {
        g.setColour (fc[fam].withAlpha (0.35f * famGlow));
        g.fillEllipse (Rectangle<float> (size * 3.2f, size * 3.2f).withCentre (ctr));
    }

    g.setColour (fc[fam]);
    if (fam == 3 || fam == 4) // fairy wings / bee wings
    {
        g.setColour (fam == 3 ? fc[fam].withAlpha (0.45f) : Colours::white.withAlpha (0.35f + 0.2f * std::sin (time * 40.0f)));
        g.fillEllipse (ctr.x - size * 1.6f, ctr.y - size * 1.2f, size * 1.2f, size * 0.9f);
        g.fillEllipse (ctr.x + size * 0.4f, ctr.y - size * 1.2f, size * 1.2f, size * 0.9f);
        g.setColour (fc[fam]);
    }
    if (fam == 0 || fam == 1) // ears: round-ish shiba, pointy gremlin
    {
        Path ears;
        const float eh = fam == 1 ? size * 1.1f : size * 0.7f;
        ears.addTriangle (ctr.x - size * 0.8f, ctr.y - size * 0.3f, ctr.x - size * 0.3f, ctr.y - size * 0.7f, ctr.x - size * (fam == 1 ? 1.3f : 0.7f), ctr.y - eh);
        ears.addTriangle (ctr.x + size * 0.8f, ctr.y - size * 0.3f, ctr.x + size * 0.3f, ctr.y - size * 0.7f, ctr.x + size * (fam == 1 ? 1.3f : 0.7f), ctr.y - eh);
        g.fillPath (ears);
    }
    const float bw = fam == 2 ? size * 2.3f : size * 1.8f;
    g.fillEllipse (Rectangle<float> (bw, size * 1.6f).withCentre (ctr));
    if (fam == 4) // bee stripes
    {
        g.setColour (Colour (0xff1a1408));
        for (int k = -1; k <= 1; ++k)
            g.fillRect (ctr.x + k * size * 0.5f - 2.0f, ctr.y - size * 0.6f, 4.0f, size * 1.2f);
    }
    if (fam == 3 && famGlow > 0.05f) // fairy: twinkling stars
    {
        g.setColour (Colours::white.withAlpha (famGlow));
        for (int k = 0; k < 4; ++k)
        {
            const float a = time * 0.7f + (float) k * 1.57f;
            Path st; st.addStar (ctr + Point<float> (std::cos (a), std::sin (a)) * size * 1.6f, 4, 1.2f, 4.0f);
            g.fillPath (st);
        }
    }
    if (fam == 2 && famGlow > 0.05f) // troll: water lapping below
    {
        g.setColour (Colour (0xff7fc4d8).withAlpha (0.6f * famGlow));
        Path wv; wv.startNewSubPath (ctr.x - size * 1.4f, ctr.y + size * 0.9f);
        for (float t = -1.4f; t <= 1.4f; t += 0.1f) wv.lineTo (ctr.x + t * size, ctr.y + size * 0.9f + 3.0f * std::sin (t * 6.0f + time * 3.0f));
        g.strokePath (wv, PathStrokeType (2.0f));
    }

    // eyes
    g.setColour (Colour (0xff120d0a));
    if (asleep)
    {
        g.drawLine (ctr.x - 9, ctr.y - 2, ctr.x - 3, ctr.y - 2, 2.0f);
        g.drawLine (ctr.x + 3, ctr.y - 2, ctr.x + 9, ctr.y - 2, 2.0f);
        g.setColour (GyrdiLookAndFeel::ink.withAlpha (0.7f));
        g.setFont (FontOptions (12.0f));
        g.drawText ("z z", (int) ctr.x + 12, (int) (ctr.y - 30 - 4 * std::sin (time * 2)), 30, 14, Justification::left);
    }
    else
    {
        const float e = 3.0f + 2.0f * famGlow;
        g.fillEllipse (ctr.x - 9, ctr.y - 5, e, e + 1);
        g.fillEllipse (ctr.x + 5, ctr.y - 5, e, e + 1);
        if (fam == 2) // troll tusks
        {
            g.setColour (Colours::ivory);
            g.fillRect (ctr.x - 7, ctr.y + 6, 3.0f, 6.0f);
            g.fillRect (ctr.x + 4, ctr.y + 6, 3.0f, 6.0f);
        }
    }
}

void InstrumentView::drawCrank (Graphics& g, int handler)
{
    const Point<float> knob = wheelCentre + Point<float> (std::cos (crankAngle), std::sin (crankAngle)) * crankLength;
    g.setColour (Colour (0xff2b2b2b));
    g.drawLine (Line<float> (wheelCentre, knob), 7.0f);
    g.setColour (GyrdiLookAndFeel::brass);
    g.fillEllipse (Rectangle<float> (16, 16).withCentre (wheelCentre));
    g.fillEllipse (Rectangle<float> (14, 14).withCentre (knob));

    // who's turning it
    const auto head = knob.translated (-16.0f, -20.0f);
    switch (handler)
    {
        case 0: // ghost
            g.setColour (Colours::white.withAlpha (0.55f + 0.2f * std::sin (time * 2)));
            g.fillEllipse (Rectangle<float> (30, 34).withCentre (head));
            g.setColour (Colours::black.withAlpha (0.6f));
            g.fillEllipse (head.x - 8, head.y - 6, 5, 7); g.fillEllipse (head.x + 3, head.y - 6, 5, 7);
            break;
        case 1: // devil
        {
            g.setColour (Colour (0xffc8262a));
            Path horns;
            horns.addTriangle (head.x - 12, head.y - 6, head.x - 6, head.y - 10, head.x - 14, head.y - 22);
            horns.addTriangle (head.x + 12, head.y - 6, head.x + 6, head.y - 10, head.x + 14, head.y - 22);
            g.fillPath (horns);
            g.fillEllipse (Rectangle<float> (28, 28).withCentre (head));
            g.setColour (Colours::yellow);
            g.fillEllipse (head.x - 7, head.y - 3, 4, 4); g.fillEllipse (head.x + 3, head.y - 3, 4, 4);
            break;
        }
        case 2: // seer
            g.setColour (Colour (0xff6a4fb0));
            g.fillEllipse (Rectangle<float> (28, 28).withCentre (head));
            g.setColour (Colours::white);
            g.fillEllipse (Rectangle<float> (16, 10).withCentre (head));
            g.setColour (Colour (0xff1a1030));
            g.fillEllipse (Rectangle<float> (6, 6).withCentre (head.translated (2.0f * std::sin (time), 0)));
            break;
        default: // witch
        {
            g.setColour (Colour (0xff7fbf6a));
            g.fillEllipse (Rectangle<float> (26, 26).withCentre (head));
            g.setColour (Colour (0xff1c1424));
            Path hat;
            hat.addTriangle (head.x - 18, head.y - 8, head.x + 18, head.y - 8, head.x + 6, head.y - 38);
            g.fillPath (hat);
            g.fillRect (head.x - 20, head.y - 10, 40.0f, 4.0f);
            g.setColour (Colours::black);
            g.fillEllipse (head.x - 6, head.y - 1, 3, 3); g.fillEllipse (head.x + 3, head.y - 1, 3, 3);
            break;
        }
    }
    g.setColour (GyrdiLookAndFeel::ink.withAlpha (0.8f));
    g.setFont (FontOptions (12.0f));
    g.drawText (hg::names::handler[handler], (int) head.x - 30, (int) head.y + 16, 60, 14, Justification::centred);
}

void InstrumentView::drawCaption (Graphics& g, const String& text, float x, float y, Justification j)
{
    g.setColour (GyrdiLookAndFeel::ink.withAlpha (0.75f));
    g.drawText (text, Rectangle<float> (220.0f, 16.0f).withCentre ({ x, y }), j);
}

//==============================================================================
String InstrumentView::hitTestPart (Point<float> p, int& index) const
{
    index = -1;
    const int base = (int) value (hg::ids::keyBase);
    if (p.y >= keyY && p.y <= keyY + keyH && p.x >= keyX0 && p.x < keyX0 + 25 * keyW && lidOpen > 0.5f)
    {
        index = (int) ((p.x - keyX0) / keyW);
        return index == 24 ? "slam key (closes the cover)" : "key " + MidiMessage::getMidiNoteName (base + index, true, true, 3);
    }
    if (pegboxRect.contains (p)) return "pegbox: string material";

    const Point<float> knob = wheelCentre + Point<float> (std::cos (crankAngle), std::sin (crankAngle)) * crankLength;
    if (p.getDistanceFrom (knob.translated (-16.0f, -20.0f)) < 26.0f || p.getDistanceFrom (knob) < 14.0f) return "handler";
    if (p.getDistanceFrom (familiarPos) < 30.0f) return "familiar";

    // cover lid region (as currently drawn)
    const float top = keyboxRect.getY() - 70.0f * lidOpen;
    const float bottom = keyboxRect.getBottom() + 28.0f - (keyboxRect.getHeight() + 28.0f) * lidOpen;
    if (Rectangle<float> (keyboxRect.getX() - 4, jmin (top, bottom), keyboxRect.getWidth() + 8, std::abs (bottom - top) + 4).contains (p))
        return "cover";

    if (std::abs (p.x - wheelCentre.x) < 12.0f && p.y > 380.0f && p.y < 460.0f) return "pads";

    // strings: pick the nearest existing one
    if (p.x > wheelCentre.x + 14 && p.x < stringEndX)
    {
        const int build = (int) value (hg::ids::build);
        float best = 5.0f;
        for (int i = 0; i < 7; ++i)
            if (hg::slotExists (build, i) && std::abs (p.y - stringY[i]) < best) { best = std::abs (p.y - stringY[i]); index = i; }
        if (index >= 0) return hg::names::stringSlot[index] + " string (click to toggle)";
    }

    if (p.getDistanceFrom (wheelCentre) < wheelRadius) return "wheel";
    Path bodyPath; bodyPath.addEllipse (bodyRect);
    if (bodyPath.contains (p) || keyboxRect.expanded (6, 10).withLeft (300).contains (p)) return "body";
    return {};
}

void InstrumentView::mouseMove (const MouseEvent& e)
{
    int idx;
    hoverPart = hitTestPart (toDesign (e.position), idx);
    setMouseCursor (hoverPart.isEmpty() ? MouseCursor::NormalCursor : MouseCursor::PointingHandCursor);
}

void InstrumentView::mouseDown (const MouseEvent& e)
{
    int idx;
    const auto part = hitTestPart (toDesign (e.position), idx);

    if (part.startsWith ("key") || part.startsWith ("slam"))
    {
        clickedNote = (int) value (hg::ids::keyBase) + idx;
        proc.keyboardState.noteOn (1, clickedNote, 0.85f);
    }
    else if (part.endsWith ("(click to toggle)"))  setParam (hg::ids::stringOn[idx], value (hg::ids::stringOn[idx]) > 0.5f ? 0.0f : 1.0f);
    else if (part == "handler")   showChoiceMenu (hg::ids::handler,  hg::names::handler,  "Handler: who turns the crank");
    else if (part == "familiar")  showChoiceMenu (hg::ids::familiar, hg::names::familiar, "Familiar: who lives in the bridge");
    else if (part == "wheel")     showChoiceMenu (hg::ids::wheel,    hg::names::wheel,    "Wheel surface");
    else if (part == "pads")      showChoiceMenu (hg::ids::pads,     hg::names::pads,     "String pads");
    else if (part.startsWith ("pegbox")) showChoiceMenu (hg::ids::strings, hg::names::strings, "String material");
    else if (part == "cover")     showCoverMenu();
    else if (part == "body")      showBodyMenu();
}

void InstrumentView::mouseUp (const MouseEvent&)
{
    if (clickedNote >= 0) { proc.keyboardState.noteOff (1, clickedNote, 0.0f); clickedNote = -1; }
}

void InstrumentView::showChoiceMenu (const char* id, const StringArray& names, const String& title)
{
    PopupMenu m;
    m.addSectionHeader (title);
    const int current = choice (id);
    for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == current);
    SafePointer<InstrumentView> safe (this);
    const std::string idCopy (id);
    m.showMenuAsync (PopupMenu::Options().withMousePosition(), [safe, idCopy] (int r)
    {
        if (safe != nullptr && r > 0) safe->setParam (idCopy.c_str(), (float) (r - 1));
    });
}

void InstrumentView::showBodyMenu()
{
    PopupMenu material, build, decor;
    for (int i = 0; i < 4; ++i)
    {
        material.addItem (100 + i, hg::names::body[i], true, choice (hg::ids::body) == i);
        build.addItem (200 + i, hg::names::build[i], true, choice (hg::ids::build) == i);
        decor.addItem (300 + i, hg::names::decor[i], true, choice (hg::ids::decor) == i);
    }
    PopupMenu m;
    m.addSectionHeader ("Body");
    m.addSubMenu ("Material", material);
    m.addSubMenu ("Build (string count)", build);
    m.addSubMenu ("Decoration", decor);
    SafePointer<InstrumentView> safe (this);
    m.showMenuAsync (PopupMenu::Options().withMousePosition(), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r >= 300)      safe->setParam (hg::ids::decor, (float) (r - 300));
        else if (r >= 200) safe->setParam (hg::ids::build, (float) (r - 200));
        else               safe->setParam (hg::ids::body,  (float) (r - 100));
    });
}

void InstrumentView::showCoverMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Cover");
    m.addItem (1, "Open");
    m.addItem (2, "Half open");
    m.addItem (3, "Closed");
    SafePointer<InstrumentView> safe (this);
    m.showMenuAsync (PopupMenu::Options().withMousePosition(), [safe] (int r)
    {
        if (safe != nullptr && r > 0) safe->setParam (hg::ids::coverOpen, r == 1 ? 1.0f : r == 2 ? 0.5f : 0.0f);
    });
}

//==============================================================================
void PaneToggle::paintButton (Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (GyrdiLookAndFeel::panel.withAlpha (highlighted ? 0.95f : 0.75f).brighter (down ? 0.2f : 0.0f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (GyrdiLookAndFeel::brass.withAlpha (highlighted ? 0.9f : 0.55f));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);

    // open pane: chevron points right ("tuck it away"); closed: points left ("pull it out")
    const auto c = r.getCentre();
    const float d = paneOpen ? 1.0f : -1.0f;
    Path chev;
    chev.startNewSubPath (c.x - 3.0f * d, c.y - 6.0f);
    chev.lineTo (c.x + 3.0f * d, c.y);
    chev.lineTo (c.x - 3.0f * d, c.y + 6.0f);
    g.setColour (GyrdiLookAndFeel::ink);
    g.strokePath (chev, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

//==============================================================================
HyrdiGyrdiEditor::HyrdiGyrdiEditor (HyrdiGyrdiProcessor& p)
    : AudioProcessorEditor (&p), proc (p), instrument (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (instrument);
    using namespace hg::ids;

    sections = {
        { "GRIND",     { addKnob (speed, "Speed"), addKnob (mult, "RPM x"), addKnob (direction, "Direction"), addKnob (handlerAmt, "Handler") } },
        { "CHARACTER", { addKnob (bodyAmt, "Body"), addKnob (wheelAmt, "Wheel"), addKnob (padsAmt, "Pads"), addKnob (exciter, "Exciter") } },
        { "TUNERS",    { addKnob (tunerKey, "Key"), addKnob (droneOct, "Drone Oct"), addKnob (transpose, "Transpose"), addKnob (detune, "Detune"), addKnob (keyBase, "Low Key") } },
        { "MIXER",     { addKnob (mixMel, "Melody"), addKnob (mixFam, "Familiar"), addKnob (mixBass, "Bass"), addKnob (mixSym, "Sympathy") } },
        { "EQ / OUT",  { addKnob (eqLow, "Low"), addKnob (eqMid, "Mid"), addKnob (eqHigh, "High"), addKnob (master, "Master") } },
        { "COVER",     { addKnob (coverOpen, "Open"), addKnob (coverVol, "Sound") } },
    };
    addToggle (hg::ids::sync, "Sync to tempo");
    addToggle (modSpeed, "Mod wheel > speed");
    addToggle (bendDir, "Bend > direction");

    crankMode.addItemList (hg::names::crankMode, 1);
    addAndMakeVisible (crankMode);
    crankModeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, hg::ids::crankMode, crankMode);

    paneToggle.setTooltip ("Show / hide the controls");
    paneToggle.onClick = [this] { setControlsVisible (! controlsVisible); };
    addAndMakeVisible (paneToggle);

    setControlsVisible (proc.apvts.state.getProperty (controlsVisibleId, true));
}

HyrdiGyrdiEditor::~HyrdiGyrdiEditor() { setLookAndFeel (nullptr); }

HyrdiGyrdiEditor::Knob* HyrdiGyrdiEditor::addKnob (const char* paramId, const String& shortName)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 14);
    k->slider.setPopupDisplayEnabled (false, false, nullptr);
    k->slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k->label.setText (shortName, dontSendNotification);
    k->label.setJustificationType (Justification::centred);
    k->label.setFont (FontOptions (12.0f));
    k->label.setColour (Label::textColourId, GyrdiLookAndFeel::dim.brighter (0.4f));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);
    if (auto* prm = proc.apvts.getParameter (paramId)) k->slider.setTooltip (prm->getName (64));
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

HyrdiGyrdiEditor::Toggle* HyrdiGyrdiEditor::addToggle (const char* paramId, const String& text)
{
    auto t = std::make_unique<Toggle>();
    t->button.setButtonText (text);
    addAndMakeVisible (t->button);
    t->attachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramId, t->button);
    toggles.push_back (std::move (t));
    return toggles.back().get();
}

void HyrdiGyrdiEditor::setControlsVisible (bool shouldShow)
{
    controlsVisible = shouldShow;
    proc.apvts.state.setProperty (controlsVisibleId, shouldShow, nullptr);
    paneToggle.paneOpen = shouldShow;
    paneToggle.repaint();

    for (auto& k : knobs) { k->slider.setVisible (shouldShow); k->label.setVisible (shouldShow); }
    for (auto& t : toggles) t->button.setVisible (shouldShow);
    crankMode.setVisible (shouldShow);

    setSize (instrumentWidth + (shouldShow ? controlsWidth : 0), editorHeight);
}

void HyrdiGyrdiEditor::paint (Graphics& g)
{
    g.fillAll (GyrdiLookAndFeel::bg);
    if (! controlsVisible) return;
    auto panel = getLocalBounds().withLeft (instrument.getRight()).toFloat();
    g.setColour (GyrdiLookAndFeel::panel);
    g.fillRect (panel);
    g.setColour (GyrdiLookAndFeel::brass.withAlpha (0.3f));
    g.drawVerticalLine (instrument.getRight(), 0.0f, (float) getHeight());

    // section headers
    g.setFont (FontOptions (12.0f, Font::bold));
    for (auto& s : sections)
        if (! s.knobs.empty())
        {
            const auto b = s.knobs.front()->label.getBounds();
            g.setColour (GyrdiLookAndFeel::brass);
            g.drawText (s.title, instrument.getRight() + 14, b.getY() - 18, 200, 16, Justification::left);
            g.setColour (GyrdiLookAndFeel::brass.withAlpha (0.2f));
            g.drawHorizontalLine (b.getY() - 10, (float) instrument.getRight() + 90, (float) getWidth() - 14);
        }
}

void HyrdiGyrdiEditor::resized()
{
    instrument.setBounds (0, 0, instrumentWidth, getHeight());
    paneToggle.setBounds (instrumentWidth - 38, 12, 26, 26);
    if (! controlsVisible) return;
    const int x0 = instrument.getRight() + 10, cellW = 80, knobH = 58, labelH = 14;
    int y = 28;
    for (size_t si = 0; si < sections.size(); ++si)
    {
        auto& s = sections[si];
        for (size_t i = 0; i < s.knobs.size(); ++i)
        {
            const int x = x0 + (int) i * cellW;
            s.knobs[i]->label.setBounds (x, y, cellW, labelH);
            s.knobs[i]->slider.setBounds (x, y + labelH, cellW, knobH);
        }
        y += labelH + knobH + 30;

        if (si == 0) // crank options sit under the grind knobs
        {
            const int bw = 128;
            for (size_t t = 0; t < toggles.size(); ++t)
                toggles[t]->button.setBounds (x0 + (int) t * (bw + 6), y - 22, bw, 24);
            crankMode.setBounds (x0, y + 6, 3 * bw + 12, 24);
            y += 56;
        }
    }
}
