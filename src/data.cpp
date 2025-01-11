#include "data.hpp"

#include <cassert>
#include <cstddef>
#include <queue>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sqlite3.h>
#include <curl/curl.h>

#include "../CppUtil/util.hpp"

static bool instanceExists = false;

bool mainData::stopCalled = false;
std::unordered_map<std::string, int> mainData::wordList_map = {};
std::unordered_set<std::string>
	mainData::completedSites_uset = {},
	mainData::curSites_uset = {};
std::queue<std::string>
	mainData::site_que = {},
	mainData::word_que = {},
	mainData::wordLock_que = {},
	mainData::siteLock_que = {};
std::vector<std::pair<bool,int>> mainData::currentThreads_vec = {};
	

// Constructor
mainData::mainData(int maxThreads_input, std::vector<std::string> initialWork_vecStr) {
	if(instanceExists) {
		util::cPrint("red","ERROR! Attempted to create mainData instance when one already exists.");
		return;
	}
	
	instanceExists = true;
	maxThreads_int = maxThreads_input;
	currentThreads_vec.reserve(maxThreads_int);

	for(int i = 0; i < maxThreads_int; ++i) {
		currentThreads_vec.push_back(std::make_pair(true, 0));
	}

	loadFromDB();

	auto curlInitResult = curl_global_init(CURL_GLOBAL_DEFAULT);
	if(curlInitResult != CURLE_OK) {
		util::cPrint("red","ERROR! Curl global init failed:",curl_easy_strerror(curlInitResult));
		throw;
	}

	for(auto& i : initialWork_vecStr) {
		possibleSite(i);
	}

	site_thread = std::thread(&mainData::siteThread_main, this);
	word_thread = std::thread(&mainData::wordThread_main, this);

}

