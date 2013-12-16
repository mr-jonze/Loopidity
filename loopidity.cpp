
#include "loopidity.h"


/* Loopidity Class private varables */

// recording state
unsigned int Loopidity::CurrentSceneN = 0 ;
unsigned int Loopidity::NextSceneN = 0 ;
//bool Loopidity::IsRolling = false ;
bool Loopidity::ShouldSceneAutoChange = false ;

// audio data
Scene* Loopidity::Scenes[N_SCENES] = {0} ;
SceneSdl* Loopidity::SdlScenes[N_SCENES] = {0} ;
unsigned int Loopidity::NFramesPerPeriod = 0 ;
unsigned int Loopidity::NFramesPerGuiInterval = 0 ;
Sample* Loopidity::Buffer1 = 0 ;
Sample* Loopidity::Buffer2 = 0 ;
vector<Sample> Loopidity::PeaksIn ;
vector<Sample> Loopidity::PeaksOut ;
Sample Loopidity::TransientPeaks[N_PORTS] = {0} ;
Sample Loopidity::TransientPeakInMix = 0 ;
Sample Loopidity::TransientPeakOutMix = 0 ;


/* Loopidity Class public functions */

// setup

bool Loopidity::Init(bool shouldMonitorInputs , bool shouldAutoSceneChange ,
										 unsigned int recordBufferSize)
{
	// sanity check
	if (Scenes[0]) return false ;

	// disable AutoSceneChange if SCENE_CHANGE_ARG given
	if (!shouldAutoSceneChange) ToggleAutoSceneChange() ;

	// instantiate Scenes (models) and SdlScenes (views)
	bool initStatus = true ;
	for (unsigned int sceneN = 0 ; sceneN < N_SCENES ; ++sceneN)
	{
		Uint32 frameColor = (!sceneN)? STATE_PLAYING_COLOR : STATE_IDLE_COLOR ;
		Scenes[sceneN]    = new Scene(sceneN , JackIO::GetRecordBufferSize()) ;
		SdlScenes[sceneN] = new SceneSdl(Scenes[sceneN] , sceneN , frameColor) ;
		UpdateView(sceneN) ;

		initStatus = initStatus && !!Scenes[sceneN] && !!SdlScenes[sceneN] ;
	}

	// initialize JACK
	string errMsg ;
	switch (JackIO::Init(Scenes[0] , 0 , shouldMonitorInputs , recordBufferSize))
	{
		case JACK_FAIL:      errMsg = JACK_FAIL_MSG ;
		case JACK_BUFF_FAIL: errMsg = ZERO_BUFFER_SIZE_MSG ;
		case JACK_MEM_FAIL:  errMsg = INSUFFICIENT_MEMORY_MSG ;
												 LoopiditySdl::Alert(errMsg.c_str()) ; return false ;
	}

	// initialize scope/VU peaks cache
	for (unsigned int peakN = 0 ; peakN < N_PEAKS_TRANSIENT ; ++peakN)
		{ PeaksIn.push_back(0.0) ; PeaksOut.push_back(0.0) ; }

	// get handles on JACK buffers for scope/VU peaks
	Buffer1 = JackIO::GetBuffer1() ; Buffer2 = JackIO::GetBuffer2() ;

	return initStatus ;
}


// user actions

void Loopidity::ToggleAutoSceneChange() { ShouldSceneAutoChange = !ShouldSceneAutoChange ; }

void Loopidity::ToggleScene()
{
DEBUG_TRACE_LOOPIDITY_TOGGLESCENE_IN

	unsigned int prevSceneN = NextSceneN ; NextSceneN = (NextSceneN + 1) % N_SCENES ;
	UpdateView(prevSceneN) ; UpdateView(NextSceneN) ;
printf("xx\n");
	JackIO::SetNextScene(Scenes[NextSceneN] , NextSceneN) ;

	if (!Scenes[CurrentSceneN]->isRolling)
	{
		prevSceneN = CurrentSceneN ; CurrentSceneN = NextSceneN ;
		Scenes[prevSceneN]->reset() ; // SdlScenes[prevSceneN]->reset() ;
		UpdateView(prevSceneN) ; UpdateView(NextSceneN) ;
printf("xx\n");
		JackIO::SetCurrentScene(Scenes[NextSceneN] , NextSceneN) ;
	}

DEBUG_TRACE_LOOPIDITY_TOGGLESCENE_OUT
}

