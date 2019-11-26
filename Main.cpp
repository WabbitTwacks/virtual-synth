#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <algorithm>
#include <cmath>
#include <deque>

#include "Helpers.h"
#include "AudioInterface.h"
#include "CfgWindow.h"
#include "Oscillator.h"
#include "Envelope.h"

#define AVERAGE_SAMPLES 441

#define C_SHARP_0 16.35

#define APP_WIDTH 800
#define APP_HEIGHT 600

#define INIT_MASTER_VOLUME 45

#define LFO_MIN 0.001
#define LFO_MAX 20.00
#define FREQ_MIN 20.00
#define FREQ_MAX 20000.00

//matrix devices
#define R_OSC1 0
#define R_OSC2 1
#define R_OSC3 2
#define R_FLTR 3
#define R_MIXR 4 //only for dOutputs

#define R_NUM_DEVS 5

//matrix routing targets
#define R_OSC1_P 0
#define R_OSC1_A 1
#define R_OSC2_P 2
#define R_OSC2_A 3
#define R_OSC3_P 4
#define R_OSC3_A 5
#define R_FLTR_C 6
#define R_MIXR_A 7

#define R_NUM_ROUTES 8

const wxString VERSION = "1.00";

using namespace std;

struct SynthVars
{
	unsigned int nMasterVolume = INIT_MASTER_VOLUME;

	Oscillator osc[3];
	Envelope ADSR;

	//route input to device mapping
	const uint8_t deviceMap[6] = { 0, 0, 1, 1, 2, 2 };

	double dNotes[12 * 9];

	vector<uint8_t> vNotesOn;
	uint16_t bKeyDown = 0;
	uint8_t numKeysDown = 0;
	int8_t nOctave = 3;

	bool octaveKeyDownState = false;

	deque<double> dCurrentOutput;

	wxString debug = "Debug: ";

} synthVars;

bool routingMatrix[R_NUM_DEVS - 1][R_NUM_ROUTES];

double synthFunction(double, byte);

class MyFrame;

class MyApp : public wxApp
{
public:
	virtual ~MyApp();

	virtual bool OnInit();
	virtual int FilterEvent(wxEvent &event);

	AudioInterface *audioIF;

	MyFrame *pFrame;
};

class MyFrame : public wxFrame
{
public:
	MyFrame();
	~MyFrame();

	wxButton *CfgButton;
	wxSlider *masterVSlider;
	wxGauge *masterVLevel;
	wxPanel *masterVBack;
	wxPanel *masterVPanel;
	wxBitmap *masterUVLevel;

	wxTimer *masterLevelTimer;

	wxPanel *oscPanel[3];

	wxChoice *choiceOscWave[3];
	wxSlider *panOscChannel[3];
	wxSlider *volOsc[3];
	wxTextCtrl *freqEditOsc[3];
	wxSlider *freqOsc[3];
	wxCheckBox *checkDrone[3];
	wxChoice *choiceOscRouting[3];
	wxCheckBox *checkLFO[3];

private:
	void OnHello(wxCommandEvent& event);
	void OnExit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	void OnConfig(wxCommandEvent& event);
	void OnMaster(wxCommandEvent& event);
	void OnMasterGauge(wxTimerEvent& event);

	void OnOscWave(wxCommandEvent& event);

	void OnOscPan(wxCommandEvent& event);
	void OnOscVol(wxCommandEvent& event);
	void OnOscFreq(wxCommandEvent& event);
	void OnOscFreqEdit(wxCommandEvent& event);
	void OnOscDrone(wxCommandEvent& event);
	void OnOscRouting(wxCommandEvent& event);
	void OnOscLFO(wxCommandEvent& event);

	void OnPaint(wxPaintEvent &event);
};

