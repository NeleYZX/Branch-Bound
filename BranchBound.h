#ifndef BRANCH_BOUND_H
#define BRANCH_BOUND_H

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <string>
#include <utility>
#include <ostream>
#include <fstream>
#include <filesystem>

//===================节点定义=======================
struct Node {
    std::unordered_map<int, std::vector<int>> S;

    double LB = 0.0;
    double completion_time = 0.0;
    double total_tardiness = 0.0;
    std::string name;
    int depth;

    // 缓存字段
    int last_batch_id = -1;
    std::unordered_set<int> assigned_parts;

    // 默认构造
    Node();

    // 自定义构造（用于子节点构造）
    Node(const std::unordered_map<int, std::vector<int>>& S_,
        double LB_,
        const std::string& name_,
        double completion_time_,
        double total_tardiness_, int depth_);

    // 更新缓存字段（构造后或修改 S 后调用）
    void update_cached_fields();

    // 相等判断（用于搜索结构）
    bool operator==(const Node& other) const;

    // 哈希函数支持
    struct Hash {
        std::size_t operator()(const Node& node) const;
    };
};

// 输出重载
std::ostream& operator<<(std::ostream& os, const Node& node);

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
    const std::vector<double>& cached_PT  // 预计算缓存
);


//==========================Branch and Bound========================
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
