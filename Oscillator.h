#pragma once

#include <cstdint>
#include <string>

#define PI 3.14159
#define PI_R PI * 2

#define WAVE_SINE 1
#define WAVE_SQUARE 2
#define WAVE_SAW 3
#define WAVE_TRI 4
#define WAVE_NOISE 5

//stereo channel definitions
#define CH_LEFT 0
#define CH_RIGHT 1

#define CH_MONO -1

struct oscParams
{
	std::uint8_t nWave = WAVE_SINE;
	double dVolume = 1.0;
	double dChannelVolume[4] = { 0.5, 0.5, 0.0, 0.0 };
	int8_t nFineTune = 0; // in cents
	double dFreq = 440.0;
	int8_t nOctaveMod = 0;

	bool bDrone = false;

	double dFM = 0.0;
	double dAmplitude = dVolume;

	bool bLFO = false;
};

class Oscillator
{
public:
	Oscillator();
	virtual ~Oscillator();

	void SetParameters(oscParams p);
	void SetVolume(double dVolume);
	void SetChannelVolume(uint8_t nChannel, double dVolume);
	void SetChannelVolume(double dChannels[4]);
	void SetWave(uint8_t nWave);
	void SetFrequency(double dFreq);
	void SetFineTune(int nCents);
	void SetDrone(bool bDrone);
	void SetFM(double dFreqMod);
	void AddFM(double dFreqMod);
	void SetAM(double dAmpMod);
	void AddAM(double dAmpMod);
	void ResetAM();
	void SetLFO(bool bLFO);
	void SetOctave(int8_t nOctaveMod); //-3 to +3
	
	double GetVolume();
	double GetChannelVolume(uint8_t nChannel);
	double GetFrequency();
	int8_t GetFineTune();
	bool GetDrone();
	bool IsLFO();
	int8_t GetOctaveMod();

	double Play(double dFreq, double dTime, int8_t nChannel = CH_MONO);

private:
	oscParams parameters;
};
