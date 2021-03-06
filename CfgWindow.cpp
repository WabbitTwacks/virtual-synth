#pragma comment(lib, "winmm.lib")

#include "CfgWindow.h"
#include "AudioInterface.h"

using namespace std;

CfgWindow::CfgWindow(wxWindow *parent)
	: wxFrame(parent, wxID_ANY, "Configuration")
{
	rootPanel = new wxPanel(this, wxID_ANY);
	aiBox = new wxChoice(rootPanel, ID_aiBox);
	wxStaticText *aiBoxLabel = new wxStaticText(rootPanel, wxID_ANY, "Audio Device:");
	aiBox->SetPosition({ 10, 32 });
	aiBox->SetSize({200, 24});
	aiBoxLabel->SetPosition({ 10, 16 });

	midiBox = new wxChoice(rootPanel, ID_midiBox);
	wxStaticText *midiBoxLabel = new wxStaticText(rootPanel, wxID_ANY, "MIDI Input Device:");
	midiBox->SetPosition({ 10, 88 });
	midiBox->SetSize({ 200, 24 });
	midiBoxLabel->SetPosition({ 10, 72 });

	//Get Audio Devices
	sAudioDevices = AudioInterface::GetDevices();
	for (auto i : sAudioDevices)
	{
		aiBox->AppendString(wxString(i));
	}

	Bind(wxEVT_CHOICE, &CfgWindow::ChangeInterface, this, ID_aiBox);

	//Get Midi Devices
	sMidiDevices = GetMidiDevices();
	for (auto i : sMidiDevices)
	{
		midiBox->AppendString(wxString(i));
	}

	Bind(wxEVT_CHOICE, &CfgWindow::ChangeMidiIn, this, ID_midiBox);
}

inline CfgWindow::~CfgWindow()
{
}

void CfgWindow::ChangeInterface(wxCommandEvent &event)
{
	wxChoice *cb = dynamic_cast<wxChoice*>(event.GetEventObject());

	if (cb)
	{
		pAI->Stop();

		if (!pAI->Create(sAudioDevices[cb->GetSelection()], 44100, 2, 128, 32))
			wxMessageBox("Failed connecting to audio interface!");
	}	
}

void CfgWindow::ChangeMidiIn(wxCommandEvent & event)
{
	wxChoice *cb = dynamic_cast<wxChoice*>(event.GetEventObject());

	if (cb)
	{
		//TODO:
		//Change connected midi devices
	}
}

vector<wstring> CfgWindow::GetMidiDevices()
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

int CfgWindow::GetActiveMidiID()
{
	UINT idMidi = -1;
	midiInGetID(*hMidiIn, &idMidi);

	return (int)idMidi;
}
