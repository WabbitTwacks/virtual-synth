#pragma once

#include <wx/wx.h>
#include <vector>
#include <string>
#include "AudioInterface.h"

class CfgWindow : public wxFrame
{
public:
	CfgWindow(wxWindow *parent);
	~CfgWindow();

	void ChangeInterface(wxCommandEvent &event);
	void ChangeMidiIn(wxCommandEvent &event);
	std::vector<std::wstring> GetMidiDevices();
	int GetActiveMidiID();

	wxPanel *rootPanel;
	wxChoice *aiBox;
	wxChoice *midiBox;
	AudioInterface *pAI;
	HMIDIIN *hMidiIn;

private:
	enum
	{
		ID_aiBox = 1001,
		ID_midiBox
	};
	

	std::vector <std::string> sAudioDevices;
	std::vector <std::wstring> sMidiDevices;
};


