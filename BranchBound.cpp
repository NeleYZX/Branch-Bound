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
    double UB_initial, // 将 UB 作为参数传入，但内部会更新
    double time_limit_seconds,
    const std::string& path
) {
    Stats stats;
    double machine_area = L[0] * W[0];

    // 预计算面积
    std::vector<double> part_areas(parts.size(), 0.0);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        // parts[i] 是零件的实际ID，确保 part_areas 的大小足以存储所有零件的面积
        // 如果 parts 是 0 到 N-1 的连续索引，那么 part_areas 可以直接用 parts[i] 作为索引
        // 否则，可能需要一个 map 或调整大小
        if (parts[i] < l.size() && parts[i] < w.size()) { // 避免越界
            part_areas[parts[i]] = l[parts[i]] * w[parts[i]];
        }
    }

    // 判断是否全部分配
    auto all_assigned = [&](const Node& nd)->bool {
        std::size_t cnt = 0;
        for (typename std::unordered_map<int, std::vector<int> >::const_iterator it = nd.S.begin();
            it != nd.S.end(); ++it) {
            cnt += it->second.size();
        }
        return cnt == parts.size();
        };

    // 初始最优解和上界
    Node best_solution(initial_S, 0.0, "Best", 0.0, 0.0, 0);
    double current_UB = UB_initial; // 使用传入的初始UB

    // 如果传入的 UB 是无限大，或者需要一个更好的初始值，则运行启发式
    //if (current_UB == std::numeric_limits<double>::infinity()) {
    //    Node heuristic_best_node;
    //    // 运行启发式算法获取一个初始的UB
    //    // 这里假设 run_initial_heuristic_solution 返回一个 Node 和其对应的目标值
    //    std::tie(heuristic_best_node, current_UB) = run_initial_heuristic_solution(
    //        parts, D, initial_infeasible, ST, VT, UT, h, v, part_areas, machine_area
    //    );
    //    best_solution = heuristic_best_node; // 初始最佳解设置为启发式找到的
    //}

    // 根节点
    Node root({}, 0.0, "Root", 0.0, 0.0, 0);
    root.LB = compute_unassigned_lower_bound(root, parts, D, ST, VT, UT, h, v);

    // 使用 deque 作为栈，但我们将手动管理其元素，以实现启发式深度优先
    // 另一种更常见且性能通常更好的方式是递归DFS，并对子节点进行排序后递归调用
    // 但如果避免递归深度限制，迭代式是好的选择
    std::deque<Node> stack;
    stack.push_back(root);

    auto t0 = std::chrono::steady_clock::now();

    // 记录初始 UB 和 Root 的 LB
    if (current_UB > 0 && current_UB < std::numeric_limits<double>::infinity()) {
        stats.UB_updates.emplace_back(0.0, current_UB);
    }
    stats.LB_convergence.emplace_back(0.0, root.LB);


    while (!stack.empty()) {
        auto t1 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();
        if (time_limit_seconds > 0.0 && elapsed > time_limit_seconds) {
            break; // 超时退出
        }

        // --- 启发式节点选择：从栈中选择下界最小的节点进行探索 ---
        // 为了实现"深度优先"的同时带有"启发式"，通常的做法是：
        // 1. 每次循环时，找到栈中LB最小的那个节点，并将其作为当前节点。
        // 2. 将其从栈中移除。
        // 3. 展开其子节点。
        // 4. 将子节点根据LB排序后，再推入栈中。
        // 注意：这种方式如果栈中节点很多，寻找最小LB的开销会比较大。
        // 如果追求严格的最佳优先，会用 std::priority_queue。
        // 这里为了“深度优先”且不完全变成BFS，我们可以选择最近生成的，且LB最低的。
        // 但最简单有效的迭代式启发式DFS是：直接从栈中 pop，但入栈前排序。
        // 鉴于你的原始代码是从 `stack.back()` 取，我们保持这个行为，但修改子节点入栈顺序。

        // 以下是原始代码的 LB_convergence 记录逻辑，与当前节点选择方式无关，保留。
        if (!stack.empty()) {
            double timestamp = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            double min_LB_in_stack = std::numeric_limits<double>::infinity();
            for (const Node& nd : stack) {
                if (nd.LB < min_LB_in_stack) {
                    min_LB_in_stack = nd.LB;
                }
            }

            // 判断是否超过 UB (这里是栈中所有节点的最小LB与UB的比较)
            if (min_LB_in_stack >= current_UB) {
                min_LB_in_stack = current_UB; // 如果最小LB已经高于UB，则收敛到UB
            }
            static constexpr double epsilon = 1e-10;

            if (min_LB_in_stack >= 0.0 && min_LB_in_stack < std::numeric_limits<double>::infinity()) {
                if (stats.LB_convergence.empty() || std::abs(min_LB_in_stack - stats.LB_convergence.back().second) > epsilon) {
                    stats.LB_convergence.emplace_back(timestamp, min_LB_in_stack);
                }
            }
        }

        Node cur = stack.back(); // 取出栈顶节点
        stack.pop_back();        // 弹出栈顶节点
        ++stats.total_nodes;

        //// 确保深度统计数组足够大
        //if (cur.depth >= stats.pruned_nodes_per_depth.size()) {
        //    stats.pruned_nodes_per_depth.resize(cur.depth + 100);
        //}

        // 剪枝1：不可行批次
        bool bad = false;
        for (typename std::unordered_map<int, std::vector<int> >::const_iterator it = cur.S.begin();
            it != cur.S.end() && !bad; ++it) {

            // 为了使用 std::includes，it->second 和 rule 都需要是排序的
            std::vector<int> sorted_batch_parts = it->second;
            std::sort(sorted_batch_parts.begin(), sorted_batch_parts.end());

            for (std::size_t ui = 0; ui < initial_infeasible.size(); ++ui) {
                // initial_infeasible[ui] 是 std::set，其迭代器提供有序访问
                if (std::includes(
                    sorted_batch_parts.begin(),
                    sorted_batch_parts.end(),
                    initial_infeasible[ui].begin(), // set的迭代器自然有序
                    initial_infeasible[ui].end()))
                {
                    bad = true;
                    break;
                }
            }
        }
        if (bad) {
            ++stats.U_pruned_nodes;
            ++stats.pruned_nodes_per_depth[cur.depth];
            continue; // 剪枝，进入下一个循环迭代
        }

        // 剪枝2：下界剪枝
        // 注意：这里的 current_UB 是全局最优上界
        if (cur.LB >= current_UB) {
            ++stats.LB_pruned_nodes;
            ++stats.pruned_nodes_per_depth[cur.depth];
            continue; // 剪枝，进入下一个循环迭代
        }

        // 叶子节点 (所有零件已分配)
        if (all_assigned(cur)) {
            ++stats.leaf_nodes;
            // 更新最优解和UB
            // 叶子节点的下界通常等于其真实成本（total_tardiness）
            if (cur.total_tardiness < current_UB) { // 应该用实际成本 total_tardiness 比较
                current_UB = cur.total_tardiness;
                best_solution = cur;
                ++stats.updated_solutions;
                double timestamp = std::chrono::duration<double>(t1 - t0).count();
                stats.UB_updates.emplace_back(timestamp, current_UB);
            }
            continue; // 剪枝，进入下一个循环迭代
        }

        // 展开子节点
        auto [kids, pruned] = generate_children(cur, parts, machine_area, part_areas);
        stats.generated_nodes += kids.size();
        stats.area_pruned_nodes += pruned;

        // 对所有子节点进行计算和评估
        for (auto& child : kids) {
            // 设置子节点深度
            child.depth = cur.depth + 1;

            // 1. 计算新增批次完成时间
            auto comp_times = compute_completion_times(child, ST, VT, UT, h, v);
            if (!comp_times.empty()) {
                child.completion_time = comp_times.begin()->second; // 或其他合适的逻辑
            }

            // 2. 基于当前批次，更新累计已分配零件的延迟
            child.total_tardiness = compute_assigned_tardiness(child, D);

            // 3. 基于更新后的 completion_time 和 total_tardiness 估算下界
            child.LB = compute_unassigned_lower_bound(child, parts, D, ST, VT, UT, h, v);
        }

        // ====== 关键修改：对子节点按LB排序，并按顺序推入栈中 ======
        // 目的：让 stack.back() 总是当前层级中（或最近生成的）下界最小的节点，
        // 从而优先探索最有希望的分支。
        std::sort(kids.begin(), kids.end(), [](const Node& a, const Node& b) {
            return a.LB < b.LB; // 升序排序，LB小的在前
            });

        // 将排序后的子节点倒序推入栈中，使得 stack.back() 始终是 LB 最小的那个
        // 这样下次循环取出 stack.back() 时，就是当前最优的子节点
        for (auto it = kids.rbegin(); it != kids.rend(); ++it) {
            if (it->LB < current_UB) { // 只将有可能改善当前最优解的子节点推入栈
                stack.push_back(*it);
            }
            else {
                // 如果子节点的下界已经不优于当前UB，则可以直接剪枝，不入栈
                ++stats.LB_pruned_nodes;
                ++stats.pruned_nodes_per_depth[it->depth];
            }
        }
    }

    return std::make_pair(best_solution, stats);
}



