//////////////////////////////////////////////////////////////////////////
//
// ZLauncher - Patcher system - Part of the ZUpdater suite
// Felipe "Zoc" Silveira - (c) 2016-2018
//
//////////////////////////////////////////////////////////////////////////
//
// ZLauncherTokenThread.cpp
// Implement worker thread used in the launcher process
//
//////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <string>
#include <sstream>
#include <errno.h>
#include "curl/curl.h"
#include <nlohmann/json.hpp>
#include "wx/thread.h"
#include "ZLauncherFrame.h"
#include "ZLauncherTokenThread.h"
#include "LogSystem.h"
#include "FileUtils.h"
#include "DownloadFileWriter.h"
#include "md5.h"
#include "ApplyPatch.h"
// uetopia stuff - windows
//#include <shellapi.h>

#include "../libs/rapidxml-1.13/rapidxml.hpp"
#include "../libs/rapidxml-1.13/rapidxml_utils.hpp"

//////////////////////////////////////////////////////////////////////////
// Define the events used to update the create patch frame

//wxDEFINE_EVENT(wxEVT_COMMAND_UPDATE_GAME_LIST, wxThreadEvent);
//wxDEFINE_EVENT(wxEVT_COMMAND_CLICKED_GAME_LIST, wxThreadEvent);

//wxDEFINE_EVENT(wxEVT_COMMAND_UPDATE_PROGRESS_BAR, wxThreadEvent);
//wxDEFINE_EVENT(wxEVT_COMMAND_UPDATE_PROGRESS_TEXT, wxThreadEvent);

//wxDEFINE_EVENT(wxEVT_COMMAND_HTML_LOAD_PAGE, wxThreadEvent);

//wxDEFINE_EVENT(wxEVT_COMMAND_ENABLE_LAUNCH_BUTTON, wxThreadEvent);

//////////////////////////////////////////////////////////////////////////

// for convenience
using json = nlohmann::json;

ZLauncherTokenThread::ZLauncherTokenThread(ZLauncherFrame* handler, const wxString& updateURL, const wxString& versionFile, const wxString& targetDirectory)
	: wxThread(wxTHREAD_DETACHED)
{
	m_pHandler = handler;
	m_updateURL = updateURL;
	m_versionFile = versionFile;
	m_targetDirectory = targetDirectory;
	m_LatestVersion = 0LL;
}

ZLauncherTokenThread::~ZLauncherTokenThread()
{
	wxCriticalSectionLocker enter(m_pHandler->m_pThreadCS);

	// No harm in being overzealous here
	ZPatcher::DestroyLogSystem();

	// the thread is being destroyed; make sure not to leave dangling pointers around
	m_pHandler->m_pThread = NULL;

	m_count = 0;
}