enum
{
	ID_Hello = 1,
	ID_Cfg,
	ID_Master,
	ID_MasterLevel,
	ID_MasterLevelTimer,
	ID_Pan1,
	ID_Pan2,
	ID_Wave1,
	ID_Wave2,
	ID_Vol1,
	ID_Vol2,
	ID_Freq1,
	ID_Freq2,
	ID_Drone1,
	ID_Drone2,
	ID_FreqEdit1,
	ID_FreqEdit2,
	ID_OscRouting1,
	ID_OscRouting2,
	ID_OscLFO1,
	ID_OscLFO2
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
	vector<string> devices = AudioInterface::GetDevices();

	audioIF = new AudioInterface(devices[0], 44100, 2, 8, 512); //use first device in list

	if (!audioIF->GetActive())
	{
		wxMessageBox("Error opening audio device.", "Error");
		audioIF->Destroy();
	}

	audioIF->SetUserFunction(synthFunction);

	ZeroMemory(routingMatrix, R_NUM_ROUTES * (R_NUM_DEVS-1));
	routingMatrix[R_OSC1][R_MIXR_A] = true;
	routingMatrix[R_OSC2][R_MIXR_A] = true;

	//synthVars.osc[1].SetLFO(true);
	//synthVars.osc[1].SetFrequency(1.0);
	synthVars.osc[1].SetOctave(-1);

	//generate all note frequency values for lookup
	for (int i = 0; i < 12 * 9; i++)
		synthVars.dNotes[i] = C_SHARP_0 * pow(2, i / 12.0);

	MyFrame *frame = new MyFrame();
	frame->SetSize({ APP_WIDTH, APP_HEIGHT });
	pFrame = frame;

	frame->Show(true);
	frame->SetFocus();

	return true;
}

