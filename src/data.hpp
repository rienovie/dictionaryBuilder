#pragma once

#include <queue>
#include <thread>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

class mainData {
public:
	mainData(int maxThreads_input, std::vector<std::string> initialWork_vecStr);
	~mainData();


	// int is count
	static std::unordered_map<std::string, int> wordList_map;
	int writeBatchSize = 8;
	static bool stopCalled;

	void stopCheck();

	void
		possibleSite(std::string site_str),
		addToWordList(std::string word_str),
		completedWork(int tId);

private:
	bool
		WTStopped = false,
		STStopped = false,
		siteLock = false,
		wordLock = false;
	int maxThreads_int = 4;
	
	// used for Thread IDs
	int threadCounter = 0;

	// used for stop check counters
	int checkCount = 1;

	// TODO: verify location
	std::string dbFile_str = "dict.db";

	static std::unordered_set<std::string>
		completedSites_uset,
		curSites_uset;

	static std::queue<std::string> site_que, word_que, wordLock_que, siteLock_que;
	
	//bool says if available
	static std::vector<std::pair<bool,int>> currentThreads_vec;

	std::thread
		stop_thread,
		site_thread,
		word_thread;

	void
		parseSite(std::string& page_str),
		newSiteThread(mainData& parent_ref, int tId, std::string site),
		siteThread_main(),
		wordThread_main(),
		joinAllThreads(),
		loadFromDB(),
		writeToDB();

	static size_t curlWriteFunc(char* ptr, size_t size, size_t nmemb, std::string* data);

};
