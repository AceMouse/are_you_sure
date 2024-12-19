#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <limits>
#include "similarity_sort.cpp"

typedef uint32_t AYS_fixture_id;
typedef uint32_t AYS_provider_id;
typedef uint32_t AYS_sport_id;
typedef uint32_t AYS_bet_type_id;
typedef float AYS_odd;

typedef std::string AYS_participant;
typedef bool (*AYS_fixture_comparison_func)(const struct AYS_fixture&, const struct AYS_fixture&);

struct AYS_fixture;
struct AYS_event;
std::string AYS_event_to_string(const AYS_event event); 
std::string AYS_fixture_to_string(const AYS_fixture fixture);
bool AYS_event_arb(AYS_event &event);
std::string AYS_event_to_string(const AYS_event event);
std::string AYS_fixture_to_string(const AYS_fixture fixture);
std::vector<AYS_event> AYS_fixtures_to_events(std::vector<AYS_fixture> fs);

struct AYS_fixture {
    uint64_t unix_time;
    AYS_provider_id pid;
    AYS_fixture_id id;
    AYS_sport_id sid;
    AYS_bet_type_id btid;
    std::vector<AYS_participant> participant_names;
    std::vector<AYS_odd> participant_not_odds;
    std::vector<AYS_odd> participant_odds;
    AYS_fixture(uint64_t unix_time,
		AYS_provider_id pid,
		AYS_sport_id sid,
		AYS_bet_type_id btid,
		std::vector<AYS_participant> &participant_names,
        std::vector<AYS_odd> &participant_not_odds, 
        std::vector<AYS_odd> &participant_odds) 
        : unix_time(unix_time)
        , pid(pid)
        , sid(sid)
        , btid(btid)
        , participant_names(std::move(participant_names))
        , participant_not_odds(std::move(participant_not_odds))
        , participant_odds(std::move(participant_odds)) { }
    AYS_fixture(uint64_t unix_time,
		AYS_provider_id pid,
		AYS_sport_id sid,
		AYS_bet_type_id btid,
		std::vector<AYS_participant> &participant_names,
        std::vector<AYS_odd> &participant_odds) 
        : unix_time(unix_time)
        , pid(pid)
        , sid(sid)
        , btid(btid)
        , participant_names(std::move(participant_names))
        , participant_not_odds((int)participant_names.size(),1)
        , participant_odds(std::move(participant_odds)) { }
};

struct AYS_event {
    uint64_t unix_time;
    AYS_sport_id sid;
    AYS_bet_type_id btid;
    float arb;
    float not_arb;
    float roi;
    uint32_t arb_flags;
    std::vector<AYS_participant> participant_names;
    std::vector<float> participant_stakes;
    std::vector<AYS_fixture> fixtures;
    AYS_event(std::vector<AYS_fixture> &fixtures) 
        : unix_time(fixtures[0].unix_time)
        , sid(fixtures[0].sid)
        , btid(fixtures[0].btid)
        , participant_names(fixtures[0].participant_names)
        , fixtures(std::move(fixtures)) {
        arb = std::numeric_limits<float>::infinity();
        not_arb = std::numeric_limits<float>::infinity();
        roi = -1;
        participant_stakes = std::vector<float>(participant_names.size(), 0);
    }
};


bool AYS_event_arb(AYS_event &event){
    std::vector<float> max_odds(event.fixtures[0].participant_odds);
    std::vector<float> max_not_odds(event.fixtures[0].participant_not_odds);
    for (int i = 1; i < (int)event.fixtures.size(); i++){
        if (max_odds.size() != event.fixtures[i].participant_odds.size()){
            std::cerr << "diffent amount of outcomes in fixtures 0 and " << i << "\n" 
                      << AYS_event_to_string(event) << std::endl;
            return false;
        }
        for (int j = 0; j < (int)max_odds.size();j++){
            if (max_odds[j] < event.fixtures[i].participant_odds[j]){
                max_odds[j] = event.fixtures[i].participant_odds[j]; 
            }
            if (max_not_odds[j] < event.fixtures[i].participant_not_odds[j]){
                max_not_odds[j] = event.fixtures[i].participant_not_odds[j]; 
            }
        }
    }
    float per = 0;
    for (int i = 0; i < (int)max_odds.size(); i++){
        float inv_odd = 1/max_odds[i];
        float inv_not_odd = 1/max_not_odds[i];
        per += inv_odd;
        if (event.not_arb > inv_odd+inv_not_odd){
            event.not_arb = inv_odd+inv_not_odd;
        }

    }
    event.arb = per;
    event.roi = 100/std::min(per, event.not_arb)-100;
    return true;
}