int MyApp::FilterEvent(wxEvent & event)
{
	if (event.GetEventType() == wxEVT_KEY_DOWN)
	{
		for (int i = 0; i < 16; i++)
		{
			if (((wxKeyEvent&)event).GetUnicodeKey() == L"AWSEDFTGYHUJKOLP"[i] && !(synthVars.bKeyDown & (1<<i)))
			{
				pFrame->SetFocus();

				if (synthVars.numKeysDown == 0)
				{
					synthVars.vNotesOn.clear();

					synthVars.ADSR.StartEnvelope();
				}					

				synthVars.bKeyDown |= (1<<i);

				uint8_t nNote = synthVars.nOctave * 12 + i;
				if (std::find(synthVars.vNotesOn.begin(), synthVars.vNotesOn.end(), nNote) == synthVars.vNotesOn.end())
					synthVars.vNotesOn.push_back(nNote);

				synthVars.numKeysDown++;

				return false;
			}
		}

		if (((wxKeyEvent&)event).GetUnicodeKey() == 'Z' /*&& !synthVars.octaveKeyDownState*/)
		{
			//synthVars.octaveKeyDownState = true;
			synthVars.nOctave--;

			if (synthVars.nOctave < 0)
				synthVars.nOctave = 0;
		}
		else if (((wxKeyEvent&)event).GetUnicodeKey() == 'X' /*&& !synthVars.octaveKeyDownState*/)
		{
			//synthVars.octaveKeyDownState = true;
			synthVars.nOctave++;

			if (synthVars.nOctave > 7)
				synthVars.nOctave = 7;
		}
	
	}
	else if (event.GetEventType() == wxEVT_KEY_UP)
	{
		for (int i = 0; i < 16; i++)
		{
			if (((wxKeyEvent&)event).GetUnicodeKey() == L"AWSEDFTGYHUJKOLP"[i])
			{
				pFrame->SetFocus();

				synthVars.bKeyDown ^= (1<<i);

				synthVars.numKeysDown--;

				if (synthVars.numKeysDown == 0)
					synthVars.ADSR.StopEnvelope();

				return false;
			}
		}
	}

	return -1;
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
	menuFile->Append(ID_Hello, "&Hello",
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
	masterVSlider = new wxSlider(mainPanel, ID_Master, 100 - INIT_MASTER_VOLUME, 0, 100, { APP_WIDTH - 100, 64 }, wxDefaultSize, wxSL_VERTICAL);
	
	masterVBack = new wxPanel(mainPanel, wxID_ANY, { APP_WIDTH - 64, 64 }, { 16, 100 }, wxSIMPLE_BORDER);
	/*masterVPanel = new wxPanel(mainPanel, wxID_ANY, { APP_WIDTH - 62, 66 }, { 12, 96 });
	masterVPanel->SetBackgroundColour(wxColor(0x33ff33));*/

	//masterVLevel = new wxGauge(mainPanel, ID_MasterLevel, 100, { APP_WIDTH - 64, 64 }, { wxDefaultSize.GetX(), 100 }, wxGA_VERTICAL);
	masterLevelTimer = new wxTimer(this, ID_MasterLevelTimer);
	Bind(wxEVT_TIMER, &MyFrame::OnMasterGauge, this, wxID_ANY);
	masterLevelTimer->Start(100);

	Bind(wxEVT_BUTTON, &MyFrame::OnConfig, this, ID_Cfg);
	Bind(wxEVT_SLIDER, &MyFrame::OnMaster, this, ID_Master);


	//oscillator panels
	for (int i = 0; i < 3; i++)
	{
		oscPanel[i] = new wxPanel(mainPanel, wxID_ANY, { 12, 6 + i*150 }, { 300, 150 }, wxSIMPLE_BORDER);
	}

	choiceOscWave[0] = new wxChoice(oscPanel[0], ID_Wave1, { 6, 6}, wxDefaultSize);
	choiceOscWave[0]->Append(vector<wxString>({ "Sine", "Square", "Saw", "Triangle", "Noise" }));
	choiceOscWave[0]->SetSelection(0);
	Bind(wxEVT_CHOICE, &MyFrame::OnOscWave, this, ID_Wave1);

	panOscChannel[0] = new wxSlider(oscPanel[0], ID_Pan1, 0, -100, 100, { 128,  6 }, wxDefaultSize, wxSL_BOTTOM);
	Bind(wxEVT_SLIDER, &MyFrame::OnOscPan, this, ID_Pan1);

	volOsc[0] = new wxSlider(oscPanel[0], ID_Vol1, 100 - (synthVars.osc[0].GetVolume() * 100), 0, 100, { 250, 6 }, wxDefaultSize, wxSL_VERTICAL);
	Bind(wxEVT_SLIDER, &MyFrame::OnOscVol, this, ID_Vol1);

	freqEditOsc[0] = new wxTextCtrl(oscPanel[0], ID_FreqEdit1, wxString::Format("%.2f", synthVars.osc[0].GetFrequency()), { 6, 50 }, { 50, wxDefaultSize.GetY() }, wxTE_CENTRE | wxTE_PROCESS_ENTER);
	Bind(wxEVT_COMMAND_TEXT_ENTER, &MyFrame::OnOscFreqEdit, this, ID_FreqEdit1);
	

	freqOsc[0] = new wxSlider(oscPanel[0], ID_Freq1, (int)LogToLin(synthVars.osc[0].GetFrequency(), FREQ_MIN, FREQ_MAX, 1.0, 1000.0), 1, 1000, { 6, 74 });
	if (synthVars.osc[0].IsLFO())
		freqOsc[0]->SetValue((int)synthVars.osc[0].GetFrequency() *  1000.0 / LFO_MAX);
	Bind(wxEVT_SLIDER, &MyFrame::OnOscFreq, this, ID_Freq1);

	checkDrone[0] = new wxCheckBox(oscPanel[0], ID_Drone1, "Drone", { 6, 32 });
	checkDrone[0]->SetValue(synthVars.osc[0].GetDrone());
	Bind(wxEVT_CHECKBOX, &MyFrame::OnOscDrone, this, ID_Drone1);

	//------

	choiceOscWave[1] = new wxChoice(oscPanel[1], ID_Wave2, { 6, 6 }, wxDefaultSize);
	choiceOscWave[1]->Append(vector<wxString>({ "Sine", "Square", "Saw", "Triangle", "Noise" }));
	choiceOscWave[1]->SetSelection(0);
	Bind(wxEVT_CHOICE, &MyFrame::OnOscWave, this, ID_Wave2);

	panOscChannel[1] = new wxSlider(oscPanel[1], ID_Pan2, 0, -100, 100, { 128, 6 }, wxDefaultSize, wxSL_BOTTOM);
	Bind(wxEVT_SLIDER, &MyFrame::OnOscPan, this, ID_Pan2);

	volOsc[1] = new wxSlider(oscPanel[1], ID_Vol2, 100 - (synthVars.osc[1].GetVolume() * 100), 0, 100, { 250, 6 }, wxDefaultSize, wxSL_VERTICAL);
	Bind(wxEVT_SLIDER, &MyFrame::OnOscVol, this, ID_Vol2);

	freqEditOsc[1] = new wxTextCtrl(oscPanel[1], ID_FreqEdit2, wxString::Format("%.2f", synthVars.osc[1].GetFrequency()), { 6, 50 }, { 50, wxDefaultSize.GetY() }, wxTE_CENTRE | wxTE_PROCESS_ENTER);
	Bind(wxEVT_COMMAND_TEXT_ENTER, &MyFrame::OnOscFreqEdit, this, ID_FreqEdit2);

	freqOsc[1] = new wxSlider(oscPanel[1], ID_Freq2, (int)LogToLin(synthVars.osc[1].GetFrequency(), FREQ_MIN, FREQ_MAX, 1.0, 1000.0), 1, 1000, { 6, 74 });
	if (synthVars.osc[1].IsLFO())
		freqOsc[1]->SetValue((int)synthVars.osc[1].GetFrequency() * 1000.0 / LFO_MAX);
	Bind(wxEVT_SLIDER, &MyFrame::OnOscFreq, this, ID_Freq2);

	checkDrone[1] = new wxCheckBox(oscPanel[1], ID_Drone2, "Drone", { 6, 32 });
	checkDrone[1]->SetValue(synthVars.osc[1].GetDrone());
	Bind(wxEVT_CHECKBOX, &MyFrame::OnOscDrone, this, ID_Drone2);

	choiceOscRouting[1] = new wxChoice(oscPanel[1], ID_OscRouting2, { 6, 120 });
	choiceOscRouting[1]->Append(vector<wxString>({"Mixer", "Osc1 Pitch", "Osc1 Amp", "Osc2 Pitch", "Osc2 Amp"}));
	choiceOscRouting[1]->SetSelection(0);
	Bind(wxEVT_CHOICE, &MyFrame::OnOscRouting, this, ID_OscRouting2);

	checkLFO[1] = new wxCheckBox(oscPanel[1], ID_OscLFO2, "LFO", { 70, 55 });
	checkLFO[1]->SetValue(synthVars.osc[1].IsLFO());
	Bind(wxEVT_CHECKBOX, &MyFrame::OnOscLFO, this, ID_OscLFO2);

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
		SetStatusText(wxString::Format("Master Volume: %d", synthVars.nMasterVolume));
	}
	SetFocus();
}

