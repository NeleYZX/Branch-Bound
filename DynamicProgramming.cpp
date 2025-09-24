#include "DynamicProgramming.h"
#include <iostream>
#include <numeric> // For std::accumulate
#include <limits>  // For std::numeric_limits

// 全局变量定义
std::vector<Job> all_jobs;
std::map<SubsetKey, DPResult> memo;

// SubsetKey 构造函数实现
SubsetKey::SubsetKey(const std::vector<int>& indices, double time) : start_time(time) {
    job_original_indices = indices;
    std::sort(job_original_indices.begin(), job_original_indices.end()); // 确保顺序一致，便于map查找
}

// SubsetKey 比较运算符实现
bool SubsetKey::operator<(const SubsetKey& other) const {
    if (start_time != other.start_time) {
        return start_time < other.start_time;
    }
    return job_original_indices < other.job_original_indices;
}

// SubsetKey 调试输出实现
std::string SubsetKey::to_string() const {
    std::string s = "{";
    for (size_t i = 0; i < job_original_indices.size(); ++i) {
        s += std::to_string(all_jobs[job_original_indices[i]].id); // 输出作业ID而不是索引
        if (i < job_original_indices.size() - 1) {
            s += ", ";
        }
    }
    s += "}, t=" + std::to_string(start_time);
    return s;
}

// DPResult 构造函数实现
DPResult::DPResult(double tardiness, int delta) : min_tardiness(tardiness), best_delta(delta) {}


// 调试函数实现
void print_debug_info(const std::string& msg) {
    // std::cout << "[DEBUG] " << msg << std::endl; // 可以取消注释以查看调试信息
}

// 辅助函数：计算子集中所有作业的总处理时间
double calculate_total_processing_time(const std::vector<int>& subset_original_indices) {
    double total_p = 0.0;
    for (int job_idx : subset_original_indices) {
        total_p += all_jobs[job_idx].p;
    }
    return total_p;
}

// 辅助函数：在给定子集中找到处理时间最长的作业的原始索引
int get_longest_processing_time_job_original_index(const std::vector<int>& subset_original_indices) {
    if (subset_original_indices.empty()) {
        return -1;
    }

    double max_p = -1.0;
    int longest_job_original_idx = -1;

    for (int job_idx : subset_original_indices) {
        if (all_jobs[job_idx].p > max_p) {
            max_p = all_jobs[job_idx].p;
            longest_job_original_idx = job_idx;
        }
    }
    return longest_job_original_idx;
}

// 动态规划核心函数实现
DPResult V(const std::vector<int>& subset_original_indices, double t) {
    SubsetKey current_key(subset_original_indices, t);
    print_debug_info("进入 V(" + current_key.to_string() + ")");

    // 检查备忘录
    if (memo.count(current_key)) {
        print_debug_info("  -> 备忘录命中，返回 {Tardiness: " + std::to_string(memo[current_key].min_tardiness) + ", Delta: " + std::to_string(memo[current_key].best_delta) + "}");
        return memo[current_key];
    }

    // 初始条件
    if (subset_original_indices.empty()) {
        print_debug_info("  -> 集合为空，返回 {Tardiness: 0.0, Delta: -1}");
        return memo[current_key] = DPResult(0.0, -1);
    }
    if (subset_original_indices.size() == 1) {
        int job_idx = subset_original_indices[0];
        double completion_time = t + all_jobs[job_idx].p;
        double tardiness = std::max(0.0, completion_time - all_jobs[job_idx].d);
        print_debug_info("  -> 集合大小为1 (" + all_jobs[job_idx].to_string() + ")，返回 {Tardiness: " + std::to_string(tardiness) + ", Delta: -1}");
        return memo[current_key] = DPResult(tardiness, -1);
    }

    // 递归关系
    double min_total_tardiness = std::numeric_limits<double>::max();
    int best_delta_for_current_key = -1;

    // 获取当前子集中处理时间最长的作业的原始索引
    int k_prime_original_idx = get_longest_processing_time_job_original_index(subset_original_indices);
    if (k_prime_original_idx == -1) {
        return DPResult(-1.0, -1); // 异常情况
    }

    // 构建除了 k_prime 之外的作业列表，并按照 due_date 排序
    std::vector<int> jobs_without_k_prime;
    for (int idx : subset_original_indices) {
        if (idx != k_prime_original_idx) {
            jobs_without_k_prime.push_back(idx);
        }
    }
    std::sort(jobs_without_k_prime.begin(), jobs_without_k_prime.end(),
        [](int a, int b) {
            return all_jobs[a].d < all_jobs[b].d;
        });

    // delta 现在表示在 jobs_without_k_prime 中，有多少个作业会排在 k_prime 之前。
    for (int delta = 0; delta <= jobs_without_k_prime.size(); ++delta) {
        std::vector<int> first_part_jobs_recurs; // 实际在 k_prime 之前调度的作业
        std::vector<int> third_part_jobs_recurs; // 实际在 k_prime 之后调度的作业

        // 将 jobs_without_k_prime 拆分
        for (int i = 0; i < delta; ++i) {
            first_part_jobs_recurs.push_back(jobs_without_k_prime[i]);
        }
        for (size_t i = delta; i < jobs_without_k_prime.size(); ++i) {
            third_part_jobs_recurs.push_back(jobs_without_k_prime[i]);
        }

        // 确保递归调用的子集是排序的，以保证 SubsetKey 的一致性
        std::sort(first_part_jobs_recurs.begin(), first_part_jobs_recurs.end());
        std::sort(third_part_jobs_recurs.begin(), third_part_jobs_recurs.end());

        // 计算 k_prime 的完成时间
        double completion_k_prime_delta = t + calculate_total_processing_time(first_part_jobs_recurs) + all_jobs[k_prime_original_idx].p;

        double current_tardiness_k_prime = std::max(0.0, completion_k_prime_delta - all_jobs[k_prime_original_idx].d);

        DPResult result_first_part = V(first_part_jobs_recurs, t);
        DPResult result_third_part = V(third_part_jobs_recurs, completion_k_prime_delta);

        double total_val = result_first_part.min_tardiness + current_tardiness_k_prime + result_third_part.min_tardiness;

        print_debug_info("  δ=" + std::to_string(delta) + ": Ck'(" + std::to_string(delta) + ")=" + std::to_string(completion_k_prime_delta) +
            ", T(k')=" + std::to_string(current_tardiness_k_prime) +
            ", V_first=" + std::to_string(result_first_part.min_tardiness) +
            ", V_third=" + std::to_string(result_third_part.min_tardiness) +
            ", Total=" + std::to_string(total_val));

        if (total_val < min_total_tardiness) {
            min_total_tardiness = total_val;
            best_delta_for_current_key = delta;
        }
    }

    print_debug_info("  -> V(" + current_key.to_string() + ") 计算完成，结果: {Tardiness: " + std::to_string(min_total_tardiness) + ", Delta: " + std::to_string(best_delta_for_current_key) + "}");
    return memo[current_key] = DPResult(min_total_tardiness, best_delta_for_current_key);
}

