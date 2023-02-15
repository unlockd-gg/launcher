//////////////////////////////////////////////////////////////////////////
//
// ZLauncher - Patcher system - Part of the ZUpdater suite
// Felipe "Zoc" Silveira - (c) 2016-2018
//
//////////////////////////////////////////////////////////////////////////
//
// ZLauncherThread.cpp
// Header file for the worker thread used in the launcher process
//
//////////////////////////////////////////////////////////////////////////

#ifndef _ZLAUNCHERTHREAD_H_
#define _ZLAUNCHERTHREAD_H_

#include <wx/thread.h>
#include <wx/event.h>
#include <vector>
#include "ZLauncherFrame.h"

// Lets avoid adding an unnecessary header here.
typedef unsigned long       DWORD;

//////////////////////////////////////////////////////////////////////////
// Declare the events used to update the main frame

wxDECLARE_EVENT(wxEVT_COMMAND_UPDATE_GAME_LIST, wxThreadEvent);
//wxDECLARE_EVENT(wxEVT_COMMAND_CLICKED_GAME_LIST, wxThreadEvent);

//wxDECLARE_EVENT(wxEVT_COMMAND_UPDATE_PROGRESS_BAR, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_COMMAND_UPDATE_PROGRESS_TEXT, wxThreadEvent);

//wxDECLARE_EVENT(wxEVT_COMMAND_HTML_SET_CONTENT, wxThreadEvent);
//wxDECLARE_EVENT(wxEVT_COMMAND_HTML_LOAD_PAGE, wxThreadEvent);

//wxDECLARE_EVENT(wxEVT_COMMAND_ENABLE_LAUNCH_BUTTON, wxThreadEvent);


//////////////////////////////////////////////////////////////////////////
// Auxiliary Patch structure

/*
struct Patch
{
	uint64_t					sourceBuildNumber;
	uint64_t					targetBuildNumber;

	std::string					fileURL;
	DWORD						fileLength;
	std::string					fileMD5;
};
*/

//////////////////////////////////////////////////////////////////////////
// Implement the worker thread

class ZLauncherThread : public wxThread
{
private:
	ZLauncherThread(); // Not allowed to be called


// Callback function. It will be called by EnumWindows function
//  as many times as there are windows
	
	static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
	{
		const DWORD TITLE_SIZE = 1024;
		WCHAR windowTitle[TITLE_SIZE];

		WCHAR buffer[8192];

		// this is working but the text is truncated and I can't figure it out
		// SO I'm trying to switch it back to the way UE4 does it, which means dealing with unicode....
		// muting this for now.
		//GetWindowTextW(hwnd, windowTitle, 2048);
		//std::wstring title(&windowTitle[0]);

		GetWindowText(hwnd, buffer, 8192);
		std::wstring title(&buffer[0]);

		//int length = ::GetWindowTextLength(hwnd);
		

		// Retrieve the pointer passed into this callback, and re-'type' it.
		// The only way for a C API to pass arbitrary data is by means of a void*.
		std::vector<std::wstring>& titles =
			*reinterpret_cast<std::vector<std::wstring>*>(lParam);
		titles.push_back(title);


		return true; // function must return true if you want to continue enumeration
	}

	// quick and dirty instead of parsing the url 
	std::string get_str_between_two_str(const std::string& s,
		const std::string& start_delim,
		const std::string& stop_delim)
	{
		unsigned first_delim_pos = s.find(start_delim);
		unsigned end_pos_of_first_delim = first_delim_pos + start_delim.length();
		unsigned last_delim_pos = s.find(stop_delim);

		return s.substr(end_pos_of_first_delim,
			last_delim_pos - end_pos_of_first_delim);
	}

	// http callback
	static std::size_t httpcallback(
		const char* in,
		std::size_t size,
		std::size_t num,
		std::string* out)
	{
		const std::size_t totalBytes(size * num);
		out->append(in, totalBytes);
		return totalBytes;
	}

	static std::size_t WriteCallback(char* contents, size_t size, size_t nmemb, void* userp)
	{
		((std::string*)userp)->append((char*)contents, size * nmemb);
		return size * nmemb;
	}

public:
	ZLauncherThread(ZLauncherFrame *handler, const wxString& updateURL, const wxString& versionFile, const wxString& targetDirectory);
	~ZLauncherThread();
	
protected:
	virtual ExitCode Entry();
	ZLauncherFrame*	m_pHandler;

	std::string					m_updateURL;
	std::string					m_versionFile;
	std::string					m_targetDirectory;

	// unused but leaving it in for reference
	uint64_t					m_LatestVersion;

	/*

	std::string					m_ApplicationName;
	uint64_t					m_LocalCurrentVersion;
	
	uint64_t					m_BestPathDownloadSize;
	std::vector<Patch>			m_Patches;
	std::vector<unsigned int>	m_BestPatchPath;
	std::string					m_UpdateDescription;
	*/

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
	* Returns the latest version fetched from the XML.
	*/
	//uint64_t GetLatestVersion();

	/**
	* With all the patches stored in g_Patches, find the best path to download updates, using the file download size as the value that should be minimized.
	* Returns true if there is a path from the source version to the target version (i.e. if the update is possible)
	*/
	//bool GetSmallestUpdatePath(const uint64_t& sourceBuildNumber, const uint64_t& targetBuildNumber, std::vector<unsigned int>& path, uint64_t& pathFileSize);

	/**
	* Returns true if it was able to determine the current version stored in configFile.
	* The found version is returned in the "version" variable.
	* NOTE: If no file is found, it returns true and version gets the value 0 (i.e. No previous version found, perform full installation)
	*/
	//bool GetTargetCurrentVersion(const std::string& configFile, uint64_t& version);

	/**
	* Calculate the MD5 hash of a file
	*/
	//std::string MD5File(std::string fileName);

	/**
	* Updates given configFile to store the supplied version
	*/
	//bool SaveTargetNewVersion(const std::string& configFile, const uint64_t& version);

	/**
	* Download file, from given URL, without renaming, to given targetPath.
	* Note: The filename to be saved is the last part (i.e. after last slash) of given URL. Complex URLs might be a problem for this function.
	*/
	bool SimpleDownloadFile(const std::string& URL, const std::string& targetPath = "./");

#ifdef _WIN32

	//////////////////////////////////////////////////////////////////////////
	// Windows specific stuff

	/**
	* If the updater finds a file with it's own name with an appended 'a' character (e.g. For Zupdater.exe, searches for ZUpdatera.exe),
	* it will replace itself with the updated found version and restart the application.
	* it will return true if no write error happened.
	* Please note, YOU (yes, you!) need to explicitly finish the application after calling this function and it returns updateFound == true
	*/
	bool SelfUpdate(bool &updateFound);

#endif // _WIN32

};

#endif // _ZLAUNCHERTHREAD_H_
