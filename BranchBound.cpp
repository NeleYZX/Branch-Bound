#include "BranchBound.h"
#include "DataRecord.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <functional>
#include <iomanip>
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
    : LB(0.0), completion_time(0.0), total_tardiness(0.0),
      closed_batches_completion_time(0.0), closed_batches_total_tardiness(0.0),
      name("N"), depth(0),
      generation_type(0), added_part(-1) {
}

Node::Node(const std::unordered_map<int, std::vector<int>>& S_,
    double LB_,
    const std::string& name_,
    double completion_time_,
    double total_tardiness_,
    int depth_,
    int generation_type_,
    int added_part_)
    : S(S_), LB(LB_), completion_time(completion_time_), total_tardiness(total_tardiness_),
    closed_batches_completion_time(0.0), closed_batches_total_tardiness(0.0),
    name(name_), depth(depth_),
    generation_type(generation_type_), added_part(added_part_) {
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

// Safe-Merge 的高频内部版本：复用 generate_children 已经解析好的父节点信息，
// 避免对同一父节点的每个 Type-I 候选反复构造 assigned 集合和统计开放批次。
static bool is_type1_safe_merge_dominated_precomputed(
    const Node& parent,
    int candidate_part,
    const std::vector<int>& parts,
    const std::unordered_set<int>& assigned,
    const std::vector<int>& open_batch,
    double open_batch_area,
    double open_batch_volume,
    double open_batch_height,
    int max_open_part,
    double machine_area,
    const std::vector<double>& part_areas,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& h,
    const std::vector<double>& v,
    const std::vector<double>& D,
    double epsilon
) {
    if (candidate_part < 0 || open_batch.empty()) return false;

    double merged_area = open_batch_area + part_areas[candidate_part];
    double merged_volume = open_batch_volume + v[candidate_part];
    double merged_height = std::max(open_batch_height, h[candidate_part]);

    // 必须存在对应的合法 Type-II 合并分支，才能用它支配 Type-I。
    if (candidate_part <= max_open_part || merged_area > machine_area + epsilon) {
        return false;
    }

    // 批内编号递增规则保证：候选批次以后只能再接收编号大于 candidate_part 的零件。
    for (int p : parts) {
        if (p > candidate_part && assigned.find(p) == assigned.end()) {
            merged_area += part_areas[p];
            merged_volume += v[p];
            merged_height = std::max(merged_height, h[p]);
        }
    }
    if (merged_area > machine_area + epsilon) return false;

    const double merged_completion =
        parent.closed_batches_completion_time + ST[0] + VT[0] * merged_volume
        + UT[0] * merged_height;
    for (int p : open_batch) {
        if (D[p] + epsilon < merged_completion) return false;
    }
    return true;
}

//=======================子节点生成（Type I & Type II）========================
// 本函数实现 Azizoglu & Webster (2000) 的增量式分支策略：
// 在已固定若干批次（B_1, ..., B_r，按时间先后排列）的部分调度上，
// 通过两类“添加动作”逐个把未排零件接入调度，从而枚举出所有可行调度：
//   - Type I 添加：把一个未排零件放入一个【全新批次】 B_{r+1}（等价于“封口”当前批次 B_r）；
//   - Type II 添加：把一个未排零件放入【当前最后一个批次】 B_r。
//
// 与文献的对应关系（已针对本文 AM / 总延误 模型做了模型相关的取舍，详见论文方法节）：
//   * Type II 仅保留两条与目标函数无关、纯结构性的过滤条件：
//       (v)  加入后不得超出平台容量 L×W（容量可行性）；
//       (vi) 仅当新零件下标大于当前批次内所有零件下标时才允许加入（对称性消除，避免重复枚举同一集合）。
//   * 文献中针对 Type I 的支配过滤条件 (i)-(iv) 基于“批加工时间 = 批内最大 p_j”及
//     “按 p/w 升序排批次”等性质，仅对 总加权完成时间 目标成立；
//     在本文 P_b = S + V·Σv_j + U·max h_j 的 M-batch + 总延误 模型下这些性质不成立，
//     故此处【不施加】(i)-(iv)。本文新增的 Safe-Merge 与相邻批次交换规则
//     在完整 Node 构造之前过滤 Type-I 候选；其余状态再由全局状态支配规则处理。
ChildGenerationResult generate_children(
    const Node& node,
    const std::vector<int>& parts,
    double machine_area,
    const std::vector<double>& part_areas,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& h,
    const std::vector<double>& v,
    const std::vector<double>& D,
    double dominance_epsilon
) {
    std::unordered_set<int> assigned;
    int max_batch_id = -1;                 // 当前最后一个批次 B_r 的下标（根节点为 -1）
    double current_batch_area = 0.0;       // 当前批次 B_r 已占用的投影面积 a(B_r)
    double current_batch_volume = 0.0;
    double current_batch_height = 0.0;
    int max_pid_in_current_batch = -1;     // 当前批次 B_r 内零件的最大下标（用于条件 vi）

    // 1. 解析父节点状态：找出已排零件集合、定位当前最后批次 B_r 及其属性
    for (const auto& kv : node.S) {
        max_batch_id = std::max(max_batch_id, kv.first);
        for (int pid : kv.second) {
            assigned.insert(pid);
        }
    }
    if (max_batch_id >= 0) {
        for (int pid : node.S.at(max_batch_id)) {
            current_batch_area += part_areas[pid];
            current_batch_volume += v[pid];
            current_batch_height = std::max(current_batch_height, h[pid]);
            max_pid_in_current_batch = std::max(max_pid_in_current_batch, pid);
        }
    }

    // 2. 按零件原始下标顺序筛选未排零件（固定枚举顺序，配合条件 vi 消除对称）
    std::vector<int> unassigned;
    for (int p : parts) {
        if (assigned.find(p) == assigned.end()) {
            unassigned.push_back(p);
        }
    }
    if (unassigned.empty()) return { {}, 0 };

    std::vector<Node> children;
    children.reserve(unassigned.size() * 2);
    long long pruned_count = 0;
    long long safe_merge_pruned_count = 0;
    long long interchange_pruned_count = 0;
    long long exchangeable_type1_pruned_count = 0;
    long long exchangeable_type2_pruned_count = 0;
    int child_index = 0;

    // 相邻交换只依赖父节点。若成立，所有 Type-I 候选都在 Node 构造前直接删除。
    const bool interchange_dominates_type1 =
        is_type1_adjacent_interchange_dominated(
            node, ST, VT, UT, h, v, D, dominance_epsilon);

    // ============================================================
    // 第一步（Phase 1）：生成 Type I 子节点 —— 为每个未排零件开一个新批次
    // 文献：根节点处即由此步生成 n 个“首批次只含单个零件”的子节点；
    //       一般节点处则对应“封口当前批次、另起新批次”。本模型不施加 (i)-(iv)。
    // ============================================================
    if (interchange_dominates_type1) {
        interchange_pruned_count = static_cast<long long>(unassigned.size());
    }
    else {
        const std::vector<int>* open_batch =
            (max_batch_id >= 0) ? &node.S.at(max_batch_id) : nullptr;
        for (int pid : unassigned) {
            // 同类型可交换零件支配：若更小编号、交期不晚且批处理属性不差的
            // 未排零件可以作为 Type-I 候选，则选择 pid 新开批次的分支可删除。
            if (is_same_type_exchangeable_part_dominated(
                    pid, unassigned, false, max_pid_in_current_batch,
                    part_areas, h, v, D)) {
                ++exchangeable_type1_pruned_count;
                continue;
            }

            // Safe-Merge 在复制 node.S 和构造完整 Node 之前执行。
            if (open_batch != nullptr && is_type1_safe_merge_dominated_precomputed(
                    node, pid, parts, assigned, *open_batch,
                    current_batch_area, current_batch_volume, current_batch_height,
                    max_pid_in_current_batch, machine_area, part_areas,
                    ST, VT, UT, h, v, D, dominance_epsilon)) {
                ++safe_merge_pruned_count;
                continue;
            }

            auto S_type1 = node.S;
            S_type1[max_batch_id + 1] = { pid };   // 开新批次 B_{r+1}，仅含 pid
            std::string name1 = node.name + "_T1_" + std::to_string(child_index++);

            children.emplace_back(
                std::move(S_type1),
                0.0,                 // LB 由调用方稍后计算
                name1,
                0.0,                 // completion_time 由调用方稍后计算
                0.0,                 // total_tardiness 同上
                node.depth + 1,
                1,
                pid
            );
        }
    }

    // ============================================================
    // 第二步（Phase 2）：生成 Type II 子节点 —— 把未排零件并入当前批次 B_r
    // 仅保留满足 (v) 容量可行 与 (vi) 下标递增（对称性消除）的子节点。
    // 根节点（max_batch_id < 0）没有“当前批次”，故此步跳过。
    // ============================================================
    if (max_batch_id >= 0) {
        for (int pid : unassigned) {
            // 条件 (v)：容量可行性 —— a(B_r) + area(pid) ≤ L×W
            bool area_ok = (current_batch_area + part_areas[pid] <= machine_area);
            // 条件 (vi)：对称性 —— 仅允许把下标更大的零件并入当前批次
            bool index_ok = (pid > max_pid_in_current_batch);

            if (area_ok && index_ok) {
                // Type-II/Type-II 可交换零件支配。支配零件还必须满足当前
                // 开放批次的编号递增条件，确保被保留的 Type-II 兄弟真实存在。
                if (is_same_type_exchangeable_part_dominated(
                        pid, unassigned, true, max_pid_in_current_batch,
                        part_areas, h, v, D)) {
                    ++exchangeable_type2_pruned_count;
                    continue;
                }

                auto S_type2 = node.S;
                S_type2[max_batch_id].push_back(pid);   // 并入当前批次 B_r
                std::string name2 = node.name + "_T2_" + std::to_string(child_index++);

                children.emplace_back(
                    std::move(S_type2),
                    0.0,
                    name2,
                    0.0,
                    0.0,
                    node.depth + 1,
                    2,
                    pid
                );
            }
            else if (!area_ok) {
                ++pruned_count;   // 记录因容量约束 (v) 被剪掉的 Type II 候选数量
            }
        }
    }

    return {
        std::move(children),
        pruned_count,
        safe_merge_pruned_count,
        interchange_pruned_count,
        exchangeable_type1_pruned_count,
        exchangeable_type2_pruned_count
    };
}

// [删除]：移除了原有的 compute_completion_times 函数
// [删除]：移除了原有的 compute_assigned_tardiness 函数

//====================== [新增] 更新节点的全局时间与延迟状态 =============================
void update_node_metrics(
    Node& node,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& h,
    const std::vector<double>& v,
    const std::vector<double>& D
) {
    double current_completion_time = 0.0;
    double current_total_tardiness = 0.0;
    double closed_batches_completion_time = 0.0;
    double closed_batches_total_tardiness = 0.0;

    int max_batch_id = -1;
    for (const auto& kv : node.S) {
        max_batch_id = std::max(max_batch_id, kv.first);
    }

    // 按批次 ID 顺序严格累加加工时间，确保时序正确
    for (int i = 0; i <= max_batch_id; ++i) {
        if (node.S.find(i) == node.S.end()) continue;

        const auto& batch = node.S.at(i);
        double vol = 0.0, mh = 0.0;

        // 提取当前批次的总体积和最大高度
        for (int pid : batch) {
            vol += v[pid];
            mh = std::max(mh, h[pid]);
        }

        // 计算当前批次的加工时间 (PT = 准备时间 + 体积相关时间 + 高度相关时间)
        double PT = ST[0] + VT[0] * vol + UT[0] * mh;
        current_completion_time += PT;

        // 累加当前批次所有零件的延迟
        for (int pid : batch) {
            current_total_tardiness += std::max(0.0, current_completion_time - D[pid]);
        }

        // 除最后一个（开放）批次外，其余批次构成已经封闭的调度前缀。
        if (i < max_batch_id) {
            closed_batches_completion_time = current_completion_time;
            closed_batches_total_tardiness = current_total_tardiness;
        }
    }

    // 固化节点的最终状态
    node.completion_time = current_completion_time;
    node.total_tardiness = current_total_tardiness;
    node.closed_batches_completion_time = closed_batches_completion_time;
    node.closed_batches_total_tardiness = closed_batches_total_tardiness;
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
    const std::vector<double>& v,
    double machine_area,
    const std::vector<double>& part_areas
) {
    std::unordered_set<int> assigned;
    int max_batch_id = -1;
    for (const auto& kv : node.S) {
        max_batch_id = std::max(max_batch_id, kv.first);
        for (int pid : kv.second) assigned.insert(pid);
    }

    const double closed_batches_completion_time = node.closed_batches_completion_time;
    const double closed_batches_total_tardiness = node.closed_batches_total_tardiness;
    double open_batch_completion_time = closed_batches_completion_time;
    double open_batch_tardiness = 0.0;
    double open_batch_volume = 0.0;
    double open_batch_height = 0.0;
    int max_part_id_in_open_batch = -1;
    // [新增：面积约束] 记录最后一个批次当前已经占用的面积。
    double open_batch_area = 0.0;

    if (max_batch_id >= 0) {
        const auto& last_batch = node.S.at(max_batch_id);
        for (int pid : last_batch) {
            open_batch_volume += v[pid];
            open_batch_height = std::max(open_batch_height, h[pid]);
            max_part_id_in_open_batch = std::max(max_part_id_in_open_batch, pid);
            // [新增：面积约束] 累加最后一个批次内已分配零件的面积。
            open_batch_area += part_areas[pid];
        }

        const double open_batch_processing_time =
            ST[0] + VT[0] * open_batch_volume + UT[0] * open_batch_height;
        open_batch_completion_time = closed_batches_completion_time + open_batch_processing_time;
        for (int pid : last_batch) {
            open_batch_tardiness += std::max(0.0, open_batch_completion_time - D[pid]);
        }
    }

    double unassigned_tardiness = 0.0;

    for (int p : parts) {
        if (assigned.find(p) == assigned.end()) {
            double single_processing_time = ST[0] + VT[0] * v[p] + UT[0] * h[p];
            double new_batch_completion_time = open_batch_completion_time + single_processing_time;
            double optimistic_completion_time = new_batch_completion_time;

            // [新增：面积约束] 未分配零件只有在加入后不超过机器面积时，
            // 才能使用“放入最后一个批次”的完成时间进行下界估计。
            if (max_batch_id >= 0 && p > max_part_id_in_open_batch &&
                open_batch_area + part_areas[p] <= machine_area) {
                double joined_volume = open_batch_volume + v[p];
                double joined_height = std::max(open_batch_height, h[p]);
                double joined_processing_time = ST[0] + VT[0] * joined_volume + UT[0] * joined_height;
                double joined_completion_time = closed_batches_completion_time + joined_processing_time;
                optimistic_completion_time = std::min(optimistic_completion_time, joined_completion_time);
            }

            unassigned_tardiness += std::max(0.0, optimistic_completion_time - D[p]);
        }
    }

    return closed_batches_total_tardiness + open_batch_tardiness + unassigned_tardiness;
}

// [修改：LBpos] Type I 和 Type II 子节点统一使用该下界；估计时将最后一个批次视为未分配。
double compute_positional_lower_bound(
    const Node& node,
    const std::vector<int>& parts,
    const std::vector<double>& D,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& L,
    const std::vector<double>& W,
    const std::vector<double>& l,
    const std::vector<double>& w,
    const std::vector<double>& h,
    const std::vector<double>& v
) {
    int max_batch_id = -1;
    std::size_t assigned_count = 0;
    for (const auto& kv : node.S) {
        max_batch_id = std::max(max_batch_id, kv.first);
        assigned_count += kv.second.size();
    }

    // [新增：LBpos] 完整方案必须返回真实目标值，避免叶子节点用松弛下界更新 UB。
    if (assigned_count == parts.size()) {
        return node.total_tardiness;
    }

    // [新增：LBpos] 只把最后批次之前的批次作为已经固定的调度前缀；
    // 最后一个批次中的零件不放入 fixed_assigned，因而会与真正未分配零件一起参与 LBpos。
    std::unordered_set<int> fixed_assigned;
    double fixed_completion_time = 0.0;
    double fixed_total_tardiness = 0.0;
    for (int batch_id = 0; batch_id < max_batch_id; ++batch_id) {
        const auto batch_it = node.S.find(batch_id);
        if (batch_it == node.S.end()) continue;

        double batch_volume = 0.0;
        double batch_height = 0.0;
        for (int pid : batch_it->second) {
            fixed_assigned.insert(pid);
            batch_volume += v[pid];
            batch_height = std::max(batch_height, h[pid]);
        }

        fixed_completion_time += ST[0] + VT[0] * batch_volume + UT[0] * batch_height;
        for (int pid : batch_it->second) {
            fixed_total_tardiness += std::max(0.0, fixed_completion_time - D[pid]);
        }
    }

    // 根节点没有最后批次，此时所有零件自然都属于待估计集合。
    if (max_batch_id < 0) {
        fixed_assigned.clear();
    }

    std::vector<double> areas;
    std::vector<double> heights;
    std::vector<double> volumes;
    std::vector<double> due_dates;

    // [修改：LBpos] 待估计集合 = 真正未分配零件 + 当前最后批次中的零件，
    // 因此这里只排除已经固定在最后批次之前的零件。
    for (int p : parts) {
        if (fixed_assigned.find(p) == fixed_assigned.end()) {
            areas.push_back(l[p] * w[p]);
            heights.push_back(h[p]);
            volumes.push_back(v[p]);
            due_dates.push_back(D[p]);
        }
    }

    const std::size_t m = due_dates.size();
    if (m == 0) {
        return fixed_total_tardiness;
    }

    std::sort(areas.begin(), areas.end());
    std::sort(heights.begin(), heights.end());
    std::sort(volumes.begin(), volumes.end());
    std::sort(due_dates.begin(), due_dates.end());

    const double machine_area = L[0] * W[0];
    const double eps = 1e-9;
    double area_prefix = 0.0;
    double volume_prefix = 0.0;
    double positional_tardiness = 0.0;

    for (std::size_t k = 1; k <= m; ++k) {
        area_prefix += areas[k - 1];
        volume_prefix += volumes[k - 1];

        int beta = static_cast<int>(std::ceil((area_prefix - eps) / machine_area));
        beta = std::max(1, std::min(beta, static_cast<int>(k)));

        double height_bound = 0.0;
        for (int r = 0; r < beta - 1; ++r) {
            height_bound += heights[r];
        }
        height_bound += heights[k - 1];

        const double completion_lb =
            fixed_completion_time +
            beta * ST[0] +
            UT[0] * height_bound +
            VT[0] * volume_prefix;

        positional_tardiness += std::max(0.0, completion_lb - due_dates[k - 1]);
    }

    return fixed_total_tardiness + positional_tardiness;
}

//=======================PDF 版本的位置松弛下界====================
// 保留当前最后一个批次作为开放批次，只对真正未分配零件 R(c) 排序。
// 返回完整节点下界：base(c) + LBpos(c)。
double compute_positional_lower_bound_relaxation(
    const Node& node,
    const std::vector<int>& parts,
    const std::vector<double>& D,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& L,
    const std::vector<double>& W,
    const std::vector<double>& l,
    const std::vector<double>& w,
    const std::vector<double>& h,
    const std::vector<double>& v
) {
    const double machine_area = L[0] * W[0];
    const double eps = 1e-9;

    std::unordered_set<int> assigned;
    int open_batch_id = -1;
    for (const auto& kv : node.S) {
        open_batch_id = std::max(open_batch_id, kv.first);
        for (int part_id : kv.second) assigned.insert(part_id);
    }

    const bool has_open_batch = open_batch_id >= 0;
    double open_batch_area = 0.0;
    double open_batch_volume = 0.0;
    double open_batch_height = 0.0;
    double base = node.closed_batches_total_tardiness;

    if (has_open_batch) {
        const auto& open_batch = node.S.at(open_batch_id);
        for (int part_id : open_batch) {
            open_batch_area += l[part_id] * w[part_id];
            open_batch_volume += v[part_id];
            open_batch_height = std::max(open_batch_height, h[part_id]);
        }

        const double open_batch_completion_time =
            node.closed_batches_completion_time + ST[0]
            + VT[0] * open_batch_volume
            + UT[0] * open_batch_height;
        for (int part_id : open_batch) {
            base += std::max(0.0, open_batch_completion_time - D[part_id]);
        }
    }

    // 只收集真正未分配零件 R(c)，不释放当前开放批次。
    std::vector<double> remaining_areas;
    std::vector<double> remaining_volumes;
    std::vector<double> remaining_heights;
    std::vector<double> remaining_due_dates;
    remaining_areas.reserve(parts.size());
    remaining_volumes.reserve(parts.size());
    remaining_heights.reserve(parts.size());
    remaining_due_dates.reserve(parts.size());

    for (int part_id : parts) {
        if (assigned.find(part_id) == assigned.end()) {
            remaining_areas.push_back(l[part_id] * w[part_id]);
            remaining_volumes.push_back(v[part_id]);
            remaining_heights.push_back(h[part_id]);
            remaining_due_dates.push_back(D[part_id]);
        }
    }

    const std::size_t remaining_count = remaining_due_dates.size();
    if (remaining_count == 0) return base;

    std::sort(remaining_areas.begin(), remaining_areas.end());
    std::sort(remaining_volumes.begin(), remaining_volumes.end());
    std::sort(remaining_heights.begin(), remaining_heights.end());
    std::sort(remaining_due_dates.begin(), remaining_due_dates.end());

    std::vector<double> height_prefix(remaining_count + 1, 0.0);
    for (std::size_t i = 0; i < remaining_count; ++i) {
        height_prefix[i + 1] = height_prefix[i] + remaining_heights[i];
    }

    // r0 是严格大于开放批次高度的第一个剩余高度位置。
    std::size_t r0 = 0;
    if (has_open_batch) {
        while (r0 < remaining_count &&
               remaining_heights[r0] <= open_batch_height + eps) {
            ++r0;
        }
    }

    double area_prefix = 0.0;
    double volume_prefix = 0.0;
    double positional_tardiness = 0.0;

    for (std::size_t k = 1; k <= remaining_count; ++k) {
        area_prefix += remaining_areas[k - 1];
        volume_prefix += remaining_volumes[k - 1];

        long long beta_area = static_cast<long long>(
            std::ceil((open_batch_area + area_prefix) / machine_area - 1e-6));
        beta_area = std::max(1LL, beta_area);

        const long long batch_cap =
            static_cast<long long>(k) + (has_open_batch ? 1LL : 0LL);
        const long long beta = std::min(batch_cap, beta_area);

        const double largest_height = has_open_batch
            ? std::max(remaining_heights[k - 1], open_batch_height)
            : remaining_heights[k - 1];

        const long long additional_height_count = beta - 1;
        double smallest_height_sum = 0.0;
        if (additional_height_count > 0) {
            if (!has_open_batch) {
                smallest_height_sum =
                    height_prefix[static_cast<std::size_t>(additional_height_count)];
            }
            else {
                const std::size_t no_greater_than_open = std::min(k, r0);
                if (static_cast<std::size_t>(additional_height_count) <=
                    no_greater_than_open) {
                    smallest_height_sum = height_prefix[
                        static_cast<std::size_t>(additional_height_count)];
                }
                else {
                    smallest_height_sum = height_prefix[
                        static_cast<std::size_t>(additional_height_count - 1)]
                        + open_batch_height;
                }
            }
        }

        const double height_bound = largest_height + smallest_height_sum;
        const double completion_lower_bound =
            node.closed_batches_completion_time
            + static_cast<double>(beta) * ST[0]
            + VT[0] * (open_batch_volume + volume_prefix)
            + UT[0] * height_bound;

        positional_tardiness += std::max(
            0.0, completion_lower_bound - remaining_due_dates[k - 1]);
    }

    return base + positional_tardiness;
}

//=======================LBpar 与 LBpos 的组合入口====================
double compute_LBpar_LBpos(
    const Node& node,
    const std::vector<int>& parts,
    const std::vector<double>& D,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& L,
    const std::vector<double>& W,
    const std::vector<double>& l,
    const std::vector<double>& w,
    const std::vector<double>& h,
    const std::vector<double>& v,
    double UB,
    bool use_pos,
    double gamma,
    double positional_bound_fraction
) {
    const double machine_area = L[0] * W[0];
    std::vector<double> part_areas(parts.size(), 0.0);
    for (int part_id : parts) {
        part_areas[part_id] = l[part_id] * w[part_id];
    }

    // LBpar 始终计算，也仍可通过 compute_unassigned_lower_bound 单独调用。
    const double lb_par = compute_unassigned_lower_bound(
        node, parts, D, ST, VT, UT, h, v, machine_area, part_areas);

    std::size_t assigned_count = 0;
    for (const auto& kv : node.S) assigned_count += kv.second.size();
    const std::size_t remaining_count = parts.size() - assigned_count;

    const bool shallow_enough =
        remaining_count >= positional_bound_fraction * parts.size();
    const bool strong_enough =
        gamma <= 0.0 || (std::isfinite(UB) && lb_par >= gamma * UB);

    if (!use_pos || !shallow_enough || !strong_enough) {
        return lb_par;
    }

    // 不改变 LBpos 的独立实现；这里只调用它并与完整 LBpar 取最大值。
    const double lb_pos = compute_positional_lower_bound_relaxation(
        node, parts, D, ST, VT, UT, L, W, l, w, h, v);
    return std::max(lb_par, lb_pos);
}

//不使用DP算法的串行计算
double compute_unassigned_lower_bound3(
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

    // 初始化未分配部分的延迟估计
    double unassigned_tardiness = 0.0;
    double vol_accumulated = 0.0;
    for (int p : parts) {
        if (assigned.find(p) == assigned.end()) {
            vol_accumulated += v[p];

            ////    // 计算该零件的加工时间
            double processing_time = ST[0] + VT[0] * vol_accumulated + UT[0] * min_height;
            // 对每个未分配零件，估算加工时间并独立批次处理
            double c = node.completion_time + processing_time;  // 假设从当前时间并行开始
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
    const std::vector<double>& v,
    const std::vector<double>& individual_part_processing_times
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
    double dp_initial_time = node.completion_time + ST[0] + UT[0] * min_height;
    std::vector<int> optimal_sequence_result;
    double min_tardiness = minimize_total_tardiness(dp_jobs_for_solver, dp_initial_time, optimal_sequence_result);


    //// 初始化延迟估计
    double unassigned_tardiness = 0.0;
    double completion_time_future = node.completion_time;
    double vol_accumulated = 0.0; // 当前已处理部分体积


    //// 假设从当前位置开始串行处理未分配的零件
    for (int p : optimal_sequence_result) {
        ////    // 累加当前零件的体积
        vol_accumulated += v[p];

        ////    // 计算该零件的加工时间
        double processing_time = ST[0] + VT[0] * vol_accumulated + UT[0] * min_height;
        double final_processing_time = std::max(processing_time, individual_part_processing_times[p]);

        double completion_time = completion_time_future + final_processing_time;
        unassigned_tardiness += std::max(0.0, completion_time - D[p]);
    }

    // 返回当前延迟 + 估计的未分配延迟下界
    return node.total_tardiness + min_tardiness;
}

static void update_type1_metrics_incrementally(
    const Node& parent,
    Node& child,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& h,
    const std::vector<double>& v,
    const std::vector<double>& D
) {
    const int added_part = child.added_part;
    if (added_part < 0) {
        update_node_metrics(child, ST, VT, UT, h, v, D);
        return;
    }

    const double processing_time = ST[0] + VT[0] * v[added_part] + UT[0] * h[added_part];
    child.completion_time = parent.completion_time + processing_time;
    child.total_tardiness =
        parent.total_tardiness + std::max(0.0, child.completion_time - D[added_part]);
    // Type I 封闭父节点的开放批次，再以 added_part 创建新的开放批次。
    child.closed_batches_completion_time = parent.completion_time;
    child.closed_batches_total_tardiness = parent.total_tardiness;
}

//========================Type-I 局部支配规则 1：Safe-Merge========================
// 注意：本判定依赖 generate_children 中的批内编号递增规则：candidate_part 加入后，
// 将来还能并入该批次的零件只能来自尚未分配且编号大于 candidate_part 的零件。
bool is_type1_safe_merge_dominated(
    const Node& parent,
    int candidate_part,
    const std::vector<int>& parts,
    double machine_area,
    const std::vector<double>& part_areas,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& h,
    const std::vector<double>& v,
    const std::vector<double>& D,
    double epsilon
) {
    if (candidate_part < 0 || parent.S.empty()) return false;

    int open_batch_id = -1;
    std::unordered_set<int> assigned;
    for (const auto& kv : parent.S) {
        open_batch_id = std::max(open_batch_id, kv.first);
        assigned.insert(kv.second.begin(), kv.second.end());
    }
    if (open_batch_id < 0) return false;

    const std::vector<int>& open_batch = parent.S.at(open_batch_id);
    if (open_batch.empty()) return false;

    double open_batch_area = 0.0;
    double open_batch_volume = 0.0;
    double open_batch_height = 0.0;
    int max_open_part = -1;
    for (int p : open_batch) {
        open_batch_area += part_areas[p];
        open_batch_volume += v[p];
        open_batch_height = std::max(open_batch_height, h[p]);
        max_open_part = std::max(max_open_part, p);
    }
    return is_type1_safe_merge_dominated_precomputed(
        parent, candidate_part, parts, assigned, open_batch,
        open_batch_area, open_batch_volume, open_batch_height, max_open_part,
        machine_area, part_areas, ST, VT, UT, h, v, D, epsilon);
}

//================Type-I 局部支配规则 2：相邻批次交换================
bool is_type1_adjacent_interchange_dominated(
    const Node& parent,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& h,
    const std::vector<double>& v,
    const std::vector<double>& D,
    double epsilon
) {
    if (parent.S.size() < 2) return false;

    int current_batch_id = -1;
    int previous_batch_id = -1;
    for (const auto& kv : parent.S) {
        if (kv.first > current_batch_id) {
            previous_batch_id = current_batch_id;
            current_batch_id = kv.first;
        }
        else if (kv.first > previous_batch_id) {
            previous_batch_id = kv.first;
        }
    }
    if (previous_batch_id < 0) return false;

    const std::vector<int>& previous_batch = parent.S.at(previous_batch_id);
    const std::vector<int>& current_batch = parent.S.at(current_batch_id);
    if (previous_batch.empty() || current_batch.empty()) return false;

    auto batch_processing_time = [&](const std::vector<int>& batch) {
        double volume = 0.0;
        double height = 0.0;
        for (int p : batch) {
            volume += v[p];
            height = std::max(height, h[p]);
        }
        return ST[0] + VT[0] * volume + UT[0] * height;
    };

    const double previous_time = batch_processing_time(previous_batch);
    const double current_time = batch_processing_time(current_batch);
    // closed_batches_completion_time 是前一批次结束时刻，因此减去其加工时间即两批共同起点 tau。
    const double tau = parent.closed_batches_completion_time - previous_time;

    const double previous_completion_original = tau + previous_time;
    const double current_completion_original = previous_completion_original + current_time;
    const double current_completion_swapped = tau + current_time;
    const double previous_completion_swapped = current_completion_swapped + previous_time;

    double original_tardiness = 0.0;
    double swapped_tardiness = 0.0;
    for (int p : previous_batch) {
        original_tardiness += std::max(0.0, previous_completion_original - D[p]);
        swapped_tardiness += std::max(0.0, previous_completion_swapped - D[p]);
    }
    for (int p : current_batch) {
        original_tardiness += std::max(0.0, current_completion_original - D[p]);
        swapped_tardiness += std::max(0.0, current_completion_swapped - D[p]);
    }

    // 两批结束后的时间保持不变；只有交换后的局部总拖期严格更小时才剪枝。
    return swapped_tardiness + epsilon < original_tardiness;
}

//================同类型局部支配规则 3：可交换零件================
// 本规则同时用于 Type-I/Type-I 和 Type-II/Type-II 兄弟节点。
// 对候选 j，若存在 i<j 且 d_i<=d_j、a_i=a_j、h_i=h_j、v_i<=v_j，
// 则在选择 j 的任何完整方案中，i 都会位于更晚批次。交换 i、j 后：
//   1) 两批面积和最大高度不变，容量可行性保持；
//   2) 较早批次加工时间不增，中间批次只会提前，较晚批次及其后缀完成时间不变；
//   3) 交期不晚的 i 被提前，因此总拖期不增。
// 由此，选择 j 的同类型分支被选择 i 的分支支配。
bool is_same_type_exchangeable_part_dominated(
    int candidate_part,
    const std::vector<int>& unassigned_parts,
    bool is_type2_branch,
    int max_part_in_open_batch,
    const std::vector<double>& part_areas,
    const std::vector<double>& h,
    const std::vector<double>& v,
    const std::vector<double>& D
) {
    if (candidate_part < 0) return false;

    for (int preferred_part : unassigned_parts) {
        // i<j 是证明所必需的：选择 j 后，编号更小的 i 不能再进入该开放批次。
        if (preferred_part >= candidate_part) continue;

        // 对 Type II，支配零件 i 本身也必须能通过编号规则加入当前开放批次。
        // 面积相等保证：既然候选 j 容量可行，则 i 也容量可行。
        if (is_type2_branch && preferred_part <= max_part_in_open_batch) continue;

        // 面积和高度使用精确相等，避免把浮点容差误当作数学等价而破坏可行性。
        if (part_areas[preferred_part] != part_areas[candidate_part]) continue;
        if (h[preferred_part] != h[candidate_part]) continue;

        if (v[preferred_part] > v[candidate_part]) continue;
        if (D[preferred_part] > D[candidate_part]) continue;

        return true;
    }
    return false;
}

//========================强化全局节点状态支配========================
// 支配比较必须同时固定：
//   1) 已调度零件集合（等价于固定未分配集合 R）；
//   2) 当前开放批次 L 的零件集合。
// 只按“已调度集合”分组不安全，因为不同开放批次具有不同的剩余容量和
// Type-II 可加入零件集合。这里用 INT_MIN 分隔两个排序后的集合。
static std::vector<int> build_dominance_key(const Node& node) {
    int open_batch_id = -1;
    std::vector<int> scheduled_parts;
    for (const auto& kv : node.S) {
        open_batch_id = std::max(open_batch_id, kv.first);
        scheduled_parts.insert(
            scheduled_parts.end(), kv.second.begin(), kv.second.end());
    }
    std::sort(scheduled_parts.begin(), scheduled_parts.end());

    std::vector<int> open_batch_parts;
    if (open_batch_id >= 0) {
        open_batch_parts = node.S.at(open_batch_id);
        std::sort(open_batch_parts.begin(), open_batch_parts.end());
    }

    std::vector<int> key;
    key.reserve(scheduled_parts.size() + open_batch_parts.size() + 1);
    key.insert(key.end(), scheduled_parts.begin(), scheduled_parts.end());
    key.push_back(std::numeric_limits<int>::min());
    key.insert(key.end(), open_batch_parts.begin(), open_batch_parts.end());
    return key;
}

// 对同一 (R,L) 状态维护 (TTcl,tprev) 的强化支配前沿。
// 若状态 A 比 B 晚 Delta，则复制相同后续决策时，每个尚未封闭零件的
// 完成时间增加 Delta，而单个零件的拖期增量至多为 Delta。因此，若
// TTcl_A + m*max(0,tprev_A-tprev_B) <= TTcl_B，则 A 仍然支配 B。
// 原来的分量式 Pareto 支配是该判定在 tprev_A<=tprev_B 时的特例。
bool is_global_state_dominated_or_insert(
    const Node& node,
    std::unordered_map<std::vector<int>, std::vector<StateMetric>, VectorHash>& frontier,
    std::size_t total_part_count,
    double epsilon
) {
    const std::vector<int> key = build_dominance_key(node);
    auto& pareto = frontier[key];
    const double closed_time = node.closed_batches_completion_time;
    const double closed_tardiness = node.closed_batches_total_tardiness;

    std::size_t scheduled_count = 0;
    std::size_t open_batch_count = 0;
    int open_batch_id = -1;
    for (const auto& kv : node.S) {
        scheduled_count += kv.second.size();
        open_batch_id = std::max(open_batch_id, kv.first);
    }
    if (open_batch_id >= 0) {
        open_batch_count = node.S.at(open_batch_id).size();
    }

    // 未最终封闭的零件 = 当前开放批次零件 + 尚未调度零件。
    // 同一 dominance key 下该数量固定，因此所有前沿状态使用同一个 m。
    const std::size_t unscheduled_count =
        (total_part_count >= scheduled_count)
        ? total_part_count - scheduled_count
        : 0;
    const double unfinished_count = static_cast<double>(
        open_batch_count + unscheduled_count);

    for (const StateMetric& old : pareto) {
        const double old_lateness_penalty =
            unfinished_count * std::max(0.0, old.c - closed_time);
        if (old.tt + old_lateness_penalty <= closed_tardiness + epsilon) {
            return true;
        }
    }

    pareto.erase(
        std::remove_if(
            pareto.begin(), pareto.end(),
            [&](const StateMetric& old) {
                const double current_lateness_penalty =
                    unfinished_count * std::max(0.0, closed_time - old.c);
                return closed_tardiness + current_lateness_penalty
                    <= old.tt + epsilon;
            }),
        pareto.end());
    pareto.push_back({ closed_tardiness, closed_time });
    return false;
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
    double UB,
    double time_limit_seconds,
    const std::string& path
) {
    Stats stats;

    reset_dp_memo_stats();
    clear_global_dp_cache(); // 【新增】清空上一轮实验留下的哈希表

    // 强化全局支配表：key=(已调度集合, 当前开放批次)，value 保存已封闭
    // 前缀 (TTcl,tprev) 的非支配状态，并用 m*Delta 补偿较晚的前缀时间。
    std::unordered_map<std::vector<int>, std::vector<StateMetric>, VectorHash> dominance_map;


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

    // 支配表中只能保存可行节点；否则不可行状态可能错误支配可行状态。
    auto is_infeasible_node = [&](const Node& nd) -> bool {
        for (const auto& batch : nd.S) {
            for (const auto& infeasible_set : initial_infeasible) {
                if (std::includes(
                        batch.second.begin(), batch.second.end(),
                        infeasible_set.begin(), infeasible_set.end())) {
                    return true;
                }
            }
        }
        return false;
    };

    Node best(initial_S, 0.0, "Best", 0.0, 0.0, 0);
    Node root({}, 0.0, "Root", 0.0, 0.0, 0);
    update_node_metrics(root, ST, VT, UT, h, v, D);
    root.LB = compute_LBpar_LBpos(
        root, parts, D, ST, VT, UT, L, W, l, w, h, v, UB);

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
    // 支配关系来自严格数学不等式，只保留很小的浮点容差，避免放宽过度。
    static constexpr double dom_epsilon = 1e-9;


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
        if (is_infeasible_node(cur)) {
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
        ChildGenerationResult generation = generate_children(
            cur, parts, machine_area, part_areas,
            ST, VT, UT, h, v, D, dom_epsilon);
        std::vector<Node>& kids = generation.children;
        stats.generated_nodes += static_cast<long long>(kids.size());
        stats.area_pruned_nodes += generation.pruned_count;
        stats.safe_merge_pruned_nodes += generation.safe_merge_pruned_count;
        stats.interchange_pruned_nodes += generation.interchange_pruned_count;
        stats.exchangeable_type1_pruned_nodes +=
            generation.exchangeable_type1_pruned_count;
        stats.exchangeable_type2_pruned_nodes +=
            generation.exchangeable_type2_pruned_count;
        stats.pruned_nodes_per_depth[cur.depth + 1] += static_cast<int>(
            generation.safe_merge_pruned_count
            + generation.interchange_pruned_count
            + generation.exchangeable_type1_pruned_count
            + generation.exchangeable_type2_pruned_count);

        for (auto& child : kids) {
            const bool is_type1_child = (child.generation_type == 1);
            if (is_type1_child) {
                update_type1_metrics_incrementally(cur, child, ST, VT, UT, h, v, D);
            }
            else {
                update_node_metrics(child, ST, VT, UT, h, v, D);
            }

            // 先排除不可行节点，避免不可行状态进入支配表并错误支配可行节点。
            if (is_infeasible_node(child)) {
                ++stats.U_pruned_nodes;
                ++stats.pruned_nodes_per_depth[child.depth];
                continue;
            }

            // 强化全局支配：Type-I 和 Type-II 子节点共用同一规则。
            // 相同 (R,L) 下比较已封闭前缀；若旧状态更晚，则用尚未封闭
            // 零件数乘时间差，补偿其未来拖期的最坏增量。
            if (is_global_state_dominated_or_insert(
                    child, dominance_map, parts.size(), dom_epsilon)) {
                ++stats.dominance_pruned_nodes;
                ++stats.pruned_nodes_per_depth[child.depth];
                continue;
            }

            //===============================下界计算=======================
                //-------------------------------1.串行下界------
            //child.LB = compute_unassigned_lower_bound2(child, parts, D, ST, VT, UT, h, v, individual_processing_times);
             //child.LB = compute_unassigned_lower_bound3(child, parts, D, ST, VT, UT, h, v);
            // Type I / Type II 统一使用 max(LBpar, LBpos)，LBpos 受浅层门控。
            child.LB = compute_LBpar_LBpos(
                child, parts, D, ST, VT, UT, L, W, l, w, h, v, UB);


        // ----------------- [修改开始] -----------------
         // 记录第一层子节点的详细信息
            if (is_root_node) {
                // 1. 找出已分配的零件 (第一层子节点肯定只有 1 个 batch)
                std::unordered_set<int> assigned_set;
                for (const auto& kv : child.S) {
                    for (int pid : kv.second) {
                        assigned_set.insert(pid);
                    }
                }

                // 2. 计算未分配的零件
                std::vector<int> unassigned_parts_list;
                unassigned_parts_list.reserve(parts.size() - assigned_set.size());
                for (int p : parts) {
                    if (assigned_set.find(p) == assigned_set.end()) {
                        unassigned_parts_list.push_back(p);
                    }
                }

                // 3. 存入 Stats (内存中快速存储)
                stats.first_level_details.push_back({
                    child.name,
                    child.LB,
                    child.completion_time,
                    static_cast<int>(unassigned_parts_list.size()),
                    std::move(unassigned_parts_list) // 使用 move 避免拷贝
                    });

                // 保留旧的记录以便兼容（如果不需要可以删除）
                stats.first_level_node_lbs.emplace_back(child.name, child.LB);
            }
            // ----------------- [修改结束] -----------------

            if (child.LB < UB) {
                stack.push_back(std::move(child));
            }
            else {
                ++stats.LB_pruned_nodes;
                ++stats.pruned_nodes_per_depth[child.depth];
            }
        }
    }

    stats.total_V_calls = dp_memo_stats.total_V_calls;       // V() 被调用的总次数
    stats.local_memo_hits = dp_memo_stats.local_memo_hits;     // 命中本次调用的 memo（SubsetKey）的次数
    stats.global_memo_hits = dp_memo_stats.global_memo_hits;    // 命中全局 global_memo 的次数（跨调用复用）
    stats.computed_states = dp_memo_stats.computed_states;     // 真正需要计算的新状态数（没命中任何缓存）

    return std::make_pair(best, stats);
}

//========================分支过程追踪（教学/调试用）========================
namespace {

// 把节点的批次构成格式化为可读字符串，例如：[B0:{0,2} | B1:{1}]
std::string batches_to_string(const Node& node) {
    if (node.S.empty()) return "[空 / 根节点]";

    std::vector<int> ids;
    ids.reserve(node.S.size());
    for (const auto& kv : node.S) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());

    std::string s = "[";
    for (std::size_t i = 0; i < ids.size(); ++i) {
        std::vector<int> b = node.S.at(ids[i]);
        std::sort(b.begin(), b.end());
        s += "B" + std::to_string(ids[i]) + ":{";
        for (std::size_t j = 0; j < b.size(); ++j) {
            s += std::to_string(b[j]);
            if (j + 1 < b.size()) s += ",";
        }
        s += "}";
        if (i + 1 < ids.size()) s += " | ";
    }
    s += "]";
    return s;
}

// 使用子节点生成时记录的类型与新增零件，避免在 trace 中再次扫描父子差异。
std::string describe_child(const Node& parent, const Node& child) {
    int parent_max_batch = -1;
    for (const auto& kv : parent.S) {
        parent_max_batch = std::max(parent_max_batch, kv.first);
    }

    int added_batch = -1;
    if (child.generation_type == 1) {
        added_batch = parent_max_batch + 1;
    }
    else if (child.generation_type == 2) {
        added_batch = parent_max_batch;
    }
    else if (child.added_part >= 0) {
        for (const auto& kv : child.S) {
            if (std::find(kv.second.begin(), kv.second.end(), child.added_part) != kv.second.end()) {
                added_batch = kv.first;
                break;
            }
        }
    }

    if (child.added_part < 0) return "(无新增零件)";
    if (child.generation_type == 1 || added_batch > parent_max_batch) {
        return "Type I : 开新批次 B" + std::to_string(added_batch) +
               " 装入零件 " + std::to_string(child.added_part);
    }
    return "Type II: 把零件 " + std::to_string(child.added_part) +
           " 并入当前批次 B" + std::to_string(added_batch);
}

} // namespace

void trace_branch_and_bound(
    const std::vector<int>& parts,
    const std::vector<double>& D,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT,
    const std::vector<double>& L,
    const std::vector<double>& W,
    const std::vector<double>& l,
    const std::vector<double>& w,
    const std::vector<double>& h,
    const std::vector<double>& v,
    double UB,
    std::ostream& os,
    long long max_nodes
) {
    const double machine_area = L[0] * W[0];

    std::vector<double> part_areas(parts.size(), 0.0);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        part_areas[parts[i]] = l[parts[i]] * w[parts[i]];
    }

    os << "========================= 分支过程追踪 (Type I / Type II) =========================\n";
    os << "零件数 n = " << parts.size()
       << "，平台容量 L×W = " << machine_area
       << "，初始上界 UB = " << UB << "\n";
    os << "本追踪与 branch_and_cut 完全一致：最优优先(best-first)出栈 + 自适应DFS，UB 仅在叶子出栈时更新。\n";
    os << "因此打印顺序就是节点真实的【出栈顺序】，不是简单的树形遍历；用节点名(Root_T1_..._T2_...)可看出父子血缘。\n";
    os << "缩进按深度，仅为可读性。\n";
    os << "----------------------------------------------------------------------------------\n";

    // ===== 与 branch_and_cut 一致的计数器 =====
    long long total_nodes = 0;       // 出栈处理的节点数
    long long generated_nodes = 0;   // 生成的子节点数
    long long area_pruned_nodes = 0; // 因容量(v)被剪的 Type II 候选数
    long long LB_pruned_nodes = 0;   // 因下界被剪的节点数
    long long dominance_pruned_nodes = 0; // 强化全局状态支配剪枝数
    long long safe_merge_pruned_nodes = 0; // Type-I Safe-Merge 支配剪枝数
    long long interchange_pruned_nodes = 0; // Type-I 相邻批次交换支配剪枝数
    long long exchangeable_type1_pruned_nodes = 0; // Type-I/Type-I 可交换零件剪枝数
    long long exchangeable_type2_pruned_nodes = 0; // Type-II/Type-II 可交换零件剪枝数
    long long leaf_nodes = 0;        // 到达的叶子数
    long long updated_solutions = 0; // UB 被刷新的次数
    std::map<int, int> pruned_nodes_per_depth; // 每个深度被剪枝的节点数

    double best_ub = UB;
    std::unordered_map<std::vector<int>, std::vector<StateMetric>, VectorHash>
        dominance_map;
    constexpr double dominance_epsilon = 1e-9;

    auto count_assigned = [&](const Node& nd) -> std::size_t {
        std::size_t c = 0;
        for (const auto& kv : nd.S) c += kv.second.size();
        return c;
    };

    Node root({}, 0.0, "Root", 0.0, 0.0, 0);
    update_node_metrics(root, ST, VT, UT, h, v, D);
    root.LB = compute_LBpar_LBpos(
        root, parts, D, ST, VT, UT, L, W, l, w, h, v, best_ub);

    // ===== 与 branch_and_cut 完全相同的栈与出栈策略 =====
    std::deque<Node> stack;
    stack.push_back(root);

    const int max_capa = 5000;   // 与正式算法一致
    const int min_capa = 2000;   // 与正式算法一致
    bool use_best_first = true;

    while (!stack.empty()) {
        if (total_nodes >= max_nodes) {
            os << "（已达到 max_nodes=" << max_nodes << " 上限，提前停止；如需完整统计请调大该上限）\n";
            break;
        }

        // 动态选择出栈策略（与 branch_and_cut 一致）
        if (stack.size() > static_cast<std::size_t>(max_capa)) use_best_first = false;
        else if (stack.size() < static_cast<std::size_t>(min_capa)) use_best_first = true;

        Node cur;
        if (use_best_first) {
            auto best_it = std::min_element(stack.begin(), stack.end(),
                [](const Node& a, const Node& b) { return a.LB < b.LB; });
            cur = *best_it;
            stack.erase(best_it);
        }
        else {
            cur = stack.back();
            stack.pop_back();
        }

        ++total_nodes;

        const std::string indent(static_cast<std::size_t>(cur.depth) * 2, ' ');
        os << indent << "* [出栈#" << total_nodes << "] 节点[" << cur.name << "] 深度=" << cur.depth << " "
           << batches_to_string(cur)
           << "  LB=" << cur.LB
           << "  C=" << cur.completion_time
           << "  TT=" << cur.total_tardiness
           << "  | 当前UB=" << best_ub << " 栈内剩余=" << stack.size() << "\n";

        // 1) 出栈时下界剪枝（注意：入栈后 UB 可能已下降，这里会再次判断）
        if (cur.LB >= best_ub) {
            ++LB_pruned_nodes;
            ++pruned_nodes_per_depth[cur.depth];
            os << indent << "  -> 出栈时被下界剪枝 (LB=" << cur.LB << " >= UB=" << best_ub << ")\n";
            continue;
        }

        // 2) 叶子节点：完整调度。UB 仅在此处（叶子出栈）更新
        if (count_assigned(cur) == parts.size()) {
            ++leaf_nodes;
            os << indent << "  -> 叶子节点：完整调度，总延误 = " << cur.total_tardiness;
            if (cur.LB < best_ub) {
                best_ub = cur.LB;
                ++updated_solutions;
                os << "  (刷新 UB -> " << best_ub << ")";
            }
            os << "\n";
            continue;
        }

        // 3) 展开子节点（与 branch_and_cut 调用同一套 generate_children / 下界）
        ChildGenerationResult res = generate_children(
            cur, parts, machine_area, part_areas,
            ST, VT, UT, h, v, D, dominance_epsilon);
        generated_nodes += static_cast<long long>(res.children.size());
        area_pruned_nodes += res.pruned_count;
        safe_merge_pruned_nodes += res.safe_merge_pruned_count;
        interchange_pruned_nodes += res.interchange_pruned_count;
        exchangeable_type1_pruned_nodes += res.exchangeable_type1_pruned_count;
        exchangeable_type2_pruned_nodes += res.exchangeable_type2_pruned_count;
        pruned_nodes_per_depth[cur.depth + 1] += static_cast<int>(
            res.safe_merge_pruned_count
            + res.interchange_pruned_count
            + res.exchangeable_type1_pruned_count
            + res.exchangeable_type2_pruned_count);

        os << indent << "  生成 " << res.children.size() << " 个子节点"
           << "（另有 " << res.pruned_count << " 个 Type II 候选因容量约束(v)被剪，"
           << res.safe_merge_pruned_count << " 个 Type I 候选因 Safe-Merge 被剪，"
           << res.interchange_pruned_count << " 个 Type I 候选因相邻交换被剪，"
           << res.exchangeable_type1_pruned_count << " 个 Type I 候选因可交换零件被剪，"
           << res.exchangeable_type2_pruned_count << " 个 Type II 候选因可交换零件被剪）:\n";

        for (Node& child : res.children) {
            const bool is_type1_child = (child.generation_type == 1);
            if (is_type1_child) {
                update_type1_metrics_incrementally(cur, child, ST, VT, UT, h, v, D);
            }
            else {
                update_node_metrics(child, ST, VT, UT, h, v, D);
            }

            // 与正式搜索一致：相同 (R,L) 下比较已封闭前缀 (tprev,TTcl)。
            if (is_global_state_dominated_or_insert(
                    child, dominance_map, parts.size(), dominance_epsilon)) {
                ++dominance_pruned_nodes;
                ++pruned_nodes_per_depth[child.depth];
                os << indent << "    - " << describe_child(cur, child)
                   << "  [强化状态支配剪枝：相同未分配集合与开放批次下，"
                      "已有节点的 TTcl 加最坏时间差补偿后仍不大]\n";
                continue;
            }
            // 与正式搜索一致：两类节点统一使用 max(LBpar, LBpos)。
            child.LB = compute_LBpar_LBpos(
                child, parts, D, ST, VT, UT, L, W, l, w, h, v, best_ub);

            os << indent << "    - " << describe_child(cur, child)
               << "  => " << batches_to_string(child)
               << "  LB=" << child.LB;

            // 与 branch_and_cut 一致：child.LB < UB 才入栈，否则在“生成阶段”即被剪
            if (child.LB < best_ub) {
                os << "  [入栈待展开]\n";
                stack.push_back(std::move(child));
            }
            else {
                ++LB_pruned_nodes;
                ++pruned_nodes_per_depth[child.depth];
                os << "  [生成时即被下界剪枝: LB>=UB]\n";
            }
        }
    }

    os << "----------------------------------------------------------------------------------\n";
    os << "追踪结束（统计口径与 branch_and_cut 完全一致）：\n";
    os << "  出栈处理节点数 total_nodes = " << total_nodes << "\n";
    os << "  生成子节点数 generated_nodes = " << generated_nodes << "\n";
    os << "  叶子节点数 leaf_nodes = " << leaf_nodes << "\n";
    os << "  UB 刷新次数 updated_solutions = " << updated_solutions << "\n";
    os << "  容量(v)剪枝 area_pruned_nodes = " << area_pruned_nodes << "\n";
    os << "  下界剪枝 LB_pruned_nodes = " << LB_pruned_nodes << "\n";
    os << "  状态支配剪枝 dominance_pruned_nodes = "
       << dominance_pruned_nodes << "\n";
    os << "  Type-I Safe-Merge 剪枝 safe_merge_pruned_nodes = "
       << safe_merge_pruned_nodes << "\n";
    os << "  Type-I 相邻交换剪枝 interchange_pruned_nodes = "
       << interchange_pruned_nodes << "\n";
    os << "  Type-I/Type-I 可交换零件剪枝 exchangeable_type1_pruned_nodes = "
       << exchangeable_type1_pruned_nodes << "\n";
    os << "  Type-II/Type-II 可交换零件剪枝 exchangeable_type2_pruned_nodes = "
       << exchangeable_type2_pruned_nodes << "\n";
    os << "  各深度被剪枝节点数 (Depth : PrunedNodes):\n";
    for (const auto& kv : pruned_nodes_per_depth) {
        os << "    深度 " << kv.first << " : " << kv.second << "\n";
    }
    os << "  最终 UB = " << best_ub << "\n";
    os << "==================================================================================\n";
}

