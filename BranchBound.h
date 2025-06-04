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
        double total_tardiness_);

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

//=======================数据记录====================
// 获取日志文件名（避免覆盖）
std::string get_log_filename(const std::string& input_filename);

// 全局日志输出对象
extern std::ofstream log_stream;

// 写入 UTF-8 BOM 到日志文件开头
void write_utf8_bom(std::ofstream& stream);

// 同时输出到控制台和日志文件
template <typename T>
void log_and_cout(const T& msg) {
    std::cout << msg;
    if (log_stream.is_open()) {
        log_stream << msg;
    }
}
#endif // BRANCH_BOUND_H