// Destructor
mainData::~mainData() {
	util::cPrint("blue","Finishing work...");

	joinAllThreads();

	writeToDB();
	
	curl_global_cleanup();

	instanceExists = false;

	util::cPrint("green","Work finished.");
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

void mainData::stopFunc() {

	if(WTStopped && STStopped) {
		stopCalled = true;
		return;
	}

	sleep(1);
}

void mainData::siteThread_main() {
	int iStopCount = 0;

// recursive calls go on the stack but this avoids it
lbl_siteThreadStart:
	if(stopCalled) {
		STStopped = true;
		return;
	}

	if(iStopCount > checkCount) {
		STStopped = true;
		return;
	}

	siteLock = true;
	for(auto& i : currentThreads_vec) {
		if(i.first && site_que.size() > 0
		&& (!completedSites_uset.contains(site_que.front()))
		&& (!curSites_uset.contains(site_que.front()))) {
			iStopCount = 0;
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

	sleep(1);
	++iStopCount;
	goto lbl_siteThreadStart;

}

void mainData::wordThread_main() {
	int iStopCounter = 0;

// recursive calls go on the stack but this avoids it
lbl_wordThreadStart:
	if(stopCalled) {
		WTStopped = true;
		return;
	}

	if(iStopCounter > checkCount) {
		WTStopped = true;
		return;
	}

	wordLock = true;
	while(word_que.size() > 0) {
		iStopCounter = 0;
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

	// adds to stack and causes overflow, so goto
	// wordThread_main();
	sleep(1);
	++iStopCounter;
	goto lbl_wordThreadStart;
}

void mainData::joinAllThreads() {
	if(site_thread.joinable()) {
		site_thread.join();
	}
	if(word_thread.joinable()) {
		word_thread.join();
	}
}

size_t mainData::curlWriteFunc(char* ptr, size_t size, size_t nmemb, std::string* data) {
	size_t dataSize = size * nmemb;
	data->append(ptr,dataSize);
	return dataSize;
}

// TODO: working on this function here
void mainData::parseSite(std::string& page_str) {
	util::qPrint(page_str);
}

void mainData::newSiteThread(mainData& parent_ref, int tId, std::string site) {
	if(parent_ref.completedSites_uset.contains(site) || parent_ref.curSites_uset.contains(site)) {
		return;
	}

	parent_ref.curSites_uset.emplace(site);
	
	CURL* curl = curl_easy_init();
	
	if(!curl) {
		util::cPrint("red","ERROR! Failed to create curl easy init on thread",tId,"with site",site);

		parent_ref.curSites_uset.erase(site);
		parent_ref.completedSites_uset.emplace(site);

		parent_ref.completedWork(tId);
		return;
	}

	std::string page_str = "";

	curl_easy_setopt(curl, CURLOPT_URL, site.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteFunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &page_str);

	CURLcode res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		util::cPrint("red","ERROR! Failed to curl easy perform on thread",tId,"with site",site);
	
		parent_ref.curSites_uset.erase(site);
		parent_ref.completedSites_uset.emplace(site);

		parent_ref.completedWork(tId);
	}

	parseSite(page_str);

	parent_ref.curSites_uset.erase(site);
	parent_ref.completedSites_uset.emplace(site);
	

	curl_easy_cleanup(curl);

}

void mainData::loadFromDB() {
	// TODO: extract sqlite open/close to another function

	sqlite3* db;
	sqlite3_stmt* dbStmt;

	int openDB = sqlite3_open(dbFile_str.c_str(),&db);
	if(openDB != SQLITE_OK) {
		util::cPrint("red","Error, db file unable to be opened. SQLite error:",sqlite3_errmsg(db));
		// want to make sure crashes
		// TODO: try using assert maybe?
		throw;
	}

	openDB = sqlite3_prepare_v2(db,"SELECT * FROM Main;",-1,&dbStmt,nullptr);
	if(openDB != SQLITE_OK) {
		util::cPrint("red","Error, unable to Select * from Main. SQLite error:",sqlite3_errmsg(db));
		sqlite3_finalize(dbStmt);
		sqlite3_close(db);
		throw;
	}

	wordList_map.clear();
	std::string sWord = "";

	while((openDB = sqlite3_step(dbStmt)) == SQLITE_ROW) {
		sWord = reinterpret_cast<const char*>(sqlite3_column_text(dbStmt, 0));
		wordList_map[sWord] = sqlite3_column_int(dbStmt,1);
	}

	sqlite3_finalize(dbStmt);
	sqlite3_close(db);
}

// assumes the db file data has already been extracted so the count will be overwritten not added
void mainData::writeToDB() {
	sqlite3* db;
	sqlite3_stmt* dbStmt;

	int openDB = sqlite3_open(dbFile_str.c_str(),&db);
	if(openDB != SQLITE_OK) {
		util::cPrint("red","Error, db file unable to be opened. SQLite error:",sqlite3_errmsg(db));
		// want to make sure crashes
		// TODO: try using assert maybe?
		throw;
	}

	// I feel like batching is better so I'll do that for now, but  TODO: test this
	std::vector<std::pair<std::string,int>> wordBatch_vec;
	std::string sStmtBuild = "";
	for(auto& i : wordList_map) {
		wordBatch_vec.push_back(i);

		if(wordBatch_vec.size() >= writeBatchSize) {
			// build statement
			sStmtBuild = "(INSERT INTO Main (word,count) VALUES ";
			for(int i = wordBatch_vec.size() - 1; i > -1; --i) {
				sStmtBuild.append(
					"(" +
					wordBatch_vec.at(i).first +
					"," +
					std::to_string(wordBatch_vec.at(i).second) +
					") ");
				wordBatch_vec.pop_back();
			}
			sStmtBuild.append("ON CONFLICT(word) DO UPDATE SET count = excluded.count;)");

			openDB = sqlite3_prepare_v2(db,sStmtBuild.c_str(),-1,&dbStmt,nullptr);
			if(openDB != SQLITE_OK) {
				util::cPrint("red","Error, unable to INSERT INTO Main (word,count) VALUES. SQLite error:",sqlite3_errmsg(db));
				sqlite3_finalize(dbStmt);
				sqlite3_close(db);
				throw;
			}

			openDB = sqlite3_step(dbStmt);
			if(openDB != SQLITE_DONE) {
				util::cPrint("red","Error executing batch insert. SQLite error:",sqlite3_errmsg(db));
				sqlite3_finalize(dbStmt);
				sqlite3_close(db);
				throw;
			}

			sqlite3_finalize(dbStmt);
			dbStmt = nullptr;

			// just to make sure
			wordBatch_vec.clear();

		}
	}

	if(wordBatch_vec.size() > 0) {
		// TODO: was lazy and just copy pasta / make better later
		sStmtBuild = "(INSERT INTO Main (word,count) VALUES ";
		for(int i = wordBatch_vec.size() - 1; i > -1; --i) {
			sStmtBuild.append(
				"(" +
				wordBatch_vec.at(i).first +
				std::to_string(wordBatch_vec.at(i).second) +				"," +
				") ");
			wordBatch_vec.pop_back();
		}
		sStmtBuild.append("ON CONFLICT(word) DO UPDATE SET count = excluded.count;)");

		openDB = sqlite3_prepare_v2(db,sStmtBuild.c_str(),-1,&dbStmt,nullptr);
		if(openDB != SQLITE_OK) {
			util::cPrint("red","Error, unable to INSERT INTO Main (word,count) VALUES. SQLite error:",sqlite3_errmsg(db));
			sqlite3_finalize(dbStmt);
			sqlite3_close(db);
			throw;
		}

		openDB = sqlite3_step(dbStmt);
		if(openDB != SQLITE_DONE) {
			util::cPrint("red","Error executing batch insert. SQLite error:",sqlite3_errmsg(db));
			sqlite3_finalize(dbStmt);
			sqlite3_close(db);
			throw;
		}

		sqlite3_finalize(dbStmt);
		dbStmt = nullptr;

		// just to make sure
		wordBatch_vec.clear();

	}

	sqlite3_close(db);

}

