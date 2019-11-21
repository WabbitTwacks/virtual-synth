#pragma once


#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <Windows.h>



class AudioInterface
{
public:
	AudioInterface(std::string sOutputDevice, unsigned int nSampleRate = 44100, unsigned int nChannels = 1, unsigned int nBlocks = 8, unsigned int nBlockSamples = 512);
	~AudioInterface();

	bool Create(std::string sOutputDevice, unsigned int nSampleRate = 44100, unsigned int nChannels = 1, unsigned int nBlocks = 8, unsigned int nBlockSamples = 512);
	void Destroy();
	void SetUserFunction(double(*func)(double));
	double Clip(double dSample, double dMax);
	void Stop();
	virtual double ProcessSample(double dTime); //override to process current sample
	double GetTime();
	const bool GetActive();

	static std::vector<std::string> GetDevices();


private:
	double(*userFunction)(double);

	unsigned int nSampleRate;
	unsigned int nChannels;
	unsigned int nBlockCount;
	unsigned int nBlockSamples;
	unsigned int nBlockCurrent;

	short *pBlockMemory;
	WAVEHDR *pWaveHeaders;
	HWAVEOUT hwDevice;

	std::thread audioThread;
	std::atomic <bool> bReady;
	std::atomic <unsigned int> nBlockFree;
	std::condition_variable cvBlockNotZero;
	std::mutex muxBlockNotZero;

	std::atomic <double> dGlobalTime;

	void MainThread();
	void waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwParam1, DWORD dwParam2);

	static void CALLBACK waveOutProcWrap(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);	
};