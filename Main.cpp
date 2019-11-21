#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "AudioInterface.h"
#include "CfgWindow.h"

#define APP_WIDTH 800
#define APP_HEIGHT 600

#define INIT_MASTER_VOLUME 20

const wxString VERSION = "1.00";

using namespace std;

struct SynthVars
{
	unsigned int nMasterVolume = INIT_MASTER_VOLUME;
} synthVars;

double synthFunction(double);

class MyApp : public wxApp
{
public:
	virtual ~MyApp();

	virtual bool OnInit();

	AudioInterface *audioIF;
};

class MyFrame : public wxFrame
{
public:
	MyFrame();
	~MyFrame();

	wxButton *CfgButton;
	wxSlider *masterVSlider;

private:
	void OnHello(wxCommandEvent& event);
	void OnExit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	void OnConfig(wxCommandEvent& event);
	void OnMaster(wxCommandEvent& event);
};

enum
{
	ID_Hello = 1,
	ID_Cfg,
	ID_Master
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
	MyFrame *frame = new MyFrame();
	frame->SetSize({ APP_WIDTH, APP_HEIGHT });

	vector<string> devices = AudioInterface::GetDevices();

	audioIF = new AudioInterface(devices[0], 44100, 1, 8, 256); //use first device in list

	if (!audioIF->GetActive())
	{
		wxMessageBox("Error opening audio device.", "Error");
		audioIF->Destroy();
	}

	audioIF->SetUserFunction(synthFunction);

	frame->Show(true);

	return true;
}

MyApp::~MyApp()
{
	if (audioIF->GetActive())
	{
		audioIF->Stop();
		audioIF->Destroy();
	}
}

MyFrame::MyFrame()
	: wxFrame(NULL, wxID_ANY, "Virtual Synth v" + VERSION)
{
	wxMenu *menuFile = new wxMenu;
	menuFile->Append(ID_Hello, "&Hello...\tCtrl-H",
		"Help string shown in status bar for this menu item");
	menuFile->AppendSeparator();
	menuFile->Append(wxID_EXIT);

	wxMenu *menuHelp = new wxMenu;
	menuHelp->Append(wxID_ABOUT);

	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuFile, "&File");
	menuBar->Append(menuHelp, "&Help");

	SetMenuBar(menuBar);

	CreateStatusBar();
	SetStatusText("Welcome to the Jungle!");

	Bind(wxEVT_MENU, &MyFrame::OnHello, this, ID_Hello);
	Bind(wxEVT_MENU, &MyFrame::OnAbout, this, wxID_ABOUT);
	Bind(wxEVT_MENU, &MyFrame::OnExit, this, wxID_EXIT);

	Bind(wxEVT_BUTTON, &MyFrame::OnConfig, this, wxID_ANY);
	Bind(wxEVT_SLIDER, &MyFrame::OnMaster, this, wxID_ANY);

	wxPanel *mainPanel = new wxPanel(this, wxID_ANY);

	//cfg button
	CfgButton = new wxButton(mainPanel, ID_Cfg);
	CfgButton->SetSize(32, 32);
	wxSize size(this->GetSize());
	SetStatusText(wxString::Format("%d, %d",size.GetWidth(), size.GetHeight()));
	CfgButton->SetPosition({ (APP_WIDTH - 64), 0 });
	wxBitmap *CfgBitmap = new wxBitmap("cfg_button.bmp", wxBITMAP_TYPE_BMP);
	CfgButton->SetBitmap(*CfgBitmap);

	//Master Volume Slider
	masterVSlider = new wxSlider(mainPanel, ID_Master, 100 - INIT_MASTER_VOLUME, 0, 100, { APP_WIDTH - 64, 64 }, wxDefaultSize, wxSL_VERTICAL);
}

void MyFrame::OnExit(wxCommandEvent& event)
{
	Close(true);
}

void MyFrame::OnAbout(wxCommandEvent& event)
{
	wxMessageBox("This is a Virtual Synth v" + VERSION, "About Virtual Synth", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnConfig(wxCommandEvent & event)
{
	CfgWindow *cfgWin = new CfgWindow(this);

	cfgWin->Show();
}

void MyFrame::OnMaster(wxCommandEvent & event)
{
	wxSlider *s = dynamic_cast<wxSlider*>(event.GetEventObject());

	if (s)
	{
		synthVars.nMasterVolume = 100 - s->GetValue();
		SetStatusText(wxString::Format("Volume: %d", synthVars.nMasterVolume));
	}
		
}

void MyFrame::OnHello(wxCommandEvent& event)
{
	wxLogMessage("Hello World Message!");
}

MyFrame::~MyFrame()
{
	if (CfgButton)
		delete CfgButton;
}

double synthFunction(double d)
{
	double dHertz = 60;
	double dOutput =  sin(dHertz * 2 * 3.14159 * d);

	if (dOutput > 0)
		dOutput = 0.5;
	if (dOutput < 0)
		dOutput = -0.5;

	return dOutput = dOutput * (synthVars.nMasterVolume / 100.0);
}