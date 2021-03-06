#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "Helpers.h"
#include "AudioInterface.h"
#include "CfgWindow.h"
#include "Oscillator.h"
#include "Envelope.h"
#include "MiscDSP.h"

#define AVERAGE_SAMPLES 441

#define C_SHARP_0 16.35

#define APP_WIDTH 800
#define APP_HEIGHT 600

#define INIT_MASTER_VOLUME 45 //45%
#define OSC_VOLUME 0.125 //-18 dBFS

#define LFO_MIN 0.001
#define LFO_MAX 20.00
#define FREQ_MIN 20.00
#define FREQ_MAX 20000.00

//matrix devices
#define R_OSC1 0
#define R_OSC2 1
#define R_OSC3 2
#define R_FLTR 3
#define R_ENV 4
#define R_MIXR 5 //only for dOutputs

#define R_NUM_DEVS 6

//matrix routing targets
#define R_OSC1_P 0
#define R_OSC1_A 1
#define R_OSC2_P 2
#define R_OSC2_A 3
#define R_OSC3_P 4
#define R_OSC3_A 5
#define R_FLTR_I 6
#define R_FLTR_C 7
#define R_MIXR_A 8

#define R_NUM_ROUTES 9

const wxString VERSION = "1.00";

using namespace std;

struct Bench
{
	atomic <double> waveGen = 0.0;
	atomic <double> modulation = 0.0;
	atomic <double> filter = 0.0;
	atomic <double> outputBuffer = 0.0;

} bench;

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

	uint16_t nFilterCutoff = 22000;
	double dResonance = 1.0;
	bool bFourthOrder = false;

	mutex muxRWOutput;
	condition_variable cvIsOutputProcessed;
	double dOutputBuffer[2][AVERAGE_SAMPLES];
	int nBufferPos = 0;
	atomic <bool> bBuffReady = false;

	bool bFilter = false;

	AudioInterface *audioIF;	
	HMIDIIN hMidiIn = 0;

	wxString debug = "Debug: ";

} synthVars;

bool routingMatrix[R_NUM_DEVS - 1][R_NUM_ROUTES];

double synthFunction(double, byte);
double SimpleLowPass(double currentSample);

class MyFrame;

class MyApp : public wxApp
{
public:
	virtual ~MyApp();

	virtual bool OnInit();
	virtual int FilterEvent(wxEvent &event);

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
	wxTextCtrl *freqFine[3];
	wxCheckBox *checkDrone[3];
	wxChoice *choiceOscRouting[3];
	wxCheckBox *checkLFO[3];
	wxRadioBox *octRadioBox[3];


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
	void OnFreqFine(wxCommandEvent& event);
	void OnOctaveSelect(wxCommandEvent& event);
	void OnEnvelope(wxCommandEvent& event);
	void OnEnvRoute(wxCommandEvent& event);
	void OnCutoff(wxCommandEvent& event);
	void OnResonance(wxCommandEvent& event);
	void OnFltrPoles(wxCommandEvent& event);

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
	ID_Fine1,
	ID_Fine2,
	ID_Drone1,
	ID_Drone2,
	ID_FreqEdit1,
	ID_FreqEdit2,
	ID_OscRouting1,
	ID_OscRouting2,
	ID_OscLFO1,
	ID_OscLFO2,
	ID_OctSel1,
	ID_OctSel2,
	ID_Att1,
	ID_Dec1,
	ID_Sus1,
	ID_Rel1,
	ID_EnvRoute1,
	ID_CutOff1,
	ID_Resonance1,
	ID_FltrPoles1
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
	ZeroMemory(synthVars.dOutputBuffer, 2 * AVERAGE_SAMPLES * sizeof(double));

	vector<string> devices = AudioInterface::GetDevices();

	synthVars.audioIF = new AudioInterface(devices[0], 44100, 2, 128, 32); //use first device in list

	if (!synthVars.audioIF->GetActive())
	{
		wxMessageBox("Error opening audio device.", "Error");
		synthVars.audioIF->Destroy();
	}

	synthVars.audioIF->SetUserFunction(synthFunction);

	ZeroMemory(routingMatrix, R_NUM_ROUTES * (R_NUM_DEVS-1));
	routingMatrix[R_OSC1][R_FLTR_I] = true;
	routingMatrix[R_OSC2][R_FLTR_I] = true;
	routingMatrix[R_FLTR][R_MIXR_A] = true;
	routingMatrix[R_ENV][R_MIXR_A] = true;