void Loopidity::ToggleState()
{
DEBUG_TRACE_LOOPIDITY_TOGGLESTATE_IN

	if (Scenes[CurrentSceneN]->isRolling) Scenes[CurrentSceneN]->toggleState() ;
	else { Scenes[CurrentSceneN]->startRolling() ; SdlScenes[CurrentSceneN]->startRolling() ; }
	UpdateView(CurrentSceneN) ;

DEBUG_TRACE_LOOPIDITY_TOGGLESTATE_OUT
}

void Loopidity::DeleteLoop(unsigned int sceneN , unsigned int loopN)
{
DEBUG_TRACE_LOOPIDITY_DELETELOOP_IN

	SdlScenes[sceneN]->deleteLoop(loopN) ; Scenes[sceneN]->deleteLoop(loopN) ;

DEBUG_TRACE_LOOPIDITY_DELETELOOP_OUT
}

void Loopidity::DeleteLastLoop()
{
DEBUG_TRACE_LOOPIDITY_DELETELASTLOOP_IN

	DeleteLoop(CurrentSceneN , Scenes[CurrentSceneN]->loops.size() - 1) ;

DEBUG_TRACE_LOOPIDITY_DELETELASTLOOP_OUT
}

void Loopidity::IncLoopVol(unsigned int sceneN , unsigned int loopN , bool isInc)
{
DEBUG_TRACE_LOOPIDITY_INCLOOPVOL_IN

	Loop* loop = Scenes[sceneN]->getLoop(loopN) ; if (!loop) return ;

	float* vol = &loop->vol ;
	if (isInc) { *vol += LOOP_VOL_INC ; if (*vol > 1.0) *vol = 1.0 ; }
	else { *vol -= LOOP_VOL_INC ; if (*vol < 0.0) *vol = 0.0 ; }

DEBUG_TRACE_LOOPIDITY_INCLOOPVOL_OUT
}

void Loopidity::ToggleLoopIsMuted(unsigned int sceneN , unsigned int loopN)
{
DEBUG_TRACE_LOOPIDITY_TOGGLELOOPISMUTED_IN

	Loop* loop = Scenes[sceneN]->getLoop(loopN) ; loop->isMuted = !loop->isMuted ;

DEBUG_TRACE_LOOPIDITY_TOGGLELOOPISMUTED_OUT
}

void Loopidity::ToggleSceneIsMuted()
	{ Scene* scene = Scenes[CurrentSceneN] ; scene->isMuted = !scene->isMuted ; }

void Loopidity::ResetScene(unsigned int sceneN)
{
DEBUG_TRACE_LOOPIDITY_RESETSCENE_IN

	Scenes[sceneN]->reset() ; SdlScenes[sceneN]->reset() ;

DEBUG_TRACE_LOOPIDITY_RESETSCENE_OUT
}

void Loopidity::ResetCurrentScene() { ResetScene(CurrentSceneN) ; }

void Loopidity::Reset()
{
DEBUG_TRACE_LOOPIDITY_RESET_IN

	CurrentSceneN = NextSceneN = 0 ; JackIO::Reset(Scenes[0] , 0) ;
	for (unsigned int sceneN = 0 ; sceneN < N_SCENES ; ++sceneN) ResetScene(sceneN) ;

DEBUG_TRACE_LOOPIDITY_RESET_OUT
}

void Loopidity::Cleanup()
{
	for (unsigned int sceneN = 0 ; sceneN < N_SCENES ; ++sceneN)
		if (SdlScenes[sceneN]) SdlScenes[sceneN]->cleanup() ;
	LoopiditySdl::Cleanup() ;
}


// getters/setters

void Loopidity::SetMetaData(unsigned int sampleRate , unsigned int frameSize , unsigned int nFramesPerPeriod)
{
	float interval = (float)GUI_UPDATE_INTERVAL_SHORT * 0.001 ;
	NFramesPerGuiInterval = (unsigned int)(sampleRate * interval) ;

	Scene::SetMetaData(sampleRate , frameSize , nFramesPerPeriod) ;
}

unsigned int Loopidity::GetCurrentSceneN() { return CurrentSceneN ; }

unsigned int Loopidity::GetNextSceneN() { return NextSceneN ; }

//unsigned int Loopidity::GetLoopPos() { return Scenes[CurrentSceneN]->getLoopPos() ; }

