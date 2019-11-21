#pragma once

#include <wx/wx.h>

class CfgWindow : public wxFrame
{
public:
	CfgWindow(wxWindow *parent);
	~CfgWindow();

	wxPanel *rootPanel;
	wxChoice *aiBox;
};