	//synthVars.osc[1].SetLFO(true);
	//synthVars.osc[1].SetFrequency(1.0);
	//synthVars.osc[1].SetOctave(-1);

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
				}	

				synthVars.ADSR.StartEnvelope();

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

		if (((wxKeyEvent&)event).GetUnicodeKey() == ' ')
		{
			synthVars.bFilter = !synthVars.bFilter;
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
	if (synthVars.audioIF->GetActive())
	{
		synthVars.audioIF->Stop();
		synthVars.audioIF->Destroy();
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
	SetStatusText(wxString::Format("%d, %d", size.GetWidth(), size.GetHeight()));
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
		oscPanel[i] = new wxPanel(mainPanel, wxID_ANY, { 12, 6 + i * (150 + 4) }, { 300, 150 }, wxSIMPLE_BORDER);
	}

	choiceOscWave[0] = new wxChoice(oscPanel[0], ID_Wave1, { 6, 6 }, wxDefaultSize);
	choiceOscWave[0]->Append(vector<wxString>({ "Sine", "Square", "Saw", "Triangle", "Noise" }));
	choiceOscWave[0]->SetSelection(0);
	Bind(wxEVT_CHOICE, &MyFrame::OnOscWave, this, ID_Wave1);

	panOscChannel[0] = new wxSlider(oscPanel[0], ID_Pan1, 0, -100, 100, { 114,  6 }, wxDefaultSize, wxSL_BOTTOM);
	Bind(wxEVT_SLIDER, &MyFrame::OnOscPan, this, ID_Pan1);
	wxStaticText *panLabel = new wxStaticText(oscPanel[0], wxID_ANY, "Pan", { 155, 28 });

	volOsc[0] = new wxSlider(oscPanel[0], ID_Vol1, 100 - (synthVars.osc[0].GetVolume() * 100), 0, 100, { 270, 6 }, wxDefaultSize, wxSL_VERTICAL);
	Bind(wxEVT_SLIDER, &MyFrame::OnOscVol, this, ID_Vol1);

	freqEditOsc[0] = new wxTextCtrl(oscPanel[0], ID_FreqEdit1, wxString::Format("%.2f", synthVars.osc[0].GetFrequency()), { 6, 50 }, { 50, wxDefaultSize.GetY() }, wxTE_CENTRE | wxTE_PROCESS_ENTER);
	Bind(wxEVT_COMMAND_TEXT_ENTER, &MyFrame::OnOscFreqEdit, this, ID_FreqEdit1);


	freqOsc[0] = new wxSlider(oscPanel[0], ID_Freq1, (int)LogToLin(synthVars.osc[0].GetFrequency(), FREQ_MIN, FREQ_MAX, 1.0, 1000.0), 1, 1000, { 6, 74 });
	if (synthVars.osc[0].IsLFO())
		freqOsc[0]->SetValue((int)synthVars.osc[0].GetFrequency() *  1000.0 / LFO_MAX);
	Bind(wxEVT_SLIDER, &MyFrame::OnOscFreq, this, ID_Freq1);
	wxStaticText *freqLabel = new wxStaticText(oscPanel[0], wxID_ANY, "Frequency", { 6, 94 });

	freqFine[0] = new wxTextCtrl(oscPanel[0], ID_Fine1, wxString::Format("%d", synthVars.osc[0].GetFineTune()), { 120, 74 }, { 40, wxDefaultSize.GetY() }, wxTE_CENTRE | wxTE_PROCESS_ENTER);
	Bind(wxEVT_COMMAND_TEXT_ENTER, &MyFrame::OnFreqFine, this, ID_Fine1);
	wxStaticText *fineLabel = new wxStaticText(oscPanel[0], wxID_ANY, "Fine", { 120, 100 });

	checkDrone[0] = new wxCheckBox(oscPanel[0], ID_Drone1, "Drone", { 6, 32 });
	checkDrone[0]->SetValue(synthVars.osc[0].GetDrone());
	Bind(wxEVT_CHECKBOX, &MyFrame::OnOscDrone, this, ID_Drone1);

	//radio buttons for octave selection
	wxArrayString octaveOptions;
	octaveOptions.Add("+2");
	octaveOptions.Add("+1");
	octaveOptions.Add("+0");
	octaveOptions.Add("-1");
	octaveOptions.Add("-2");

	octRadioBox[0] = new wxRadioBox(oscPanel[0], ID_OctSel1, "Octave", { 214, 6 }, wxDefaultSize, octaveOptions, 1, wxRA_SPECIFY_COLS);
	octRadioBox[0]->SetSelection(synthVars.osc[0].GetOctaveMod() + 2);
	Bind(wxEVT_RADIOBOX, &MyFrame::OnOctaveSelect, this, ID_OctSel1);

	choiceOscRouting[0] = new wxChoice(oscPanel[0], ID_OscRouting1, { 6, 120 });
	choiceOscRouting[0]->Append(vector<wxString>({ "Mixer", "Osc1 Pitch", "Osc1 Amp", "Osc2 Pitch", "Osc2 Amp", "---", "---","Filter In", "Fltr Cutoff" }));
	choiceOscRouting[0]->SetSelection(7);
	Bind(wxEVT_CHOICE, &MyFrame::OnOscRouting, this, ID_OscRouting1);

	//------

	choiceOscWave[1] = new wxChoice(oscPanel[1], ID_Wave2, { 6, 6 }, wxDefaultSize);
	choiceOscWave[1]->Append(vector<wxString>({ "Sine", "Square", "Saw", "Triangle", "Noise" }));
	choiceOscWave[1]->SetSelection(0);
	Bind(wxEVT_CHOICE, &MyFrame::OnOscWave, this, ID_Wave2);

	panOscChannel[1] = new wxSlider(oscPanel[1], ID_Pan2, 0, -100, 100, { 114, 6 }, wxDefaultSize, wxSL_BOTTOM);
	Bind(wxEVT_SLIDER, &MyFrame::OnOscPan, this, ID_Pan2);
	wxStaticText *panLabel2 = new wxStaticText(oscPanel[1], wxID_ANY, "Pan", { 155, 28 });

	volOsc[1] = new wxSlider(oscPanel[1], ID_Vol2, 100 - (synthVars.osc[1].GetVolume() * 100), 0, 100, { 270, 6 }, wxDefaultSize, wxSL_VERTICAL);
	Bind(wxEVT_SLIDER, &MyFrame::OnOscVol, this, ID_Vol2);

	freqEditOsc[1] = new wxTextCtrl(oscPanel[1], ID_FreqEdit2, wxString::Format("%.2f", synthVars.osc[1].GetFrequency()), { 6, 50 }, { 50, wxDefaultSize.GetY() }, wxTE_CENTRE | wxTE_PROCESS_ENTER);
	Bind(wxEVT_COMMAND_TEXT_ENTER, &MyFrame::OnOscFreqEdit, this, ID_FreqEdit2);

	freqOsc[1] = new wxSlider(oscPanel[1], ID_Freq2, (int)LogToLin(synthVars.osc[1].GetFrequency(), FREQ_MIN, FREQ_MAX, 1.0, 1000.0), 1, 1000, { 6, 74 });
	if (synthVars.osc[1].IsLFO())
		freqOsc[1]->SetValue((int)synthVars.osc[1].GetFrequency() * 1000.0 / LFO_MAX);
	Bind(wxEVT_SLIDER, &MyFrame::OnOscFreq, this, ID_Freq2);
	wxStaticText *freqLabel2 = new wxStaticText(oscPanel[1], wxID_ANY, "Frequency", { 6, 94 });

	freqFine[1] = new wxTextCtrl(oscPanel[1], ID_Fine2, wxString::Format("%d", synthVars.osc[1].GetFineTune()), { 120, 74 }, { 40, wxDefaultSize.GetY() }, wxTE_CENTRE | wxTE_PROCESS_ENTER);
	Bind(wxEVT_COMMAND_TEXT_ENTER, &MyFrame::OnFreqFine, this, ID_Fine2);
	wxStaticText *fineLabel2 = new wxStaticText(oscPanel[1], wxID_ANY, "Fine", { 120, 100 });

	checkDrone[1] = new wxCheckBox(oscPanel[1], ID_Drone2, "Drone", { 6, 32 });
	checkDrone[1]->SetValue(synthVars.osc[1].GetDrone());
	Bind(wxEVT_CHECKBOX, &MyFrame::OnOscDrone, this, ID_Drone2);

	choiceOscRouting[1] = new wxChoice(oscPanel[1], ID_OscRouting2, { 6, 120 });
	choiceOscRouting[1]->Append(vector<wxString>({ "Mixer", "Osc1 Pitch", "Osc1 Amp", "Osc2 Pitch", "Osc2 Amp", "---", "---","Filter In", "Fltr Cutoff" }));
	choiceOscRouting[1]->SetSelection(7);
	Bind(wxEVT_CHOICE, &MyFrame::OnOscRouting, this, ID_OscRouting2);

	checkLFO[1] = new wxCheckBox(oscPanel[1], ID_OscLFO2, "LFO", { 70, 55 });
	checkLFO[1]->SetValue(synthVars.osc[1].IsLFO());
	Bind(wxEVT_CHECKBOX, &MyFrame::OnOscLFO, this, ID_OscLFO2);

	octRadioBox[1] = new wxRadioBox(oscPanel[1], ID_OctSel2, "Octave", { 214, 6 }, wxDefaultSize, octaveOptions, 1, wxRA_SPECIFY_COLS);
	octRadioBox[1]->SetSelection(synthVars.osc[1].GetOctaveMod() + 2);
	Bind(wxEVT_RADIOBOX, &MyFrame::OnOctaveSelect, this, ID_OctSel2);


	//envelope
	wxPanel *envPanel = new wxPanel(mainPanel, wxID_ANY, { 320, 6 }, { 175, 150 }, wxSIMPLE_BORDER);

	wxSlider *attSlider = new wxSlider(envPanel, ID_Att1, 1000 - synthVars.ADSR.GetAttack(), 1, 1000, { 6, 6 }, wxDefaultSize, wxSL_VERTICAL);
	Bind(wxEVT_SLIDER, &MyFrame::OnEnvelope, this, ID_Att1);
	wxStaticText *attLabel = new wxStaticText(envPanel, wxID_ANY, "A", { 14, 104 });

	wxSlider *decSlider = new wxSlider(envPanel, ID_Dec1, 500 - synthVars.ADSR.GetDecay() * 10, 1, 500, { 30, 6 }, wxDefaultSize, wxSL_VERTICAL);
	Bind(wxEVT_SLIDER, &MyFrame::OnEnvelope, this, ID_Dec1);
	wxStaticText *decLabel = new wxStaticText(envPanel, wxID_ANY, "D", { 38, 104 });

	wxSlider *susSlider = new wxSlider(envPanel, ID_Sus1, 100 - (int)(synthVars.ADSR.GetSustain()*100.0), 0, 100, { 54, 6 }, wxDefaultSize, wxSL_VERTICAL);
	Bind(wxEVT_SLIDER, &MyFrame::OnEnvelope, this, ID_Sus1);
	wxStaticText *susLabel = new wxStaticText(envPanel, wxID_ANY, "S", { 62, 104 });

	wxSlider *relSlider = new wxSlider(envPanel, ID_Rel1,100 - (synthVars.ADSR.GetRelease()<=1000.0?(int)(synthVars.ADSR.GetRelease()*0.05):(int)(50 + (synthVars.ADSR.GetRelease()-1000.0)/(8999/49.0))), 1, 100, { 78, 6 }, wxDefaultSize, wxSL_VERTICAL);
	Bind(wxEVT_SLIDER, &MyFrame::OnEnvelope, this, ID_Rel1);
	wxStaticText *relLabel = new wxStaticText(envPanel, wxID_ANY, "R", { 86, 104 });

	wxChoice *envRoute = new wxChoice(envPanel, ID_EnvRoute1, { 6, 120 }, wxDefaultSize);
	envRoute->Append(vector<wxString>({ "Mixer Amp", "Filter Cutoff" }));
	envRoute->SetSelection(0);
	Bind(wxEVT_CHOICE, &MyFrame::OnEnvRoute, this, ID_EnvRoute1);


	//filter
	wxPanel *ftrPanel = new wxPanel(mainPanel, wxID_ANY, { 320, 160 }, { 175, 150 }, wxSIMPLE_BORDER);

	wxSlider *cutoffSlider = new wxSlider(ftrPanel, ID_CutOff1, 100, 1, 100, { 6, 6 });
	Bind(wxEVT_SLIDER, &MyFrame::OnCutoff, this, ID_CutOff1);
	wxStaticText *coLabel = new wxStaticText(ftrPanel, wxID_ANY, "Cutoff", { 38, 26 });

	wxSlider *resSlider = new wxSlider(ftrPanel, ID_Resonance1, 10, 1, 100, { 6, 40 });
	Bind(wxEVT_SLIDER, &MyFrame::OnResonance, this, ID_Resonance1);
	wxStaticText *resLabel = new wxStaticText(ftrPanel, wxID_ANY, "Resonance", { 38, 60 });

	wxChoice *fltrPoles = new wxChoice(ftrPanel, ID_FltrPoles1, { 6, 120 }, wxDefaultSize);
	fltrPoles->Append(vector<wxString>({ "12 dB/Oct", "24 dB/Oct" }));
	fltrPoles->SetSelection(0);
	Bind(wxEVT_CHOICE, &MyFrame::OnFltrPoles, this, ID_FltrPoles1);
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

	cfgWin->pAI = synthVars.audioIF;
	cfgWin->aiBox->SetSelection(synthVars.audioIF->GetActiveDevice());
	cfgWin->hMidiIn = &synthVars.hMidiIn;
	cfgWin->midiBox->SetSelection(cfgWin->GetActiveMidiID());
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
	/*unique_lock<mutex> outputMutex(synthVars.muxRWOutput);
	synthVars.cvIsOutputProcessed.wait(outputMutex);*/

	//calculate RMS for Output Signal
	double dRMSVolume[2] = { 0.0, 0.0 };

	for (int i = 0; i < AVERAGE_SAMPLES; i++)
	{
		while (!synthVars.bBuffReady.load())
		{
		}

		dRMSVolume[CH_LEFT] += synthVars.dOutputBuffer[CH_LEFT][i] * synthVars.dOutputBuffer[CH_LEFT][i];
		dRMSVolume[CH_RIGHT] += synthVars.dOutputBuffer[CH_RIGHT][i] * synthVars.dOutputBuffer[CH_RIGHT][i];
	}

	/*outputMutex.unlock();
	synthVars.cvIsOutputProcessed.notify_one();*/

	dRMSVolume[CH_LEFT] /= (double)AVERAGE_SAMPLES;
	dRMSVolume[CH_RIGHT] /= (double)AVERAGE_SAMPLES;
	dRMSVolume[CH_LEFT] = sqrt(dRMSVolume[CH_LEFT]);
	dRMSVolume[CH_RIGHT] = sqrt(dRMSVolume[CH_RIGHT]);

	double dB = (dRMSVolume[CH_LEFT] + dRMSVolume[CH_RIGHT] > 0.0) ? 20 * log10((dRMSVolume[CH_LEFT] + dRMSVolume[CH_RIGHT]) / 1.0) : -100000000.0;

	//level + benchmarking
	//SetStatusText(wxString::Format("dB: %.2f    Benchmarks: osc: %.4f, mod: %.4f, fltr: %.4f, buff: %.4f, sample: %.4f", dB, bench.waveGen.load(), bench.modulation.load(), bench.filter.load(), bench.outputBuffer.load(), 1000.0/41000.0));
	SetStatusText(wxString::Format("dB: %.2f", dB));

	double dMinDB = 20 * log10(0.001 / 1.0); //-60 dB
	double dMaxDB = 0.0;

	wxClientDC dc((wxFrame*)masterVBack);
	dc.SetBackground({wxColor(0x555555)});
	dc.Clear();
	dc.SetBrush({wxColor(0x33ff33)});
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.DrawRectangle(13, 97, -12, -(int)((dB - dMinDB) * 97.0/(dMaxDB - dMinDB)));

	/*int linePoint = 97 - (int)((-3 - dMinDB) * 97.0/(dMaxDB - dMinDB));
	dc.SetPen({ wxColor(0x0000ff) });
	dc.DrawLine({ 0, linePoint }, { 16, linePoint });*/
}