void MyFrame::OnMasterGauge(wxTimerEvent & event)
{
	if (synthVars.dCurrentOutput.size() < AVERAGE_SAMPLES)
		return;

	//calculate RMS for Output Signal
	double dRMSVolume = 0.0;

	for (auto i : synthVars.dCurrentOutput)
	{
		dRMSVolume += i*i;
	}

	dRMSVolume /= (double)AVERAGE_SAMPLES;
	dRMSVolume = sqrt(dRMSVolume);

	//SetStatusText(wxString::Format("RMS: %.2f", dRMSVolume));

	double dB = (dRMSVolume > 0.0) ? 10 * log10(dRMSVolume / 1.0) : -100000000.0;

	SetStatusText(wxString::Format("dB: %.2f", dB));

	double dMinDB = 10 * log10(0.000001 / 1.0);
	double dMaxDB = 0.0;

	wxClientDC dc((wxFrame*)masterVBack);
	dc.SetBackground({wxColor(0x555555)});
	dc.Clear();
	dc.SetBrush({wxColor(0x33ff33)});
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.DrawRectangle(13, 97, -12, -(int)((dB - dMinDB) * 100.0/(dMaxDB - dMinDB)));
}

void MyFrame::OnOscWave(wxCommandEvent & event)
{
	wxChoice *c = dynamic_cast<wxChoice*>(event.GetEventObject());

	if (c)
	{
		if (c->GetId() == ID_Wave1)
			synthVars.osc[0].SetWave(c->GetSelection() + 1);
		else if (c->GetId() == ID_Wave2)
			synthVars.osc[1].SetWave(c->GetSelection() + 1);
	}
	SetFocus();
}

