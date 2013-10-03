
#ifndef _JACK_IO_H_
#define _JACK_IO_H_


#include <jack/jack.h>

#include "loopidity.h"
class Loop ;
class Scene ;


class JackIO
{
	friend class Loopidity ;

  private:

		// setup
    static unsigned int Init(Scene* currentScene , unsigned int currentSceneN , bool isMonitorInputs) ;
		static void Reset(Scene* currentScene , unsigned int currentSceneN) ;

		// getters/setters
		static SAMPLE* GetBuffer1() ;
		static SAMPLE* GetBuffer2() ;
		static unsigned int GetRecordBufferSize() ;
		static bool SetRecordBufferSize(unsigned int recordBufferSize) ;
		static unsigned int GetNFramesPerPeriod() ;
		static const unsigned int GetFrameSize() ;
		static unsigned int GetSampleRate() ;
		static unsigned int GetBytesPerSecond() ;
		static void SetCurrentScene(Scene* currentScene , unsigned int currentSceneN) ;
		static void SetNextScene(Scene* nextScene , unsigned int nextSceneN) ;

  private:

		// JACK handles
    static jack_client_t* Client ;
    static jack_port_t* PortInput1 ;
    static jack_port_t* PortInput2 ;
    static jack_port_t* PortOutput1 ;
    static jack_port_t* PortOutput2 ;

		// audio data
		static Scene* CurrentScene ;
		static Scene* NextScene ;
		static unsigned int CurrentSceneN ;
		static unsigned int NextSceneN ;
		static SAMPLE* Buffer1 ;
		static SAMPLE* Buffer2 ;
		static unsigned int RecordBufferSize ;

		// event structs
		static SDL_Event EventLoopCreation ;
		static Loop* NewLoop ;
		static SDL_Event EventSceneChange ;
		static unsigned int EventLoopCreationSceneN ;
		static unsigned int EventSceneChangeSceneN ;

		// server state
		static unsigned int NFramesPerPeriod ;
		static const unsigned int FRAME_SIZE ;
		static unsigned int PeriodSize ;
		static unsigned int SampleRate ;
		static unsigned int BytesPerSecond ;

		// misc flags
		static bool ShouldMonitorInputs ;

		// JACK callbacks
    static int ProcessCallback(jack_nframes_t nframes , void* arg) ;
    static int SetBufferSizeCallback(jack_nframes_t nframes , void* arg) ;
    static void ShutdownCallback(void* arg) ;

// DEBUG
public:
static unsigned int GetPeriodSize() { return PeriodSize ; }
// DEBUG
} ;


#endif // #ifndef _JACK_IO_H_
