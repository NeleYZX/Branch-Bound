#include "DynamicProgramming.h"
#include <iostream>
#include <numeric> // For std::accumulate
#include <limits>  // For std::numeric_limits

// 全局变量定义
std::map<SubsetKey, DPResult> memo;
std::vector<Job> all_jobs;

// 调试函数实现
void print_debug_info(const std::string& msg) {
     std::cout << "[DEBUG] " << msg << std::endl;
}

// 辅助函数：计算子集中所有作业的总处理时间
int calculate_total_processing_time(const std::vector<int>& subset_indices) {
    int total_p = 0;
    for (int job_idx : subset_indices) {
        total_p += all_jobs[job_idx].p;
    }
    return total_p;
}

// 辅助函数：在给定子集中找到处理时间最长的作业的索引
// 返回作业在 all_jobs 列表中的索引
int get_longest_processing_time_job_index(const std::vector<int>& subset_indices) {
    if (subset_indices.empty()) {
        return -1; // 或者抛出异常，表示无效输入
    }

    int max_p = -1;
    int longest_job_idx = -1;

    for (int job_idx : subset_indices) {
        if (all_jobs[job_idx].p > max_p) {
            max_p = all_jobs[job_idx].p;
            longest_job_idx = job_idx;
        }
    }
    return longest_job_idx;
}

// 动态规划核心函数实现
DPResult V(const std::vector<int>& subset_indices, int t) {
    SubsetKey current_key(subset_indices, t);
    print_debug_info("进入 V(" + current_key.to_string() + ")");

    // 检查备忘录
    if (memo.count(current_key)) {
        print_debug_info("  -> 备忘录命中，返回 {Tardiness: " + std::to_string(memo[current_key].min_tardiness) + ", Delta: " + std::to_string(memo[current_key].best_delta) + "}");
        return memo[current_key];
    }

    // 初始条件
    if (subset_indices.empty()) {
        print_debug_info("  -> 集合为空，返回 {Tardiness: 0, Delta: -1}");
        return memo[current_key] = DPResult(0, -1); // best_delta = -1 表示无delta
    }
    if (subset_indices.size() == 1) {
        int job_idx = subset_indices[0];
        int completion_time = t + all_jobs[job_idx].p;
        int tardiness = std::max(0, completion_time - all_jobs[job_idx].d);
        print_debug_info("  -> 集合大小为1 (" + all_jobs[job_idx].to_string() + ")，返回 {Tardiness: " + std::to_string(tardiness) + ", Delta: -1}");
        return memo[current_key] = DPResult(tardiness, -1); // best_delta = -1 表示无delta
    }

    // 递归关系
    int min_total_tardiness = std::numeric_limits<int>::max(); // 用一个非常大的值初始化
    int best_delta_for_current_key = -1; // 记录当前状态的最佳delta

    int k_prime_idx = get_longest_processing_time_job_index(subset_indices);
    if (k_prime_idx == -1) {
        return DPResult(-1, -1); // 异常情况
    }

    std::vector<int> smaller_jobs;
    std::vector<int> larger_jobs;

    for (int idx : subset_indices) {
        if (idx < k_prime_idx) {
            smaller_jobs.push_back(idx);
        }
        else if (idx > k_prime_idx) {
            larger_jobs.push_back(idx);
        }
    }
    std::sort(smaller_jobs.begin(), smaller_jobs.end());
    std::sort(larger_jobs.begin(), larger_jobs.end());

    // 遍历所有可能的 δ 值
    for (int delta = 0; delta <= larger_jobs.size(); ++delta) {
        std::vector<int> first_part_jobs = smaller_jobs;
        for (int i = 0; i < delta; ++i) {
            first_part_jobs.push_back(larger_jobs[i]);
        }
        std::sort(first_part_jobs.begin(), first_part_jobs.end());

        std::vector<int> third_part_jobs;
        for (size_t i = delta; i < larger_jobs.size(); ++i) {
            third_part_jobs.push_back(larger_jobs[i]);
        }
        std::sort(third_part_jobs.begin(), third_part_jobs.end());

        int completion_k_prime_delta = t + calculate_total_processing_time(first_part_jobs) + all_jobs[k_prime_idx].p;

        int current_tardiness_k_prime = std::max(0, completion_k_prime_delta - all_jobs[k_prime_idx].d);

        DPResult result_first_part = V(first_part_jobs, t);
        DPResult result_third_part = V(third_part_jobs, completion_k_prime_delta);

        int total_val = result_first_part.min_tardiness + current_tardiness_k_prime + result_third_part.min_tardiness;

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
int minimize_total_tardiness(const std::vector<Job>& jobs, std::vector<int>& optimal_sequence) {
    all_jobs = jobs;

    std::vector<int> initial_subset_indices(jobs.size());
    std::iota(initial_subset_indices.begin(), initial_subset_indices.end(), 0);

    memo.clear();

    DPResult result = V(initial_subset_indices, 0);

    // 回溯重建最优序列
    optimal_sequence.clear();
    reconstruct_optimal_sequence(initial_subset_indices, 0, optimal_sequence);

    return result.min_tardiness;
}

// 回溯函数：从备忘录中重建最优序列
void reconstruct_optimal_sequence(const std::vector<int>& current_subset_indices, int current_time, std::vector<int>& sequence) {
    if (current_subset_indices.empty()) {
        return;
    }
    if (current_subset_indices.size() == 1) {
        sequence.push_back(all_jobs[current_subset_indices[0]].id); // 添加作业ID
        return;
    }

    SubsetKey current_key(current_subset_indices, current_time);
    DPResult stored_result = memo[current_key];
    int best_delta = stored_result.best_delta;

    int k_prime_idx = get_longest_processing_time_job_index(current_subset_indices);

    std::vector<int> smaller_jobs;
    std::vector<int> larger_jobs;

    for (int idx : current_subset_indices) {
        if (idx < k_prime_idx) {
            smaller_jobs.push_back(idx);
        }
        else if (idx > k_prime_idx) {
            larger_jobs.push_back(idx);
        }
    }
    std::sort(smaller_jobs.begin(), smaller_jobs.end());
    std::sort(larger_jobs.begin(), larger_jobs.end());

    std::vector<int> first_part_jobs = smaller_jobs;
    for (int i = 0; i < best_delta; ++i) {
        first_part_jobs.push_back(larger_jobs[i]);
    }
    std::sort(first_part_jobs.begin(), first_part_jobs.end());

    std::vector<int> third_part_jobs;
    for (size_t i = best_delta; i < larger_jobs.size(); ++i) {
        third_part_jobs.push_back(larger_jobs[i]);
    }
    std::sort(third_part_jobs.begin(), third_part_jobs.end());

    // 递归重建第一部分
    reconstruct_optimal_sequence(first_part_jobs, current_time, sequence);

    // 添加 k_prime_idx
    sequence.push_back(all_jobs[k_prime_idx].id); // 添加作业ID

    // 计算 k_prime 的完成时间，作为第三部分的开始时间
    int completion_k_prime_delta = current_time + calculate_total_processing_time(first_part_jobs) + all_jobs[k_prime_idx].p;

    // 递归重建第三部分
    reconstruct_optimal_sequence(third_part_jobs, completion_k_prime_delta, sequence);
}