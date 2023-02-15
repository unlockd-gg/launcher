//////////////////////////////////////////////////////////////////////////
//
// ZLauncher - Patcher system - Part of the ZUpdater suite
// Felipe "Zoc" Silveira - (c) 2016-2018
//
//////////////////////////////////////////////////////////////////////////
//
// ZLauncherPatcherThread.cpp
// Header file for the worker thread used in the patcher process
//
//////////////////////////////////////////////////////////////////////////

#ifndef _ZLAUNCHERPATCHERTHREAD_H_
#define _ZLAUNCHERPATCHERTHREAD_H_

#include <wx/thread.h>
#include <wx/event.h>
#include <vector>
#include "ZLauncherFrame.h"

// Lets avoid adding an unnecessary header here.
typedef unsigned long       DWORD;

//////////////////////////////////////////////////////////////////////////
// Declare the events used to update the main frame

//wxDECLARE_EVENT(wxEVT_COMMAND_UPDATE_GAME_LIST, wxThreadEvent);

wxDECLARE_EVENT(wxEVT_COMMAND_UPDATE_PROGRESS_BAR, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_COMMAND_PATCHER_UPDATE_PROGRESS_TEXT, wxThreadEvent);

wxDECLARE_EVENT(wxEVT_COMMAND_HTML_SET_CONTENT, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_COMMAND_HTML_LOAD_PAGE, wxThreadEvent);

wxDECLARE_EVENT(wxEVT_COMMAND_ENABLE_LAUNCH_BUTTON, wxThreadEvent);


//////////////////////////////////////////////////////////////////////////
// Auxiliary Patch structure

struct Patch
{
	uint64_t					sourceBuildNumber;
	uint64_t					targetBuildNumber;

	std::string					fileURL;
	DWORD						fileLength;
	std::string					fileMD5;
};

//////////////////////////////////////////////////////////////////////////
// Implement the worker thread

class ZLauncherPatcherThread : public wxThread
{
private:
	ZLauncherPatcherThread(); // Not allowed to be called

public:
	ZLauncherPatcherThread(ZLauncherFrame* handler, const wxString& updateURL, const wxString& versionFile, const wxString& targetDirectory);
	~ZLauncherPatcherThread();

protected:
	virtual ExitCode Entry();
	ZLauncherFrame* m_pHandler;

	std::string					m_updateURL;

	// VersionFile is the name of text file that contains the LocalCurrentVersion
	// This is passed into the thread itself when it is created.

	std::string					m_versionFile;
	std::string					m_targetDirectory;

	std::string					m_ApplicationName;

	// Local Current Version is the version of the game that exists on the client platform currently.
	// In default ZPathcer there is only one version, but in this launcher, each game has it's own version.
	// It gets stored in a text file on the client's computer, with the format: 194278612894.zversion

	uint64_t					m_LocalCurrentVersion;

	// Latest version comes from the server patch file.  
	//  It should get passed into the thread - or something

	uint64_t					m_LatestVersion;
	uint64_t					m_BestPathDownloadSize;
	std::vector<Patch>			m_Patches;
	std::vector<unsigned int>	m_BestPatchPath;
	std::string					m_UpdateDescription;

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
	uint64_t GetLatestVersion();

	/**
	* With all the patches stored in g_Patches, find the best path to download updates, using the file download size as the value that should be minimized.
	* Returns true if there is a path from the source version to the target version (i.e. if the update is possible)
	*/
	bool GetSmallestUpdatePath(const uint64_t& sourceBuildNumber, const uint64_t& targetBuildNumber, std::vector<unsigned int>& path, uint64_t& pathFileSize);

	/**
	* Returns true if it was able to determine the current version stored in configFile.
	* The found version is returned in the "version" variable.
	* NOTE: If no file is found, it returns true and version gets the value 0 (i.e. No previous version found, perform full installation)
	*/
	bool GetTargetCurrentVersion(const std::string& configFile, uint64_t& version);

	/**
	* Calculate the MD5 hash of a file
	*/
	std::string MD5File(std::string fileName);

	/**
	* Updates given configFile to store the supplied version
	*/
	bool SaveTargetNewVersion(const std::string& configFile, const uint64_t& version);

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
	bool SelfUpdate(bool& updateFound);

#endif // _WIN32

};

#endif // _ZLAUNCHERTHREAD_H_
