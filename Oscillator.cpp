#include "Oscillator.h"

#include <cmath>

Oscillator::Oscillator()
{
}

Oscillator::~Oscillator()
{
}

void Oscillator::SetParameters(oscParams p)
{
	parameters = p;
}

void Oscillator::SetVolume(double dVolume)
{
	parameters.dVolume = dVolume;
	parameters.dAmplitude = dVolume;
}

void Oscillator::SetChannelVolume(uint8_t nChannel, double dVolume)
{
	if (nChannel < 0 || nChannel > 3)
		return;

	parameters.dChannelVolume[nChannel] = dVolume;
}

void Oscillator::SetChannelVolume(double dChannels[4])
{
	for (int i = 0; i < 4; i++)
		parameters.dChannelVolume[i] = dChannels[i];
}

void Oscillator::SetWave(uint8_t nWave)
{
	parameters.nWave = nWave;
}

void Oscillator::SetFrequency(double dFreq)
{
	parameters.dFreq = dFreq;
}

void Oscillator::SetFineTune(int nCents)
{
}

void Oscillator::SetDrone(bool bDrone)
{
	parameters.bDrone = bDrone;
}

void Oscillator::SetFM(double dFreqMod)
{
	parameters.dFM = dFreqMod;
}

void Oscillator::AddFM(double dFreqMod)
{
	parameters.dFM += dFreqMod;
}

void Oscillator::SetAM(double dAmpMod)
{
	if (dAmpMod < 0.00)
		dAmpMod = 0.0;

	parameters.dAmplitude = dAmpMod;
}

void Oscillator::AddAM(double dAmpMod)
{
	if (dAmpMod < 0.00)
		dAmpMod = 0.0;

	parameters.dAmplitude *= dAmpMod;
}

void Oscillator::ResetAM()
{
	parameters.dAmplitude = parameters.dVolume;
}

void Oscillator::SetLFO(bool bLFO)
{
	parameters.bLFO = bLFO;
}

void Oscillator::SetOctave(int8_t nOctaveMod)
{
	if (nOctaveMod < -4)
		nOctaveMod = -4;
	else if (nOctaveMod > 4)
		nOctaveMod = 4;

	parameters.nOctaveMod = nOctaveMod;
}

double Oscillator::GetVolume()
{
	return parameters.dVolume;
}

double Oscillator::GetChannelVolume(uint8_t nChannel)
{
	if (nChannel < 0 || nChannel > 3)
		return 0.0;

	return parameters.dChannelVolume[nChannel];
}

double Oscillator::GetFrequency()
{
	return parameters.dFreq;
}

bool Oscillator::GetDrone()
{
	return parameters.bDrone;
}

bool Oscillator::IsLFO()
{
	return parameters.bLFO;
}

int8_t Oscillator::GetOctaveMod()
{
	return parameters.nOctaveMod;
}

double Oscillator::Play(double dFreq, double dTime, int8_t nChannel)
{
	double dOutput = 0.0;

	dOutput = sin(dFreq * PI_R * dTime + parameters.dFM);

	switch (parameters.nWave)
	{
	case WAVE_SINE:
		
		break;
	case WAVE_SQUARE:

		if (dOutput > 0.0)
			dOutput = 1.0;
		else
			dOutput = -1.0;

		break;
	case WAVE_SAW:
		dOutput = 0.0;

		/*for (double n = 1.0; n < 100.0; n++) //too slow
			dOutput += (-sin(n * dFreq * PI_R * dTime)) / n;*/

		dOutput = (dFreq + parameters.dFM) * PI_R * fmod(dTime, 1.0 / (dFreq + parameters.dFM)) - (PI / 2.0);

		break;
	case WAVE_TRI:
		dOutput = asin(dOutput);
		break;
	case WAVE_NOISE:
		dOutput = 2.0 * (double(rand()) / double(RAND_MAX)) - 1.0;
		break;
	default:
		dOutput = 0.0;
	}

	if (nChannel > 3)
		nChannel = -1;

	if (nChannel == CH_MONO)
		return dOutput * parameters.dAmplitude;
	else
		return dOutput * parameters.dChannelVolume[nChannel] * parameters.dAmplitude;
}
