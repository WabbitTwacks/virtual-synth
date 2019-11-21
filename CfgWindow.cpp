#include <vector>
#include <string>

#include "CfgWindow.h"
#include "AudioInterface.h"

#pragma comment(lib, "winmm.lib")

using namespace std;

CfgWindow::CfgWindow(wxWindow *parent)
	: wxFrame(parent, wxID_ANY, "Configuration")
{
	rootPanel = new wxPanel(this, wxID_ANY);
	aiBox = new wxChoice(rootPanel, wxID_ANY);
	wxStaticText *aiBoxLabel = new wxStaticText(rootPanel, wxID_ANY, "Audio Device:");

	aiBox->SetPosition({ 10, 32 });
	aiBox->SetSize({200, 24});

	aiBoxLabel->SetPosition({ 10, 16 });

	//quick code for getting audio devices
	/*int nDeviceCount = waveOutGetNumDevs();
	vector<wstring> sDevices;
	WAVEOUTCAPS woc;

	for (int i = 0; i < nDeviceCount; i++)
	{
		if (waveOutGetDevCaps(i, &woc, sizeof(WAVEOUTCAPS)) == S_OK)
		{
			sDevices.push_back(woc.szPname);

			aiBox->AppendString(woc.szPname);
		}			
	}*/

	vector <string> sDevices = AudioInterface::GetDevices();
	for (auto i : sDevices)
	{
		aiBox->AppendString(wxString(i));
	}

	aiBox->SetSelection(0);
}

inline CfgWindow::~CfgWindow()
{
}