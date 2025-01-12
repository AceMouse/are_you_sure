#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <limits>
#include <iostream>
#include <ctime>
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include <fmt/format.h>

#include "includes/levenshtein-sse.hpp"

#include <lemon/smart_graph.h>
#include <lemon/network_simplex.h>
#include <lemon/cost_scaling.h>
#include <lemon/graph_to_eps.h>

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
std::vector<AYS_event> AYS_fixtures_to_events(std::vector<AYS_fixture> fs);
#ifndef AYS_IMPLEMENTATION
#define AYS_IMPLEMENTATION  

struct AYS_fixture {
    time_t start_time;
    time_t expiry_time;
    AYS_provider_id pid;
    AYS_fixture_id id;
    AYS_sport_id sid;
    AYS_bet_type_id btid;
    int line;
    std::string currency;
    float max_nominal_bet;
    std::vector<AYS_participant> participant_names;
    std::vector<AYS_odd> participant_not_odds;
    std::vector<AYS_odd> participant_odds;
    AYS_fixture(time_t start_time,
        time_t expiry_time,
		AYS_provider_id pid,
		AYS_fixture_id id,
		AYS_sport_id sid,
		AYS_bet_type_id btid,
		int line,
        std::string currency,
        float max_nominal_bet,
		std::vector<AYS_participant> &participant_names,
        std::vector<AYS_odd> &participant_not_odds, 
        std::vector<AYS_odd> &participant_odds) 
        : start_time(start_time)
        , expiry_time(expiry_time)
        , pid(pid)
        , id(id)
        , sid(sid)
        , btid(btid)
        , line(line)
        , currency(currency)
        , max_nominal_bet(max_nominal_bet)
        , participant_names(std::move(participant_names))
        , participant_not_odds(std::move(participant_not_odds))
        , participant_odds(std::move(participant_odds)) { }
};

struct AYS_event {
    time_t start_time;
    AYS_sport_id sid;
    AYS_bet_type_id btid;
    int line;
    float arb;
    float not_arb;
    float roi;
    uint32_t arb_flags;
    std::vector<AYS_participant> participant_names;
    std::vector<float> participant_stakes;
    std::vector<AYS_fixture> fixtures;
    int max_not_arb_idx;
    AYS_event(std::vector<AYS_fixture> &fixtures) 
        : start_time(fixtures[0].start_time)
        , sid(fixtures[0].sid)
        , btid(fixtures[0].btid)
        , line(fixtures[0].line)
        , participant_names(fixtures[0].participant_names)
        , fixtures(std::move(fixtures)) {
        arb = std::numeric_limits<float>::infinity();
        not_arb = std::numeric_limits<float>::infinity();
        roi = -1;
        participant_stakes = std::vector<float>(participant_names.size(), 0);
    }
};


bool AYS_event_arb(AYS_event &event){
    std::vector<float> max_odds(event.participant_names.size(),1);
    std::vector<float> max_not_odds(event.participant_names.size(),1);
    for (int i = 0; i < (int)event.fixtures.size(); i++){
        if (max_odds.size() != event.fixtures[i].participant_odds.size()){
            std::cerr << "diffent amount of outcomes in fixtures 0 and " << i << "\n" 
                      << AYS_event_to_string(event) << std::endl;
            return false;
        }
        for (int j = 0; j < (int)max_odds.size();j++){
            if (max_odds[j] > event.fixtures[i].participant_odds[j]){
                max_odds[j] = event.fixtures[i].participant_odds[j]; 
            }
            if (max_not_odds[j] > event.fixtures[i].participant_not_odds[j]){
                max_not_odds[j] = event.fixtures[i].participant_not_odds[j]; 
            }
        }
    }
    float per = 0;
    for (int i = 0; i < (int)max_odds.size(); i++){
        per += max_odds[i];
        if (event.not_arb > max_odds[i]+max_not_odds[i]){
            event.not_arb = max_odds[i]+max_not_odds[i];
            event.max_not_arb_idx = i;
        }

    }
    event.arb = per;
    event.roi = 100/std::min(per, event.not_arb)-100;
    return true;
}

