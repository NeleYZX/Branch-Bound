#include "BranchBound.h"
#include "DataRecord.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include "DynamicProgramming.h"

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
    : LB(0.0),
    completion_time(0.0),
    total_tardiness(0.0),
    name("N"),
    depth(0)
    , last_batch_id(-1)           // [ADD] 初始化 last_batch_id
{
}

Node::Node(const std::unordered_map<int, std::vector<int>>& S_,
    double LB_,
    const std::string& name_,
    double completion_time_,
    double total_tardiness_,
    int depth_)
    : S(S_),
    LB(LB_),
    completion_time(completion_time_),
    total_tardiness(total_tardiness_),
    name(name_),
    depth(depth_),
    last_batch_id(-1)             // [ADD] 默认 -1
{
    // [ADD] 从 S 计算 last_batch_id 和 assigned_sorted
    for (const auto& kv : S) {
        if (kv.first > last_batch_id) last_batch_id = kv.first;
        for (int pid : kv.second) {
            assigned_sorted.push_back(pid);
        }
    }
    std::sort(assigned_sorted.begin(), assigned_sorted.end());
}

// [ADD] 新构造函数：当我们已经知道 last_batch_id 和 assigned_sorted 时使用
Node::Node(const std::unordered_map<int, std::vector<int>>& S_,
    double LB_,
    const std::string& name_,
    double completion_time_,
    double total_tardiness_,
    int depth_,
    int last_batch_id_,
    std::vector<int>&& assigned_sorted_)
    : S(S_),
    LB(LB_),
    completion_time(completion_time_),
    total_tardiness(total_tardiness_),
    name(name_),
    depth(depth_),
    last_batch_id(last_batch_id_),
    assigned_sorted(std::move(assigned_sorted_))
{
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

// [ADD] 辅助函数：根据 node.assigned_sorted 快速得到未分配零件列表
static inline std::vector<int> get_unassigned_parts(
    const Node& node,
    const std::vector<int>& parts)
{
    std::vector<int> unassigned;
    unassigned.reserve(parts.size() - node.assigned_sorted.size());
    for (int p : parts) {
        if (!std::binary_search(node.assigned_sorted.begin(),
            node.assigned_sorted.end(), p)) {
            unassigned.push_back(p);
        }
    }
    return unassigned;
}

//=======================子节点生成（位掩码 + 面积即时剪枝）========================
ChildGenerationResult generate_children(
    const Node& node,
    const std::vector<int>& parts,
    double machine_area,
    const std::vector<double>& part_areas
) {
    // [MOD] 使用 node.assigned_sorted + 二分搜索，替换原先的 unordered_set 方式
    std::vector<int> unassigned = get_unassigned_parts(node, parts);

    const int n = static_cast<int>(unassigned.size());
    if (n == 0) return { {}, 0 };

    std::vector<Node> children;
    children.reserve((1u << n) - 1);
    int pruned_count = 0;

    // [MOD] 不再扫描 S 求最大批次号，直接使用 node.last_batch_id
    int base_batch_id = node.last_batch_id;  // 可能为 -1（根节点）

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
        int new_bid = base_batch_id + 1;      // [MOD] 新批次号
        newS[new_bid] = subset;

        std::string child_name = node.name + "_" + std::to_string(child_index++);

        // [ADD] 增量构造子节点的 assigned_sorted（在父节点基础上插入新零件）
        std::vector<int> new_assigned = node.assigned_sorted;
        for (int pid : subset) {
            auto it = std::lower_bound(new_assigned.begin(), new_assigned.end(), pid);
            new_assigned.insert(it, pid);
        }

        // [MOD] 使用轻量构造函数，避免在构造函数里再次扫描 S
        children.emplace_back(
            newS,
            0.0,
            child_name,
            node.completion_time,
            node.total_tardiness,
            node.depth + 1,
            new_bid,                     // last_batch_id
            std::move(new_assigned)      // assigned_sorted
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

    // [MOD] 使用 node.last_batch_id，避免再次扫描 S
    if (node.S.empty() || node.last_batch_id < 0) return comp_times;

    const auto& part_ids = node.S.at(node.last_batch_id);

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
    if (node.S.empty() || node.last_batch_id < 0)
        return node.total_tardiness;

    // [MOD] 使用 node.last_batch_id 直接取最后批次
    const std::vector<int>& part_ids = node.S.at(node.last_batch_id);

    // 已知 node.completion_time 是该批次的完成时间，直接使用
    double new_completion_time = node.completion_time;

    double tardiness = 0.0;
    for (int pid : part_ids) {
        tardiness += std::max(0.0, new_completion_time - D[pid]);
    }

    return node.total_tardiness + tardiness;
}

//=======================未分配零件总延迟下界估计====================
//初始LB的计算，假设未分配零件在并行批次上进行
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
    // [MOD] 使用 get_unassigned_parts，替代 unordered_set
    std::vector<int> unassigned_parts = get_unassigned_parts(node, parts);

    // 初始化未分配部分的延迟估计
    double unassigned_tardiness = 0.0;

    for (int p : unassigned_parts) {
        // 对每个未分配零件，估算加工时间并独立批次处理
        double pt = ST[0] + VT[0] * v[p] + UT[0] * h[p];
        double c = node.completion_time + pt;  // 假设从当前时间并行开始
        unassigned_tardiness += std::max(0.0, c - D[p]);
    }

    // 返回当前延迟 + 未来估计
    return node.total_tardiness + unassigned_tardiness;
}

//提出的更加收敛的LB的计算：未分配零件串行计算
double compute_unassigned_lower_bound2(
    const Node& node,
    const std::vector<int>& parts,
    const std::vector<double>& D,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& h,
    const std::vector<double>& v
) {
    // [MOD] 使用 get_unassigned_parts，替代 unordered_set
    std::vector<int> unassigned_parts = get_unassigned_parts(node, parts);

    // 找出未分配零件的最小高度
    double min_height = std::numeric_limits<double>::max();
    for (int p : unassigned_parts) {
        min_height = std::min(min_height, h[p]); // 更新最小高度
    }

    //===========================引入动态规划算法获得未分配零件的最优序列============================
    std::vector<Job> dp_jobs_for_solver;
    dp_jobs_for_solver.reserve(unassigned_parts.size());
    for (int pid : unassigned_parts) {
        dp_jobs_for_solver.push_back(Job{
            pid,                 // id
            v[pid] * VT[0],      // p (近似：只考虑体积相关的处理时间)
            D[pid]               // d
            });
    }
    double dp_initial_time = node.completion_time;
    std::vector<int> optimal_sequence_result;
    double min_tardiness = minimize_total_tardiness(
        dp_jobs_for_solver, dp_initial_time, optimal_sequence_result
    );
    (void)min_tardiness; // 未直接使用，可忽略

    // 初始化延迟估计
    double unassigned_tardiness = 0.0;
    double completion_time_future = node.completion_time;
    double vol_accumulated = 0.0; // 当前已处理部分体积

    // 假设从当前位置开始串行处理未分配的零件
    for (int p : optimal_sequence_result) {
        // 累加当前零件的体积
        vol_accumulated += v[p];

        // 计算该零件的加工时间
        double processing_time = ST[0] + VT[0] * vol_accumulated + UT[0] * min_height;
        completion_time_future = node.completion_time + processing_time;
        double completion_time = completion_time_future;
        unassigned_tardiness += std::max(0.0, completion_time - D[p]);
    }

    // 返回当前延迟 + 估计的未分配延迟下界
    return node.total_tardiness + unassigned_tardiness;
}


//===========================辅助结构与哈希定义===============================

// 用于缓存节点信息的结构体
struct CachedInfo {
    double total_tardiness; // TT
    double completion_time; // C
    double LB;              // LB
};

// 自定义哈希函数：用于 std::vector<int>
// 注意：为了让{1,2}和{2,1}被视为相同的key，传入的vector必须预先排序
struct VectorHash {
    std::size_t operator()(const std::vector<int>& v) const {
        std::size_t seed = 0;
        for (int i : v) {
            // 使用 Boost 风格的 hash combine 算法
            seed ^= std::hash<int>()(i) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};



//========================Branch and Bound（无任何调试输出）========================
std::pair<Node, Stats> branch_and_cut(
    const std::vector<int>& parts,
    const std::vector<double>& D,
    const std::vector<std::set<int> >& initial_infeasible,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& L,
    const std::vector<double>& W,
    const std::vector<double>& l,
    const std::vector<double>& w,
    const std::vector<double>& h,
    const std::vector<double>& v,
    const std::unordered_map<int, std::vector<int> >& initial_S,
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

    auto all_assigned = [&](const Node& nd) -> bool {
        // [MOD] 使用 assigned_sorted 的大小判断是否全部分配
        return nd.assigned_sorted.size() == parts.size();
        };

    //========================= 1. 定义哈希表 =========================
    std::unordered_map<std::vector<int>, CachedInfo, VectorHash> memo_table;

    Node best(initial_S, 0.0, "Best", 0.0, 0.0, 0);
    Node root({}, 0.0, "Root", 0.0, 0.0, 0);
    root.LB = compute_unassigned_lower_bound(root, parts, D, ST, VT, UT, h, v);

    std::deque<Node> stack;
    stack.push_back(root);

    auto t0 = std::chrono::steady_clock::now();
    if (UB > 0 && UB < std::numeric_limits<double>::infinity()) {
        stats.UB_updates.emplace_back(0.0, UB);
        stats.LB_convergence.emplace_back(0.0, root.LB);
    }

    // ========== 新增：自适应出栈策略控制 ==========
    int max_capa = 5000;
    int min_capa = 2000;
    bool use_best_first = true;
    static constexpr double epsilon = 1e-10;

    while (!stack.empty()) {
        auto t1 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();
        if (time_limit_seconds > 0.0 && elapsed > time_limit_seconds) break;

        // === 实时记录当前最小 LB（用于收敛曲线） ===
        if (!stack.empty()) {
            double timestamp = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            double min_LB = std::numeric_limits<double>::infinity();
            for (const Node& nd : stack) {
                if (nd.LB < min_LB) min_LB = nd.LB;
            }
            if (min_LB >= UB) min_LB = UB;
            if (min_LB >= 0.0 && min_LB < std::numeric_limits<double>::infinity()) {
                if (stats.LB_convergence.empty() || std::abs(min_LB - stats.LB_convergence.back().second) > epsilon) {
                    stats.LB_convergence.emplace_back(timestamp, min_LB);
                }
            }
        }

        // === 动态选择出栈策略 ===
        if (stack.size() > static_cast<std::size_t>(max_capa)) {
            use_best_first = false;
        }
        else if (stack.size() < static_cast<std::size_t>(min_capa)) {
            use_best_first = true;
        }

        Node cur;
        if (use_best_first) {
            auto best_it = std::min_element(stack.begin(), stack.end(),
                [](const Node& a, const Node& b) {
                    return a.LB < b.LB;
                });
            cur = *best_it;
            stack.erase(best_it);
        }
        else {
            cur = stack.back();
            stack.pop_back();
        }

        ++stats.total_nodes;

        // 不可行剪枝
        bool bad = false;
        for (const auto& it : cur.S) {
            for (const auto& infeasible_set : initial_infeasible) {
                if (std::includes(it.second.begin(), it.second.end(),
                    infeasible_set.begin(), infeasible_set.end())) {
                    bad = true;
                    break;
                }
            }
            if (bad) break;
        }
        if (bad) {
            ++stats.U_pruned_nodes;
            ++stats.pruned_nodes_per_depth[cur.depth];
            continue;
        }

        // 下界剪枝
        if (cur.LB >= UB) {
            ++stats.LB_pruned_nodes;
            ++stats.pruned_nodes_per_depth[cur.depth];
            continue;
        }

        // 叶子节点
        if (all_assigned(cur)) {
            ++stats.leaf_nodes;
            if (cur.LB < UB) {
                UB = cur.LB;
                best = cur;
                ++stats.updated_solutions;
                double timestamp = std::chrono::duration<double>(t1 - t0).count();
                stats.UB_updates.emplace_back(timestamp, UB);
            }
            continue;
        }

        // 展开子节点
        bool is_root_node = (cur.depth == 0);
        auto [kids, pruned] = generate_children(cur, parts, machine_area, part_areas);
        stats.generated_nodes += kids.size();
        stats.area_pruned_nodes += pruned;

        for (auto& child : kids) {
            auto comp_times = compute_completion_times(child, ST, VT, UT, h, v);
            if (!comp_times.empty()) {
                child.completion_time = comp_times.begin()->second;
            }

            child.total_tardiness = compute_assigned_tardiness(child, D);

            // [MOD] 使用 child.assigned_sorted 作为 key，避免每次重新收集+排序
            if (child.depth == 1) {
                // 深度1：用较强 LB2，并强制存表
                child.LB = compute_unassigned_lower_bound2(child, parts, D, ST, VT, UT, h, v);

                memo_table[child.assigned_sorted] = CachedInfo{
                    child.total_tardiness,
                    child.completion_time,
                    child.LB
                };
            }
            else if (child.depth == 2) {
                // 深度2：查表复用 / 简单 LB1 + 存表
                bool lb_found = false;

                auto memo_it = memo_table.find(child.assigned_sorted);
                if (memo_it != memo_table.end()) {
                    const CachedInfo& cached = memo_it->second;
                    if (child.total_tardiness >= cached.total_tardiness &&
                        child.completion_time >= cached.completion_time) {
                        child.LB = cached.LB + (child.completion_time - cached.completion_time);
                        lb_found = true;
                    }
                }

                if (!lb_found) {
                    child.LB = compute_unassigned_lower_bound(child, parts, D, ST, VT, UT, h, v);

                    if (memo_it == memo_table.end()) {
                        memo_table[child.assigned_sorted] = CachedInfo{
                            child.total_tardiness,
                            child.completion_time,
                            child.LB
                        };
                    }
                    else {
                        if (child.total_tardiness <= memo_it->second.total_tardiness &&
                            child.completion_time <= memo_it->second.completion_time) {
                            memo_it->second = CachedInfo{
                                child.total_tardiness,
                                child.completion_time,
                                child.LB
                            };
                        }
                    }
                }
            }
            else {
                // 其他深度 -> 仅简单计算
                child.LB = compute_unassigned_lower_bound(child, parts, D, ST, VT, UT, h, v);
            }

            // 保持下界单调性
            if (child.LB < cur.LB) {
                child.LB = cur.LB;
            }

            if (child.LB < UB) {
                stack.push_back(std::move(child));
            }
            else {
                ++stats.LB_pruned_nodes;
                ++stats.pruned_nodes_per_depth[child.depth];
            }
        }
    }

    return std::make_pair(best, stats);
}