#pragma once

#include <ctime>

struct EnvelopeParameters
{
	double dAttack = 1.0;
	double dDecay = 10.0;
	double dSustain = 0.8;
	double dRelease = 100.0;
};

class Envelope
{
public:
	Envelope();
	~Envelope();

	void SetAttack(double dAttack);
	void SetDecay(double dDecay);
	void SetSustain(double dSustain);
	void SetRelease(double dRelease);

	double GetAmplitude();
	void StartEnvelope();
	void StopEnvelope();



private:
	std::clock_t dTriggerStartTime;
	std::clock_t dTriggerEndTime;

	EnvelopeParameters parameters;

	bool bActive = false;
	bool bReleased= false;

	double dTriggerEndAmplitude = 0.0;
};