void MyFrame::OnOscPan(wxCommandEvent & event)
{
	wxSlider *s = dynamic_cast<wxSlider*>(event.GetEventObject());

	if (s)
	{
		if (s->GetValue() < 2 && s->GetValue() > -2)
		{
			
		}

		double pan[2];
		pan[0] = (double)min<int>(100 - s->GetValue(), 100)/100.0;
		pan[1] = (double)min<int>(100 + s->GetValue(), 100)/100.0;

		if (s->GetId() == ID_Pan1)
		{
			synthVars.osc[0].SetChannelVolume(CH_LEFT, pan[0]);
			synthVars.osc[0].SetChannelVolume(CH_RIGHT, pan[1]);

			SetStatusText(wxString::Format("Pan 1: %d", s->GetValue()));
		}		
		else if (s->GetId() == ID_Pan2)
		{
			synthVars.osc[1].SetChannelVolume(CH_LEFT, pan[0]);
			synthVars.osc[1].SetChannelVolume(CH_RIGHT, pan[1]);

			SetStatusText(wxString::Format("Pan 2: %d", s->GetValue()));
		}
	}

	SetFocus();
}

void MyFrame::OnOscVol(wxCommandEvent & event)
{
	wxSlider *s = dynamic_cast<wxSlider*>(event.GetEventObject());

	if (s)
	{
		if (s->GetId() == ID_Vol1)
		{
			synthVars.osc[0].SetVolume(1.0 - (double)s->GetValue() / 100.0);

			SetStatusText(wxString::Format("Volume 1: %d", 100 - s->GetValue()));
		}
		else if (s->GetId() == ID_Vol2)
		{
			synthVars.osc[1].SetVolume(1.0 - (double)s->GetValue() / 100.0);

			SetStatusText(wxString::Format("Volume 2: %d", 100 - s->GetValue()));
		}
	}

	SetFocus();
}

void MyFrame::OnOscFreq(wxCommandEvent & event)
{
	wxSlider *s = dynamic_cast<wxSlider*>(event.GetEventObject());

	if (s)
	{
		if (s->GetId() == ID_Freq1)
		{
			double f1;
			if (synthVars.osc[0].IsLFO())
				f1 = s->GetValue() * LFO_MAX / 1000.0;
			else
				f1 = LinToLog(s->GetValue(), 1.0, 1000.0, FREQ_MIN, FREQ_MAX);

			synthVars.osc[0].SetFrequency(f1);

			freqEditOsc[0]->SetValue(wxString::Format("%.2f", f1));

			SetStatusText(wxString::Format("Pitch 1: %.2f Hz", f1));
		}
		else if (s->GetId() == ID_Freq2)
		{
			double f1;
			if (synthVars.osc[1].IsLFO())
				f1 = s->GetValue() * LFO_MAX/1000.0;
			else
				f1 = LinToLog(s->GetValue(), 1.0, 1000.0, FREQ_MIN, FREQ_MAX);

			synthVars.osc[1].SetFrequency(f1);

			freqEditOsc[1]->SetValue(wxString::Format("%.2f", f1));

			SetStatusText(wxString::Format("Pitch 2: %.2f Hz", f1));
		}
	}

	SetFocus();
}

