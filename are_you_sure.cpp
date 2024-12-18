#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
typedef uint32_t AYS_fixture_id;
typedef uint32_t AYS_provider_id;
typedef uint32_t AYS_sport_id;
typedef uint32_t AYS_bet_type_id;
typedef std::string AYS_participant;
typedef bool (*AYS_fixture_comparison_func)(const struct AYS_fixture&, const struct AYS_fixture&);

struct AYS_fixture {
    uint64_t unix_time;
    AYS_participant p1;
    AYS_participant p2;
    AYS_provider_id pid;
    AYS_fixture_id id;
    AYS_sport_id sid;
    AYS_bet_type_id btid;
};

struct AYS_event {
    uint64_t unix_time;
    AYS_participant p1;
    AYS_participant p2;
    AYS_sport_id sid;
    AYS_bet_type_id btid;
    std::vector<AYS_fixture> fixtures;
    AYS_event(  uint64_t unix_time,
		AYS_participant p1,
		AYS_participant p2,
		AYS_sport_id sid,
		AYS_bet_type_id btid,
		std::vector<AYS_fixture> &fixtures) 
        : unix_time(unix_time)
        , p1(p1)
        , p2(p2)
        , sid(sid)
        , btid(btid)
        , fixtures(std::move(fixtures)) 
    {}
};

std::string AYS_fixture_to_string(struct AYS_fixture fixture) {
    return fixture.p1 + " vs. " + fixture.p2 + " @ " + std::to_string(fixture.unix_time) + " sid: " + std::to_string(fixture.sid) + " pid "+ std::to_string(fixture.pid);
}

bool AYS_unix_time_compare(const struct AYS_fixture &f1, const struct AYS_fixture &f2){
    return f1.unix_time < f2.unix_time ;
}

bool AYS_btid_compare(const struct AYS_fixture &f1, const struct AYS_fixture &f2){
    return f1.btid < f2.btid ;
}

bool AYS_sid_compare(const struct AYS_fixture &f1, const struct AYS_fixture &f2){
    return f1.sid < f2.sid ;
}

bool AYS_p1_compare(const struct AYS_fixture &f1, const struct AYS_fixture &f2){
    return f1.p1.compare(f2.p1) < 0;
}

bool AYS_p2_compare(const struct AYS_fixture &f1, const struct AYS_fixture &f2){
    return f1.p2.compare(f2.p2) < 0;
}

void split_on_diff( std::vector<struct AYS_fixture> fs, 
                    std::vector<std::vector<AYS_fixture>> &out,
                    std::vector<AYS_fixture_comparison_func> comps) {
    if (fs.size() == 0) return;
    for (auto& comp : comps){
        std::stable_sort(fs.begin(), fs.end(), comp);
    }
    int start_idx = 0;
    for (size_t idx = 0; idx < fs.size(); idx++){
        bool skip = true;
        for (auto& comp : comps){
            skip &= !comp(fs[idx], fs[start_idx]) and !comp(fs[start_idx], fs[idx]);
        }
        if (skip) {
            std::cout << idx << " skipped" << std::endl;
            continue;
        }
        out.emplace_back(fs.begin()+start_idx, fs.begin()+idx);
        start_idx = idx;
    }
    out.emplace_back(fs.begin()+start_idx, fs.end());
}

std::vector<AYS_event> AYS_fixtures_to_events(std::vector<AYS_fixture> fs) {
    std::vector<AYS_event> result;
    if (fs.size() == 0){
        return result;
    }

    std::vector<std::vector<AYS_fixture>> fixture_groups;
    split_on_diff(fs, fixture_groups, {AYS_unix_time_compare, AYS_btid_compare, AYS_sid_compare, AYS_p1_compare, AYS_p2_compare});
    
    for (std::vector<struct AYS_fixture> group : fixture_groups){
        for (struct AYS_fixture &f : group){
            std::cout << AYS_fixture_to_string(f) << std::endl; 
        }
        std::cout << std::endl; 
        std::cout << "-------------" << std::endl; 
        std::cout << std::endl; 
        result.emplace_back(group[0].unix_time, group[0].p1, group[0].p2, group[0].btid, group[0].sid, group);
    }
    
    return result;
}
