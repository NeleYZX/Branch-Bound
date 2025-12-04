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
    // 找出已分配的零件
    std::unordered_set<int> assigned;
    for (const auto& [_, part_ids] : node.S) {
        for (int pid : part_ids) {
            assigned.insert(pid);
        }
    }

    // 找出未分配零件的最小高度
    double min_height = std::numeric_limits<double>::max();
    std::vector<int> unassigned_parts;
    for (int p : parts) {
        if (assigned.find(p) == assigned.end()) {
            unassigned_parts.push_back(p);
            min_height = std::min(min_height, h[p]); // 更新最小高度
        }
    }
    //===========================引入动态规划算法获得未分配零件的最优序列============================
    std::vector<Job> dp_jobs;
    std::vector<Job> dp_jobs_for_solver;
    for (int pid : unassigned_parts) {
        dp_jobs_for_solver.push_back(Job{  // 显式调用构造函数，或者可以省略 Job
            pid,                 // id
            v[pid] * VT[0],      // p (近似：只考虑体积相关的处理时间)
            D[pid]               // d
            });
    }
    double dp_initial_time = node.completion_time;
    std::vector<int> optimal_sequence_result;
    double min_tardiness = minimize_total_tardiness(dp_jobs_for_solver, dp_initial_time, optimal_sequence_result);


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
        double completion_time = completion_time_future + processing_time; // 时刻更新为当前零件的完成时间
        unassigned_tardiness += std::max(0.0, completion_time - D[p]);


    }

    // 返回当前延迟 + 估计的未分配延迟下界
    return node.total_tardiness + unassigned_tardiness;
}


//=======================Dynamic programming动态规划算法获得未分配零件的最优序列================================




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
        std::size_t cnt = 0;
        for (const auto& it : nd.S) {
            cnt += it.second.size();
        }
        return cnt == parts.size();
        };

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
        // 只有当当前节点是根节点 (depth == 0) 时，才记录其子节点的名称和 LB
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
            // 如果是第一层子节点 (depth == 1)，使用较强的 LB2,否则使用较快的 LB1

            if (child.depth == 1) {
                child.LB = compute_unassigned_lower_bound2(child, parts, D, ST, VT, UT, h, v);
            }
            else {
                child.LB = compute_unassigned_lower_bound(child, parts, D, ST, VT, UT, h, v);
            }

            // 2. 保持下界单调性：如果计算出的子节点 LB 小于父节点 LB，则继承父节点的 LB
           //    cur 是当前父节点
            if (child.LB < cur.LB) {
                child.LB = cur.LB;
            }

            // 记录第一层子节点的名称和LB
            //if (is_root_node) {
            //    stats.first_level_node_lbs.emplace_back(child.name, child.LB);
            //}

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