void MyFrame::OnOscWave(wxCommandEvent & event)
{
	wxChoice *c = dynamic_cast<wxChoice*>(event.GetEventObject());

	if (c)
	{
		int id = c->GetId() - ID_Wave1;
		
		synthVars.osc[id].SetWave(c->GetSelection() + 1);		
	}
	SetFocus();
}

void MyFrame::OnOscPan(wxCommandEvent & event)
{
	wxSlider *s = dynamic_cast<wxSlider*>(event.GetEventObject());

	if (s)
	{		
		double pan[2];
		//linear panning
		pan[CH_RIGHT] = s->GetValue() / 200.0 + 0.5;
		pan[CH_LEFT] = 1.0 - pan[CH_RIGHT];

		//sine law panning
		pan[CH_RIGHT] = sin(pan[CH_RIGHT] * PI/2.0);
		pan[CH_LEFT] = sin(pan[CH_LEFT] * PI/2.0);


		int id = s->GetId() - ID_Pan1;

		synthVars.osc[id].SetChannelVolume(CH_LEFT, pan[CH_LEFT]);
		synthVars.osc[id].SetChannelVolume(CH_RIGHT, pan[CH_RIGHT]);

		SetStatusText(wxString::Format("Pan %d: %d",id+1, s->GetValue()));
	}

	SetFocus();
}

