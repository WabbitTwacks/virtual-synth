#pragma once

#include <wx/wx.h>
#include <vector>
#include "AudioInterface.h"

class CfgWindow : public wxFrame
{
public:
	CfgWindow(wxWindow *parent);
	~CfgWindow();

	void ChangeInterface(wxCommandEvent &event);

	wxPanel *rootPanel;
	wxChoice *aiBox;
	AudioInterface *pAI;

private:
	int ID_aiBox = 1001;

	std::vector <std::string> sDevices;
};


