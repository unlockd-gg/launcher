//////////////////////////////////////////////////////////////////////////
//
// ZLauncher - Patcher system - Part of the ZUpdater suite
// Felipe "Zoc" Silveira - (c) 2016-2018
//
//////////////////////////////////////////////////////////////////////////
//
// ZLauncherThread.cpp
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
#include "ZLauncherThread.h"
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

wxDEFINE_EVENT(wxEVT_COMMAND_UPDATE_GAME_LIST, wxThreadEvent);
//wxDEFINE_EVENT(wxEVT_COMMAND_CLICKED_GAME_LIST, wxThreadEvent);

//wxDEFINE_EVENT(wxEVT_COMMAND_UPDATE_PROGRESS_BAR, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_COMMAND_UPDATE_PROGRESS_TEXT, wxThreadEvent);

//wxDEFINE_EVENT(wxEVT_COMMAND_HTML_LOAD_PAGE, wxThreadEvent);

//wxDEFINE_EVENT(wxEVT_COMMAND_ENABLE_LAUNCH_BUTTON, wxThreadEvent);

//////////////////////////////////////////////////////////////////////////

// for convenience
using json = nlohmann::json;

ZLauncherThread::ZLauncherThread(ZLauncherFrame *handler, const wxString& updateURL, const wxString& versionFile, const wxString& targetDirectory)
	: wxThread(wxTHREAD_DETACHED)
{
	m_pHandler = handler;

	m_updateURL = updateURL;
	m_versionFile = versionFile;
	m_targetDirectory = targetDirectory;
	m_LatestVersion = 0LL;
}

ZLauncherThread::~ZLauncherThread()
{
	wxCriticalSectionLocker enter(m_pHandler->m_pThreadCS);

	// No harm in being overzealous here
	ZPatcher::DestroyLogSystem();

	// the thread is being destroyed; make sure not to leave dangling pointers around
	m_pHandler->m_pThread = NULL;

	m_count = 0;
}

