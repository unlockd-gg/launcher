//////////////////////////////////////////////////////////////////////////
//
// ZLauncher - Patcher system - Part of the ZUpdater suite
// Felipe "Zoc" Silveira - (c) 2016-2018
//
//////////////////////////////////////////////////////////////////////////
//
// ZLauncherTokenThread.cpp
// Header file for the worker thread used in the launcher process
//
//////////////////////////////////////////////////////////////////////////

#ifndef _ZLAUNCHERTOKENTHREAD_H_
#define _ZLAUNCHERTOKENTHREAD_H_

#include <wx/thread.h>
#include <wx/event.h>
#include <vector>
#include "ZLauncherFrame.h"

// Lets avoid adding an unnecessary header here.
typedef unsigned long       DWORD;

//////////////////////////////////////////////////////////////////////////
// Declare the events used to update the main frame

//wxDECLARE_EVENT(wxEVT_COMMAND_UPDATE_GAME_LIST, wxThreadEvent);
//wxDECLARE_EVENT(wxEVT_COMMAND_CLICKED_GAME_LIST, wxThreadEvent);

//wxDECLARE_EVENT(wxEVT_COMMAND_UPDATE_PROGRESS_BAR, wxThreadEvent);
//wxDECLARE_EVENT(wxEVT_COMMAND_UPDATE_PROGRESS_TEXT, wxThreadEvent);

//wxDECLARE_EVENT(wxEVT_COMMAND_HTML_SET_CONTENT, wxThreadEvent);
//wxDECLARE_EVENT(wxEVT_COMMAND_HTML_LOAD_PAGE, wxThreadEvent);

//wxDECLARE_EVENT(wxEVT_COMMAND_ENABLE_LAUNCH_BUTTON, wxThreadEvent);


//////////////////////////////////////////////////////////////////////////
// Auxiliary Patch structure


//////////////////////////////////////////////////////////////////////////
// Implement the worker thread

class ZLauncherTokenThread : public wxThread
{
private:
	ZLauncherTokenThread(); // Not allowed to be called

	static std::size_t WriteCallback(char* contents, size_t size, size_t nmemb, void* userp)
	{
		((std::string*)userp)->append((char*)contents, size * nmemb);
		return size * nmemb;
	}

public:
	ZLauncherTokenThread(ZLauncherFrame* handler, const wxString& updateURL, const wxString& versionFile, const wxString& targetDirectory);
	~ZLauncherTokenThread();

protected:
	virtual ExitCode Entry();
	ZLauncherFrame* m_pHandler;

	std::string					m_updateURL;
	std::string					m_versionFile;
	std::string					m_targetDirectory;

	// unused but leaving it in for reference
	uint64_t					m_LatestVersion;

	// auth timeout count
	unsigned m_count;

	//////////////////////////////////////////////////////////////////////////
	// This functions are everything the thread needs to get the updates applied correctly.
	// Those functions 

	/**
	* Download the Update XML from the server and process it. The output is stored in m_Patches.
	* Returns if file was downloaded and processed successfully
	*/
	bool CheckForUpdates(const std::string& updateURL, const uint64_t& currentBuildNumber);

	/**
	* Download file, from given URL, without renaming, to given targetPath.
	* Note: The filename to be saved is the last part (i.e. after last slash) of given URL. Complex URLs might be a problem for this function.
	*/
	bool SimpleDownloadFile(const std::string& URL, const std::string& targetPath = "./");

};

#endif // _ZLAUNCHERTHREAD_H_