bool AYS_event_max_arb_stakes(const AYS_event &event, std::vector<float> &stakes, std::vector<int> &max_idx, std::vector<int> &max_not_idx, float *max_profit){
    max_idx.resize(event.participant_names.size(),-1);
    max_not_idx.resize(event.participant_names.size(),-1);
    std::vector<float> max_odds(event.participant_names.size(),1);
    std::vector<float> max_not_odds(event.participant_names.size(),1);
    for (int i = 0; i < (int)event.fixtures.size(); i++){
        for (int j = 0; j < (int)max_odds.size();j++){
            if (max_odds[j] > event.fixtures[i].participant_odds[j]){
                max_odds[j] = event.fixtures[i].participant_odds[j]; 
                max_idx[j] = i; 
            }
            if (max_not_odds[j] > event.fixtures[i].participant_not_odds[j]){
                max_not_odds[j] = event.fixtures[i].participant_not_odds[j]; 
                max_not_idx[j] = i; 
            }
        }
    }
    float max_total_stake = std::numeric_limits<float>::infinity();
    float total_percentage_odds = std::min(event.not_arb,event.arb);
    if (event.arb <= event.not_arb){
        stakes.resize(event.participant_names.size(),0);
        for (int idx = 0; idx < (int)event.participant_names.size(); idx++){
            float potential_stake = event.fixtures[max_idx[idx]].max_nominal_bet*total_percentage_odds/max_odds[idx];
            if (max_total_stake > potential_stake) {
                max_total_stake = potential_stake;
            }
        }
        for (int idx = 0; idx < (int)event.participant_names.size(); idx++){
            stakes[idx] = max_total_stake*max_odds[idx]/total_percentage_odds;
        }
    } else {
        stakes.resize(2,0);
        int idx = event.max_not_arb_idx;
        max_total_stake = std::min(event.fixtures[max_idx[idx]].max_nominal_bet*total_percentage_odds/max_odds[idx],
                                   event.fixtures[max_not_idx[idx]].max_nominal_bet*total_percentage_odds/max_not_odds[idx]);
        stakes[0] = max_total_stake*max_odds[idx]/total_percentage_odds;
        stakes[1] = max_total_stake*max_not_odds[idx]/total_percentage_odds;
    }
    *max_profit = max_total_stake/total_percentage_odds-max_total_stake;
    return true;
}


