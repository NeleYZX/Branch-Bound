#include "BranchBound.h"
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
    : LB(0.0), completion_time(0.0), total_tardiness(0.0), name("N") {
}

Node::Node(const std::unordered_map<int, std::vector<int>>& S_,
    double LB_,
    const std::string& name_,
    double completion_time_,
    double total_tardiness_)
    : S(S_), LB(LB_), name(name_),
    completion_time(completion_time_), total_tardiness(total_tardiness_) {
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
            node.total_tardiness
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

    if (node.S.empty()) return comp_times;

    // 找最大编号批次（只需保留当前新增的）
    auto last_batch_it = std::max_element(
        node.S.begin(), node.S.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; }
    );

    const auto& part_ids = last_batch_it->second;

    // 内联计算 PT
    double vol = 0.0, mh = 0.0;
    for (int pid : part_ids) {
        vol += v[pid];
        mh = std::max(mh, h[pid]);
    }
    double PT = ST[0] + VT[0] * vol + UT[0] * mh;
    double completion_time = node.completion_time;

    for (int pid : part_ids) {
        comp_times[pid] = completion_time + PT;
    }

    return comp_times;
}

//=======================已分配零件总延迟===================
double compute_assigned_tardiness(
    const Node& node,
    const std::vector<double>& D
) {
    if (node.S.empty()) return 0.0;

    auto last_batch_it = std::max_element(
        node.S.begin(), node.S.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; }
    );

    const auto& part_ids = last_batch_it->second;
    double completion_time = node.completion_time;
    double tardiness = 0.0;

    for (int pid : part_ids) {
        tardiness += std::max(0.0, completion_time - D[pid]);
    }

    return node.total_tardiness + tardiness;
}

//=======================未分配零件总延迟下界估计====================
double compute_unassigned_lower_bound(
    const Node& node,
    const std::vector<int>& parts,
    const std::vector<double>& D,
    const std::vector<double>& cached_PT  // 改为缓存值
) {
    std::unordered_set<int> assigned;
    for (const auto& [_, plist] : node.S) {
        for (int pid : plist) {
            assigned.insert(pid);
        }
    }

    double est_tardiness = 0.0;
    double start_time = node.completion_time;

    for (int pid : parts) {
        if (assigned.count(pid)) continue;

        double pt = cached_PT[pid];  // 查询缓存
        double c = start_time + pt;
        est_tardiness += std::max(0.0, c - D[pid]);
    }

    return node.total_tardiness + est_tardiness;
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
    double machine_area = L[0] * W[0];

    // 预计算面积
    std::vector<double> part_areas(parts.size(), 0.0);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        part_areas[parts[i]] = l[parts[i]] * w[parts[i]];
    }

    //PT值预缓存
    std::vector<double> cached_PT(parts.size());
    for (int p : parts) {
        cached_PT[p] = ST[0] + VT[0] * v[p] + UT[0] * h[p];
    }

    // 判断是否全分配
    auto all_assigned = [&](const Node& nd)->bool {
        std::size_t cnt = 0;
        for (typename std::unordered_map<int, std::vector<int> >::const_iterator it = nd.S.begin();
            it != nd.S.end(); ++it) {
            cnt += it->second.size();
        }
        return cnt == parts.size();
        };

    Node best(initial_S, 0.0, "Best", 0.0, 0.0);
    Node root({}, 0.0, "Root", 0.0, 0.0);  // 修复初始化
    //root.LB = compute_total_lower_bound(root, parts, D, ST, VT, UT, h, v);

    std::deque<Node> stack;
    stack.push_back(root);

    auto t0 = std::chrono::steady_clock::now();

    while (!stack.empty()) {
        auto t1 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();
        if (time_limit_seconds > 0.0 && elapsed > time_limit_seconds) {
            break;
        }

        Node cur = stack.back();
        stack.pop_back();
        ++stats.total_nodes;

        // 初步不可行剪枝
        bool bad = false;
        for (typename std::unordered_map<int, std::vector<int> >::const_iterator it = cur.S.begin();
            it != cur.S.end() && !bad; ++it) {
            for (std::size_t ui = 0; ui < initial_infeasible.size(); ++ui) {
                if (std::includes(
                    it->second.begin(),
                    it->second.end(),
                    initial_infeasible[ui].begin(),
                    initial_infeasible[ui].end()))
                {
                    bad = true;
                    break;
                }
            }
        }
        if (bad) {
            ++stats.U_pruned_nodes;
            continue;
        }

        // LB 剪枝
        if (cur.LB >= UB) {
            ++stats.LB_pruned_nodes;
            continue;
        }

        // 叶子节点
        if (all_assigned(cur)) {
            ++stats.leaf_nodes;
            if (cur.LB < UB) {
                UB = cur.LB;
                best = cur;
                ++stats.updated_solutions;
            }
            continue;
        }

        // 展开子节点
        auto [kids, pruned] = generate_children(cur, parts, machine_area, part_areas);
        stats.generated_nodes += kids.size();
        stats.area_pruned_nodes += pruned;
        for (auto& child : kids) {
            // 1. 计算新增批次完成时间
            auto comp_times = compute_completion_times(child, ST, VT, UT, h, v);
            if (!comp_times.empty()) {
                child.completion_time = comp_times.begin()->second;
            }

            // 2. 基于当前批次，更新累计已分配零件的延迟
            child.total_tardiness = compute_assigned_tardiness(child, D);

            // 3. 基于更新后的 completion_time 和 total_tardiness 估算下界
            child.LB = compute_unassigned_lower_bound(child, parts, D, cached_PT);

            // 4. 剪枝判断
            if (child.LB < UB) {
                stack.push_back(child);
            }
            else {
                ++stats.LB_pruned_nodes;
            }
        }

    }

    return std::make_pair(best, stats);
}


//==========================数据记录================================
namespace fs = std::filesystem;

// 定义全局日志流对象
std::ofstream log_stream;

std::string get_log_filename(const std::string& input_filename) {
    std::string base = fs::path(input_filename).stem().string();  // 提取文件名（不含路径与后缀）
    std::string log_dir = "logs_IncrementalLB/";
    fs::create_directories(log_dir);  // 创建 logs 目录（若不存在）

    int count = 1;
    std::string log_filename;
    do {
        log_filename = log_dir + base + "_log_" + std::to_string(count) + ".txt";
        count++;
    } while (fs::exists(log_filename));

    return log_filename;
}

void write_utf8_bom(std::ofstream& stream) {
    // 写入 UTF-8 BOM: EF BB BF
    stream << "\xEF\xBB\xBF";
}