// 主函数：计算给定作业列表的最小总延迟
double minimize_total_tardiness(const std::vector<Job>& jobs_input, double initial_start_time, std::vector<int>& optimal_sequence) {
    all_jobs = jobs_input; // 将输入作业存储到全局变量

    std::vector<int> initial_subset_original_indices(all_jobs.size());
    std::iota(initial_subset_original_indices.begin(), initial_subset_original_indices.end(), 0); // 包含所有作业的原始索引

    memo.clear(); // 清空备忘录

    DPResult result = V(initial_subset_original_indices, initial_start_time);

    // 回溯重建最优序列
    optimal_sequence.clear();
    reconstruct_optimal_sequence(initial_subset_original_indices, initial_start_time, optimal_sequence);

    return result.min_tardiness;
}

// 回溯函数：从备忘录中重建最优序列
void reconstruct_optimal_sequence(const std::vector<int>& current_subset_original_indices, double current_time, std::vector<int>& sequence) {
    if (current_subset_original_indices.empty()) {
        return;
    }
    if (current_subset_original_indices.size() == 1) {
        sequence.push_back(all_jobs[current_subset_original_indices[0]].id); // 添加作业ID
        return;
    }

    SubsetKey current_key(current_subset_original_indices, current_time);
    DPResult stored_result = memo[current_key];
    int best_delta = stored_result.best_delta;

    int k_prime_original_idx = get_longest_processing_time_job_original_index(current_subset_original_indices);

    // 重新构建 jobs_without_k_prime 并排序，以与 V 函数中的逻辑一致
    std::vector<int> jobs_without_k_prime;
    for (int idx : current_subset_original_indices) {
        if (idx != k_prime_original_idx) {
            jobs_without_k_prime.push_back(idx);
        }
    }
    std::sort(jobs_without_k_prime.begin(), jobs_without_k_prime.end(),
        [](int a, int b) {
            return all_jobs[a].d < all_jobs[b].d;
        });

    std::vector<int> first_part_jobs_recurs;
    std::vector<int> third_part_jobs_recurs;

    for (int i = 0; i < best_delta; ++i) {
        first_part_jobs_recurs.push_back(jobs_without_k_prime[i]);
    }
    for (size_t i = best_delta; i < jobs_without_k_prime.size(); ++i) {
        third_part_jobs_recurs.push_back(jobs_without_k_prime[i]);
    }

    // 确保递归调用的子集是排序的
    std::sort(first_part_jobs_recurs.begin(), first_part_jobs_recurs.end());
    std::sort(third_part_jobs_recurs.begin(), third_part_jobs_recurs.end());

    // 递归重建第一部分
    reconstruct_optimal_sequence(first_part_jobs_recurs, current_time, sequence);

    // 添加 k_prime_original_idx
    sequence.push_back(all_jobs[k_prime_original_idx].id); // 添加作业ID

    // 计算 k_prime 的完成时间，作为第三部分的开始时间
    double completion_k_prime_delta = current_time + calculate_total_processing_time(first_part_jobs_recurs) + all_jobs[k_prime_original_idx].p;

    // 递归重建第三部分
    reconstruct_optimal_sequence(third_part_jobs_recurs, completion_k_prime_delta, sequence);
}