#include "plugin.hpp"

using simd::float_4;

template <typename T>
T expCurve(T x) {
	return (3 + x * (-13 + 5 * x)) / (3 + 2 * x);
}

template <int OVERSAMPLE, int QUALITY, typename T>
struct VoltageControlledOscillator {
	bool analog = false;
	bool soft = false;
	bool syncEnabled = false;
	int channels = 0;
	T phase = 0.f;
	T freq = 0.f;
	T pulseWidth = 0.5f;

	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> sqrMinBlep;
	dsp::MinBlepGenerator<QUALITY, OVERSAMPLE, T> sawMinBlep;

	T sqrValue = 0.f;
	T sawValue = 0.f;

	void setPulseWidth(T pulseWidth) {
		const float pwMin = 0.01f;
		this->pulseWidth = simd::clamp(pulseWidth, pwMin, 1.f - pwMin);
	}

	void process(float deltaTime, T syncValue) {
		// Advance phase
		T deltaPhase = simd::clamp(freq * deltaTime, 0.f, 0.35f);
		phase += deltaPhase;
		phase -= simd::floor(phase);

		// Jump sqr when crossing `pulseWidth`
		T pulseCrossing = (pulseWidth - (phase - deltaPhase)) / deltaPhase;
		int pulseMask = simd::movemask((0 < pulseCrossing) & (pulseCrossing <= 1.f));
		if (pulseMask) {
			for (int i = 0; i < channels; i++) {
				if (pulseMask & (1 << i)) {
					T mask = simd::movemaskInverse<T>(1 << i);
					float p = pulseCrossing[i] - 1.f;
					T x = mask & (-2.f);
					sqrMinBlep.insertDiscontinuity(p, x);
				}
			}
		}

		// Jump saw when crossing 0.5
		T halfCrossing = (0.5f - (phase - deltaPhase)) / deltaPhase;
		int halfMask = simd::movemask((0 < halfCrossing) & (halfCrossing <= 1.f));
		if (halfMask) {
			for (int i = 0; i < channels; i++) {
				if (halfMask & (1 << i)) {
					T mask = simd::movemaskInverse<T>(1 << i);
					float p = halfCrossing[i] - 1.f;
					T x = mask & (-2.f);
					sawMinBlep.insertDiscontinuity(p, x);
				}
			}
		}

		// Square
		sqrValue = sqr(phase);
		sqrValue += sqrMinBlep.process();

		// Saw
		sawValue = saw(phase);
		sawValue += sawMinBlep.process();
	}

	T saw(T phase) {
		T v;
		T x = phase + 0.5f;
		x -= simd::trunc(x);
		if (analog) {
			v = -expCurve(x);
		} else {
			v = 2 * x - 1;
		}
		return v;
	}

	T saw() {
		return sawValue;
	}

	T sqr(T phase) {
		T v = simd::ifelse(phase < pulseWidth, 1.f, -1.f);
		return v;
	}

	T sqr() {
		return sqrValue;
	}
};

struct VCO : Module {
	enum ParamId {
		COARSE_PARAM,
		FINE_PARAM,
		PULSEW_PARAM,
		FMLVL_PARAM,
		PWMCVLVL_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		PWMCV_INPUT,
		_1V_INPUT,
		FMCV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		SAW_OUTPUT,
		PULSE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	VoltageControlledOscillator<16, 16, float_4> oscillators[4];

	VCO() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(COARSE_PARAM, -54.f, 54.f, 0.f, "Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine frequency", " Hz", 0.f, 1.f / 12.f);  // Adjusted fine-tune parameter
		configParam(PULSEW_PARAM, 0.01f, 0.99f, 0.5f, "Pulse width", "%", 0.f, 100.f);
		configParam(FMLVL_PARAM, -1.f, 1.f, 0.f, "Frequency modulation", "%", 0.f, 100.f);
		configParam(PWMCVLVL_PARAM, -1.f, 1.f, 0.f, "Pulse width modulation", "%", 0.f, 100.f);
		configInput(PWMCV_INPUT, "PWM input");
		configInput(_1V_INPUT, "1V/OCT");
		configInput(FMCV_INPUT, "FM input");
		configOutput(SAW_OUTPUT, "SAW");
		configOutput(PULSE_OUTPUT, "PULSE");
	}

	void process(const ProcessArgs& args) override {
		float freqParam = params[COARSE_PARAM].getValue() / 12.f;
		float fineParam = params[FINE_PARAM].getValue() / 12.f;  // Apply fine tune as a fraction of a semitone
		float fmParam = params[FMLVL_PARAM].getValue();
		float pwParam = params[PULSEW_PARAM].getValue();
		float pwCvParam = params[PWMCVLVL_PARAM].getValue();

		int channels = std::max(inputs[_1V_INPUT].getChannels(), 1);

		for (int c = 0; c < channels; c += 4) {
			auto& oscillator = oscillators[c / 4];
			oscillator.channels = std::min(channels - c, 4);
			oscillator.analog = true;

			// Get frequency
			float_4 pitch = freqParam + fineParam + inputs[_1V_INPUT].getPolyVoltageSimd<float_4>(c);
			pitch += inputs[FMCV_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;
			float_4 freq = dsp::FREQ_C4 * dsp::exp2_taylor5(pitch);
			freq = clamp(freq, 0.f, args.sampleRate / 2.f);
			oscillator.freq = freq;

			// Get pulse width
			float_4 pw = pwParam + inputs[PWMCV_INPUT].getPolyVoltageSimd<float_4>(c) / 10.f * pwCvParam;
			oscillator.setPulseWidth(pw);

			oscillator.process(args.sampleTime, 0.f);

			// Set output
			if (outputs[SAW_OUTPUT].isConnected())
				outputs[SAW_OUTPUT].setVoltageSimd(5.f * oscillator.saw(), c);
			if (outputs[PULSE_OUTPUT].isConnected())
				outputs[PULSE_OUTPUT].setVoltageSimd(5.f * oscillator.sqr(), c);
		}

		outputs[SAW_OUTPUT].setChannels(channels);
		outputs[PULSE_OUTPUT].setChannels(channels);
	}
};


struct VCOWidget : ModuleWidget {
	VCOWidget(VCO* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/VCO.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(25.584, 22.49)), module, VCO::COARSE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(13.698, 49.551)), module, VCO::FINE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(37.47, 49.551)), module, VCO::PULSEW_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(13.698, 62.739)), module, VCO::FMLVL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(37.47, 62.739)), module, VCO::PWMCVLVL_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(37.418, 81.327)), module, VCO::PWMCV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.606, 114.473)), module, VCO::_1V_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.048, 114.66)), module, VCO::FMCV_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(31.239, 114.286)), module, VCO::SAW_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(42.194, 114.66)), module, VCO::PULSE_OUTPUT));
	}
};


Model* modelVCO = createModel<VCO, VCOWidget>("VCO");