void MyFrame::OnOscDrone(wxCommandEvent & event)
{
	wxCheckBox *cb = dynamic_cast<wxCheckBox*>(event.GetEventObject());

	if (cb)
	{
		if (cb->GetId() == ID_Drone1)
		{
			synthVars.osc[0].SetDrone(cb->GetValue());
		}
		else if (cb->GetId() == ID_Drone2)
		{
			synthVars.osc[1].SetDrone(cb->GetValue());
		}
	}
	SetFocus();
}

void MyFrame::OnOscRouting(wxCommandEvent & event)
{
	wxChoice *cb = dynamic_cast<wxChoice*>(event.GetEventObject());
	
	if (cb)
	{
		if (cb->GetId() == ID_OscRouting1)
		{
			ZeroMemory(routingMatrix[R_OSC1], R_NUM_ROUTES);
		}
		else if (cb->GetId() == ID_OscRouting2)
		{
			ZeroMemory(routingMatrix[R_OSC2], R_NUM_ROUTES);

			if (cb->GetSelection() == 0) //Mixer
			{
				routingMatrix[R_OSC2][R_MIXR_A] = true;
			}
			else
			{
				routingMatrix[R_OSC2][cb->GetSelection() - 1] = true;
			}
		}
	}

}

void MyFrame::OnOscLFO(wxCommandEvent & event)
{
	wxCheckBox *cb = dynamic_cast<wxCheckBox*>(event.GetEventObject());

	if (cb)
	{
		if (cb->GetId() == ID_OscLFO1)
		{

		}
		else if (cb->GetId() == ID_OscLFO2)
		{
			synthVars.osc[1].SetLFO(cb->GetValue());

			if (cb->GetValue()) //LFO scaling on
			{
				double fFreq = freqOsc[1]->GetValue() * LFO_MAX / 1000.0;
				freqEditOsc[1]->SetValue(wxString::Format("%.2f", fFreq));
				synthVars.osc[1].SetFrequency(fFreq);
			}
			else //LFO scaling off
			{
				double fFreq = LinToLog(freqOsc[1]->GetValue(), 1.0, 1000.0, FREQ_MIN, FREQ_MAX);
				freqEditOsc[1]->SetValue(wxString::Format("%.2f", fFreq));
				synthVars.osc[1].SetFrequency(fFreq);
			}
		}
	}
	SetFocus();
}

void MyFrame::OnPaint(wxPaintEvent & event)
{
	wxPaintDC(this);
}