wxThread::ExitCode ZLauncherTokenThread::Entry()
{
	// TestDestroy() checks if the thread should be cancelled/destroyed
	if (TestDestroy()) { ZPatcher::DestroyLogSystem(); return (wxThread::ExitCode)0; }

	ZPatcher::SetActiveLog("ZLauncher");
	std::ostringstream tmphtml;

	// this thread will loop and refresh the token until it is killed.

	// Instead of continuing here, we need to wait for auth to complete.
	for (m_count = 0; (m_count < 100); m_count++)
	{
		if (TestDestroy()) { ZPatcher::DestroyLogSystem(); return (wxThread::ExitCode)0; }

		// initially we wait, since we just got a new token
		wxMilliSleep(5 * 60 * 1000);  // 5 min

		std::string access_token = m_pHandler->CurrentAccessToken;

		// we should already have the token here.
		// Use it to get the player data and list of games that can be patched
		// this does not seem to be working

		CURLcode ret;
		CURL* hnd;
		//struct curl_slist* slist1;

		std::string MeUrl = "https://uetopia.com/_ah/api/users/v1/refreshToken";

		//slist1 = NULL;
		//slist1 = curl_slist_append(slist1, "Content-Type: application/json");
		// this is not actually used on this request.  leaving it here for future ref.
		//slist1 = curl_slist_append(slist1, "Authorization: Bearer <my_token>");



		// some parts from:  https://curl.se/libcurl/c/getinmemory.html

		hnd = curl_easy_init();

		struct curl_slist* headers = NULL;

		headers = curl_slist_append(headers, "Content-Type: application/json");
		std::string authHeader = "x-uetopia-auth: " + access_token;
		headers = curl_slist_append(headers, authHeader.c_str());

		curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, headers);

		curl_easy_setopt(hnd, CURLOPT_URL, MeUrl.c_str());
		// Don't wait forever, time out after 10 seconds.
		curl_easy_setopt(hnd, CURLOPT_TIMEOUT, 10000);
		curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.38.0");
		//curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
		curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
		//curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "GET");
		curl_easy_setopt(hnd, CURLOPT_POST, 1);
		curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);

		// Response information.
		long httpCode(0);
		std::string httpData;

		// Hook up data handling function.
		curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, WriteCallback);
		/* we pass our 'chunk' struct to the callback function */
		curl_easy_setopt(hnd, CURLOPT_WRITEDATA, &httpData);

		// Hook up data container (will be passed as the last parameter to the
		// callback handling function).  Can be any pointer type, since it will
		// internally be passed as a void pointer.
		// curl_easy_setopt(hnd, CURLOPT_WRITEDATA, httpData);

		ret = curl_easy_perform(hnd);
		curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &httpCode);

		/* check for errors */
		if (ret != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(ret));
		}
		else {
			/*
			 * Now, our chunk.memory points to a memory block that is chunk.size
			 * bytes big and contains the remote file.
			 *
			 * Do something nice with it!
			 */

			 //printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
		}

		// ok so here...  WE've got the callback setup, but we don't actually have any httpData.
		// Wait for the callback and return or go somewhere else.

		// some from: https://stackoverflow.com/questions/24884490/using-libcurl-and-jsoncpp-to-parse-from-https-webserver

		if (httpCode == 200)
		{
			std::cout << "\nGot successful response from " << MeUrl << std::endl;
			ZLauncherFrame::UpdateProgress(0, "Token Refreshed");

			// Response looks good - done using Curl now.  Try to parse the results
			// and print them out.

			auto j3 = json::parse(httpData);

			//std::cout << j3.at("id");
			// ZPatcher::Log(ZPatcher::LOG_WARNING, "ID: %s", j3["id"].get<std::string>()); // this is garbled for some reason

			// we don't actually need this, just doing this for debugging
			std::string refresh_token = j3["accessToken"].get<std::string>();
			ZPatcher::Log(ZPatcher::LOG_WARNING, "AccessToken: %s", &refresh_token); // this is garbled for some reason

			// But, refresh_token looks totally correct...  so lets go with that.
			ZLauncherFrame::UpdateAccessToken(refresh_token);
			m_pHandler->UpdateTitle(refresh_token);


		}
		else
		{
			ZPatcher::Log(ZPatcher::LOG_WARNING, "Could not get url: %s", MeUrl);
			//std::cout << "Couldn't GET from " << MeUrl << " - exiting" << std::endl;
			//return 1;
			ZLauncherFrame::UpdateProgress(0, "An error occurred while refreshing your login token.  Please try again.");

			//curl_global_cleanup();
			curl_easy_cleanup(hnd);

			hnd = NULL;
			curl_slist_free_all(headers);
			headers = NULL;

			ZPatcher::DestroyLogSystem();
			return (wxThread::ExitCode)0;
		}

		//curl_global_cleanup();
		curl_easy_cleanup(hnd);
		hnd = NULL;
		curl_slist_free_all(headers);
		headers = NULL;

	}

	

	return (wxThread::ExitCode)0;     // success
}

