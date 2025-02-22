#include "data.hpp"

#include <cassert>
#include <cstddef>
#include <mutex>
#include <queue>
#include <string>
#include <sys/types.h>
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
std::queue<std::string> mainData::wordQ = {};
std::queue<std::pair<std::string,int>> mainData::siteQ = {};

// Constructor
mainData::mainData(int maxThreads_input, int maxDepth_input, std::vector<std::string> initialWork_vecStr) {
	if(instanceExists) {
		util::cPrint("red","ERROR! Attempted to create mainData instance when one already exists.");
		return;
	}
	
	instanceExists = true;
	maxThreads_int = maxThreads_input;
	maxDepth = maxDepth_input;

	loadFromDB();

	auto curlInitResult = curl_global_init(CURL_GLOBAL_DEFAULT);
	if(curlInitResult != CURLE_OK) {
		util::cPrint("red","ERROR! Curl global init failed:",curl_easy_strerror(curlInitResult));
		throw;
	}

	for(auto& i : initialWork_vecStr) {
		possibleSite(i,0);
	}

	site_thread = std::thread(&mainData::siteThread_main);
	word_thread = std::thread(&mainData::wordThread_main);

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

void mainData::possibleSite(std::string site_str, int depth_int) {
	if(depth_int > mainData::maxDepth) {
		return;
	}
	
	// probably a valid site
	if(site_str.length() > 8 && site_str.substr(0,8) == "https://" && (!util::containsAny(site_str, ";#%"))) {
		util::qPrint("Possible site called for:",site_str);

		site_mutex.lock();
		siteQ.push(std::pair(site_str,depth_int));
		site_mutex.unlock();

	}

}

void mainData::addToWordList(std::string word_str) {
	util::toLowercase(word_str);

	word_mutex.lock();
	wordQ.push(word_str);
	word_mutex.unlock();
}

void mainData::mainLoop() {
// recursive calls go on the stack but this avoids it
lbl_StopCheckStart:
	if(WTStopped && STStopped) {
		stopCalled = true;
		return;
	}
	goto lbl_StopCheckStart;
}

void mainData::siteThread_main() {
	int iStopCount = 0;
	std::vector<std::thread> workThreads;

	for(int i = 0; i < maxThreads_int; ++i) {
		workThreads.push_back(std::thread(&mainData::workThread));
	}

// recursive calls go on the stack but this avoids it
// and workThreads stays within scope
lbl_siteThreadStart:
	if(stopCalled || iStopCount > checkCount) {
		STStopped = true;
		for(auto& t : workThreads) {
			if(t.joinable()) {
				t.join();
			}
		}
		return;
	}

	util::sleep(0.25);
	++iStopCount;
	goto lbl_siteThreadStart;

}

void mainData::wordThread_main() {
// recursive calls go on the stack but this avoids it
lbl_wordThreadStart:
	word_mutex.lock();
	while(wordQ.size() > 0) {
		util::unordered_mapIncrement(wordList_map, wordQ.front());
		wordQ.pop();
	}
	word_mutex.unlock();

	if(STStopped) {
		WTStopped = true;
		return;
	} else {
		util::sleep(0.25);
		goto lbl_wordThreadStart;
	}
	
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

void mainData::parseSite(std::string& page_str, const int depth_int) {
	std::unordered_map<std::string,int> mElementCounter;
	std::string sBuild = "";
	bool
		insideAngleBrackets = false,
		insideBody = false,
		ignoreCurrent = false,
		workingHref = false,
		determingElement = true;
	std::unordered_set<std::string> elementsToIgnore = {
		"footer",
		"img",
		"script",
		"head",
		"input",
		"!--",
		"title",
		"meta"
	};

	for(char& c : page_str) {
		if(insideAngleBrackets) {
			if(determingElement) {
				if(c == ' ' | c == '>' | c == '\n') {
					determingElement = false;
					if(c == '>') insideAngleBrackets = false;
					if(sBuild[0] == '/') {
						ignoreCurrent = false;
						sBuild = sBuild.substr(1);
						if(sBuild == "body") {
							insideBody = false;
							sBuild.clear();
							continue;
						}
						util::unordered_mapIncrement(mElementCounter, sBuild,-1);
					} else if(util::contains(elementsToIgnore,sBuild)) {
						sBuild.clear();
						ignoreCurrent = true;
						continue;
					} else {
						util::unordered_mapIncrement(mElementCounter, sBuild);
					}
					if(sBuild == "body") insideBody = true;
					// util::qPrint(sBuild);
					sBuild.clear();
				} else {
					sBuild.push_back(c);
				}
			} else if(workingHref) {
				if(c == ' ' || c == '>') {
					possibleSite(sBuild,depth_int + 1);
					sBuild.clear();
					workingHref = false;
				} else if(c != '"') {
					sBuild.push_back(c);
				}
			} else if (c == '>') {
				insideAngleBrackets = false;
				sBuild.clear();
			} else if (c == '=' && sBuild == "href"){
				workingHref = true;
				sBuild.clear();
			} else if(c == ' ') {
				sBuild.clear();
			} else {
				sBuild.push_back(c);
			}
		} else if(c == '<') {
			insideAngleBrackets = true;
			determingElement = true;
			ignoreCurrent = false;
			if(sBuild.length() > 0) {
				if(util::onlyContains(sBuild,"'",true)
				&& (sBuild.length() > 1)
				&& (!util::onlyContains(sBuild,"ABCDEFGHIJKLMNOPQRSTUVWXYZ"))) {
					addToWordList(sBuild);
				}
				sBuild.clear();
			}
		} else if (ignoreCurrent) {
			continue;
		} else if(c == ' ') {
			if(sBuild.length() > 0) {
				if(util::onlyContains(sBuild,"",true)) {
					addToWordList(sBuild);
				}
				sBuild.clear();
			}
		} else {
			sBuild.push_back(c);
		}
	}

}

void mainData::workThread() {
	std::pair<std::string,int> workingData;

// recursive call adds to stack this avoids it
lbl_WorkThread:
	util::sleep(0.25);
	if(STStopped) {
		return;
	}

	site_mutex.lock();
	if(siteQ.size() > 0) {
		workingData = siteQ.front();
		siteQ.pop();
	} else {
		site_mutex.unlock();
		goto lbl_WorkThread;
	}
	site_mutex.unlock();

	thread_mutex.lock();
	if(completedSites_uset.contains(workingData.first) || curSites_uset.contains(workingData.first)) {
		thread_mutex.unlock();
		goto lbl_WorkThread;
	}
	curSites_uset.emplace(workingData.first);
	thread_mutex.unlock();

	CURL* curl = curl_easy_init();
	
	if(!curl) {
		util::cPrint("red","ERROR! Failed to create curl easy init on thread",std::this_thread::get_id(),"with site",workingData.first);

		thread_mutex.lock();
		curSites_uset.erase(workingData.first);
		completedSites_uset.emplace(workingData.first);
		thread_mutex.unlock();

		goto lbl_WorkThread;
	}

	std::string page_str = "";

	curl_easy_setopt(curl, CURLOPT_URL, workingData.first.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteFunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &page_str);

	CURLcode res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		util::cPrint("red","ERROR! Failed to curl easy perform on thread",std::this_thread::get_id(),"with site",workingData.first);
	
		thread_mutex.lock();
		curSites_uset.erase(workingData.first);
		completedSites_uset.emplace(workingData.first);
		thread_mutex.unlock();
	}

	util::cPrint("cyan","Attempting to parse site:",workingData.first," with a depth value of:",workingData.second);
	parseSite(page_str,workingData.second);
	util::cPrint("green","Finished parsing site:",workingData.first);

	thread_mutex.lock();
	curSites_uset.erase(workingData.first);
	completedSites_uset.emplace(workingData.first);
	thread_mutex.unlock();

	curl_easy_cleanup(curl);

	goto lbl_WorkThread;

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
		openDB = sqlite3_prepare_v2(db,
							  "CREATE TABLE Main ('word' TEXT UNIQUE,'count' INTEGER, PRIMARY KEY('word'))",
							  -1, &dbStmt, nullptr);
		if(openDB != SQLITE_OK) {
			util::cPrint("red","Error, unable to Select or Create table Main. SQLite error:",sqlite3_errmsg(db));
			sqlite3_finalize(dbStmt);
			sqlite3_close(db);
			throw;
		}
		sqlite3_step(dbStmt);
		sqlite3_finalize(dbStmt);

		openDB = sqlite3_prepare_v2(db,"SELECT * FROM Main;",-1,&dbStmt,nullptr);
		if(openDB != SQLITE_OK) {
			util::cPrint("red","Error, unable to Select * From Main after creation. SQLite error:",sqlite3_errmsg(db));
			sqlite3_finalize(dbStmt);
			sqlite3_close(db);
			throw;
		}
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
			sStmtBuild = "INSERT OR REPLACE INTO Main (word,count) VALUES ";
			for(int i = wordBatch_vec.size() - 1; i > -1; --i) {
				sStmtBuild.append(
					"('" +
					wordBatch_vec.at(i).first +
					"'," +
					std::to_string(wordBatch_vec.at(i).second) +
					")");
				if(i == 0) {
					sStmtBuild.append(";");
				} else {
					sStmtBuild.append(", ");
				}
				wordBatch_vec.pop_back();
			}

			openDB = sqlite3_prepare_v2(db,sStmtBuild.c_str(),-1,&dbStmt,nullptr);
			if(openDB != SQLITE_OK) {
				util::cPrint("red","Error, unable to INSERT OR REPLACE INTO Main (word,count) VALUES.\n\nSQLite error:",sqlite3_errmsg(db),"\n\nSQLite stmt:",sStmtBuild);
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
		sStmtBuild = "INSERT OR REPLACE INTO Main (word,count) VALUES ";
		for(int i = wordBatch_vec.size() - 1; i > -1; --i) {
				sStmtBuild.append(
					"('" +
					wordBatch_vec.at(i).first +
					"'," +
					std::to_string(wordBatch_vec.at(i).second) +
					")");
				if(i == 0) {
					sStmtBuild.append(";");
				} else {
					sStmtBuild.append(", ");
				}
				wordBatch_vec.pop_back();
			}

		openDB = sqlite3_prepare_v2(db,sStmtBuild.c_str(),-1,&dbStmt,nullptr);
		if(openDB != SQLITE_OK) {
			util::cPrint("red","Error, unable to INSERT OR REPLACE INTO Main (word,count) VALUES.\n\nSQLite error:",sqlite3_errmsg(db),"\n\nSQLite stmt:",sStmtBuild);
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