void MyFrame::OnOscVol(wxCommandEvent & event)
{
	wxSlider *s = dynamic_cast<wxSlider*>(event.GetEventObject());

	if (s)
	{
		int id = s->GetId() - ID_Vol1;
		
		synthVars.osc[id].SetVolume(1.0 - (double)s->GetValue() / 100.0);

		SetStatusText(wxString::Format("Volume %d: %d", id+1, 100 - s->GetValue()));		
	}

	SetFocus();
}

void MyFrame::OnOscFreq(wxCommandEvent & event)
{
	wxSlider *s = dynamic_cast<wxSlider*>(event.GetEventObject());

	if (s)
	{
		int id = s->GetId() - ID_Freq1;
		
		double f1;
		if (synthVars.osc[id].IsLFO())
			f1 = s->GetValue() * LFO_MAX / 1000.0;
		else
			f1 = LinToLog(s->GetValue(), 1.0, 1000.0, FREQ_MIN, FREQ_MAX);

		synthVars.osc[id].SetFrequency(f1);

		freqEditOsc[id]->SetValue(wxString::Format("%.2f", f1));

		SetStatusText(wxString::Format("Pitch %d: %.2f Hz", id+1, f1));		
	}

	SetFocus();
}

void MyFrame::OnOscDrone(wxCommandEvent & event)
{
	wxCheckBox *cb = dynamic_cast<wxCheckBox*>(event.GetEventObject());

	if (cb)
	{
		int id = cb->GetId() - ID_Drone1;

		synthVars.osc[id].SetDrone(cb->GetValue());
	}
	SetFocus();
}

