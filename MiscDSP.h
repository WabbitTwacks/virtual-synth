#pragma once

#include <cmath>
#include <deque>

double SimpleLowPass(double currentSample);
double SimpleHighPass(double currentSample);
double SimpleNotch(double currentSample);
double SimpleBandPass(double currentSample);

double SoftClip(double dInput, double dGain = 5.0);
double SoftClip(double dInput, double dPosGain, double dNegGain);
double BitCrush(double dInput, double dBits = 8.0);

double BiQuadLowPass(double dInput, double(&dDelayBuffer)[2], double dFrequency, double dQ, int nSampleFreq = 44100);
double StateVLowPass(double dInput, double(&dICEQ)[2], double dFrequency, double dQ, int nSampleRate = 44100);

double SimpleLowPass(double currentSample)
{
	static double lastSample[2] = { 0.0, 0.0 };
	static int channel = 0;

	double lowPassed = currentSample + lastSample[channel];

	lastSample[channel] = currentSample;
	channel++;
	channel %= 2;

	return lowPassed * 0.5;
}

double SimpleHighPass(double currentSample)
{
	static double lastSample[2] = { 0.0, 0.0 };
	static int channel = 0;

	double highPassed = currentSample + lastSample[channel] * -1.0;

	lastSample[channel] = currentSample;
	channel++;
	channel %= 2;

	return highPassed * 0.5;
}

double SimpleNotch(double currentSample)
{
	static std::deque<double> samples[2];
	static int channel = 0;

	if (samples[channel].size() < 2)
	{
		samples[channel].push_front(currentSample);

		return currentSample;
	}		

	double notched = currentSample + samples[channel].back();

	samples[channel].push_front(currentSample);
	samples[channel].pop_back();
	channel++;
	channel %= 2;

	return notched * 0.5;
}

double SimpleBandPass(double currentSample)
{
	static std::deque<double> samples[2];
	static int channel = 0;

	if (samples[channel].size() < 2)
	{
		samples[channel].push_front(currentSample);

		return currentSample;
	}

	double bandPassed = currentSample + samples[channel].back() * -1.0;

	samples[channel].push_front(currentSample);
	samples[channel].pop_back();
	channel++;
	channel %= 2;

	return bandPassed * 0.5;
}

double SoftClip(double dInput, double dGain)
{
	return (2 / PI) * atan(dInput * dGain);
}

double SoftClip(double dInput, double dPosGain, double dNegGain)
{
	if (dInput >= 0.0)
		return SoftClip(dInput, dPosGain);
	else
		return SoftClip(dInput, dNegGain);
}

double BitCrush(double dInput, double dBits)
{
	double dScale = pow(2, dBits - 1.0);

	return ceil(dInput * dScale) / dScale;
}

double BiQuadLowPass(double dInput, double (&dDelayBuffer)[2], double dFrequency, double dQ, int nSampleRate)
{
	double w0 = 2 * PI * dFrequency / nSampleRate;
	double alpha = sin(w0) / (2 * dQ);

	//low pass filter coefficients
	double b0 = (1 - cos(w0)) / 2.0;
	double b1 = 1 - cos(w0);
	double b2 = b0;
	double a0 = 1 + alpha;
	double a1 = -2 * cos(w0);
	double a2 = 1 - alpha;

	//Direct From II
	double w1 = dDelayBuffer[1];
	double w2 = dDelayBuffer[0];

	double w = dInput + (-a1 / a0)*w1 + (-a2 / a0)*w2;
	double dOut = (b0 / a0)*w + (b1 / a0)*w1 + (b2 / a0)*w2;
	
	dDelayBuffer[0] = w1;
	dDelayBuffer[1] = w;

	return dOut;
}

double StateVLowPass(double dInput, double(&dICEQ)[2], double dFrequency, double dQ, int nSampleRate) //better for modulating with LFOs (no artifacts)
{
	double g = tan(PI * dFrequency / nSampleRate);
	double k = 1.0 / dQ;
	double a1 = 1.0 / (1.0 + g*(g + k));
	double a2 = g * a1;
	double a3 = g * a2;

	double iceq1 = dICEQ[0];
	double iceq2 = dICEQ[1];
	double v0 = dInput;

	double v3 = v0 - iceq2;
	double v1 = a1 * iceq1 + a2 * v3;
	double v2 = iceq2 + a2 * iceq1 + a3 * v3;
	
	iceq1 = 2 * v1 - iceq1;
	iceq2 = 2 * v2 - iceq2;
	dICEQ[0] = iceq1;
	dICEQ[1] = iceq2;

	return v2;
}