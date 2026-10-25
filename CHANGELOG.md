# Changelog

Each version's notes appear on its GitHub Release page.

## 0.34.0

- **MOD can go before the amp:** a new **POSITION** switch, like GLITCH's.
  - **POST** (as before): after the amp and cab, a studio-style effect on the finished tone.
  - **PRE:** after the drive and into the amp, where a chorus pedal sits on a pedalboard. The amp works on the modulated sound, so the warble, the swirl and the chorus's treble all come through stronger, as with a real pedal into an amp.
  - PRE is mono, because one amp follows it, as with a single pedal into a single amp. STEREO only applies in POST.
- Vortex's OUTPUT choice is now a **STEREO** on/off switch: off is mono, on is the pedal's two outputs. Switching saves room on the panel.

## 0.33.1

- **Vortex is stronger**, closer to the real pedal turned up:
  - **DEPTH** sweeps the delay wider: about 3.9 to 13.3 ms at full, up from 3.4 to 9.1 ms. That roughly doubles the pitch bend at every RATE: at 0.8 Hz, about ±90 cents at full DEPTH, up from ±40.
  - **MIX** (E.LEVEL) now goes past an even mix, as the pedal's knob does. Full up, the chorus is about 3.5 dB louder than the guitar, for a warbling, almost vibrato sound. It matches the guitar at about two thirds.

## 0.33.0

- **Vortex replaces the old Chorus**, the first of the modulation upgrades to model specific pedals. It's modelled on a classic late-'80s compact stereo chorus pedal, built from its schematic:
  - **Bucket-brigade delay:** the delay sweeps the way a delay chip's does when its clock is swept, further on the long side than the short. The LFO is a smooth triangle wave.
  - **Pre-emphasis and de-emphasis:** treble is lifted about 15 dB before the delay and cut back after, which gives the clear, glassy sound. A very loud, bright signal rounds off in that stage, as the original's manual warns.
  - **Filters either side of the delay** roll the chorus's top end off above about 9 kHz.
- **MIX now works as the pedal's E.LEVEL:** the guitar stays at full level and the chorus is added on top. Before, MIX faded the guitar down as the chorus came up.
- **New controls, shown only for Vortex:**
  - **EQ:** the pedal's tone knob, about ±10 dB of treble on the chorus sound only.
  - **OUTPUT:**
    - **MONO**, the pedal's main output alone: guitar plus chorus on both sides.
    - **STEREO**, its two outputs: chorus alone on the left, guitar alone on the right, so the chorus happens between the speakers. As on the pedal, turning MIX down moves the sound towards the dry side.
- Rigs and projects that used Chorus now use Vortex, in MONO. They'll be a little fuller, since the guitar no longer drops as MIX goes up.
- Knob values in narrow cards (e.g. "0.80 Hz") squeeze to fit instead of being cut off with "…".

## 0.32.1

- **The small EXPRESSION screen beside the pedal is gone.** The bigger one in the dock's EXPRESSION ASSIGN panel does the same job.
- **The AMP, CABINET and RIG panels** now share their row evenly, filling the space it left. The keys inside them are the same size as before.

## 0.32.0

- **EXPRESSION ASSIGN is a screen.** The dock's list of tiny buttons is replaced by the same orange LCD as the EXPRESSION screen beside the pedal, enlarged to fill the panel.
  - Its lettering is bigger than the small screen's, about 1.4 times.
  - **▲ / ▼ buttons** beside it scroll the list, dimming when there's no more that way. Click a line to assign the pedal, or click it again to unassign; the mouse wheel steps through the list.
  - MIN, the range slider and MAX now share one row underneath, to give the screen more height.
  - The small screen beside the pedal stays.
- **Opening size fits above the taskbar:** the window now sizes itself to the screen *above* the taskbar, leaving room for the DAW's title bar and plugin toolbar. On a 1080p screen it opens at about 1706 × 959, so the bottom isn't cut off. On a larger screen it still opens at 1824 × 1026.

## 0.31.1

- **Opens bigger:** the window now opens at **1824 × 1026** (it was 1280 × 720). On a screen too small for that, it opens at the largest size that fits. You can still drag it to any size.
- **Expression pedal on the neon finishes:** tipped heel or toe down (at 0% or 100%), the pedal no longer looks washed out and grey. The end tipping away now just darkens, keeping its colour, and the raised end isn't lightened. Pearl White's pedal is unchanged.

## 0.31.0

- **New finish: Pearl White + Neon.** This is the pearl white unit with Pearl Black's neon keys. The footswitches, AMP / CABINET / RIG keys, small keys, RIG LEVEL and PRESENCE knobs, and the expression pedal's grip plates all run pink through red to orange.
  - Choose it in **SETTINGS → FINISH → WHITE + NEON**.
  - Over its neon keys, the six button names (UNDO, REDO, BYPASS, TUNER, MUTE, TAP) are printed in dark ink, since red on red was hard to read.
- Pearl White and Pearl Black are unchanged.

## 0.30.1

- **Pearl Black:**
  - The neon keys and knobs are 25% darker.
  - The expression pedal's two ribbed grip plates now have the same pink-to-orange neon.
  - The six button names (UNDO, REDO, BYPASS, TUNER, MUTE, TAP) are white, so they read clearly above the neon keys. HOLD TO TUNER stays red.
- Pearl White is unchanged.

## 0.30.0

- **Pearl Black now has neon keys and knobs** instead of white ones. The footswitches, the AMP / CABINET / RIG keys, the small keys (SAVE, UNDO, TUNER...) and the RIG LEVEL and PRESENCE knobs run hot pink through red to orange, left to right.
  - The colours are measured off a reference swatch: `#e10639` → `#d42e2a` → `#cd6619`.
  - They're shaded like the swatch too: lit along the top and falling into deep red at the front, so they still look like glossy keys.
  - On the small keys, an unlit LED stripe shows as a dark slot, so it doesn't vanish red-on-red. Lit stripes are the same neon as before.
- Pearl White is unchanged.

## 0.29.1

- **Pearl Black is much deeper black.** It read as navy, so:
  - The chassis, pedal bay and treadle are now true deep black, about half as bright as before. Only the top edges catch the light.
  - The cool violet and teal shimmer is gone. What's left is a faint warm rose, so it sits with the red neon.
  - The white keys, lettering and neon stand out more strongly against it.
- Pearl White is unchanged.

## 0.29.0

- **Pearl Black finish.** SETTINGS has a new **FINISH** choice: the original **Pearl White**, or **Pearl Black**. Pearl Black is the same unit in black pearl, with the same satin grain, sheen and pearlescent shimmer, plus white keys and knobs:
  - The chassis, the expression pedal's plate and the pedal bay are black pearl. The lettering printed on the unit turns pearl white, and the red printing stays red.
  - The footswitches, the AMP / CABINET / RIG keys, the small keys (SAVE, UNDO, TUNER...) and the RIG LEVEL and PRESENCE knobs are gloss white.
  - The screens, the neon, the chrome bezels and the panels below the unit don't change.
  - The choice is saved on your computer, not in the project, so every Paragon PR7-G you open uses it. Every open window switches together.