void MyFrame::OnOscRouting(wxCommandEvent & event)
{
	wxChoice *cb = dynamic_cast<wxChoice*>(event.GetEventObject());
	
	if (cb)
	{
		int id = cb->GetId() - ID_OscRouting1;

		
		ZeroMemory(routingMatrix[R_OSC1 + id], R_NUM_ROUTES);

		if (cb->GetSelection() == 0) //Mixer
		{
			routingMatrix[R_OSC1 + id][R_MIXR_A] = true;
		}
		else
		{
			routingMatrix[R_OSC1 + id][cb->GetSelection() - 1] = true;
		}
		
	}

}

void MyFrame::OnOscLFO(wxCommandEvent & event)
{
	wxCheckBox *cb = dynamic_cast<wxCheckBox*>(event.GetEventObject());

	if (cb)
	{
		int id = cb->GetId() - ID_OscLFO1;
		
		synthVars.osc[id].SetLFO(cb->GetValue());

		if (cb->GetValue()) //LFO scaling on
		{
			double fFreq = freqOsc[id]->GetValue() * LFO_MAX / 1000.0;
			freqEditOsc[id]->SetValue(wxString::Format("%.2f", fFreq));
			synthVars.osc[id].SetFrequency(fFreq);
		}
		else //LFO scaling off
		{
			double fFreq = LinToLog(freqOsc[id]->GetValue(), 1.0, 1000.0, FREQ_MIN, FREQ_MAX);
			freqEditOsc[id]->SetValue(wxString::Format("%.2f", fFreq));
			synthVars.osc[id].SetFrequency(fFreq);
		}
		
	}
	SetFocus();
}

