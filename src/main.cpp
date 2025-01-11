
#include "../CppUtil/util.hpp"
#include "data.hpp"
#include <string>
#include <vector>

std::vector<std::string> initialSites = {
	"wikipedia.com"
};

int main(int argc, char* argv[] ) {

	util::cPrint("red","Hello there.");
	auto MD = mainData(2,initialSites);
	util::cPrint("blue","Finished.");

}