- Pearl White looks exactly as before.

## 0.28.0

- **Talk Box.** A new FILTER / PITCH type. A real talk box sends the amp's sound up a tube into your mouth, which shapes it into vowels. This does the same with three resonant formants, the peaks a voice makes for each vowel.
  - The expression pedal (its new **TALK BOX** setting) glides the "mouth" through **oo, oh, ah, eh, ee**, heel to toe.
  - Like the real thing, it works on the amp's sound: it runs after the amp and cab, where the drive gives it plenty of harmonics to shape.
  - **RESONANCE** sets how sharp and vocal it is, and **MIX** sets the blend with the plain guitar.
  - Choosing TALK BOX on the EXPRESSION screen switches FILTER / PITCH on as a Talk Box. If the pedal isn't assigned to it, it rests at "oo".
- **The EXPRESSION screen scrolls.** With eight pedal settings, it shows six lines at the readable size, with arrows in its heading: lit when there's more that way, and clickable to scroll. Choosing a setting, by clicking or with the mouse wheel, keeps it on screen.
- **New song rigs in bank E:**
  - **E7 Jambi Talk Box (Tool):** the talk box on a tight, heavy tone. Load a high-gain amp capture.
  - **E8 Show Me the Way (Frampton):** the talk box on a warm, lightly driven tone.

## 0.27.0

- **New PITCH block**, first in the signal chain, before anything else shapes the guitar:
  - **TUNING:** virtual retuning, -12 to +12 semitones. The knob names the result: D standard (-2), B (-5), A (-7)... or Capo 1-12 going up. It shifts every string together (like tuning the whole guitar down), so it can't drop one string alone the way drop D does. At E standard the guitar passes untouched.
  - **OCTAVER:** a POG-style octave pedal. Levels for two octaves down, one octave down, the dry guitar and one octave up, so you can blend organ-like stacks. Chords track too, with a little shimmer on dense ones.
  - **FREEZE:** switch it on and what you just played is caught and held as an endless, smooth pad while you play over it. LEVEL sets how loud it is, ATTACK how it swells in, RELEASE how it fades when switched off.
- The PITCH tile in the signal chain lights when the octaver or freeze is on. Double-click it to switch them off, or the octaver on.
- With twelve blocks in the chain, the modulation tile now reads MOD.
- Times of a second or more show in seconds (1.5 s).

## 0.26.1

- **Chassis bottom edge fixed.** The face plate came down past the rim's polished top and overhung its front edge, so the inner border didn't line up along the bottom. Now:
  - The rim shows below the face plate about as much as above it.
  - The rim's edge lines up with the bottom of the expression pedal's housing.
  - The unit's front edge is a little slimmer.
  - The footswitches are about 3 pixels shorter to make room, too little to notice.

## 0.26.0

- **Two fuzz models** in OD / DISTORTION:
  - **Muff Fuzz** follows the Big Muff's circuit: two clipping stages one after another, for long, violin-like sustain (DRIVE is its sustain). Its tone stack scoops the mids at noon, from warm and woolly to bright. This is David Gilmour's lead sound.
  - **Face Fuzz** follows the silicon Fuzz Face: one high-gain stage that clips one half of the wave harder than the other, for a raspy, round fuzz. Turn your guitar's volume down and it cleans up. It also darkens the guitar slightly, as the real pedal's input does. The original has no tone knob, so TONE here is a gentle treble cut.
  - Both sit near your guitar's own level at LEVEL 0 dB.
- **Pink Floyd rigs in bank E**, built from David Gilmour's documented gear:
  - **E3 Numb Solo (Floyd):** the "Comfortably Numb" solos. Muff Fuzz (sustain 7, tone 3.5) into a 370 ms delay, through a subtly mixed rotating speaker, with hall reverb.
  - **E4 Us and Them (Floyd):** the neck pickup, clean, through a slow rotating speaker with tape echo. For the louder chorus, stomp footswitch 2: a Face Fuzz is set up and waiting.
  - **E5 Run Like Hell (Floyd):** muted eighths into a 380 ms delay with five or six repeats (the gallop), with flanger and light compression.
  - **E6 Money Solo (Floyd):** the bridge pickup into the Face Fuzz, with a multi-tap echo in the spirit of his Binson Echorec.
  - These are our settings for the sound. The rest is in your hands: Gilmour's tech says his tone is mostly his fingers.

## 0.25.0

- **Bank E: song-inspired rigs.** A fifth bank for rigs built around particular songs' sounds. These are our settings for the sound, not the artists' own rigs. The first two:
  - **E1 Killing Solo (RATM):** the "Killing in the Name" solo squeal. A Whammy an octave up on the expression pedal (rock to the toe), into a cranked crunch with no reverb. For an even higher squeal, set PROGRAM to 14 Up 2 Octaves.
  - **E2 Bulls Scratch (RATM):** the "Bulls on Parade" scratch. A wah on the pedal (rock it as you rub muted strings over the pickup), and GLITCH's new Killswitch chopping in 1/16s, on the song's grid when the DAW plays.
- **GLITCH: new Killswitch mode.** A guitar's toggle switch flicked in time, as Tom Morello and Buckethead do. Each step plays, then cuts to silence for its last part. DENSITY sets how much is cut (a tenth to nine tenths of the step), and CHAOS leaves some steps uncut. With SYNC on and the DAW playing, it locks to the song's grid like Sequence and Interfere.
- **Sharper GLITCH edges.** The fade into and out of a stutter, dropout or cut is now a straight 1.5 ms ramp, so cuts are completely silent and stutter edges are crisp. Before, it eased out over about 15 ms.
- **Banks as keys.** The dock's rig browser shows the banks as a row of letter keys (A B C D E), so they all fit. Before, bank D was cut off.

## 0.24.1

- **GLITCH locks to the song's grid.** With SYNC on and your DAW playing, **Sequence** and **Interfere** start each step exactly on the song's grid lines: every 1/16 on a 16th note, every 1-bar step on the downbeat.
  - Stutters now lock to the drums, and fall in the same places every time you play the song back.
  - If the DAW loops or you jump to another spot, the steps follow the new position.
  - When the transport is stopped, or the tempo is tapped (which doesn't match the DAW's grid), the steps count from when GLITCH switches on, as before. Pitch Delay and Texturizer aren't grid-based, so they're unchanged.

## 0.24.0

