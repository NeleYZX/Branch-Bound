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
    double LB;                   // 当前节点的下界
    double mother_LB;           // 母节点下界
    double completion_time;      // 当前累计完成时间
    double total_tardiness;      // 当前已产生总延迟
    std::string name;            // 节点名称
    int depth;                   // 树深度

    Node(); // 默认构造函数
    Node(const std::unordered_map<int, std::vector<int>>& S_,
        double LB_,
        double mother_LB_ = std::numeric_limits<double>::infinity(),
        const std::string& name_ = "N",
        double completion_time_ = 0.0,
        double total_tardiness_ = 0.0,
        int depth_ = 0); // 带参数构造函数

    bool operator==(const Node& other) const; // 重载相等运算符

    friend std::ostream& operator<<(std::ostream& os, const Node& node); // 输出重载

    struct Hash {
        std::size_t operator()(const Node& node) const; // 哈希函数
    };
};

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