void MyFrame::OnOscFreqEdit(wxCommandEvent & event)
{
	wxTextCtrl *tx = dynamic_cast<wxTextCtrl*>(event.GetEventObject());

	if (tx)
	{
		if (tx->GetId() == ID_FreqEdit1 /*&& event.GetKeyCode() == 13*/)
		{
			double r;
			tx->GetValue().ToCDouble(&r);
			synthVars.osc[0].SetFrequency(r);

			if (synthVars.osc[0].IsLFO())
				freqOsc[0]->SetValue((int)synthVars.osc[0].GetFrequency() * 1000.0 / LFO_MAX);
			else
				freqOsc[0]->SetValue((int)LogToLin(r, FREQ_MIN, FREQ_MAX, 1.0, 1000.0));
		}
		else if (tx->GetId() == ID_FreqEdit2 /*&& event.GetKeyCode() == 13*/)
		{
			double r;
			tx->GetValue().ToCDouble(&r);
			synthVars.osc[1].SetFrequency(r);

			if (synthVars.osc[1].IsLFO())
				freqOsc[1]->SetValue((int)synthVars.osc[1].GetFrequency() * 1000.0/LFO_MAX);
			else
				freqOsc[1]->SetValue((int)LogToLin(r, FREQ_MIN, FREQ_MAX, 1.0, 1000.0));
		}
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

double synthFunction(double d, byte channel)
{
	double dOutputs[R_NUM_DEVS] = { 0.0, 0.0, 0.0, 0.0, 0.0 };

	const uint8_t *deviceMap = synthVars.deviceMap;
	 

	//Generate outputs from oscillators
	for (int i = 0; i < 3; i++)
	{
		if (synthVars.osc[i].GetDrone())
		{
			dOutputs[i] = synthVars.osc[i].Play(synthVars.osc[i].GetFrequency(), d, channel);
		}
		else
		{
			for (auto note : synthVars.vNotesOn)
			{
					uint8_t nSemiTone = note + synthVars.osc[i].GetOctaveMod()*12;
					if (nSemiTone >= 12 * 9 || nSemiTone < 0) continue;
					//double dFrequency = C_SHARP_0 * pow(2, nSemiTone / 12.0);
					dOutputs[i] += synthVars.ADSR.GetAmplitude() * synthVars.osc[i].Play(synthVars.dNotes[nSemiTone], d, channel);				
			}
		}
	}

	//Check Routing Matrix
	for (int i = 0; i < R_NUM_ROUTES; i++)
	{
		if (i == R_OSC1_P || i == R_OSC2_P || i == R_OSC3_P)
		{
			//do some frequency modulation here
			synthVars.osc[deviceMap[i]].SetFM(0.0);

			for (int m = 0; m < 3; m++)
			{
				if (routingMatrix[m][i])
				{
					synthVars.osc[deviceMap[i]].AddFM(0.25 * synthVars.osc[deviceMap[i]].GetFrequency() * synthVars.osc[m].Play(synthVars.osc[m].GetFrequency(), d, channel));
				}

			}

			//regenerate outputs
			if (synthVars.osc[deviceMap[i]].GetDrone())
				dOutputs[deviceMap[i]] = synthVars.osc[deviceMap[i]].Play(synthVars.osc[deviceMap[i]].GetFrequency(), d, channel);
		}
		else if (i == R_OSC1_A || i == R_OSC2_A || i == R_OSC3_A)
		{
			//amplitude modulation
			synthVars.osc[deviceMap[i]].ResetAM();

			for (int m = 0; m < 3; m++)
			{
				if (routingMatrix[m][i])
				{
					double dAM = synthVars.osc[m].Play(synthVars.osc[m].GetFrequency(), d, channel) + 0.1*synthVars.osc[m].GetVolume();
					synthVars.osc[deviceMap[i]].AddAM(1.0 - dAM);
				}
			}

			//regenerate outputs
			if (synthVars.osc[deviceMap[i]].GetDrone())
				dOutputs[deviceMap[i]] = synthVars.osc[deviceMap[i]].Play(synthVars.osc[deviceMap[i]].GetFrequency(), d, channel);
		}
		else if (i == R_FLTR_C)
		{
			//do some filter magic here
		}
		else if (i == R_MIXR_A)
		{
			for (int j = 0; j < R_NUM_ROUTES; j++)
				dOutputs[R_MIXR] += routingMatrix[j][R_MIXR_A] ? dOutputs[j] : 0.0;
		}
	}

	double dOut = dOutputs[R_MIXR] * 0.5 * (synthVars.nMasterVolume / 100.0);

	synthVars.dCurrentOutput.push_front(dOut);
	if (synthVars.dCurrentOutput.size() > AVERAGE_SAMPLES)
		synthVars.dCurrentOutput.pop_back();
	
	return dOut;
}