- **GLITCH**, a new block for granular glitch, stutter and texture effects. It keeps the last few seconds of your playing and plays pieces of it back. Four MODEs:
  - **Pitch Delay:** echoes where each repeat is shifted by PITCH from the last, so they climb (or fall) away.
  - **Sequence:** tempo-locked stutters. At each step it may grab the slice you just played and repeat it instead, like a beat-repeat edit.
  - **Interfere:** a failing signal. Your playing is cut into segments, some dropped out, played backwards, or caught in a short buzzing loop.
  - **Texturizer:** a cloud of overlapping grains from the last moments of playing, smeared into a pad. Lovely on cleans.
- **Controls:**
  - **LENGTH:** the size of the pieces. With SYNC on it's a note value from 1/64 to a bar, at the tapped or host tempo; otherwise milliseconds.
  - **DENSITY:** how many repeats, stutters, breaks or grains.
  - **PITCH:** ±12 semitones.
  - **CHAOS:** the macro. At 0 everything is regular and rhythmic. Turn it up and lengths, pitches (including octave jumps), directions and positions are randomised together.
  - **MIX.**
- **POSITION:** **Post** (the default) puts it after the amp and cab, before modulation, so the chops are exact. **Pre** puts it before the amp, so the chops hit the drive for a wilder, sputtering sound.
- **New expression setting: GLITCH.** The pedal fades GLITCH in: dry at the heel, its MIX at the toe. Choosing it switches GLITCH on. The EXPRESSION screen now has seven lines.
- **New factory rigs:** **Stutter Edit** (C10: 1/16 stutters on a crunch, on the pedal) and **Grain Cloud** (D10: a Texturizer pad on a clean tone).
- The signal chain has eleven blocks now. GLITCH sits after EQ, where it runs in Post.
- This is our own design, built from Neural DSP's description of the Glitch pedal in *Archetype: Misha*. It isn't a copy of their processing.

## 0.23.0

- **Daybreak compressor.** COMP / GATE has a TYPE: **Classic** (the original, still the default, so your rigs sound the same) or **Daybreak**, a modern pedal compressor laid out like the boutique ones metal players use:
  - **COMP:** how much it squeezes, in one knob. Its make-up gain turns up with it, so more COMP means more squeeze rather than just quieter.
  - **DETAIL:** how much pick attack gets through before it clamps. Low: the pick is flattened for a smooth, even sound. High: every pick snaps through.
  - **MIX:** parallel compression. Blend the squeezed sound with your dry guitar for sustain and evenness with a natural attack.
  - **INTENSITY:** off is smooth and gentle, for cleans and leads. On grabs earlier and harder and lets go faster, for tight modern rhythm.
  - **LEVEL:** the volume after it.
  - Its detector ignores the lowest bass, so low palm mutes don't make it pump.
- **Gain-reduction meter.** The COMP / GATE panel's header has a row of LEDs and a dB readout showing how hard the compressor is working. It works for both types.
- The panel shows only the knobs for the chosen TYPE. The others hide and the cards close up.
- **New factory rig: Daybreak Clean (A5).** Daybreak at half mix into a light chorus and plate reverb.

## 0.22.1

- **New factory rig: Nu-Metal Screech (B10).** The late-'90s octave screech.
  - **The sound:** the Whammy-Wah on **21 Down Oct / Up Oct** (an octave below and an octave above, blended with your guitar at 85%), already on the expression pedal.
  - **Into:** a tight Screamer boost (low gain, high level) with the noise gate on, the graphic EQ's **Tight Low End** curve so the octave-down voice stays clear under high gain, and a touch of room reverb.
  - **How to play it:** load a Rectifier-style high-gain amp capture, hold a high note or a pinch harmonic, and rock the pedal back and forth. Fast rocking warbles and screeches; a slow sweep gives the eerie rise.

## 0.22.0

- **Whammy-Wah now follows the real XP-100's program list.** MODE is now **PROGRAM**, numbered 01–28 as on the pedal, with what the pedal does in each:
  - **01 Volume Pedal:** the pedal is a volume pedal.
  - **02–06 Wah:** Original Cry Wah, Cry Wah 2, Dark & Deep, Sweet Spot 1, Sweet Spot 2. The pedal sweeps the wah. The Sweet Spots cover just the middle of a wah's travel, finely, for a "parked" wah you can nudge.
  - **07–12 Auto Wah** (new): Original Cry Auto, Envelope Filter and Brass Attack, each twice:
    - In 07–09 the pedal sets the sensitivity: heel barely opens, toe opens on the lightest touch.
    - In 10–12 it sets the attack: heel snaps open, toe swells in like a brass note.
  - **13–20 Whammy:** Up 1 Octave, Up 2 Octaves, Up 2nd, Down 2nd, Down 4th, Down 1 Octave, Down 2 Octaves and Down 6 Octaves. Heel is no shift, toe is the full interval. **Down 6 Octaves** really does dive six octaves, down to a rumble.
  - **21–28 Harmony:** Down Oct/Up Oct, Down 4th/Down 3rd, Up 5th/Up 6th, Up 4th/Up 5th, Up minor 3rd/Up major 3rd, Up 2nd/Up 3rd, Down 4th/Up 5th, Up 5th/Up Oct. Your note plus a voice the pedal moves from the first interval at the heel to the second at the toe.
  - The list and what the pedal controls come from the XP-100's program chart. The wah ranges and auto-wah speeds are ours, since DigiTech didn't publish theirs.
- A rig saved with 0.21.0's Whammy-Wah loads with program 02 (Original Cry Wah). The old modes didn't match the real list, so they aren't carried over. The factory rigs are updated: **Whammy Lead** uses 13 Up 1 Octave, **XP Wah Crunch** uses 02 Original Cry Wah, and **Harmony Bends** uses 24 Up 4th/Up 5th.
- Drop-down lists with long entries get a wider card, so names like "22 Down 4th / Down 3rd" read in full.

## 0.21.0

- **Whammy-Wah.** A new FILTER / PITCH type in the style of the DigiTech XP-100 Whammy-Wah, played with the expression pedal. Its MODE picks one of the XP-100's three kinds of effect:
  - **Wah (5):** Classic, Vocal (narrow and talky), Wide (a long sweep), Deep (dark and low) and Bright. The pedal sweeps the wah from heel to toe.
  - **Whammy (8):** 2 Oct Up, Oct Up, 5th Down, 4th Down, Oct Down, 2 Oct Down, Dive Bomb and Drop Tune (a whole step down). The pedal bends the pitch from none at the heel to the full interval at the toe, and only the bent note is heard. Dive Bomb hangs on and then plunges, like a whammy bar. It goes down two octaves, the pitch shifter's limit.
  - **Harmony (9):** 2nd/3rd Up, b3rd/3rd Up, 3rd/4th Up, 4th/5th Up, 5th/6th Up, 5th/Oct Up, 3rd/4th Down, 4th/5th Down and Oct Down/Up. Your note plus a second voice, which the pedal moves from the first interval at the heel to the second at the toe. MIX sets how loud the harmony is.
  - These follow the XP-100's layout and the classic Whammy settings. They're not a copy of DigiTech's processing.