//bool Loopidity::GetIsRolling() { return IsRolling ; }

//bool Loopidity::GetShouldSaveLoop() { return Scenes[CurrentSceneN]->shouldSaveLoop ; }

//bool Loopidity::GetDoesPulseExist() { return Scenes[CurrentSceneN]->doesPulseExist ; }

Sample* Loopidity::GetTransientPeakIn() { return &TransientPeakInMix ; }

//Sample* Loopidity::GetTransientPeakOut() { return &TransientPeakOutMix ; }

Sample Loopidity::GetPeak(Sample* buffer , unsigned int nFrames)
{
	Sample peak = 0.0 ;
	try // TODO: this function is unsafe
	{
		for (unsigned int frameN = 0 ; frameN < nFrames ; ++frameN)
			{ Sample sample = fabs(buffer[frameN]) ; if (peak < sample) peak = sample ; }
	}
	catch(int ex) { printf(GETPEAK_ERROR_MSG) ; }
	return peak ;
}


// event handlers

void Loopidity::OnLoopCreation(unsigned int sceneN , Loop* newLoop)
{
DEBUG_TRACE_LOOPIDITY_ONLOOPCREATION_IN

	Scene* scene = Scenes[sceneN] ; SceneSdl* sdlScene = SdlScenes[sceneN] ;
	if (scene->addLoop(newLoop)) sdlScene->addLoop(newLoop , scene->loops.size() - 1) ;

	UpdateView(sceneN) ;

DEBUG_TRACE_LOOPIDITY_ONLOOPCREATION_OUT
}

void Loopidity::OnSceneChange(unsigned int nextSceneN)
{
DEBUG_TRACE_LOOPIDITY_ONSCENECHANGE_IN //else { Scenes[CurrentSceneN]->startRolling() ; SdlScenes[CurrentSceneN]->startRolling() ; }

//	if (!Scenes[CurrentSceneN]->isRolling) { Scenes[CurrentSceneN]->reset() ; }

	unsigned int prevSceneN = CurrentSceneN ; CurrentSceneN = NextSceneN = nextSceneN ;
	SdlScenes[prevSceneN]->drawScene(SdlScenes[prevSceneN]->inactiveSceneSurface , 0 , 0) ;
	UpdateView(prevSceneN) ; UpdateView(nextSceneN) ;

	if (ShouldSceneAutoChange) do ToggleScene() ; while (!Scenes[NextSceneN]->loops.size()) ;

DEBUG_TRACE_LOOPIDITY_ONSCENECHANGE_OUT
}


// helpers

void Loopidity::ScanTransientPeaks()
{
#if SCAN_TRANSIENT_PEAKS_DATA
	Scene* currentScene = Scenes[CurrentSceneN] ; unsigned int frameN = currentScene->frameN ;
// TODO; first NFramesPerGuiInterval duplicated
	unsigned int currentFrameN = (frameN < NFramesPerGuiInterval)? 0 : frameN - NFramesPerGuiInterval ;

	// initialize with inputs
	Sample peakIn1 = GetPeak(&(Buffer1[currentFrameN]) , NFramesPerGuiInterval) ;
	Sample peakIn2 = GetPeak(&(Buffer2[currentFrameN]) , NFramesPerGuiInterval) ;

	// add in unmuted tracks
	Sample peakOut1 = 0.0 , peakOut2 = 0.0 ; unsigned int nLoops = currentScene->loops.size() ;
	for (unsigned int loopN = 0 ; loopN < nLoops ; ++loopN)
	{
		Loop* loop = currentScene->getLoop(loopN) ; if (currentScene->isMuted && loop->isMuted) continue ;

		peakOut1 += GetPeak(&(loop->buffer1[currentFrameN]) , NFramesPerGuiInterval) * loop->vol ;
		peakOut2 += GetPeak(&(loop->buffer2[currentFrameN]) , NFramesPerGuiInterval) * loop->vol ;
	}

	Sample peakIn  = (peakIn1 + peakIn2)   / N_INPUT_CHANNELS ;
	Sample peakOut = (peakOut1 + peakOut2) / N_OUTPUT_CHANNELS ;
	if (peakOut > 1.0) peakOut = 1.0 ;

	// load scope peaks (mono mix)
	PeaksIn.pop_back()  ; PeaksIn.insert(PeaksIn.begin()   , peakIn) ;
	PeaksOut.pop_back() ; PeaksOut.insert(PeaksOut.begin() , peakOut) ;

	// load VU peaks (per channel)
	TransientPeakInMix = peakIn ;   TransientPeakOutMix = peakOut ;
	TransientPeaks[0]  = peakIn1 ;  TransientPeaks[1]   = peakIn2 ;
	TransientPeaks[2]  = peakOut1 ; TransientPeaks[3]   = peakOut2 ;
#endif // #if SCAN_TRANSIENT_PEAKS_DATA
}

