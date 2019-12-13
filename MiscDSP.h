#pragma once

#include <cmath>
#include <deque>

double SimpleLowPass(double currentSample);
double SimpleHighPass(double currentSample);
double SimpleNotch(double currentSample);
double SimpleBandPass(double currentSample);

double SoftClip(double dInput, double dGain = 5.0);
double BitCrush(double dInput, double dBits = 8.0);

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

double BitCrush(double dInput, double dBits)
{
	double dScale = pow(2, dBits - 1.0);

	return ceil(dInput * dScale) / dScale;
}