#include "are_you_sure.cpp"
#include <random>

int main (int argc, char *argv[]) {
    std::vector<AYS_fixture> market_maker1;
    std::vector<std::vector<std::string>> names = {{"FC.K", "FC KBH", "FC København"}, {"Brøndby IF", "Brøndbyernes Idrætsforening", "BIF"}, {"Fremad Amager", "fremad Amager", "frm. amager"}};
    for (uint64_t i = 0; i < 100 ; i++){
        std::random_device rd;
        std::mt19937 g(rd());
     
        std::shuffle(names.begin(), names.end(), g);
        int participants = 2 + (rand()%(names.size()-1));
        std::vector<std::string> pn(participants);
        std::vector<float> po(participants);
        std::vector<float> pno(participants);
        int margin = 20;
        int per_tot = 1000+margin;
        int per_left = per_tot;
        int lines[] = {0,1000,2000,3000};
        int line = lines[rand()%4];

        for (int p = 0; p < participants-1; p++){
            int per = rand()%(per_left-participants+1-margin)+1;
            po[p] = 1/(per/1000.f);
            pno[p] = 1/((per_tot-per)/1000.f);
            per_left-=per;
        }
        po[participants-1] = 1/(per_left/1000.f);
        pno[participants-1] = 1/((per_tot-per_left)/1000.f);
        for (int p = 0; p< participants; p++){
            pn[p] = names[p][rand()%names[p].size()];
        }
        uint64_t ut = rand()% 10;
        time_t now = time(0);
        struct tm now_tm = *localtime( &now);
        struct tm exp_tm = now_tm;
        exp_tm.tm_sec += (rand()%50);   // add 50 seconds to the time
        time_t exp = mktime( &exp_tm);
        uint32_t pid = rand() % 2;
        uint32_t sid = rand() % 3;
        uint32_t btid = participants;
        market_maker1.emplace_back(ut, exp, pid, sid, btid, line , pn, pno,po);

    }
#if 0
    for (auto& fix : market_maker1){
        if (fix.pid > 0) { 
            std::cout << AYS_fixture_to_string(fix) << '\n';
        }
    }
#endif
    std::vector<AYS_event> res = AYS_fixtures_to_events(market_maker1);
    for (auto& event : res) {
        if (event.roi > 0) { 
            std::cout << AYS_event_to_string_pretty(event) << '\n';
        }
    }
    return 0;
}