wxThread::ExitCode ZLauncherThread::Entry()
{
	// TestDestroy() checks if the thread should be cancelled/destroyed
	if (TestDestroy()) { ZPatcher::DestroyLogSystem(); return (wxThread::ExitCode)0; }

	ZPatcher::SetActiveLog("ZLauncher");
	std::ostringstream tmphtml;

	//////////////////////////////////////////////////////////////////////////
	// Before checking for updates.  Do auth!
	ShellExecute(0, 0, L"https://uetopia.com/token_login", 0, 0, SW_SHOW);

	std::string access_token;

	// Instead of continuing here, we need to wait for auth to complete.
	for (m_count = 0; (m_count < 100); m_count++)
	{
		if (TestDestroy()) { ZPatcher::DestroyLogSystem(); return (wxThread::ExitCode)0; }

		//wxLogMessage("Thread progress: %u", m_count);
		ZLauncherFrame::UpdateProgress(0, "Waiting for auth to complete.");

		// this section from WindowsPlatformApplicationMisc in the unreal engine codebase

		bool bWasFound = false;
		const wchar_t* TitleStartsWith = L"ue4topia.appspot.com/api/v1/login/redirect";
		LPWSTR Buffer = NULL;
		//WCHAR buffer[8192];

		wchar_t wcs[] = L"uetopia.com/token_login";

		// get the first window to begin searching for a title match
		//HWND hWnd = FindWindowEx(NULL, NULL, NULL, NULL);
		//HWND hWnd = GetTopWindow(NULL);

		std::vector<std::wstring> titles;
		EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&titles));
		// At this point, titles if fully populated and could be displayed, e.g.:
		for (const auto& title : titles)
			//if (wcsstr(wcs, title))
			//if (_tcsncmp(TitleStartsWith, title.c_str(), 8192))
			// std::string::find returns 0 if toMatch is found at starting
			if (title.find(TitleStartsWith) == 0)
			{
				ZLauncherFrame::UpdateProgress(0, "Found auth window");

				// do something
				// Parse access token from the login redirect url

				// quick convert to string.   there are no special characters here, it's just a hash.
				// But...  It's not working...  Getting a "Could not verify signature" on it....
				// Found it...  

				// At this point we have a window title which has some junk at the end like
				// ...XQitHYQUbB5G3FOsZkK2f-VA1uBLQHufFBLcM2GHlw - Google Chrome
				// by setting the stop_delim to " - " we can catch whatever browser this might be
				std::string str(title.length(), 0);
				std::transform(title.begin(), title.end(), str.begin(), [](wchar_t c) {
					return (char)c;
					});

				std::string start_delim = "access_token=";
				std::string stop_delim = " - ";

				access_token = get_str_between_two_str(str, start_delim, stop_delim);
				// then update the varible
				ZLauncherFrame::UpdateAccessToken(access_token);
				m_pHandler->UpdateTitle(access_token);
				ZPatcher::Log(ZPatcher::LOG_WARNING, "AUTH TOKEN: %s", access_token.c_str());

				//  Done here.  get out of the loop.
				m_count = 101;

				// Unreal Engine Client does it like:
				// This uses Epic's Fparse url parser.
				/*
				FString AccessToken;
				if (FParse::Value(*Title, TEXT("access_token="), AccessToken) &&
					!AccessToken.IsEmpty())
				{
					UE_LOG_ONLINE(Display, TEXT("FOnlineIdentityUEtopia::TickLogin Found access_token"));
					// strip off any url parameters and just keep the token itself
					FString AccessTokenOnly;
					if (AccessToken.Split(TEXT("&"), &AccessTokenOnly, NULL))
					{
						AccessToken = AccessTokenOnly;
					}
					// kick off http request to get user info with the new token
					TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
					LoginUserRequests.Add(&HttpRequest.Get(), FPendingLoginUser(LocalUserNumPendingLogin, AccessToken));

					FString MeUrl = TEXT("https://ue4topia.appspot.com/me?access_token=`token");

					HttpRequest->OnProcessRequestComplete().BindRaw(this, &FOnlineIdentityUEtopia::MeUser_HttpRequestComplete);
					HttpRequest->SetURL(MeUrl.Replace(TEXT("`token"), *AccessToken, ESearchCase::IgnoreCase));
					HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
					HttpRequest->SetVerb(TEXT("GET"));
					HttpRequest->ProcessRequest();
				}
				else
				{
					TriggerOnLoginCompleteDelegates(LocalUserNumPendingLogin, false, FUniqueNetIdString(TEXT("")),
						FString(TEXT("RegisterUser() failed to parse the user registration results")));
				}
				*/
		

			}

		/*
		if (hWnd)
		{
			size_t TitleStartsWithLen = _tcslen(TitleStartsWith);

			do
			{
				//int written = GetWindowTextW(hWnd, Buffer, 8192);
				int written = GetWindowTextA(hWnd, buffer, 8192);

				// if this matches, then grab the full text
				if (written != NULL)
				{
					//if (_tcsncmp(TitleStartsWith, Buffer, 8192))
					if (strstr("uetopia.com/token_login", buffer))
					{
						ZLauncherFrame::UpdateProgress(0, "Found auth window");

						// do something
						hWnd = NULL;

					}
					else {
						// get the next window
						hWnd = GetNextWindow(hWnd, GW_HWNDNEXT);

					}
				}
				else
				{
					// just exit
					hWnd = NULL;
				}

				

			} while (hWnd != NULL);
		}
		*/


		wxMilliSleep(2000);
	}

	// we should hopefully have the token here.

	// Start up the token refresh thread
	m_pHandler->DoStartCreateTokenThread();


	// Use it to get the player data and list of games that can be patched
	// this does not seem to be working
	
	CURLcode ret;
	CURL* hnd;
	struct curl_slist* slist1;
	//std::string jsonstr = "{\"username\":\"bob\",\"password\":\"12345\"}";

	std::string MeUrl = "https://ue4topia.appspot.com/gamepatch/me?access_token=" + access_token;

	slist1 = NULL;
	slist1 = curl_slist_append(slist1, "Content-Type: application/json");
	// this is not actually used on this request.  leaving it here for future ref.
	//slist1 = curl_slist_append(slist1, "Authorization: Bearer <my_token>");

	

	// some parts from:  https://curl.se/libcurl/c/getinmemory.html
	//struct MemoryStruct chunk;
	//chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
	//chunk.size = 0;    /* no data at this point */

	//curl_global_init(CURL_GLOBAL_DEFAULT);
	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_URL, MeUrl.c_str());
	// Don't wait forever, time out after 10 seconds.
	curl_easy_setopt(hnd, CURLOPT_TIMEOUT, 10000);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	//curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, jsonstr.c_str());
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.38.0");
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "GET");
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

	//curl_easy_cleanup(hnd);
	//free(chunk.memory);

	/* we're done with libcurl, so clean it up */
	//curl_global_cleanup();

	// ok so here...  WE've got the callback setup, but we don't actually have any httpData.
	// Wait for the callback and return or go somewhere else.

	// some from: https://stackoverflow.com/questions/24884490/using-libcurl-and-jsoncpp-to-parse-from-https-webserver

	if (httpCode == 200)
	{
		std::cout << "\nGot successful response from " << MeUrl << std::endl;
		ZLauncherFrame::UpdateProgress(0, "Got HTTPS verification response");

		

		// Response looks good - done using Curl now.  Try to parse the results
		// and print them out.

		auto j3 = json::parse(httpData);

		//std::cout << j3.at("id");
		// ZPatcher::Log(ZPatcher::LOG_WARNING, "ID: %s", j3["id"].get<std::string>()); // this is garbled for some reason

		// we don't actually need this, just doing this for debugging
		std::string player_id = j3["id"].get<std::string>();
		ZPatcher::Log(ZPatcher::LOG_WARNING, "ID: %s", &player_id); // this is garbled for some reason

		// But, player_id looks totally correct...  so lets go with that.
		// 
		// get the games
		ZLauncherPatchableGame* newgame;
		std::vector < ZLauncherPatchableGame*> patchable_games;
		patchable_games.empty();

		for (auto& td : j3["patchable_game_list"])
		{
			newgame = new ZLauncherPatchableGame() ;
			newgame->Title = td["title"].get<std::string>();
			newgame->Description = td["description"].get<std::string>();
			newgame->key_id = td["key_id"].get<std::string>();
			//newgame->patcher_details_xml = td["patcher_details_xml"].get<std::string>();

			//ZLauncherFrame::UpdatePatchableGame(newgame);

			patchable_games.push_back(newgame);
		}

		ZLauncherFrame::UpdatePatchableGames(patchable_games);
			//for (auto& prop : td["d"]) {
				//prop["e"] = prop["e"].get<std::string>() + std::string(" MODIFIED");
			//}


	}
	else
	{
		ZPatcher::Log(ZPatcher::LOG_WARNING, "Could not get url: %s", MeUrl);
		//std::cout << "Couldn't GET from " << MeUrl << " - exiting" << std::endl;
		//return 1;
		ZLauncherFrame::UpdateProgress(0, "An error occurred while verifying your login token.  Please try again.");

		//curl_global_cleanup();
		curl_easy_cleanup(hnd);
		
		hnd = NULL;
		curl_slist_free_all(slist1);
		slist1 = NULL;

		ZPatcher::DestroyLogSystem();
		return (wxThread::ExitCode)0;
	}

	//curl_global_cleanup();
	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_slist_free_all(slist1);
	slist1 = NULL;
	

	// Att this point we just want tht list of patchable games....  
	// And to wait for the user to select which game they want to patch.


	return (wxThread::ExitCode)0;     // success
}