void Loopidity::UpdateView(unsigned int sceneN) { SdlScenes[sceneN]->updateStatus() ; }
// { if (Scenes[sceneN]->isRolling) SdlScenes[sceneN]->updateStatus() ; }

void Loopidity::OOM() { DEBUG_TRACE_LOOPIDITY_OOM_IN LoopiditySdl::SetStatusC(OUT_OF_MEMORY_MSG) ; }


/* main */

int Loopidity::Main(int argc , char** argv)
{
#if DEBUG_TRACE
cout << INIT_MSG << endl ;
#endif // #if DEBUG_TRACE

  // parse command line arguments
  bool isMonitorInputs = true , isAutoSceneChange = true ; unsigned int bufferSize ;
  for (int argN = 0 ; argN < argc ; ++argN)
    if (!strcmp(argv[argN] , MONITOR_ARG)) isMonitorInputs = false ;
    else if (!strcmp(argv[argN] , SCENE_CHANGE_ARG)) isAutoSceneChange = false ;
    // TODO: user defined buffer sizes via command line
#if USER_DEFINED_BUFFER
//    else if (!strcmp(argv[argN] , BUFFER_SIZE_ARG)) isAutoSceneChange = bufferSize = arg ;
  bufferSize = 0 ; // nyi
#else
  bufferSize = 0 ;
#endif // #if DEBUG_STATIC_BUFFER_SIZE

  // initialize Loopidity (controller) and instantiate SdlScenes (views)
  if (!Init(isMonitorInputs , isAutoSceneChange , bufferSize)) return EXIT_FAILURE ;

  // initialize Loopidity
  if (!LoopiditySdl::Init(SdlScenes , &PeaksIn , &PeaksOut , TransientPeaks))
    return EXIT_FAILURE ;

#if DEBUG_TRACE
cout << INIT_SUCCESS_MSG << endl ;
#endif // #if DEBUG_TRACE

	// blank screen and draw header
	SDL_FillRect(LoopiditySdl::Screen , 0 , LoopiditySdl::WinBgColor) ;
	LoopiditySdl::DrawHeader() ;

	// main loop
	bool done = false ; SDL_Event event ; Uint16 timerStart = SDL_GetTicks() , elapsed ;
	while (!done)
	{
		// poll events and pass them off to our controller
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_QUIT: done = true ; break ;
				case SDL_KEYDOWN: LoopiditySdl::HandleKeyEvent(&event) ; break ;
//				case SDL_MOUSEBUTTONDOWN: LoopiditySdl::HandleMouseEvent(&event) ; break ;
				case SDL_USEREVENT: LoopiditySdl::HandleUserEvent(&event) ; break ;
				default: break ;
			}
		}
/*
Uint64 now = SDL_GetPerformanceCounter() ;
Uint64 elapsed = (now - DbgMainLoopTs) / SDL_GetPerformanceFrequency() ; DbgMainLoopTs = now ;
if (LoopiditySdl::GuiLongCount == GUI_LONGCOUNT)
	cout << "DbgMainLoopTs=" << elapsed << endl ;
*/
		// throttle framerate
		elapsed = SDL_GetTicks() - timerStart ;
		if (elapsed >= GUI_UPDATE_INTERVAL_SHORT) timerStart = SDL_GetTicks() ;
		else { SDL_Delay(1) ; continue ; }

		// draw low priority
		if (!(LoopiditySdl::GuiLongCount = (LoopiditySdl::GuiLongCount + 1) % GUI_LONGCOUNT))
			{ /*updateMemory() ;*/ LoopiditySdl::DrawStatusText() ; }

		// draw high priority
		ScanTransientPeaks() ; //updateVUMeters() ;
		LoopiditySdl::DrawScenes() ;
		LoopiditySdl::DrawScopes() ;

		SDL_Flip(LoopiditySdl::Screen) ;
	} // while (!done)

	return EXIT_SUCCESS ;
}