std::string AYS_event_to_string_pretty(const AYS_event &event, std::vector<std::string> provider_names) {
    std::vector<float> max_odds(event.participant_names.size(),1);
    std::vector<float> max_not_odds(event.participant_names.size(),1);
    for (int i = 0; i < (int)event.fixtures.size(); i++){
        for (int j = 0; j < (int)max_odds.size();j++){
            if (max_odds[j] > event.fixtures[i].participant_odds[j]){
                max_odds[j] = event.fixtures[i].participant_odds[j]; 
            }
            if (max_not_odds[j] > event.fixtures[i].participant_not_odds[j]){
                max_not_odds[j] = event.fixtures[i].participant_not_odds[j]; 
            }
        }
    }
    bool not_odds = event.not_arb < event.arb;
    std::tm * ptm = std::localtime(&event.start_time);
    char buffer[32];
    std::strftime(buffer, 32, "%d/%m-%YT%H:%M:%S", ptm);
    std::vector<float> stakes; 
    std::vector<int> max_idx; 
    std::vector<int> max_not_idx; 
    float max_profit = 0;
    if (!AYS_event_max_arb_stakes(event, stakes, max_idx, max_not_idx, &max_profit)){
        return "STAKES FAIL";
    }
    std::string result = fmt::format("@ {} sid: {} btid: {}{} {}ARB: {:.2f}% ROI: {:.2f}% profit: {:.2f} assuming same currency\n\t", buffer, event.sid, event.btid, 
                          (event.line?fmt::format(" line: {}",event.line):""),
                          event.arb <= event.not_arb?"":"n", 
                          std::min(event.arb,event.not_arb)*100,
                          event.roi,
                          max_profit);

    int min_width = 5;
    if (not_odds){
        int width = std::max(min_width,(int)event.fixtures[0].participant_names[event.max_not_arb_idx].size());
        int not_width = std::max(min_width,(int)event.fixtures[0].participant_names[event.max_not_arb_idx].size()+4);
        result += fmt::format("{0:^{1}.2f} | {2:^{3}.2f}", stakes[0], width, stakes[1], not_width); 
        result += " <- stakes for max profit\n"; 
        result += fmt::format("\t{0:^{1}} | Not {0:^{1}}", event.participant_names[event.max_not_arb_idx], min_width);
    } else {
        for (int i = 0; i < (int) stakes.size(); i++ ){
            int width = std::max(min_width,(int)event.fixtures[0].participant_names[i].size());
            result += fmt::format("{:^{}.2f} | ", stakes[i], width); 
        }
        result += " <- stakes for max profit\n"; 
        result += fmt::format("\t{:^{}}", event.participant_names[0], min_width);
        for (int i = 1; i< (int)event.participant_names.size(); i++){
            result += fmt::format(" | {:^{}}", event.participant_names[i], min_width);
        }
    }
    result += " | max bet | currency | provider | event id \n";
    for (int i = 0; i < (int)event.fixtures.size(); i++){
        bool is_maximal = false;
        if (not_odds){
            is_maximal = event.fixtures[i].participant_not_odds[event.max_not_arb_idx] == max_not_odds[event.max_not_arb_idx]
                       | event.fixtures[i].participant_odds[event.max_not_arb_idx] == max_odds[event.max_not_arb_idx];
        } else {
            for (int idx = 0; idx < (int)event.fixtures[i].participant_odds.size(); idx++){
                is_maximal |= event.fixtures[i].participant_odds[idx] == max_odds[idx];
            } 
        }
        if (!is_maximal) continue;
        result += "\t";
        std::string s = " ";
        if (not_odds){
            int width = std::max(min_width,(int)event.fixtures[0].participant_names[event.max_not_arb_idx].size());
            s = " ";
            if (event.fixtures[i].participant_odds[event.max_not_arb_idx] < 1.){
                s = fmt::format("{:.2f}", 1/event.fixtures[i].participant_odds[event.max_not_arb_idx]); 
            }
            result += fmt::format("{:^{}}", s, width); 
            width = std::max(min_width,(int)event.fixtures[0].participant_names[event.max_not_arb_idx].size()+4);
            s = " ";
            if (event.fixtures[i].participant_not_odds[event.max_not_arb_idx] < 1.){
                s = fmt::format("{:.2f}", 1/event.fixtures[i].participant_not_odds[event.max_not_arb_idx]); 
            }
            result += fmt::format(" | {:^{}}", s, width); 
        } else {
            int width = std::max(min_width,(int)event.fixtures[0].participant_names[0].size());
            if (event.fixtures[i].participant_odds[0] < 1.){
                s = fmt::format("{:.2f}", 1/event.fixtures[i].participant_odds[0]); 
            }
            result += fmt::format("{:^{}}", s, width); 
            for (int j = 1; j < (int)event.fixtures[i].participant_odds.size(); j++){
                int width = std::max(min_width,(int)event.fixtures[0].participant_names[j].size());
                s = " ";
                if (event.fixtures[i].participant_odds[j] < 1.){
                    s = fmt::format("{:.2f}", 1/event.fixtures[i].participant_odds[j]); 
                }
                result += fmt::format(" | {:^{}}", s, width); 
            }
        }
        result += fmt::format(" | {:^7.2f} | {:^8} | {:^8} | {}\n",event.fixtures[i].max_nominal_bet,event.fixtures[i].currency, provider_names[event.fixtures[i].pid], event.fixtures[i].id); 
    }
    return result;
}

