hir is readme :>

# Table of Contents


1. [Modules](#modules)
   - [VCO - Voltage Controlled Oscillator](#vco---voltage-controlled-oscillator)

## Modules

### 1. VCO - Voltage Controlled Oscillator

#### Overview

The VCO (Voltage Controlled Oscillator) module is designed to generate precise waveforms for various audio applications. It provides both saw and pulse wave outputs, offering versatility in sound synthesis. The module features controls for tuning and modulation, allowing for a wide range of sonic possibilities.

#### Features

- **Waveform Outputs:**
  - **Saw Wave:** Generates a sawtooth waveform.
  - **Pulse Wave:** Produces a pulse waveform.

- **Controls:**
  - **Coarse Tuning:** Adjusts the overall frequency range.
  - **Fine Tuning:** Refines the frequency for precise adjustments.
  - **Pulse Width Control:** Modify the width of the pulse waveform using either a knob or an external voltage source.
  - **Pulse Width Modulation (PWM) Voltage Level:** Control the modulation depth of the pulse width.

- **Inputs:**
  - **1V/OCT Input:** Provides pitch control based on the 1V/octave standard.
  - **Frequency Modulation Input:** Allows external signals to modulate the VCO's frequency.

#### Connections

- **Saw Output:** Connect to your audio or CV processing module.
- **Pulse Output:** Connect to your audio or CV processing module.

#### Example Usage

To achieve complex modulation effects, you can:
- Use the **Pulse Width Control** to shape the character of the pulse wave.
- Apply an external signal to the **Frequency Modulation Input** to introduce dynamic changes in pitch.
- Utilize the **1V/OCT Input** to integrate with other pitch-controlled devices or sequencers.

#### Visual

For now its pretty ugly but it will get better

![VCO Module](https://github.com/user-attachments/assets/8995be05-5f45-4fab-9119-c2692e9d438b)

Inspired by [Erica Synth edu diy system](https://www.ericasynths.lv/shop/diy-kits-1/mki-x-esedu-diy-system/)


