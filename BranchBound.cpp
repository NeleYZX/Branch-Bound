#include "BranchBound.h"
#include "DataRecord.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

//=========================生成初始解（无任何调试输出）=========================
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
) {
    std::vector<int> sorted_parts = parts;
    std::sort(sorted_parts.begin(), sorted_parts.end(),
        [&D](int a, int b) { return D[a] < D[b]; });

    double machine_area = L[0] * W[0];
    BatchMap batches;
    std::vector<int> current_batch;
    current_batch.reserve(parts.size());
    double current_batch_area = 0.0;
    int batch_id = 0;

    for (std::size_t i = 0; i < sorted_parts.size(); ++i) {
        int p = sorted_parts[i];
        double area = l[p] * w[p];
        if (current_batch_area + area <= machine_area) {
            current_batch.push_back(p);
            current_batch_area += area;
        }
        else {
            batches[batch_id++] = std::set<int>(current_batch.begin(), current_batch.end());
            current_batch.clear();
            current_batch.push_back(p);
            current_batch_area = area;
        }
    }
    if (!current_batch.empty()) {
        batches[batch_id] = std::set<int>(current_batch.begin(), current_batch.end());
    }

    std::vector<double> completion(parts.size(), 0.0);
    double time_cursor = 0.0;
    double total_tardiness = 0.0;

    for (typename BatchMap::const_iterator it = batches.begin(); it != batches.end(); ++it) {
        double vol = 0.0, mh = 0.0;
        for (typename std::set<int>::const_iterator jt = it->second.begin(); jt != it->second.end(); ++jt) {
            vol += v[*jt];
            if (h[*jt] > mh) mh = h[*jt];
        }
        double PT = ST[0] + VT[0] * vol + UT[0] * mh;
        for (typename std::set<int>::const_iterator jt = it->second.begin(); jt != it->second.end(); ++jt) {
            completion[*jt] = time_cursor + PT;
        }
        time_cursor += PT;
    }

    for (std::size_t i = 0; i < parts.size(); ++i) {
        total_tardiness += std::max(0.0, completion[parts[i]] - D[parts[i]]);
    }

    return std::make_pair(batches, total_tardiness);
}

//===========================Node 定义===============================
Node::Node()
    : LB(0.0), completion_time(0.0), total_tardiness(0.0), name("N"),depth(0) {
}

Node::Node(const std::unordered_map<int, std::vector<int>>& S_,
    double LB_,
    const std::string& name_,
    double completion_time_,
    double total_tardiness_,
    int depth_)
    : S(S_), LB(LB_), name(name_),
    completion_time(completion_time_), total_tardiness(total_tardiness_) ,depth(depth_){
}

bool Node::operator==(const Node& other) const {
    return S == other.S;
}