void MyFrame::OnFreqFine(wxCommandEvent & event)
{
	wxTextCtrl *tx = dynamic_cast<wxTextCtrl*>(event.GetEventObject());

	if (tx)
	{
		long temp;
		tx->GetValue().ToLong(&temp);
		if (temp < -100)
			temp = -100;
		else if (temp > 100)
			temp = 100;

		tx->SetValue(wxString::Format("%d", temp));

		int id = tx->GetId() - ID_Fine1;
		if (id > 3 || id < 0)
			return;		
			
		synthVars.osc[id].SetFineTune((int8_t)temp);		
	}
}

void MyFrame::OnOctaveSelect(wxCommandEvent & event)
{
	wxRadioBox *rb = dynamic_cast<wxRadioBox*>(event.GetEventObject());

	if (rb)
	{
		int id = rb->GetId() - ID_OctSel1;

		synthVars.osc[id].SetOctave(2 - rb->GetSelection());
	}

	SetFocus();
}

void MyFrame::OnEnvelope(wxCommandEvent & event)
{
	wxSlider *s = dynamic_cast<wxSlider*>(event.GetEventObject());

	if (s)
	{
		int sID = s->GetId();

		if (sID == ID_Att1)
		{
			synthVars.ADSR.SetAttack(1000 - s->GetValue());
		}
		else if (sID == ID_Dec1)
		{
			synthVars.ADSR.SetDecay((1000 - s->GetValue()));
		}
		else if (sID == ID_Sus1)
		{
			synthVars.ADSR.SetSustain((100 - s->GetValue())/100.0);
		}
		else if (sID == ID_Rel1)
		{
			if (s->GetValue() >= 50)
				synthVars.ADSR.SetRelease((100 - s->GetValue()) * 20.0);
			else
				synthVars.ADSR.SetRelease((51 - s->GetValue()) * 196.078);
		}
	}

	SetFocus();
}

