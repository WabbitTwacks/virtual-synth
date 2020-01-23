#pragma once

#include <wx/wx.h>
#include <vector>
#include <string>
#include "AudioInterface.h"
#include "MidiInterface.h"

class CfgWindow : public wxFrame
{
public:
	CfgWindow(wxWindow *parent);
	~CfgWindow();

	void ChangeInterface(wxCommandEvent &event);
	void ChangeMidiIn(wxCommandEvent &event);

	wxPanel *rootPanel;
	wxChoice *aiBox;
	wxChoice *midiBox;
	AudioInterface *pAI;
	MidiInterface *pMIDI;

private:
	enum
	{
		ID_aiBox = 1001,
		ID_midiBox
	};
	

	std::vector <std::string> sAudioDevices;
	std::vector <std::wstring> sMidiDevices;
};