- **New expression setting: WHAMMY-WAH**, on the EXPRESSION screen and in EXPRESSION ASSIGN. Choosing it switches the FILTER / PITCH block on as a Whammy-Wah. If the pedal isn't assigned to it, the Whammy-Wah rests at the heel.
- **New factory rigs:** **Whammy Lead** (B7, octave-up whammy), **XP Wah Crunch** (B8, the Classic wah into a crunch drive) and **Harmony Bends** (B9, a third rising to a fourth).
- The EXPRESSION screen fits six lines now.

## 0.20.0

- **EQ presets.** The graphic EQ has a **PRESETS** menu next to FLAT with twelve starting curves. These are the shapes guitarists most often dial into an octave-band graphic EQ, like an MXR 10-band or the graphic EQ in an amp sim, after the amp and cab:
  - **Metal Scoop:** the classic V, big lows and highs, mids pulled down.
  - **Tight Low End:** cuts the boom under high gain so palm mutes stay tight.
  - **Mud Cut:** clears the boxy low mids around 250–500 Hz.
  - **Fizz Tamer:** rolls off the harsh top that high-gain captures can have.
  - **Lead Mid Boost:** pushes 500 Hz–2 kHz so solos sit on top.
  - **Cut Through the Mix:** less low end, more upper mids, to be heard in a dense band.
  - **Fat Rhythm:** thick lows and low mids for big rhythm parts.
  - **British Crunch:** a forward upper-mid bark.
  - **Bright Clean:** sparkle and air for cleans and chorus.
  - **Funk Scoop:** scooped low mids with a crisp top.
  - **Warm Jazz:** round and dark, the highs rolled off.
  - **Radio:** a narrow mid band, like an old AM radio.
- Choosing one sets all nine faders and LEVEL and switches the EQ on, so you hear it straight away. Then tweak to taste.
- Each preset's LEVEL is set to cancel the volume change of its curve (measured across a guitar's range), so switching between them compares tone, not loudness.
- The menu shows the preset's name while the faders match it. Move a fader and it goes back to PRESETS, because it's now your own curve. Presets are saved with rigs like any other EQ setting.

## 0.19.0