void MyFrame::OnEnvRoute(wxCommandEvent & event)
{
	wxChoice *cb = dynamic_cast<wxChoice*>(event.GetEventObject());

	if (cb)
	{
		ZeroMemory(routingMatrix[R_ENV], R_NUM_ROUTES);

		if (cb->GetSelection() == 0) //Mixer Amp
		{
			routingMatrix[R_ENV][R_MIXR_A] = true;
		}
		else if (cb->GetSelection() == 1) //Filter Cutoff
		{
			routingMatrix[R_ENV][R_FLTR_C] = true;
		}
	}

	SetFocus();
}

void MyFrame::OnCutoff(wxCommandEvent & event)
{
	wxSlider *s = dynamic_cast<wxSlider*>(event.GetEventObject());

	if (s)
	{
		synthVars.nFilterCutoff = LinToLog(s->GetValue(), 1, 100, 30, 22000);
	}

	SetFocus();
}

void MyFrame::OnResonance(wxCommandEvent & event)
{
	wxSlider *s = dynamic_cast<wxSlider*>(event.GetEventObject());

	if (s)
	{
		synthVars.dResonance = s->GetValue() / 20.0;
	}

	SetFocus();
}

void MyFrame::OnFltrPoles(wxCommandEvent & event)
{
	wxChoice *cb = dynamic_cast<wxChoice*>(event.GetEventObject());

	if (cb)
	{
		synthVars.bFourthOrder = cb->GetSelection() ? true : false;
	}
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
		int id = tx->GetId() - ID_FreqEdit1;
		
		double r;
		tx->GetValue().ToCDouble(&r);
		synthVars.osc[id].SetFrequency(r);

		if (synthVars.osc[id].IsLFO())
			freqOsc[id]->SetValue((int)synthVars.osc[id].GetFrequency() * 1000.0 / LFO_MAX);
		else
			freqOsc[id]->SetValue((int)LogToLin(r, FREQ_MIN, FREQ_MAX, 1.0, 1000.0));
	}
}


void MyFrame::OnHello(wxCommandEvent& event)
{
	wxLogMessage("Hello!");
}

MyFrame::~MyFrame()
{
	if (CfgButton)
		delete CfgButton;
}

