
#include "../CppUtil/util.hpp"
#include "data.hpp"
#include <string>
#include <unistd.h>
#include <vector>

std::vector<std::string> initialSites = {
	"https://en.wikipedia.org/wiki/Most_common_words_in_English"
};

int main(int argc, char* argv[] ) {

	util::cPrint("blue","Dictionary Builder.");
	auto MD = mainData(8,1,initialSites);
	
	MD.mainLoop();

}
