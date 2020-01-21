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
	aiBox = new wxChoice(rootPanel, ID_aiBox);
	wxStaticText *aiBoxLabel = new wxStaticText(rootPanel, wxID_ANY, "Audio Device:");

	aiBox->SetPosition({ 10, 32 });
	aiBox->SetSize({200, 24});

	aiBoxLabel->SetPosition({ 10, 16 });

	sDevices = AudioInterface::GetDevices();
	for (auto i : sDevices)
	{
		aiBox->AppendString(wxString(i));
	}

	Bind(wxEVT_CHOICE, &CfgWindow::ChangeInterface, this, ID_aiBox);
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

		if (!pAI->Create(sDevices[cb->GetSelection()], 44100, 2, 128, 32))
			wxMessageBox("Failed to create!");
	}	
}