double synthFunction(double d, byte channel)
{
	double dOutputs[R_NUM_DEVS] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

	const uint8_t *deviceMap = synthVars.deviceMap;	 

	//auto tStart = std::chrono::high_resolution_clock::now();

	//Generate outputs from oscillators
	for (int i = 0; i < 3; i++)
	{
		if (synthVars.osc[i].GetDrone())
		{
			dOutputs[i] = OSC_VOLUME * synthVars.osc[i].Play(synthVars.osc[i].GetFrequency(), d, channel);
		}
		else
		{
			for (auto note : synthVars.vNotesOn)
			{
					uint8_t nSemiTone = note + synthVars.osc[i].GetOctaveMod()*12;
					if (nSemiTone >= 12 * 9 || nSemiTone < 0) continue;
					//double dFrequency = C_SHARP_0 * pow(2, nSemiTone / 12.0);
					if (routingMatrix[R_ENV][R_MIXR_A])
						dOutputs[i] += synthVars.ADSR.GetAmplitude() * OSC_VOLUME * synthVars.osc[i].Play(synthVars.dNotes[nSemiTone], d, channel);
					/*else if (synthVars.numKeysDown > 0)*/
					else
						dOutputs[i] += OSC_VOLUME * synthVars.osc[i].Play(synthVars.dNotes[nSemiTone], d, channel);
			}
		}
	}

	/*auto tOsc = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(tOsc - tStart).count();
	bench.waveGen.store(duration);

	tStart = std::chrono::high_resolution_clock::now();*/
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
				dOutputs[deviceMap[i]] = OSC_VOLUME * synthVars.osc[deviceMap[i]].Play(synthVars.osc[deviceMap[i]].GetFrequency(), d, channel);
		}
		else if (i == R_OSC1_A || i == R_OSC2_A || i == R_OSC3_A)
		{
			//amplitude modulation
			synthVars.osc[deviceMap[i]].ResetAM();

			for (int m = 0; m < 3; m++)
			{
				if (routingMatrix[m][i])
				{
					double dAM = synthVars.osc[m].Play(synthVars.osc[m].GetFrequency(), d, channel) + (1.0 - synthVars.osc[m].GetVolume());
					synthVars.osc[deviceMap[i]].AddAM(dAM);
				}
			}

			//regenerate outputs
			if (synthVars.osc[deviceMap[i]].GetDrone())
				dOutputs[deviceMap[i]] = OSC_VOLUME * synthVars.osc[deviceMap[i]].Play(synthVars.osc[deviceMap[i]].GetFrequency(), d, channel);
		}
		else if (i == R_FLTR_I) //Filter input
		{
			/*auto tMod = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(tMod - tStart).count();
			bench.modulation.store(duration);

			tStart = std::chrono::high_resolution_clock::now();*/

			for (int j = 0; j < 3; j++)
			{
				dOutputs[R_FLTR] += routingMatrix[j][R_FLTR_I] ? dOutputs[j] : 0.0;
			}			
		}
		else if (i == R_FLTR_C) //Filter cutoff modulation
		{
			double dCutoff = synthVars.nFilterCutoff;

			for (int j = 0; j < 3; j++)
			{
				if (routingMatrix[j][R_FLTR_C]) //osc modulation
				{
					double dMod = synthVars.osc[j].Play(synthVars.osc[j].GetFrequency(), d, channel) + (1.0 - synthVars.osc[j].GetVolume());
					double dScale = LinToLog(dMod, -1.0, 1.0, 0.001, 1.0);

					dCutoff *= dScale;
				}
			}

			if (routingMatrix[R_ENV][R_FLTR_C]) //env modulation
			{
				double dMod = synthVars.ADSR.GetAmplitude();
				double dScale = LinToLog(dMod, 0.0, 1.0, 0.000001, 1.0);

				dCutoff *= dScale;
			}

			//Apply Low Pass Filtering to signals going through filter	
			static double dDelayBuffer[2][2] = { {0.0, 0.0}, {0.0, 0.0} };
			static double dDelayBuffer2[2][2] = { {0.0, 0.0}, {0.0, 0.0} };
			
			dOutputs[R_FLTR] = StateVLowPass(dOutputs[R_FLTR], dDelayBuffer[channel], dCutoff, synthVars.dResonance); //-6 dB/Oct
			//second order
			dOutputs[R_FLTR] = StateVLowPass(dOutputs[R_FLTR], dDelayBuffer2[channel], dCutoff, synthVars.dResonance); //-12 dB/Oct

			if (synthVars.bFourthOrder)
			{
				static double dDelayBuffer3[2][2] = { {0.0, 0.0}, {0.0, 0.0} };
				static double dDelayBuffer4[2][2] = { {0.0, 0.0}, {0.0, 0.0} };

				dOutputs[R_FLTR] = StateVLowPass(dOutputs[R_FLTR], dDelayBuffer3[channel], dCutoff, synthVars.dResonance);
				dOutputs[R_FLTR] = StateVLowPass(dOutputs[R_FLTR], dDelayBuffer4[channel], dCutoff, synthVars.dResonance); //-24 dB/Oct
			}

			/*auto tFltr = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(tFltr - tStart).count();
			bench.filter.store(duration);*/

		}
		else if (i == R_MIXR_A)
		{
			for (int j = 0; j < R_NUM_DEVS-1; j++)
				dOutputs[R_MIXR] += routingMatrix[j][R_MIXR_A] ? dOutputs[j] : 0.0;
		}
	}

	double dOut = dOutputs[R_MIXR] * (synthVars.nMasterVolume / 100.0);

	//quick distortion
	//dOut = BitCrush(SoftClip(dOut, 20.0, 10.0));	

	//tStart = std::chrono::high_resolution_clock::now();

	//unique_lock<mutex> outputMutex(synthVars.muxRWOutput);
	synthVars.bBuffReady.store(false);
	synthVars.dOutputBuffer[channel][synthVars.nBufferPos] = dOut;

	if (channel == CH_RIGHT) //let output data be available after both channels have been porcessed
	{
		synthVars.bBuffReady.store(true);

		synthVars.nBufferPos++;
		synthVars.nBufferPos %= AVERAGE_SAMPLES;

		//synthVars.cvIsOutputProcessed.notify_one();		
	}
		
	/*auto tBuff = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::microseconds>(tBuff - tStart).count();
	bench.outputBuffer.store(duration);*/
	
	static double dHPBuffer[2][2] = { {0.0, 0.0}, {0.0, 0.0 } };
	return BiQuadHighPass(dOut, dHPBuffer[channel], 30.0, 1.0); //filter off everything below 30Hz
}