std::size_t Node::Hash::operator()(const Node& node) const {
    std::size_t seed = 0;
    for (typename std::unordered_map<int, std::vector<int> >::const_iterator it = node.S.begin();
        it != node.S.end(); ++it) {
        std::size_t h1 = std::hash<int>()(it->first);
        for (std::size_t i = 0; i < it->second.size(); ++i) {
            h1 ^= std::hash<int>()(it->second[i])
                + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
        }
        seed ^= h1 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
}

std::ostream& operator<<(std::ostream& os, const Node& node) {
    os << "Node(" << node.name << "):\n";
    os << "  LB = " << node.LB << "\n";
    os << "  S = {\n";
    for (const auto& pair : node.S) {
        os << "    Batch " << pair.first << ": [";
        for (size_t i = 0; i < pair.second.size(); ++i) {
            os << pair.second[i];
            if (i != pair.second.size() - 1)
                os << ", ";
        }
        os << "]\n";
    }
    os << "  }";
    return os;
}

//=======================子节点生成（位掩码 + 面积即时剪枝）========================
ChildGenerationResult generate_children(
    const Node& node,
    const std::vector<int>& parts,
    double machine_area,
    const std::vector<double>& part_areas
) {
    std::unordered_set<int> assigned;
    for (const auto& [_, batch_parts] : node.S) {
        for (int pid : batch_parts) assigned.insert(pid);
    }

    std::vector<int> unassigned;
    for (int p : parts) {
        if (!assigned.count(p)) unassigned.push_back(p);
    }

    const int n = static_cast<int>(unassigned.size());
    if (n == 0) return { {}, 0 };

    std::vector<Node> children;
    children.reserve((1u << n) - 1);
    int pruned_count = 0;

    int max_batch_id = -1;
    for (const auto& [bid, _] : node.S) {
        max_batch_id = std::max(max_batch_id, bid);
    }

    int child_index = 0;

    for (unsigned mask = 1; mask < (1u << n); ++mask) {
        double area = 0.0;
        std::vector<int> subset;

        for (int i = 0; i < n; ++i) {
            if (mask & (1u << i)) {
                int pid = unassigned[i];
                area += part_areas[pid];
                if (area > machine_area) break;
                subset.push_back(pid);
            }
        }

        if (area > machine_area) {
            ++pruned_count;
            continue;
        }

        auto newS = node.S;
        newS[max_batch_id + 1] = subset;

        std::string child_name = node.name + "_" + std::to_string(child_index++);

        children.emplace_back(
            std::move(newS),
            0.0,
            child_name,
            node.completion_time,
            node.total_tardiness,
            node.depth + 1
        );
    }

    return { children, pruned_count };
}

//======================完成时间计算=============================
std::unordered_map<int, double> compute_completion_times(
    const Node& node,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& h,
    const std::vector<double>& v
) {
    std::unordered_map<int, double> comp_times;

    // 查找最新生成的批次（编号最大）
    if (node.S.empty()) return comp_times;

    int max_batch_id = -1;
    for (const auto& [bid, _] : node.S) {
        max_batch_id = std::max(max_batch_id, bid);
    }

    const auto& part_ids = node.S.at(max_batch_id);

    // 计算体积与最大高度
    double vol = 0.0, mh = 0.0;
    for (int pid : part_ids) {
        vol += v[pid];
        mh = std::max(mh, h[pid]);
    }

    // 计算加工时间
    double PT = ST[0] + VT[0] * vol + UT[0] * mh;
    double start_time = node.completion_time;

    for (int pid : part_ids) {
        comp_times[pid] = start_time + PT;
    }

    return comp_times;
}

//=======================已分配零件总延迟===================
double compute_assigned_tardiness(
    const Node& node,
    const std::vector<double>& D
) {
    if (node.S.empty()) return node.total_tardiness;

    // 查找最新批次（编号最大）
    int max_batch_id = -1;
    for (const auto& [bid, _] : node.S) {
        max_batch_id = std::max(max_batch_id, bid);
    }

    const std::vector<int>& part_ids = node.S.at(max_batch_id);

    // 已知 node.completion_time 是该批次的完成时间，直接使用
    double new_completion_time = node.completion_time;

    double tardiness = 0.0;
    for (int pid : part_ids) {
        tardiness += std::max(0.0, new_completion_time - D[pid]);
    }

    return node.total_tardiness + tardiness;
}

//=======================未分配零件总延迟下界估计====================
double compute_unassigned_lower_bound(
    const Node& node,
    const std::vector<int>& parts,
    const std::vector<double>& D,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& h,
    const std::vector<double>& v
) {
    // 找出已分配的零件
    std::unordered_set<int> assigned;
    for (const auto& [_, part_ids] : node.S) {
        for (int pid : part_ids) {
            assigned.insert(pid);
        }
    }

    // 初始化未分配部分的延迟估计
    double unassigned_tardiness = 0.0;

    for (int p : parts) {
        if (assigned.find(p) == assigned.end()) {
            // 对每个未分配零件，估算加工时间并独立批次处理
            double pt = ST[0] + VT[0] * v[p] + UT[0] * h[p];
            double c = node.completion_time + pt;  // 假设从当前时间并行开始
            unassigned_tardiness += std::max(0.0, c - D[p]);
        }
    }

    // 返回当前延迟 + 未来估计
    return node.total_tardiness + unassigned_tardiness;
}




//========================Branch and Bound（无任何调试输出）========================
// 深度优先搜索递归函数
void dfs(
    Node& cur,
    Node& best,
    double& UB,
    const std::vector<int>& parts,
    const std::vector<std::set<int>>& initial_infeasible,
    const std::vector<double>& D,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& h,
    const std::vector<double>& v,
    const std::vector<double>& part_areas,
    double machine_area,
    Stats& stats,
    std::chrono::steady_clock::time_point t0,
    double time_limit_seconds
) {
    auto t1 = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    if (time_limit_seconds > 0.0 && elapsed > time_limit_seconds) return;

    ++stats.total_nodes;

    // 剪枝1：不可行批次
    for (const auto& batch : cur.S) {
        for (const auto& rule : initial_infeasible) {
            if (std::includes(batch.second.begin(), batch.second.end(), rule.begin(), rule.end())) {
                ++stats.U_pruned_nodes;
                ++stats.pruned_nodes_per_depth[cur.depth];
                return;
            }
        }
    }

    // 剪枝2：下界大于等于当前最优解
    if (cur.LB >= UB) {
        ++stats.LB_pruned_nodes;
        ++stats.pruned_nodes_per_depth[cur.depth];
        return;
    }

    // 判断是否为叶节点（所有零件已分配）
    std::size_t assigned_cnt = 0;
    for (const auto& kv : cur.S) assigned_cnt += kv.second.size();
    if (assigned_cnt == parts.size()) {
        ++stats.leaf_nodes;
        if (cur.LB < UB) {
            UB = cur.LB;
            best = cur;
            ++stats.updated_solutions;
            stats.UB_updates.emplace_back(elapsed, UB);
        }
        return;
    }

    // 生成子节点
    auto [children, pruned] = generate_children(cur, parts, machine_area, part_areas);
    stats.generated_nodes += children.size();
    stats.area_pruned_nodes += pruned;

    for (auto& child : children) {
        auto comp_times = compute_completion_times(child, ST, VT, UT, h, v);
        if (!comp_times.empty()) {
            child.completion_time = comp_times.begin()->second;
        }
        child.total_tardiness = compute_assigned_tardiness(child, D);
        child.LB = compute_unassigned_lower_bound(child, parts, D, ST, VT, UT, h, v);
    }

    std::sort(children.begin(), children.end(), [](const Node& a, const Node& b) {
        return a.LB < b.LB;
        });

    for (auto& child : children) {
        dfs(child, best, UB, parts, initial_infeasible, D, ST, VT, UT, h, v,
            part_areas, machine_area, stats, t0, time_limit_seconds);
    }
}


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
) {
    Stats stats;
    double machine_area = L[0] * W[0];

    std::vector<double> part_areas(parts.size(), 0.0);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        part_areas[parts[i]] = l[parts[i]] * w[parts[i]];
    }

    Node best(initial_S, 0.0, "Best", 0.0, 0.0, 0);
    Node root({}, 0.0, "Root", 0.0, 0.0, 0);
    root.LB = compute_unassigned_lower_bound(root, parts, D, ST, VT, UT, h, v);

    auto t0 = std::chrono::steady_clock::now();

    if (UB > 0 && UB < std::numeric_limits<double>::infinity()) {
        stats.UB_updates.emplace_back(0.0, UB);
        stats.LB_convergence.emplace_back(0.0, root.LB);
    }

    dfs(root, best, UB, parts, initial_infeasible, D, ST, VT, UT, h, v,
        part_areas, machine_area, stats, t0, time_limit_seconds);

    return std::make_pair(best, stats);
}



