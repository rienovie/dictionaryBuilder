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
	std::unordered_map<std::string, int> wordList_map;
	int writeBatchSize = 8;


	void
		possibleSite(std::string site_str),
		addToWordList(std::string word_str),
		completedWork(int tId);

private:
	bool
		stopCalled = false,
		siteLock = false,
		wordLock = false;
	int maxThreads_int = 4;
	
	// used for Thread IDs
	int threadCounter = 0;

	// TODO: verify location
	std::string dbFile_str = "dict.db";

	std::unordered_set<std::string>
		completedSites_uset,
		curSites_uset;

	std::queue<std::string> site_que, word_que, wordLock_que, siteLock_que;
	
	//bool says if available
	std::vector<std::pair<bool,int>> currentThreads_vec;

	std::thread
		site_thread,
		word_thread;

	void
		newSiteThread(mainData& parent_ref, int tId, std::string site),
		siteThread_main(),
		wordThread_main(),
		joinAllThreads(),
		loadFromDB(),
		writeToDB();

};