std::string AYS_fixture_to_string(const AYS_fixture fixture) {
    std::string result = fixture.participant_names[0];
    for (int i = 1; i< (int)fixture.participant_names.size(); i++){
        result += " | " + fixture.participant_names[i];
    }
    for (int i = 0; i< (int)fixture.participant_names.size(); i++){
        result += " | NOT "  + fixture.participant_names[i];
    }
    result += " @ " + std::to_string(fixture.unix_time) + " sid: " + std::to_string(fixture.sid) + " pid "+ std::to_string(fixture.pid) + " ";
    result += std::to_string(fixture.participant_odds[0]);
    for (int i = 1; i< (int)fixture.participant_odds.size(); i++){
        result += " | " + std::to_string(fixture.participant_odds[i]);
    }
    for (int i = 0; i< (int)fixture.participant_not_odds.size(); i++){
        result += " | " + std::to_string(fixture.participant_not_odds[i]);
    }
    return result;
}
std::string AYS_event_to_string(const AYS_event event) {
    std::string result = event.participant_names[0];
    for (int i = 1; i< (int)event.participant_names.size(); i++){
        result += " | " + event.participant_names[i];
    }
    result += " @ " + std::to_string(event.unix_time) + " sid: " + std::to_string(event.sid) + " ARB(not): " + std::to_string(event.arb*100) + "% (" +std::to_string(event.not_arb*100) + "%)" + " ROI: " + std::to_string(event.roi) + "%";
    for (int i = 0; i< (int)event.fixtures.size(); i++){
        result += "\n    " + AYS_fixture_to_string(event.fixtures[i]);
    }
    return result;

}

bool AYS_roi_compare(const AYS_event &e1, const AYS_event &e2){
    return e1.roi < e2.roi;
}
bool AYS_arb_compare(const AYS_event &e1, const AYS_event &e2){
    return std::min(e1.arb,e1.not_arb) > std::min(e2.arb,e2.not_arb);
}
bool AYS_unix_time_compare(const AYS_fixture &f1, const AYS_fixture &f2){
    return f1.unix_time < f2.unix_time;
}

bool AYS_btid_compare(const AYS_fixture &f1, const AYS_fixture &f2){
    return f1.btid < f2.btid;
}

bool AYS_sid_compare(const AYS_fixture &f1, const AYS_fixture &f2){
    return f1.sid < f2.sid;
}


void split_on_diff(std::vector<AYS_fixture> fs, 
                   std::vector<std::vector<AYS_fixture>> &out,
                   std::vector<AYS_fixture_comparison_func> comps) {
    if (fs.size() == 0) return;
    for (auto& comp : comps){
        std::stable_sort(fs.begin(), fs.end(), comp);
    }
    int start_idx = 0;
    for (int idx = 0; idx < (int) fs.size(); idx++){
        bool skip = true;
        for (auto& comp : comps){
            skip &= !comp(fs[idx], fs[start_idx]) and !comp(fs[start_idx], fs[idx]);
        }
        if (skip) {
            continue;
        }
        std::vector<AYS_fixture> group(fs.begin()+start_idx, fs.begin()+idx);
        out.emplace_back(group);
        start_idx = idx;
    }
    if(fs.begin()+start_idx < fs.end()) {
        std::vector<AYS_fixture> group(fs.begin()+start_idx, fs.end());
        out.emplace_back(group);
    }
}

std::vector<AYS_event> AYS_fixtures_to_events(std::vector<AYS_fixture> fs) {
    std::vector<AYS_event> result;
    if (fs.size() == 0){
        return result;
    }

    std::vector<std::vector<AYS_fixture>> fixture_groups;
    split_on_diff(fs, fixture_groups, {AYS_unix_time_compare, AYS_btid_compare, AYS_sid_compare});
    while (fixture_groups.size() > 0) { 
        std::vector<std::vector<AYS_fixture>> dissimilar_fixture_groups;
        for (std::vector<AYS_fixture> group : fixture_groups){
            std::vector<AYS_fixture> dissimilar_fixtures;
            std::vector<AYS_fixture> similar_fixtures;
            AYS_fixture f = group[0];
            for (int i = 1; i < (int)group.size(); i++){
                std::vector<int> sol;
                float sim_score = similarity_sort(f.participant_names, group[i].participant_names, &sol);
                if (sim_score > 0.25){
                    dissimilar_fixtures.emplace_back(group[i]);
                    continue;
                }
                for (int j = 0; j<(int)sol.size(); j++){
                    if (sol[j] < j){
                        std::swap(group[i].participant_names[j], group[i].participant_names[sol[j]]);
                        std::swap(group[i].participant_odds[j], group[i].participant_odds[sol[j]]);
                        std::swap(group[i].participant_not_odds[j], group[i].participant_not_odds[sol[j]]);
                    }
                }
                similar_fixtures.emplace_back(group[i]);
            }
            similar_fixtures.emplace_back(f);
            result.emplace_back(similar_fixtures);
            if (dissimilar_fixtures.size()> 0){
                dissimilar_fixture_groups.emplace_back(dissimilar_fixtures);
            }
        }
        fixture_groups = std::move(dissimilar_fixture_groups);         
    }  
    for (auto& event : result) {
        AYS_event_arb(event);
    }

    std::sort(result.begin(), result.end(), AYS_roi_compare);
    return result;
}



