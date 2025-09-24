#ifndef JOB_SCHEDULING_H
#define JOB_SCHEDULING_H

#include <vector>
#include <string>
#include <map>
#include <algorithm> // for std::sort
#include <numeric>   // for std::iota


// 定义作业结构体
struct Job {
    int id;
    int p; // processing_time
    int d; // due_date

    // 用于调试输出
    std::string to_string() const {
        return "Job{" + std::to_string(id) + ", p=" + std::to_string(p) + ", d=" + std::to_string(d) + "}";
    }
};

// 存储所有作业
extern std::vector<Job> all_jobs;

// 用于map的key：表示一个作业子集及其开始时间
struct SubsetKey {
    std::vector<int> job_indices; // 存储作业在原始列表中的索引
    int start_time;

    // 构造函数
    SubsetKey(const std::vector<int>& indices, int time) : start_time(time) {
        job_indices = indices;
        std::sort(job_indices.begin(), job_indices.end()); // 确保顺序一致，便于map查找
    }

    // 比较运算符，用于map的key
    bool operator<(const SubsetKey& other) const {
        if (start_time != other.start_time) {
            return start_time < other.start_time;
        }
        return job_indices < other.job_indices;
    }

    // 用于调试输出
    std::string to_string() const {
        std::string s = "{";
        for (size_t i = 0; i < job_indices.size(); ++i) {
            s += std::to_string(all_jobs[job_indices[i]].id); // 输出作业ID而不是索引
            if (i < job_indices.size() - 1) {
                s += ", ";
            }
        }
        s += "}, t=" + std::to_string(start_time);
        return s;
    }
};

// 存储 V(J, t) 的结果和导致该结果的最佳 delta 值
struct DPResult {
    int min_tardiness;
    int best_delta; // 记录导致 min_tardiness 的 delta 值，-1表示初始条件或空集

    DPResult(int tardiness = 0, int delta = -1) : min_tardiness(tardiness), best_delta(delta) {}
};

// 备忘录存储 V(J, t) 的结果和最佳 delta
extern std::map<SubsetKey, DPResult> memo;


// 调试函数
void print_debug_info(const std::string& msg);

// 辅助函数：计算子集中所有作业的总处理时间
int calculate_total_processing_time(const std::vector<int>& subset_indices);

// 辅助函数：在给定子集中找到处理时间最长的作业的索引
// 返回作业在 original_jobs 列表中的索引
int get_longest_processing_time_job_index(const std::vector<int>& subset_indices);

// 动态规划核心函数：计算给定子集 J 和开始时间 t 的最小总延迟
DPResult V(const std::vector<int>& subset_indices, int t);

// 主函数：计算给定作业列表的最小总延迟
int minimize_total_tardiness(const std::vector<Job>& jobs, std::vector<int>& optimal_sequence);

// 回溯函数：从备忘录中重建最优序列
void reconstruct_optimal_sequence(const std::vector<int>& current_subset_indices, int current_time, std::vector<int>& sequence);

#endif // JOB_SCHEDULING_H