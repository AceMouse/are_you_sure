#include <iostream>
#include "are_you_sure.cpp"

int main (int argc, char *argv[]) {
    std::vector<AYS_fixture> market_maker1;
    for (uint64_t i = 0; i < 10 ; i++){
	for (int j = 0; j < i; j++){
	    AYS_fixture e = {.unix_time = 100-i, .p1 = "Me", .p2 = "You", .pid = j % 3, .sid = j%2};
	    market_maker1.push_back(e);
	}
    }
    std::vector<AYS_event> res = AYS_fixtures_to_events(market_maker1);
    return 0;
}