std::string AYS_fixture_to_string(const AYS_fixture fixture) {
    bool print_not_odds = false;
    for (int i = 0; i< (int)fixture.participant_not_odds.size(); i++){
        print_not_odds |= fixture.participant_not_odds[i] > 1.;
    }
    std::string result = fixture.participant_names[0];
    for (int i = 1; i< (int)fixture.participant_names.size(); i++){
        result += " | " + fixture.participant_names[i];
    }
    for (int i = 0; print_not_odds && i < (int)fixture.participant_names.size(); i++){
        result += " | NOT "  + fixture.participant_names[i];
    }
    std::tm * ptm = std::localtime(&fixture.start_time);
    char buffer[32];
    std::strftime(buffer, 32, "%d/%m-%YT%H:%M:%S", ptm);
    result += " @ " + std::string(buffer) + " sid: " + std::to_string(fixture.sid) + " line: " + std::to_string(fixture.line)  + " pid "+ std::to_string(fixture.pid) + " ";
    result += std::to_string(fixture.participant_odds[0]);
    for (int i = 1; i< (int)fixture.participant_odds.size(); i++){
        result += " | " + std::to_string(fixture.participant_odds[i]);
    }
    for (int i = 0; print_not_odds && i < (int)fixture.participant_not_odds.size(); i++){
        result += " | " + std::to_string(fixture.participant_not_odds[i]);
    }
    return result;
}
std::string AYS_event_to_string(const AYS_event event) {
    std::string result = event.participant_names[0];
    for (int i = 1; i< (int)event.participant_names.size(); i++){
        result += " | " + event.participant_names[i];
    }
    std::tm * ptm = std::localtime(&event.start_time);
    char buffer[32];
    std::strftime(buffer, 32, "%d/%m-%YT%H:%M:%S", ptm);
    result += " @ " + std::string(buffer)+ " sid: " + std::to_string(event.sid) + " btid: " + std::to_string(event.btid) + " line: " + std::to_string(event.line) + " ARB(not): " + std::to_string(event.arb*100) + "% (" +std::to_string(event.not_arb*100) + "%)" + " ROI: " + std::to_string(event.roi) + "%";
    time_t now = std::time(0);
    for (int i = 0; i< (int)event.fixtures.size(); i++){
        if (difftime(event.fixtures[i].expiry_time, now) > 0){
            result += "\n    " + AYS_fixture_to_string(event.fixtures[i]);
        }
    }
    return result;

}
bool AYS_roi_compare(const AYS_event &e1, const AYS_event &e2){
    return e1.roi < e2.roi;
}
bool AYS_arb_compare(const AYS_event &e1, const AYS_event &e2){
    return std::min(e1.arb,e1.not_arb) > std::min(e2.arb,e2.not_arb);
}
bool AYS_line_compare(const AYS_fixture &f1, const AYS_fixture &f2){
    return f1.line < f2.line;
}
bool AYS_start_time_compare(const AYS_fixture &f1, const AYS_fixture &f2){
    return difftime(f1.start_time, f2.start_time) < 0;
}
bool AYS_participant_number_compare(const AYS_fixture &f1, const AYS_fixture &f2){
    return f1.participant_names.size() < f2.participant_names.size();
}

bool AYS_btid_compare(const AYS_fixture &f1, const AYS_fixture &f2){
    return f1.btid < f2.btid;
}

bool AYS_sid_compare(const AYS_fixture &f1, const AYS_fixture &f2){
    return f1.sid < f2.sid;
}