- **New name: Paragon PR7-G**, the guitar processor, ready for more processors in the Paragon line. The logo on the unit and in the header now reads PR7-G, in the same slanted, speed-cut lettering.
  - **When you update:** delete the old `Paragon PR7.vst3` from your VST3 folder and copy in `Paragon PR7-G.vst3`. It's still the same plugin to your DAW, so your projects open with it as before. Keep only one, though: two copies of the same plugin can confuse the DAW.
  - Your rigs come along: the first time it runs, Paragon PR7-G copies them from the Paragon PR7 folder (or from Paragon X1, if that's where they are) into its own folder. The originals are left where they were.
- **AMP, CABINET and RIG keys** no longer have icons on them, so they match the four footswitches below them. The title and LED above each key and the caption below still say what it is and whether something is loaded.

## 0.18.0

- **EXPRESSION screen.** The expression pedal's assign buttons on the unit are now a small LCD screen: a little sibling of the main display, with the same chrome frame, black bezel and orange-lit glass.
  - "EXPRESSION" is shown on the screen itself, above a menu of the five targets: VOLUME, WAH / FILTER, MORPH / FX, PITCH / WHAMMY and LO-FI.
  - The chosen target is highlighted as a solid orange bar and the others dim. With nothing assigned, all five show normally.
  - Along the bottom, a segmented bar and a percentage show where the pedal is, live as you rock it. It stays dark while the pedal isn't assigned.
  - Click a line to assign the pedal, and click the highlighted line again to unassign it. Or scroll over the screen to step through the list.
  - It's the same height as the AMP, CABINET and RIG windows and lines up with them as a fourth window in the row.
  - EXPRESSION ASSIGN in the dock works as before.

## 0.17.2

- **Pearl white expression pedal.** The treadle now matches the chassis: the same bright pearl white satin finish with its fine speckle, mica flecks and rose-peach-lilac shimmer, in place of brushed chrome.
  - Its edges and visible thickness are pearl too, shaded rose rather than grey.
  - As the pedal rocks, the end tipping away darkens to a warm rose instead of grey.
  - The two ribbed rubber pads stay black, as in the mockup.

## 0.17.1

- **Pearl white chassis.** The unit's body is now a bright pearl white instead of grey, to go with the neon red, orange and pink.
  - The brushed-aluminium lines are gone. The finish is smooth satin, like the original mockup, with a very fine speckle and a few mica-like flecks so it still looks like a real painted surface up close.
  - A faint pearlescent shift runs across the face plate: rose at the left, peach in the middle, lilac at the right.
  - The bevelled rim, recessed panels, edge highlights, screws and shadows are all as sharp as before. Shading is rose-tinted rather than grey, so the corners stay white.
  - This changes the chassis only (rim, face plate, panels and the expression pedal's housing). The display, buttons, footswitches, knobs and expression pedal are unchanged.

## 0.17.0

- **DUCK knob on DELAY and REVERB.** It turns the repeats or the reverb down while you play and lets them swell back in the gaps, so big delays and spaces don't blur your notes.
  - At 0% (the default) it's off, so nothing you've saved changes.
  - At 100% the effect drops by up to 24 dB under your playing and comes back within about a fifth of a second after you stop.
  - It works with every type: a ducked Tape delay, a ducked Cathedral, and so on.
- The **Ducking** delay type is now simply Digital with DUCK set high, so rigs that used it sound the same. Turning DUCK past its preset ducks it further still.
- **Digital is the default reverb for new rigs:** the bright, lush 80s arena-rock reverb. Digital is also the default delay. Projects and rigs saved before reverb types existed still load with Classic.
- On the delay's crowded panel, the drop-down lists give up a little width so the knobs stay a good size, and long choices like "1/8 triplet" squeeze to fit instead of being cut off.

## 0.16.0

**Eight delay types.** The DELAY block has a TYPE setting. Every type keeps tempo SYNC, FEEDBACK, TONE, MIX, PING-PONG and trails (repeats ring out when you switch the delay off).

- **Digital:** clean, exact repeats; the original Paragon delay. It's the default, so your existing projects and rigs sound exactly the same.
- **Analog:** a bucket-brigade pedal, like a Boss DM-2 or MXR Carbon Copy. Warm, dark repeats that soften and blur each time round, with a slight wobble. Turn FEEDBACK right up and they build and sing rather than just fade. TONE sweeps across the chip's narrower range.
- **Tape:** an Echoplex or Space Echo. Wow, flutter and a slow random drift in the pitch, tape saturation that squashes loud repeats, a little hiss, and repeats that warm as they fade.
- **Mod:** clean repeats with chorus on them, wider in stereo. The lush lead delay.
- **Reverse:** each repeat plays backwards, the psychedelic classic. Each backwards chunk is one delay time long.
- **Multi-Tap:** four repeats spread through each delay time: at a quarter, a half, three quarters and the full time, the last the loudest. They dance between the speakers, for galloping, rhythmic patterns.
- **Shimmer:** each repeat is pitched an octave higher than the last, rising into a glowing wash. Great for ambient pads with a long reverb.
- **Ducking:** repeats drop by up to 18 dB while you play and swell back in the gaps, so you can run big delays on leads without clutter.

**Pitch shifter:** Shimmer uses the same in-phase, correlation-spliced pitch shifter as the FILTER / PITCH block, now shared between the two.

**Factory rigs:** they get fitting delays:
- Tape on Slapback and Cassette Dreams.
- Analog on Lead Boost.
- Shimmer on Ambient Swells.
- Ducking on Octave Lead.
- Mod on Dimension Clean.
- Multi-Tap on Wide Pan.

There are also two new rigs: **Shimmer Pad** (bank D) and **Backwards Echo** (bank C).

**Panel labels:** long control names like PING-PONG now squeeze to fit their card instead of being cut off.

## 0.15.0

**Eight reverb types, and PRE-DELAY.**

The REVERB block has a TYPE setting and a new **PRE-DELAY** knob (0 to 250 ms: the gap before the reverb starts, which keeps your notes clear in front of a big space).

- **Room:** a small room. Strong early reflections (the first echoes off nearby walls) and a short tail: space without wash.
- **Hall:** a concert hall. A smooth, long, warm tail.
- **Cathedral:** a huge stone space. A very long, darker tail.
- **Plate:** the classic studio plate. No early reflections; dense and bright from the first moment.
- **Spring:** an amp's spring tank. Each echo arrives as a falling "drip", band-limited like a real tank, with the boing when you hit it hard.
- **Digital:** an 80s rack reverb. Clean and bright, with a lush, chorused tail.
- **Reverse:** the reverb played backwards, so it swells up into each note. SIZE sets how long each swell is.
- **Classic:** the original Paragon reverb.

**How it's built:** Room, Hall, Cathedral, Plate, Digital and Reverse use a new engine, a feedback delay network: eight delay lines feeding back into each other through a mixing matrix, the design behind most good studio reverbs. It builds a dense, smooth tail without the metallic ringing of the original. Each type has its own room size, early reflections, diffusion, damping and gentle tail modulation. SIZE sets how long the tail lasts, from about a third of a second (a small room) to 12 seconds (a cathedral).

**Your existing sounds:** projects and rigs saved before this version load with **Classic**, so they sound exactly as they did. New rigs start on Hall.

**Factory rigs:** the factory rigs now use fitting types:
- Spring on Surf Tremolo, Slapback, Brownface Trem, Old Radio and Warble Vibrato.
- Plate on the studio cleans and Lead Boost.
- Cathedral on Ambient Swells and Wide Pan.
- Room on Edge of Breakup and others.

There's also a new **Reverse Swell** rig (bank C).

## 0.14.1

- The knob on the unit that was labelled **OUTPUT LEVEL** is now **RIG LEVEL**, so it isn't confused with the master **OUTPUT** knob on the right. It works as before: it's each rig's own volume, saved with the rig for balancing rigs against each other, and it adds to OUTPUT, the overall volume that stays put when you switch rigs. Hover over it for a reminder.

## 0.14.0

- **The graphic EQ is now the dock's main view.** The TAGS / RIG SUMMARY panel is gone: the middle of the dock shows the graphic EQ until you pick another block in the signal chain or click a footswitch's name. A **GRAPHIC EQ** button on the other blocks' panels takes you back. You can still edit tags on the PRESETS page.
- **AMP / CABINET / RIG polished to match the footswitches:**
  - Each key stands in a raised chrome bezel with a dark well round it.
  - Their status LEDs (and EXPRESSION ASSIGN's and MIN's) sit in chrome-rimmed lenses.
  - The titles, captions and ◀ ▶ arrows look stamped into the metal.

## 0.13.2

- **Footswitches polished to match the unit:**
  - Each pad now stands in a raised chrome bezel, with a dark well round the key and a shadow on the panel, the way a real floor unit's footswitches are mounted.
  - The status LEDs sit behind recessed lenses with chrome rims.
  - The numbers and names look stamped into the metal.
- **The expression pedal starts heel down (0%),** where a real pedal rests, in new projects and new instances of the plugin. Each target starts at its "off" end: MORPH is dry, WAH is at the heel, PITCH is unshifted and LO-FI is full band. Assigned to **VOLUME**, it's silent until you rock it forward, just like a real volume pedal. The pedal's position is saved with your projects as before, so existing projects open where you left it.

## 0.13.1

**Knobs, display and buttons polished to match the chassis**, and a fix to the unit's left edge.

- **Left edge:** the bright line along the top of the chassis rim stopped abruptly halfway down its sides. It now fades out smoothly down the sides, as light catching a curved edge does. The pedal bay's edge gets the same fix.
- **OUTPUT LEVEL and PRESENCE knobs:**
  - A knurled grip: fine ridges round the cylinder, closer together where the sides turn away.
  - A chrome collar where each knob meets the face, like the nut and washer a real pot is mounted with.
  - A crisp lit chamfer on the top's back edge.
  - A brighter, sharper pointer.
- **Display:**
  - A polished chrome frame round the black bezel.
  - Glass in front of the readout: faint pixel rows, the bezel's shadow along the top, and diagonal reflections across it.
- **Small buttons** (UNDO, REDO, BYPASS, TUNER, MUTE, TAP, SAVE, SAVE AS): each key sits in a recessed chrome surround, and its top edge catches the light more crisply.

## 0.13.0

**A crisper, more polished unit.** The chassis looked flat and washed out: one pale shade, a faint texture, soft edges, and panels that barely stood out. It's redrawn for depth and polish:

- **Real brushed metal:** a fine grain across the face, rim, panels and pedal bay: every row slightly different, with longer streaks and a light speckle. It's built at twice the resolution, so it stays fine on high-resolution screens.
- **More tonal range:** the face is bright under the light at the top, with a darker reflection band through the middle. It has a sharp diagonal sheen across the upper left and darkens towards its edges, as polished metal does. The chassis rim runs from near-white highlights to deep shadow.
- **Crisp machined edges:** a bright line along the rim's top, the red glow reflected along its bottom, dark outlines, and a bright lip where the face steps down from the rim.
- **Recessed panels:** the slot row and footswitch row sit down in the face, with a shadow under the face's edge along their top and a lit lip along their bottom. The panels behind the slot windows are more deeply engraved.
- **Screws:** recessed hex-head screws in the face's four corners.
- **Less pink wash:** the red glow now catches only the face's bottom edge instead of tinting its whole lower half.

## 0.12.5

- **Expression pedal corners:** the treadle's body under its face was drawn as a square box, so its corners poked out past the face's rounded corners as little points. The body now follows the face's rounded corners all the way down.
- The PR7 mark is no longer on the expression pedal: the treadle is plain brushed chrome at the heel. PR7 stays on the unit's badge and in the header.

## 0.12.4

- **A new expression pedal, drawn square and true.** The mockup's pedal artwork was drawn at a slight angle, so its right edge leaned. Stretched to cover the base and rocked in 3D, that showed up as a crooked treadle, a warped right side and odd bands at the ends. The pedal is now drawn in vector graphics, with no picture to bend:
  - A brushed chrome treadle in the unit's rose-tinted chrome.
  - The mockup's two black rubber pads split by a diagonal chrome band, now ribbed across like the DigiTech RP6's.
  - The PR7 mark engraved at the heel.
  - A dark base with a chrome lip.
- The treadle covers the base and stays sharp at any window size. As it rocks, only the base shows round the end that drops.
- The position readout you see while hovering now sits in the middle of the treadle, clear of the PR7 mark.

## 0.12.3

- **Expression pedal covers its base:** the treadle is now as big as the base it sits on, as on the DigiTech RP6, rather than sitting inside it with gaps round it. The mockup's treadle artwork was narrower than the base, so it's widened to the base's shape (drawn at twice the size, so it stays sharp), with the PR7 mark re-engraved at its own proportions. The base shows only where the treadle rocks away from it, and the treadle stays within the base's footprint, so it never spills over the unit.

## 0.12.2

- **Expression pedal sits in the unit:** the treadle now sits down in a well in the housing, as on a real floor unit like the DigiTech RP6, rather than being drawn on top of it.
  - It's set slightly inside the opening, with a gap all round.
  - It's drawn only within the well, with a chrome rim and the rim's shadow on top.
  - As it rocks, its ends tuck under the rim instead of sliding over the side rails or above the unit.
  - The rocking shows as the raised end catching the light and the lowered end dropping away into shadow, with the gap at that end opening up.

## 0.12.1

- **Expression pedal:** only the treadle rocks now. Its artwork had been cut from the mockup as a rectangle, so it carried pieces of the housing with it: the chrome strip down the right, the base's lip and corner along the bottom, and a dark band across the top. Those moved with the treadle, as if part of the unit were tilting. Everything outside the treadle's own outline is now transparent. Its sides, shadow and shading follow that outline, and the perspective is slightly gentler, so the lifted end stays within the housing's rails.

## 0.12.0

**LO-FI: telephone, radio, megaphone, cassette and bitcrusher, for intros and breakdowns.**

- A new **LO-FI** block, last in the signal chain after the reverb, so the whole sound (echoes and reverb included) goes "through the phone", as it's done in the studio. Click LO-FI in the signal chain to open it. Five types:
  - **Telephone:** only 300 Hz to 3.4 kHz gets through, with steep edges and a little carbon-mic grit, in mono.
  - **AM Radio:** a small radio speaker: wider than a phone, a nasal midrange bump, gentle saturation, in mono.
  - **Megaphone:** a horn speaker: narrow, honky and driven hard, in mono.
  - **Cassette:** a worn tape: the pitch wobbles (slow wow, fast flutter and a random drift), the top end dulls, and the tape saturates and hisses.
  - **Bitcrusher:** fewer bits (down to 3) and a lower sample rate (down to 3 kHz), for early-sampler and video-game crunch.
- **AMOUNT** sets how strong it is: narrower, dirtier, wobblier or crunchier. **NOISE** adds line hiss, static or tape hiss. **MIX** and **LEVEL** blend it and match its volume.
- **LO-FI on the expression pedal** (new EXPRESSION ASSIGN target): fades the effect in, from the full band at the heel to full lo-fi at the toe. Start toe-down for a phone-sounding intro, then rock back to the heel as the band comes in. Assigning it switches LO-FI on.
- To switch it at a set point in a song, automate **Lo-Fi On** (or **Lo-Fi Mix**) in your DAW.
- Five new factory rigs: Phone Intro and Megaphone Verse (bank B), Old Radio (bank C), Cassette Dreams and 8-Bit Crunch (bank D).
- The EXPRESSION ASSIGN list has larger text, and the signal chain's gaps are narrower to fit the tenth block.

## 0.11.0

**Paragon X1 is now Paragon PR7, and the expression pedal can bend pitch.**

**Upgrading from Paragon X1:** the plugin file is now `Paragon PR7.vst3`. Close your DAW, delete the old `Paragon X1.vst3` from your VST3 folder, then copy in the new one. Keeping both would give your DAW two copies of the same plugin. Your projects still open: the plugin keeps the identity DAWs know it by, only the name has changed.

- **New name and logo:** PR7 is a heavy, forward-slanted mark with speed cuts, in the spirit of late-'90s floor processors. It appears in the header, on the unit's badge, and engraved on the expression pedal.
- **Your rigs come with you:** the rigs folder is now `Paragon\Paragon PR7\Rigs`. The first time the new version runs, it copies your rigs over from `Paragon\Paragon X1` and leaves the originals in place. New rigs save as `.pr7rig` files, and `.x1rig` files still load.
- **PITCH / WHAMMY** (new EXPRESSION ASSIGN target): the expression pedal bends the pitch shifter, from no shift at the heel to its INTERVAL at the toe. For example, set INTERVAL to +12 for an octave dive-up, or -24 for a two-octave drop. Assigning it switches the FILTER / PITCH block on as a pitch shifter, so it works straight away. MIN and MAX limit the sweep as usual. It sits fourth on the pedal's assign buttons and in the EXPRESSION ASSIGN panel.

## 0.10.0

**Tube Screamer drive, pitch shifter, auto-wah and doubler.**

- **Screamer drive:** the OD / DISTORTION block has a new MODEL setting, and its new default, **Screamer (TS)**, is modelled on the Ibanez TS808 Tube Screamer's circuit:
  - The op-amp only amplifies above about 720 Hz. Its diodes clip just that part, which is added back on top of the clean guitar, so the bass stays clean and tight while the mids are pushed (the famous mid "hump").
  - The diodes round the peaks off softly and symmetrically.
  - TONE works against a fixed 723 Hz treble cut, from dark to slightly bright.
  - DRIVE has the pedal's audio-taper pot.
  - It's the classic boost in front of a driven amp: try DRIVE low and LEVEL up.
- **Classic** is the original overdrive. Projects and rigs saved with earlier versions load with Classic, so they sound exactly as before.
- **New FILTER / PITCH block,** after the compressor and before the drive (where these pedals go on a real board). Click it in the signal chain to open it. Two types:
  - **Auto-Wah:** an envelope filter. A resonant wah sweeps open as you pick and closes as the note fades. SENSITIVITY sets how hard you need to play to open it fully; RESONANCE sets how sharp and vocal it is.
  - **Pitch Shifter:** two octaves down to two up, in semitones. Use MIX 100% for an all-wet pitch drop, or blend it in for octaves and harmonies (+12 for an octave up, +7 for a fifth). Each time it splices, it finds the point where the waveform matches and cross-fades there, so notes keep a clean, steady pitch instead of warbling.
- **Doubler** (new MODULATION type): two extra "takes" of your part, one each side, each a little late and drifting slightly in timing and pitch, so it sounds double-tracked. DEPTH sets how loose the takes are, RATE how fast they drift, and MIX how loud they are.
- Four new factory rigs: Octave Lead and Harmony Fifths (bank B), Funk Envelope (bank C) and Wide Doubler (bank D).
- With nine blocks now, the signal chain's gaps are a little narrower, and the RIG SUMMARY cards use short titles (hover for the full name).

## 0.9.0

**Six new modulation effects, and tempo sync.**

The MODULATION block's TYPE list grows from four to ten. The original chorus, flanger, phaser and tremolo sound exactly as before.

- **Dimension Chorus:** after the Roland Dimension D and Boss CE-1. Two chorus voices swept in opposite directions, so their pitch changes largely cancel. The result is wide and lush without the seasick wobble, and is slightly darkened like the originals' bucket-brigade chips.
- **Uni-Vibe:** after the Shin-ei Uni-Vibe. Four phase-shift stages tuned far apart, swept by a simulated lamp that brightens quickly, fades slowly and lingers dark, which gives the Vibe its lopsided throb. MIX works as the original's switch: all the way up is VIBRATO, halfway is CHORUS.
- **Rotary:** a Leslie speaker. The sound splits at 800 Hz into a treble horn and a bass drum, each spinning past two microphones with its own Doppler pitch shift and level swing, in stereo. RATE sets the speed. The heavy drum takes longer to spin up and down than the horn, so turning RATE (or assigning it to automation) gives the classic chorale-to-tremolo ramp.
- **Vibrato:** pure pitch vibrato, with no dry signal.
- **Harmonic Tremolo:** the early-'60s Fender "brownface" tremolo. Lows and highs pulse in turn, for a softer, swirling throb with a hint of phaser.
- **Auto-Pan:** sweeps the sound between the speakers, at constant loudness.
- **SYNC** (new, for every type): Off, or a note length from 1/1 to 1/16, including dotted and triplet values. The rate then follows the tapped tempo, or the DAW's. RATE greys out while synced, and the footswitch shows the note value.
- MIX greys out for types that have no dry/wet blend (tremolo, vibrato, harmonic tremolo, auto-pan). For those, MORPH / FX on the expression pedal moves DEPTH instead, as it already did for tremolo.
- Six new factory rigs to try them: Leslie Spin, Machine Gun Vibe, Dimension Clean, Warble Vibrato (bank D), and Brownface Trem and Wide Pan (bank C, tempo-synced).
- Effect panels now size their cards by contents: drop-down lists get more room and ON lights less, so long names like "Harmonic Tremolo" and "1/4 triplet" fit.

## 0.8.1

- The graphic EQ now glows in the plugin's neon orange-red (its curve, faders, icon and signal-chain block), with a white-hot core on the curve like the unit's neon LEDs.

## 0.8.0

**9-band graphic EQ.**

- A graphic EQ with one band per octave (63, 125, 250, 500 Hz, 1, 2, 4, 8 and 16 kHz), each cutting or boosting up to 12 dB, plus an output LEVEL. It sits after the cabinet and PRESENCE, before modulation, delay and reverb, where hardware modellers put theirs: it shapes the amp and cab's tone without colouring the echoes separately.
- Click the new **EQ** block in the signal chain (or its card in RIG SUMMARY) to open it: nine faders standing at their frequencies over a live drawing of the EQ's response, which passes through the faders' caps as you move them. Double-click a fader for 0 dB; **FLAT** resets every band in one step (and one undo).
- Saved with each rig. It's off by default, so existing rigs and projects sound exactly the same.

## 0.7.5

**The mockup's own expression pedal.**

- The expression pedal is now the mockup's artwork itself (with X1 in place of RPX): the chrome housing with its neon-lit rail, and the treadle with its two black pads. It still rocks in 3D: the artwork is drawn through the same perspective as it tilts, so the lifted end comes towards you and the lowered end drops away, over a dark recess with the treadle's shadow.
- Hover over the pedal (or drag it) to see what it controls and its position.

## 0.7.4

**A real rocking pedal, piano-black keys and neon LEDs.**

- **Expression pedal in 3D:** the treadle is now modelled as a solid slab rocking on an axle, seen through a perspective camera, the way Neural DSP and IK Multimedia show theirs. As it rocks, the lifted end comes towards you, the lowered end drops away, its thick edge and its shadow on the base show, and the gloss on its pads slides with the tilt. It still glides smoothly between positions.
- **Piano-black keys:** footswitches, the AMP / CABINET / RIG blocks and the small buttons are now glossy black keys with chamfered edges: a light catch along the top bevel and the red glow reflected in the side bevels, as in the mockup.
- **Neon LEDs:** lit LEDs (footswitch bars, button stripes, ring lights) have a white-hot core inside a red-orange outline, like the mockup's; unlit ones are a dimmer solid red.

## 0.7.3

**The mockup's knobs, and a moving expression pedal.**

- **OUTPUT LEVEL and PRESENCE** are now drawn like the mockup's: a black cylinder seen slightly from above with a spun-metal top, red light reflected up its sides, and a thin neon ring round its base. The ring is open at the front, with hooked ends and marks at the sides. A fine engraved line on the top shows the setting.
- **The expression pedal moves:** as it goes towards the heel the treadle tilts back, its toe lifting, so it looks shorter and the well shows beyond its tip. It glides smoothly to each new position, whether you drag it, use the mouse wheel, automate it, or play a MIDI pedal.

## 0.7.2

**Richer metal and the mockup's LCD.** Colours were measured off the mockup and matched point by point.

- **LCD screen:** proper seven-segment digits that lean like the mockup's, sized and spaced the same, with unlit segments hidden. Dark orange-tinted glass in a thick glossy black bezel, a larger rig name, a bigger BANK / PRESET line, and a small glowing red dot for unsaved changes.
- **Metal:** a deeper rose-silver face that shades towards its edges and picks up pink light at the bottom, a dark mauve chrome rim, pink chrome round the expression pedal, and pure black pedal pads. Smoother brushed finish.
- **Knobs** sit in a pool of red light, and the button cluster's red stripes glow even when off, as in the mockup.
- Darker, near-black background and panels, as in the mockup.
- Fixed stray light arcs on the unit's left edge and on the panels.

## 0.7.1

**Closer still to the mockup.**

- **Deeper chrome:** higher-contrast metal, a thicker rim with the unit's darker front edge showing along the bottom, and a soft red glow round it instead of a hard neon outline. The face plate is neutral brushed silver, with the red light reflected only near its lower edge.
- **Footswitches** are single glossy rubber blocks, as in the mockup.
- **Expression pedal:** darker bay, no dot strip, and X1 stamped in outlined letters like the mockup's RPX.
- **Block knobs** (when you open a block from the summary, a footswitch name or the signal chain) now have the block's icon and name at the top and each control on its own card, matching the rest of the bottom panels.

## 0.7.0

**The mockup's look, in detail.**

- **Inter typeface** built in, so text looks crisp and modern on every computer, with a heavy italic **PARAGON X1** badge as in the mockup.
- **The floor unit:** rose-tinted chrome with a bevelled rim and red light spilling round it. The slot row and the footswitch row sit on raised panels, and the **expression pedal is built into the unit** with two rubber pads and an embossed X1.
- **3D footswitches**, black slot blocks with glowing icons, the mockup's red ring LEDs, red labels on the button cluster with the HOLD TO TUNER bracket, light outlined VOLUME / WAH / MORPH buttons, and a **MIN** light that shows when the pedal is fully back.
- **Header:** grouped tabs (now with **SETTINGS**), the rig name between orange arrows, BPM + TAP together, and undo, redo, save and settings icons.
- **Bottom of the window, as in the mockup:** **RIG BROWSER** shortcuts, **SELECTED RIG** (name, favourite star, bank and number, save), **TAGS** (+ to add, click to remove) and **RIG SUMMARY** cards for every block, plus **EXPRESSION ASSIGN**. Click a card, a footswitch name or a signal-chain block to edit that block's knobs; RIG SUMMARY goes back.
- **SETTINGS page:** the tuner's reference pitch (A4 = 430-450 Hz), mute while tuning, which **MIDI CC** moves the expression pedal, the rigs folder, and credits.
- Slim dotted **level meters**, and a bigger **signal chain**.
- The AMP and CABINET windows show the loaded file's name underneath; hover for the full path and details. The unload (x) button appears on hover.

## 0.6.0

**Performance features: tuner, tap tempo, undo and the expression pedal.**

- **Tuner:** hold **TAP** or press **TUNER** on the pedal. The LED screen shows the note, a cents meter (green when in tune) and the frequency, accurate to under a cent from a 7-string's low B upwards. **MUTE** silences the output while you tune (on by default).
- **Tap tempo:** **TAP** in the header or on the pedal. The tapped tempo (in orange) drives the synced delay instead of your DAW's; click the BPM to go back to the DAW's tempo. The TAP light flashes on the beat, and the tapped tempo is saved with your project.
- **Undo / redo:** header arrows, **UNDO / REDO** on the pedal, or Ctrl+Z and Ctrl+Shift+Z. A whole knob turn, click, file load or rig change is one step, and undoing a rig change brings back the previous rig's name too.
- **Expression pedal** beside the unit, as in the mockup. Drag it, scroll it, or move it from a real pedal on **MIDI CC 11**. Assign it to:
  - **Volume** (with a volume pedal's feel: half way is about -12 dB)
  - **Wah / Filter** (a 350 Hz-2.2 kHz wah before the drive)
  - **Morph / FX** (fades modulation, delay and reverb in from dry at the heel).
  **MIN / MAX** in the new EXPRESSION ASSIGN panel set the heel and toe settings. The assignment and range are saved with each rig.
- Icons on the PROCESSOR and PRESETS tabs.

## 0.5.0

**The pedalboard look from the mockup.**

- **Silver floor unit:** brushed metal with the red-orange edge glow, PARAGON X1 badge, and an orange LED display showing the rig number, name, bank and preset, with a dot when there are unsaved changes. Click the display to browse rigs.
- **Amp, Cabinet and Rig windows** with icons, arrows and captions, as in the mockup. The Rig window steps through rigs; click it to open the browser.
- **Big OUTPUT LEVEL and PRESENCE knobs** on the pedal face, **SAVE / SAVE AS** under the display, and the four footswitches as rubber pads with LED bars.
- **BYPASS button:** bypasses the whole plugin. It's the same switch as your DAW's bypass, and stays in time with the rest of your mix.
- **Signal chain** strip along the bottom: each block lights in its own colour when it's on. Click a block to show its knobs; double-click to switch it on or off.
- **Edit panel** for the selected block (now one each for compressor, drive, amp, cabinet, modulation, delay and reverb), and a **Current Rig** panel with favourite, bank, tags and save buttons.
- Your DAW's **tempo** shows in the header.
- The window opens at 1280 x 720 and scales smoothly up to 1920 x 1080, staying sharp at any size.

## 0.4.0

**Rigs and presets.** Save complete sounds and switch between them.

- **Rigs:** a rig stores the amp model, cabinet IR and every effect setting. Save the current sound with **SAVE** in the header, step through rigs with the **<** and **>** arrows, and see a `*` next to the name when you've changed something since loading or saving.
- **PRESETS tab:** browse all rigs, favourites, your own rigs or factory rigs; filter by bank A-D; search by name or tag; rename, re-file, tag, favourite or delete rigs; open the rigs folder.
- **Factory rigs:** ten starter sounds (Studio Clean, Edge of Breakup, Lead Boost, High Gain Tight, Ambient Swells, Slapback and more). They set the effects and keep your own amp and cabinet.
- **OUTPUT LEVEL** knob (next to Presence): each rig's own volume, for balancing rigs. The OUTPUT knob on the right is still the overall volume, and INPUT and OUTPUT don't change when you switch rigs.
- Rigs are files in your user folder (`%APPDATA%\Paragon\Paragon X1\Rigs` on Windows), so you can back them up and share them. Your DAW project remembers which rig was loaded.

## 0.3.0

**Effects.** Four footswitches, as in the mockup.

- **Comp / Gate:** compressor and noise gate before the amp.
- **OD / Distortion:** overdrive with drive, tone and level, 8x oversampled.
- **Modulation:** stereo chorus and flanger, phaser, tremolo.
- **Reverb / Delay:** tempo-synced or free delay with tone and ping-pong, and stereo reverb; both ring out when switched off.
- **Presence** knob after the cabinet.

## 0.2.0

**Amp and cabinet.**

- Load `.nam` models and cabinet IRs (WAV, AIFF, FLAC), by clicking or drag and drop; step through a folder with the arrows.
- Normalise option to level-match models; same resampler and IR level as the official NAM plugin.
- Click-free switching; files are remembered in your DAW project.

## 0.1.0

First build: input and output gain with level meters.
