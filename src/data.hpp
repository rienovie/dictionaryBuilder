#pragma once

#include <queue>
#include <set>
#include <thread>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

class mainData {
public:
	mainData(int maxThreads_input, int maxDepth_input, std::vector<std::string> initialWork_vecStr);
	~mainData();


	// int is count
	static std::unordered_map<std::string, int> wordList_map;
	int writeBatchSize = 8;
	int maxDepth = 3;
	static bool stopCalled;

	void stopCheck();

	void
		possibleSite(std::string site_str, int depth_int),
		addToWordList(std::string word_str);

private:
	bool
		WTStopped = false,
		STStopped = false,
		siteLock = false,
		wordLock = false;
	int maxThreads_int = 4;

	// used for stop check counters
	int checkCount = 1;

	// TODO: verify location
	std::string dbFile_str = "dict.db";

	static std::unordered_set<std::string>
		completedSites_uset,
		curSites_uset;

	static std::unordered_set<std::thread::id> thread_set;
	static std::queue<std::string> word_que, wordLock_que;
	static std::queue<std::pair<std::string,int>> siteQ, siteLockQ;
	
	//bool says if available
	static std::vector<std::pair<bool,std::thread>> currentThreads_vec;

	std::thread
		stop_thread,
		site_thread,
		word_thread;

	void
		parseSite(std::string& page_str, int& depth_int),
		newSiteThread(mainData& parent_ref, std::string site, int depth_int),
		siteThread_main(),
		wordThread_main(),
		joinAllThreads(),
		loadFromDB(),
		writeToDB();

	static size_t curlWriteFunc(char* ptr, size_t size, size_t nmemb, std::string* data);

};
