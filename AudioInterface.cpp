#pragma comment(lib, "winmm.lib")

#include "AudioInterface.h"

#include <cmath>
#include <limits.h>

using namespace std;

AudioInterface::AudioInterface(string sOutputDevice, unsigned int nSampleRate, unsigned int nChannels, unsigned int nBlocks, unsigned int nBlockSamples)
{
	Create(sOutputDevice, nSampleRate, nChannels, nBlocks, nBlockSamples);
}

AudioInterface::~AudioInterface()
{
	Destroy();
}

bool AudioInterface::Create(string sOutputDevice, unsigned int nSampleRate, unsigned int nChannels, unsigned int nBlocks, unsigned int nBlockSamples)
{
	this->bReady = false;
	this->nSampleRate = nSampleRate;
	this->nChannels = nChannels;
	this->nBlockCount = nBlocks;
	this->nBlockSamples = nBlockSamples;
	this->nBlockFree = nBlockCount;
	this->nBlockCurrent = 0;
	this->pBlockMemory = nullptr;
	this->pWaveHeaders = nullptr;

	this->userFunction = nullptr;

	//check device
	vector<string> devices = GetDevices();
	auto d = find(devices.begin(), devices.end(), sOutputDevice);

	if (d!= devices.end())
	{
		//device available
		int nDeviceID = distance(devices.begin(), d);
		WAVEFORMATEX waveFormat;
		waveFormat.wFormatTag = WAVE_FORMAT_PCM;
		waveFormat.nSamplesPerSec = nSampleRate;
		waveFormat.wBitsPerSample = sizeof(short) * 8;
		waveFormat.nChannels = nChannels;
		waveFormat.nBlockAlign = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
		waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
		waveFormat.cbSize = 0;

		//open if valid
		if (waveOutOpen(&hwDevice, nDeviceID, &waveFormat, (DWORD_PTR)waveOutProcWrap, (DWORD_PTR)this, CALLBACK_FUNCTION) != S_OK)
			return false;
	}

	//Allocate wave/block memory
	pBlockMemory = new short[nBlockCount * nBlockSamples];

	if (pBlockMemory == nullptr)
		return false;

	ZeroMemory(pBlockMemory, sizeof(short) * nBlockCount * nBlockSamples);

	pWaveHeaders = new WAVEHDR[nBlockCount];
	if (pWaveHeaders == nullptr)
	{
		Destroy();
		return false;
	}
		
	ZeroMemory(pWaveHeaders, sizeof(WAVEHDR) * nBlockCount);

	//Link headers to block memory
	for (unsigned int i = 0; i < nBlockCount; i++)
	{
		pWaveHeaders[i].dwBufferLength = nBlockSamples * sizeof(short);
		pWaveHeaders[i].lpData = (LPSTR)(pBlockMemory + (i * nBlockSamples));
	}


	this->bReady = true;

	audioThread = thread(&AudioInterface::MainThread, this);

	//start the thread
	unique_lock<mutex> lockMutex(muxBlockNotZero);
	cvBlockNotZero.notify_one();

	return true;
}

void AudioInterface::Destroy()
{
	if (pBlockMemory != nullptr)
		delete[] pBlockMemory;

	if (pWaveHeaders != nullptr)
		delete[] pWaveHeaders;
}

void AudioInterface::Stop()
{
	bReady = false;
	audioThread.join();
}

double AudioInterface::ProcessSample(double dTime)
{
	return 0.0;
}

double AudioInterface::GetTime()
{
	return dGlobalTime;
}

vector<string> AudioInterface::GetDevices()
{
	int nDeviceCount = waveOutGetNumDevs();
	vector<string> sDevices;
	WAVEOUTCAPS woc;
	

	for (int i = 0; i < nDeviceCount; i++)
	{
		if (waveOutGetDevCaps(i, &woc, sizeof(WAVEOUTCAPS)) == S_OK)
		{
			sDevices.push_back(woc.szPname);
		}
	}

	return sDevices;
}

void AudioInterface::SetUserFunction(double(*func)(double))
{
	this->userFunction = func;
}

const bool AudioInterface::GetActive()
{
	return bReady;
}

double AudioInterface::Clip(double dSample, double dMax)
{
	if (dSample >= 0.0)
		return fmin(dSample, dMax);
	else
		return fmax(dSample, -dMax);
}

//Handler for requesting more data
void AudioInterface::waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwParam1, DWORD dwParam2)
{
	if (uMsg != WOM_DONE)
		return;

	nBlockFree++;
	unique_lock<mutex> lockMutex(muxBlockNotZero);
	cvBlockNotZero.notify_one();
}

//static wrapper for waveOutProc
void CALLBACK AudioInterface::waveOutProcWrap(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	((AudioInterface*)dwInstance)->waveOutProc(hWaveOut, uMsg, dwParam1, dwParam2);
}

//TODO:
void AudioInterface::MainThread()
{
	dGlobalTime = 0.0;
	double dTimeStep = 1.0 / (double)nSampleRate;

	double dMaxSample = (double)SHRT_MAX;
	short nPreviousSample = 0;

	while (bReady)
	{
		//wait until block available
		if (nBlockFree == 0)
		{
			unique_lock<mutex> lockMutex(muxBlockNotZero);
			cvBlockNotZero.wait(lockMutex);
		}

		nBlockFree--;

		//prepare block for processing
		if (pWaveHeaders[nBlockCurrent].dwFlags & WHDR_PREPARED)
		{
			waveOutUnprepareHeader(hwDevice, &pWaveHeaders[nBlockCurrent], sizeof(WAVEHDR));
		}

		short nNewSample = 0;
		int nCurrentBlock = nBlockCurrent * nBlockSamples;

		for (unsigned int i = 0; i < nBlockSamples; i++)
		{
			if (userFunction == nullptr)
				nNewSample = (short)(Clip(ProcessSample(dGlobalTime), 1.0) * dMaxSample);
			else
				nNewSample = (short)(Clip(userFunction(dGlobalTime), 1.0) * dMaxSample);

			pBlockMemory[nCurrentBlock + i] = nNewSample;
			nPreviousSample = nNewSample;
			dGlobalTime = dGlobalTime + dTimeStep;
		}

		//send block to sound device
		waveOutPrepareHeader(hwDevice, &pWaveHeaders[nBlockCurrent], sizeof(WAVEHDR));
		waveOutWrite(hwDevice, &pWaveHeaders[nBlockCurrent], sizeof(WAVEHDR));
		nBlockCurrent++;
		nBlockCurrent %= nBlockCount;
	}
}