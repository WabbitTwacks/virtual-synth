#pragma once

#include <Windows.h>
#include <vector>
#include <string>

class MidiInterface
{
public:
	MidiInterface();
	virtual ~MidiInterface();

	bool OpenDevice(std::wstring sDeviceName);
	bool CloseDevice();
	int GetActiveMidiID();

	static std::vector<std::wstring> GetMidiDevices();


private:
	HMIDIIN hMidiIn;

	static void CALLBACK processMidiWrap(HMIDIIN hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
	void processMidi(HMIDIIN hWaveOut, UINT uMsg, DWORD dwParam1, DWORD dwParam2);
};

