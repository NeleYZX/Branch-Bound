#ifndef BRANCH_BOUND_H
#define BRANCH_BOUND_H

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <deque>
#include <string>
#include <utility>
#include <ostream>
#include <fstream>
#include <filesystem>

//===================节点定义=======================
class Node {
public:
    // 每个批次对应的零件列表
    std::unordered_map<int, std::vector<int>> S;
    double LB;            // 当前节点的下界
    double completion_time;     // 当前累计完成时间
    double total_tardiness;     // 当前已产生总延迟
    std::string name;     // 节点名称
    int depth;

    Node();
    Node(const std::unordered_map<int, std::vector<int>>& S_,
        double LB_,
        const std::string& name_ = "N",
        double completion_time_ = 0.0,
        double total_tardiness_ = 0.0,
        int depth_ = 0);

    bool operator==(const Node& other) const;

    friend std::ostream& operator<<(std::ostream& os, const Node& node);

    struct Hash {
        std::size_t operator()(const Node& node) const;
    };
};

//std::ostream& operator<<(std::ostream& os, const Node& node);

//========================生成初始解==============================
typedef std::map<int, std::set<int>> BatchMap;

std::pair<BatchMap, double> generateInitialSolution(
    const std::vector<int>& parts,
    const std::vector<double>& L,
    const std::vector<double>& W,
    const std::vector<double>& l,
    const std::vector<double>& w,
    const std::vector<double>& v,
    const std::vector<double>& h,
    const std::vector<double>& D,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT
);

//=========================子节点生成==============================
struct ChildGenerationResult {
    std::vector<Node> children;
    int pruned_count;
};


//std::vector<Node> generate_children(
//    const Node& node,
//    const std::vector<int>& parts,
//    double machine_area,
//    const std::vector<double>& part_areas
//);


//=========================下界计算================================
std::unordered_map<int, double> compute_completion_times(
    const Node& node,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& h,
    const std::vector<double>& v
);

double compute_assigned_tardiness(
    const Node& node,
    const std::vector<double>& D
);

double compute_unassigned_lower_bound(
    const Node& node,
    const std::vector<int>& parts,
    const std::vector<double>& D,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& h,
    const std::vector<double>& v
);

//==========================Branch and Bound========================
struct FirstLevelNodeInfo {
    std::string name;
    double lb;
    double completion_time;
    int unassigned_count;
    std::vector<int> unassigned_parts; // 存储未分配零件的 ID
};

struct Stats {
    int updated_solutions = 0;
    int total_nodes = 0;
    int generated_nodes = 0;
    int area_pruned_nodes = 0;
    int LB_pruned_nodes = 0;
    int U_pruned_nodes = 0;
    int leaf_nodes = 0;
    std::unordered_map<int, int> pruned_nodes_per_depth;     //每个深度被剪枝的节点数
    std::vector<std::pair<double, double>> UB_updates;       // <时间戳, 新UB>
    std::vector<std::pair<double, double>> LB_convergence;   // <时间戳, 当前最小LB>
    // [新增] 替换原来的 pair vector，或者新增这个详细信息的 vector
    std::vector<FirstLevelNodeInfo> first_level_details;
    std::vector<std::pair<std::string, double>> first_level_node_lbs; // 新增：存储第一层子节点的名称和LB
    long long total_V_calls;       // V() 被调用的总次数
    long long local_memo_hits;     // 命中本次调用的 memo（SubsetKey）的次数
    long long global_memo_hits;    // 命中全局 global_memo 的次数（跨调用复用）
    long long computed_states;     // 真正需要计算的新状态数（没命中任何缓存）
    long long delta_trigger_count = 0;   // 记录Delta策略触发次数
    long long serial_pruning_count = 0;  // 记录并行无法剪枝但串行成功剪枝的次数
    long long serial_missing_count = 0;  // 记录串行比并行小或相等的次数
};

std::pair<Node, Stats> branch_and_cut(
    const std::vector<int>& parts,
    const std::vector<double>& D,
    const std::vector<std::set<int>>& initial_infeasible,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& L,
    const std::vector<double>& W,
    const std::vector<double>& l,
    const std::vector<double>& w,
    const std::vector<double>& h,
    const std::vector<double>& v,
    const std::unordered_map<int, std::vector<int>>& initial_S,
    double UB,
    double time_limit_seconds,
    const std::string& path
);


#endif // BRANCH_BOUND_H
