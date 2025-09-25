#include "DynamicProgramming.h" // 确保这里引用的是修改后的头文件
#include <iostream>
#include <numeric> // For std::accumulate
#include <limits>  // For std::numeric_limits

// 全局变量定义
std::vector<Job> all_jobs;
std::map<SubsetKey, DPResult> memo;

// SubsetKey 构造函数实现
SubsetKey::SubsetKey(const std::vector<int>& indices, double time) : start_time(time) {
    job_indices_in_all_jobs = indices;
    std::sort(job_indices_in_all_jobs.begin(), job_indices_in_all_jobs.end()); // 确保顺序一致，便于map查找
}

// SubsetKey 比较运算符实现
bool SubsetKey::operator<(const SubsetKey& other) const {
    if (start_time != other.start_time) {
        return start_time < other.start_time;
    }
    return job_indices_in_all_jobs < other.job_indices_in_all_jobs;
}

// SubsetKey 调试输出实现
std::string SubsetKey::to_string() const {
    std::string s = "{";
    for (size_t i = 0; i < job_indices_in_all_jobs.size(); ++i) {
        // 输出作业的原始ID
        s += std::to_string(all_jobs[job_indices_in_all_jobs[i]].id);
        if (i < job_indices_in_all_jobs.size() - 1) {
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
     //std::cout << "[DEBUG] " << msg << std::endl; // 可以取消注释以查看调试信息
}

// 辅助函数：计算子集中所有作业的总处理时间
double calculate_total_processing_time(const std::vector<int>& subset_indices_in_all_jobs) {
    double total_p = 0.0;
    for (int job_idx : subset_indices_in_all_jobs) {
        total_p += all_jobs[job_idx].p;
    }
    return total_p;
}

// 辅助函数：在给定子集中找到处理时间最长的作业的索引 (在all_jobs中的索引)
int get_longest_processing_time_job_index_in_all_jobs(const std::vector<int>& subset_indices_in_all_jobs) {
    if (subset_indices_in_all_jobs.empty()) {
        return -1;
    }

    double max_p = -1.0;
    int longest_job_current_idx = -1; // 存储的是在 all_jobs 列表中的索引

    for (int job_idx : subset_indices_in_all_jobs) {
        if (all_jobs[job_idx].p > max_p) {
            max_p = all_jobs[job_idx].p;
            longest_job_current_idx = job_idx;
        }
    }
    return longest_job_current_idx;
}

// 动态规划核心函数实现 (V 函数签名不变)
DPResult V(const std::vector<int>& subset_indices_in_all_jobs, double t) {
    SubsetKey current_key(subset_indices_in_all_jobs, t);
    print_debug_info("进入 V(" + current_key.to_string() + ")");

    // 检查备忘录
    if (memo.count(current_key)) {
        print_debug_info("  -> 备忘录命中，返回 {Tardiness: " + std::to_string(memo[current_key].min_tardiness) + ", Delta: " + std::to_string(memo[current_key].best_delta) + "}");
        return memo[current_key];
    }

    // 初始条件
    if (subset_indices_in_all_jobs.empty()) {
        print_debug_info("  -> 集合为空，返回 {Tardiness: 0.0, Delta: -1}");
        return memo[current_key] = DPResult(0.0, -1);
    }
    if (subset_indices_in_all_jobs.size() == 1) {
        int job_idx = subset_indices_in_all_jobs[0]; // 这里 job_idx 是 all_jobs 中的索引
        double completion_time = t + all_jobs[job_idx].p;
        double tardiness = std::max(0.0, completion_time - all_jobs[job_idx].d);
        print_debug_info("  -> 集合大小为1 (" + all_jobs[job_idx].to_string() + ")，返回 {Tardiness: " + std::to_string(tardiness) + ", Delta: -1}");
        return memo[current_key] = DPResult(tardiness, -1);
    }

    // 递归关系
    double min_total_tardiness = std::numeric_limits<double>::max();
    int best_delta_for_current_key = -1;

    // 获取当前子集中处理时间最长的作业的在 all_jobs 列表中的索引
    int k_prime_idx_in_all_jobs = get_longest_processing_time_job_index_in_all_jobs(subset_indices_in_all_jobs);
    if (k_prime_idx_in_all_jobs == -1) {
        return DPResult(-1.0, -1); // 异常情况
    }

    // 拆分作业集合 J 为 smaller_jobs 和 larger_jobs
    // smaller_jobs: 在 all_jobs 中的索引小于 k_prime_idx_in_all_jobs 且在当前子集中的作业
    // larger_jobs: 在 all_jobs 中的索引大于 k_prime_idx_in_all_jobs 且在当前子集中的作业
    std::vector<int> smaller_jobs_in_subset;
    std::vector<int> larger_jobs_in_subset;

    for (int idx : subset_indices_in_all_jobs) {
        if (idx < k_prime_idx_in_all_jobs) {
            smaller_jobs_in_subset.push_back(idx);
        }
        else if (idx > k_prime_idx_in_all_jobs) {
            larger_jobs_in_subset.push_back(idx);
        }
    }
    // 确保这些子集内部也是排序的，以保证 SubsetKey 的一致性
    std::sort(smaller_jobs_in_subset.begin(), smaller_jobs_in_subset.end());
    std::sort(larger_jobs_in_subset.begin(), larger_jobs_in_subset.end());

    // 遍历所有可能的 δ 值
    // δ 代表 larger_jobs_in_subset 中有多少个作业排在 k_prime_idx_in_all_jobs 之前
    for (int delta = 0; delta <= larger_jobs_in_subset.size(); ++delta) {
        std::vector<int> first_part_jobs_recurs = smaller_jobs_in_subset; // 这部分总是排在 k_prime 之前
        for (int i = 0; i < delta; ++i) { // 从 larger_jobs 中取 delta 个也排在 k_prime 之前
            first_part_jobs_recurs.push_back(larger_jobs_in_subset[i]);
        }
        std::sort(first_part_jobs_recurs.begin(), first_part_jobs_recurs.end()); // 确保排序

        std::vector<int> third_part_jobs_recurs; // 剩余的 larger_jobs 排在 k_prime 之后
        for (size_t i = delta; i < larger_jobs_in_subset.size(); ++i) {
            third_part_jobs_recurs.push_back(larger_jobs_in_subset[i]);
        }
        std::sort(third_part_jobs_recurs.begin(), third_part_jobs_recurs.end()); // 确保排序

        // 计算 k_prime_idx_in_all_jobs 的完成时间
        double completion_k_prime_delta = t + calculate_total_processing_time(first_part_jobs_recurs) + all_jobs[k_prime_idx_in_all_jobs].p;

        double current_tardiness_k_prime = std::max(0.0, completion_k_prime_delta - all_jobs[k_prime_idx_in_all_jobs].d);

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
    // 预处理：将 jobs_input 复制到 all_jobs 并记录原始索引，然后按 due_date 排序
    all_jobs.clear();
    for (size_t i = 0; i < jobs_input.size(); ++i) {
        Job j = jobs_input[i];
        j.original_input_index = static_cast<int>(i); // 记录在原始输入中的位置
        all_jobs.push_back(j);
    }

    // 按照 due_date 升序排列 all_jobs
    std::sort(all_jobs.begin(), all_jobs.end(), [](const Job& a, const Job& b) {
        return a.d < b.d;
        });

    // Debug: 打印排序后的 all_jobs
    // std::cout << "Sorted all_jobs by due_date:" << std::endl;
    // for(const auto& job : all_jobs) {
    //     std::cout << "  " << job.to_string() << std::endl;
    // }

    std::vector<int> initial_subset_indices_in_all_jobs(all_jobs.size());
    // 此时 initial_subset_indices_in_all_jobs 包含 0 到 N-1，
    // 这些索引现在指向 all_jobs 中已经按 due_date 排序后的作业。
    std::iota(initial_subset_indices_in_all_jobs.begin(), initial_subset_indices_in_all_jobs.end(), 0);

    memo.clear(); // 清空备忘录

    DPResult result = V(initial_subset_indices_in_all_jobs, initial_start_time);

    // 回溯重建最优序列
    optimal_sequence.clear();
    // reconstruct_optimal_sequence 将会使用 all_jobs[idx].id 来获取原始ID
    reconstruct_optimal_sequence(initial_subset_indices_in_all_jobs, initial_start_time, optimal_sequence);

    return result.min_tardiness;
}

// 回溯函数：从备忘录中重建最优序列
void reconstruct_optimal_sequence(const std::vector<int>& current_subset_indices_in_all_jobs, double current_time, std::vector<int>& sequence) {
    if (current_subset_indices_in_all_jobs.empty()) {
        return;
    }
    if (current_subset_indices_in_all_jobs.size() == 1) {
        sequence.push_back(all_jobs[current_subset_indices_in_all_jobs[0]].id); // 添加作业原始ID
        return;
    }

    SubsetKey current_key(current_subset_indices_in_all_jobs, current_time);
    DPResult stored_result = memo[current_key];
    int best_delta = stored_result.best_delta;

    int k_prime_idx_in_all_jobs = get_longest_processing_time_job_index_in_all_jobs(current_subset_indices_in_all_jobs);

    // 重新构建 smaller_jobs_in_subset 和 larger_jobs_in_subset，与 V 函数中的逻辑一致
    std::vector<int> smaller_jobs_in_subset;
    std::vector<int> larger_jobs_in_subset;

    for (int idx : current_subset_indices_in_all_jobs) {
        if (idx < k_prime_idx_in_all_jobs) {
            smaller_jobs_in_subset.push_back(idx);
        }
        else if (idx > k_prime_idx_in_all_jobs) {
            larger_jobs_in_subset.push_back(idx);
        }
    }
    std::sort(smaller_jobs_in_subset.begin(), smaller_jobs_in_subset.end());
    std::sort(larger_jobs_in_subset.begin(), larger_jobs_in_subset.end());


    std::vector<int> first_part_jobs_recurs = smaller_jobs_in_subset;
    for (int i = 0; i < best_delta; ++i) {
        first_part_jobs_recurs.push_back(larger_jobs_in_subset[i]);
    }
    std::sort(first_part_jobs_recurs.begin(), first_part_jobs_recurs.end());

    std::vector<int> third_part_jobs_recurs;
    for (size_t i = best_delta; i < larger_jobs_in_subset.size(); ++i) {
        third_part_jobs_recurs.push_back(larger_jobs_in_subset[i]);
    }
    std::sort(third_part_jobs_recurs.begin(), third_part_jobs_recurs.end());

    // 递归重建第一部分
    reconstruct_optimal_sequence(first_part_jobs_recurs, current_time, sequence);

    // 添加 k_prime_idx_in_all_jobs
    sequence.push_back(all_jobs[k_prime_idx_in_all_jobs].id); // 添加作业原始ID

    // 计算 k_prime 的完成时间，作为第三部分的开始时间
    double completion_k_prime_delta = current_time + calculate_total_processing_time(first_part_jobs_recurs) + all_jobs[k_prime_idx_in_all_jobs].p;

    // 递归重建第三部分
    reconstruct_optimal_sequence(third_part_jobs_recurs, completion_k_prime_delta, sequence);
}