int min_weight_matching(const int pairs, const std::vector<std::vector<int>> &weightMatrix, std::vector<int> &solution){
    using Graph = lemon::SmartDigraph;
    using Node = Graph::Node;
    using Arc = Graph::Arc;
    using MCF = lemon::NetworkSimplex<Graph, int32_t, int32_t>;

    int nodeNum = pairs*2+2; // worker and task in each pair + source and sink
    int arcNum = pairs*(pairs+2); // all to all between workers and tasks 
                                  // + one to all source to workers
                                  // + all to one tasks to sink 
    
    Graph g;
    std::vector<Node> nodes(nodeNum);
    std::vector<Arc> arcs(arcNum);
    Graph::ArcMap<int32_t> weights(g);
    Graph::ArcMap<int32_t> capacities(g);
    for(int i = 0; i < nodeNum; i++){
        nodes[i] = g.addNode();
    }
    int sourceIDX = 0;
    int sinkIDX = nodeNum-1;
    Node sourceNode = nodes[sourceIDX];
    Node sinkNode = nodes[sinkIDX];
    Arc arc;
    Node toNode;
    Node fromNode;
    int arcIDX = 0; 
    for (int to = 1; to < nodeNum-1; to+=2){ // source to workers
        toNode = nodes[to];
        arc = g.addArc(sourceNode, toNode);
        capacities[arc] = 1;
        weights[arc] = 1;
        //std::cout << "edge: " << sourceIDX << "->" << to << " @ " << weights[arc] << std::endl ;
        arcs[arcIDX++] = arc;
    }
    for (int from = 2; from < nodeNum-1; from+=2){ // tasks to sink
        fromNode = nodes[from];
        arc = g.addArc(fromNode, sinkNode);
        capacities[arc] = 1;
        weights[arc] = 1;
        //std::cout << "edge: " << from << "->" << sinkIDX << " @ " << weights[arc] << std::endl ;
        arcs[arcIDX++] = arc;
    }
    for (int worker = 0; worker < pairs; worker++ ){ // workers to tasks
        for (int task = 0; task < pairs; task++ ){
            int from = worker*2+1;
            int to = task*2+2;
            toNode = nodes[to];
            fromNode = nodes[from];
            arc = g.addArc(fromNode, toNode);
            capacities[arc] = 1;
            weights[arc] = weightMatrix[worker][task];
            //std::cout << "edge: " << from << "->" << to << " @ " << weights[arc] << std::endl ;
            arcs[arcIDX++] = arc;
        }
    }

    Graph::ArcMap<int32_t> flows(g);
    MCF solver(g);
    MCF::ProblemType status = solver.upperMap(capacities).costMap(weights).stSupply(sourceNode, sinkNode, pairs).run();
    switch (status) 
    {
        case MCF::INFEASIBLE:
            std::cerr << "insufficient flow" << std::endl;
            return 1;
        case MCF::OPTIMAL:
            solver.flowMap(flows);
            for (int worker = 0; worker < pairs; worker++ ){ // workers to tasks
                for (int task = 0; task < pairs; task++ ){
                    arc = arcs[pairs*2 + worker*pairs + task];
                    if(flows[arc] != 0){
                        solution[worker] = task;
                    }
                }
            }
            break;
        case MCF::UNBOUNDED:
            std::cerr << "infinite flow" << std::endl;
            return 2;
        default:
            break;
    }
    return 0;
}


float similarity_sort(const std::vector<std::string> &a1,const std::vector<std::string> &a2, std::vector<int> *solution){
    if (a1.size() != a2.size()){
        std::cerr << "different lengths a1:" <<a1.size() << " != a2:" << a2.size() << std::endl;
        return 1.f;
    }
    int pairs = a1.size();
    (*solution).resize(pairs);
    std::vector<std::vector<int>> sim_matrix(pairs, std::vector<int> (pairs, 0)) ;
    for (int i = 0; i<pairs; i++){
        for (int j = 0; j<pairs; j++){
            sim_matrix[i][j] = levenshteinSSE::levenshtein(a1[i], a2[j]);//https://stackoverflow.com/a/24063783   to lower?
        }
    }
    if (min_weight_matching(pairs, sim_matrix, *solution)){
        return 1.f;
    }
    float max = 0;
    for (int i = 0; i<pairs; i++){
        float x = (float)sim_matrix[i][(*solution)[i]]/((a1[i].size() + a2[i].size()) >>1);
        if (x>max){
            max = x;
        }
    }

    return max; 
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

std::vector<AYS_event> AYS_fixtures_to_events(std::vector<AYS_fixture> fs, bool includeLive) {
    std::vector<AYS_event> result;
    if (fs.size() == 0){
        return result;
    }
    time_t now = std::time(0);
    std::vector<AYS_fixture> filtered_fs;
    filtered_fs.reserve(fs.size());
    for (auto& fixture : fs) {
        if (difftime(fixture.expiry_time, now) > 0 ){
            if (includeLive || difftime(fixture.start_time, now) > 0){
                if (fixture.participant_names.size()<=3){
                    filtered_fs.push_back(fixture);
                }
            } 
        }
    }
    fs = filtered_fs;

    std::vector<std::vector<AYS_fixture>> fixture_groups;
    split_on_diff(fs, fixture_groups, {AYS_start_time_compare, AYS_btid_compare, AYS_sid_compare, AYS_participant_number_compare, AYS_line_compare});
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


#endif
