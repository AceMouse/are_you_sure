#include <iostream>
#include <string>
#include <vector>
#include "includes/levenshtein-sse.hpp"

#include <lemon/smart_graph.h>
#include <lemon/network_simplex.h>
#include <lemon/cost_scaling.h>
#include <lemon/graph_to_eps.h>


int min_weight_matching(const int pairs, const std::vector<std::vector<int>> &weightMatrix, std::vector<int> &solution){
    using MCF = lemon::NetworkSimplex<lemon::SmartDigraph, int32_t, int32_t>;
    using Graph = lemon::SmartDigraph;
    using Node = Graph::Node;
    using Arc = Graph::Arc;

    int nodeNum = pairs*2+2; // worker and task in each pair + source and sink
    int arcNum = pairs*(pairs+2); // all to all between workers and tasks 
                                  // + one to all source to workers
                                  //+ all to one tasks to sink 
    
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

/*int main(){
    std::vector<std::string> a1 = {"hej", "abe", "Louise"};
    std::vector<std::string> a2 = {"Aber", "Hej", "louIs"};
    std::vector<int> sol; 
    float sim_score = similarity_sort(a1, a2, &sol);
    std::cout << "similatity score " << sim_score << std::endl;
    for (int i = 0; i<(int)sol.size(); i++){
        std::cout << i << ":" << a1[i] << " -> " << sol[i] << ":" <<a2[sol[i]] << std::endl; 
    }
    return 0;
}*/
