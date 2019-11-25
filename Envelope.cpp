#include "Envelope.h"

Envelope::Envelope()
{
	
}

Envelope::~Envelope()
{
}

void Envelope::SetAttack(double dAttack)
{
	parameters.dAttack = dAttack;
}

void Envelope::SetDecay(double dDecay)
{
	parameters.dDecay = dDecay;
}

void Envelope::SetSustain(double dSustain)
{
	parameters.dSustain = dSustain;
}

void Envelope::SetRelease(double dRelease)
{
	parameters.dRelease = dRelease;
}

double Envelope::GetAmplitude()
{
	if (!bActive)
		return 0.0;

	clock_t time = std::clock();

	
	clock_t attack = (int)(parameters.dAttack * (double)CLOCKS_PER_SEC / 1000.0);
	clock_t decay = (int)(parameters.dDecay * (double)CLOCKS_PER_SEC / 1000.0);

	if ((time - dTriggerStartTime) <= attack && !bReleased) //Attack
	{		
		double k = (double)(time - dTriggerStartTime) / (double)attack;

		return k;		
	}
	else if (parameters.dSustain != 0.0 && !bReleased)//return decay and sustain amplitude
	{
		if ((time - dTriggerStartTime - attack) <= decay) //Decay
		{
			double k = (double)(time - dTriggerStartTime - attack) / (double)decay;

			return 1.0 - (k * (1.0 - parameters.dSustain));
		}
		else
			return parameters.dSustain;
	}
	else //calculate release amplitude
	{
		clock_t release = (int)(parameters.dRelease * (double)CLOCKS_PER_SEC/1000.0);

		if (bReleased)
		{
			if ((time - dTriggerEndTime) <= release)
			{
				double k = (double)(time - dTriggerEndTime) / (double)release;

				return dTriggerEndAmplitude - k* dTriggerEndAmplitude;
			}
			else
				bActive = false;
		}
		else
		{
			if ((time - dTriggerStartTime - attack) <= release)
			{
				double k = (double)(time - dTriggerStartTime + attack) / (double)release;

				return 1.0 - k;
			}
			else
				bActive = false;
		}
		
	}		

	return 0.0;
}

void Envelope::StartEnvelope()
{
	dTriggerStartTime = std::clock();
	bActive = true;
	bReleased = false;
}

void Envelope::StopEnvelope()
{
	dTriggerEndAmplitude = GetAmplitude();

	dTriggerEndTime = std::clock();
	bReleased = true;
}
