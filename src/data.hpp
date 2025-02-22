#pragma once

#include <mutex>
#include <queue>
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

	// Will return when finished
	// TODO: make a while loop for status ui later
	void mainLoop();

	void
		possibleSite(std::string site_str, int depth_int),
		addToWordList(std::string word_str);

private:
	std::mutex site_mutex, word_mutex, thread_mutex;

	bool
		WTStopped = false,
		STStopped = false;
	int maxThreads_int = 4;

	// used for stop check counters
	int checkCount = 1;

	// TODO: verify location
	std::string dbFile_str = "dict.db";

	static std::unordered_set<std::string>
		completedSites_uset,
		curSites_uset;

	static std::queue<std::string> wordQ;
	static std::queue<std::pair<std::string,int>> siteQ;
	
	//bool says if available
	static std::vector<std::pair<bool,std::thread>> currentThreads_vec;

	std::thread
		site_thread,
		word_thread;

	void
		parseSite(std::string& page_str, const int depth_int),
		workThread(),
		siteThread_main(),
		wordThread_main(),
		joinAllThreads(),
		loadFromDB(),
		writeToDB();

	static size_t curlWriteFunc(char* ptr, size_t size, size_t nmemb, std::string* data);

};
