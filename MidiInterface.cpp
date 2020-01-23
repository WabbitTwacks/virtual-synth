#pragma comment(lib, "winmm.lib")

#define UNICODE

#include "MidiInterface.h"

using namespace std;


MidiInterface::MidiInterface()
{
}


MidiInterface::~MidiInterface()
{
}

bool MidiInterface::OpenDevice(wstring sDeviceName)
{
	vector <wstring> sDevices = GetMidiDevices();
	auto device = find(sDevices.begin(), sDevices.end(), sDeviceName);

	if (device != sDevices.end())
	{
		int nDevID = distance(sDevices.begin(), device);

		MMRESULT mmRes = midiInOpen(&hMidiIn, nDevID, (DWORD_PTR)processMidiWrap, (DWORD_PTR)this, CALLBACK_FUNCTION);

		if (mmRes != S_OK)
			return false;

		return true;
	}

	return false;
}

bool MidiInterface::CloseDevice()
{
	if (hMidiIn)
	{
		midiInReset(hMidiIn);
		if (midiInClose(hMidiIn) != S_OK)
			return false;
	}

	return true;
}

int MidiInterface::GetActiveMidiID()
{
	if (hMidiIn == nullptr)
		return -1;

	UINT idMidi = 0;
	if (midiInGetID(hMidiIn, &idMidi) != S_OK)
		return -1;

	return (int)idMidi;
}

std::vector<std::wstring> MidiInterface::GetMidiDevices()
{
	int nMidiCount = midiInGetNumDevs();
	vector<wstring> sMidiDevs;
	MIDIINCAPS mic;

	for (int i = 0; i < nMidiCount; i++)
	{
		if (midiInGetDevCaps(i, &mic, sizeof(MIDIINCAPS)) == S_OK)
		{
			sMidiDevs.push_back(mic.szPname);
		}
	}

	return sMidiDevs;
}

void MidiInterface::processMidiWrap(HMIDIIN hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	((MidiInterface*)dwInstance)->processMidi(hWaveOut, uMsg, dwParam1, dwParam2);
}

void MidiInterface::processMidi(HMIDIIN hWaveOut, UINT uMsg, DWORD dwParam1, DWORD dwParam2)
{
	switch (uMsg)
	{
	case MIM_CLOSE:
		break;

	case MIM_DATA: //process incomming MIDI messages
		break;

	case MIM_ERROR:
		break;
	}
}
