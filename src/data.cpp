#include "data.hpp"

#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "../CppUtil/util.hpp"

static bool instanceExists = false;

// Constructor
mainData::mainData(int maxThreads_input, std::vector<std::string> initialWork_vecStr) {
	if(instanceExists) {
		util::cPrint("red","ERROR! Attempted to create mainData instance when one already exists.");
		return;
	}
	
	instanceExists = true;
	maxThreads_int = maxThreads_input;
	currentThreads_vec.reserve(maxThreads_int);

	// TODO: load from DB

	for(auto& i : initialWork_vecStr) {
		possibleSite(i);
	}

	site_thread = std::thread(&mainData::siteThread_main, this);
	word_thread = std::thread(&mainData::wordThread_main, this);

}

// Destructor
mainData::~mainData() {
	stopCalled = true;
	joinAllThreads();
	instanceExists = false;

	// TODO: write to DB
}

void mainData::possibleSite(std::string site_str) {
	if(siteLock) {
		siteLock_que.push(site_str);
	} else {
		site_que.push(site_str);
	}
}

void mainData::addToWordList(std::string word_str) {
	if(wordLock) {
		wordLock_que.push(word_str);
	} else {
		word_que.push(word_str);
	}
}

void mainData::completedWork(int tId) {
	for(auto& i : currentThreads_vec) {
		if(i.second == tId) {
			i.first = true;
			return;
		}
	}

	util::cPrint("red","Attempted to complete work on threadID",tId,"but thread not found.\nPrinting current thread vector:");

	for(auto& i : currentThreads_vec) {
		util::cPrint("yellow","Thread ID:",i.second,"/ Available:",i.first);
	}
}

void mainData::siteThread_main() {
	if(stopCalled) {
		return;
	}

	siteLock = true;
	for(auto& i : currentThreads_vec) {
		if(i.first && site_que.size() > 0
		&& (!completedSites_uset.contains(site_que.front()))
		&& (!curSites_uset.contains(site_que.front()))) {
			i.first = false;
			++threadCounter;
			i.second = threadCounter;
			newSiteThread(*this, threadCounter, site_que.front());
			site_que.pop();
		}
	}
	siteLock = false;

	while(siteLock_que.size() > 0) {
		site_que.push(siteLock_que.front());
		siteLock_que.pop();
	}

	siteThread_main();

}

void mainData::wordThread_main() {
	if(stopCalled) {
		return;
	}
	wordLock = true;

	while(word_que.size() > 0) {
		if(wordList_map.contains(word_que.front())) {
			wordList_map.at(word_que.front()) += 1;
		} else {
			wordList_map[word_que.front()] = 1;
		}
		word_que.pop();
	}

	wordLock = false;
	
	while(wordLock_que.size() > 0) {
		word_que.push(wordLock_que.front());
		wordLock_que.pop();
	}

	wordThread_main();
}

void mainData::joinAllThreads() {
	if(site_thread.joinable()) {
		site_thread.join();
	}
}

void mainData::newSiteThread(mainData& parent_ref, int tId, std::string site) {
	// TODO: working here
}

