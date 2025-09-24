#ifndef JOB_SCHEDULING_H
#define JOB_SCHEDULING_H

#include <vector>
#include <string>
#include <map>
#include <algorithm> // for std::sort, std::max
#include <numeric>   // for std::iota
#include <random>
#include <chrono>
#include <iostream>  // For std::cout
#include <limits>    // For std::numeric_limits

//==================================================动态规划算法===============================================
// 定义作业结构体
struct Job {
    int id;
    double p; // processing_time
    double d; // due_date

    // 用于调试输出
    std::string to_string() const {
        return "Job{" + std::to_string(id) + ", p=" + std::to_string(p) + ", d=" + std::to_string(d) + "}";
    }
};

// 用于map的key：表示一个作业子集及其开始时间
struct SubsetKey {
    std::vector<int> job_original_indices; // 存储作业在原始 all_jobs 列表中的索引
    double start_time;

    // 构造函数
    SubsetKey(const std::vector<int>& indices, double time);

    // 比较运算符，用于map的key
    bool operator<(const SubsetKey& other) const;

    // 用于调试输出
    std::string to_string() const;
};

// 存储 V(J, t) 的结果和导致该结果的最佳 delta 值
struct DPResult {
    double min_tardiness;
    int best_delta; // 记录导致 min_tardiness 的 delta 值

    DPResult(double tardiness = 0.0, int delta = -1);
};

// 全局变量声明
extern std::vector<Job> all_jobs; // 存储所有作业（原始顺序）
extern std::map<SubsetKey, DPResult> memo; // 备忘录

// 函数声明
void print_debug_info(const std::string& msg); // 调试函数

// 辅助函数：计算子集中所有作业的总处理时间
double calculate_total_processing_time(const std::vector<int>& subset_original_indices);

// 辅助函数：在给定子集中找到处理时间最长的作业的原始索引
int get_longest_processing_time_job_original_index(const std::vector<int>& subset_original_indices);

// 动态规划核心函数：计算给定子集 J 和开始时间 t 的最小总延迟
DPResult V(const std::vector<int>& subset_original_indices, double t);

// 主函数：计算给定作业列表的最小总延迟
double minimize_total_tardiness(const std::vector<Job>& jobs_input, double initial_start_time, std::vector<int>& optimal_sequence);

// 回溯函数：从备忘录中重建最优序列
void reconstruct_optimal_sequence(const std::vector<int>& current_subset_original_indices, double current_time, std::vector<int>& sequence);

//================================枚举函数，共用job结构体===================================

// 计算给定作业序列的总延迟时间
double calculate_total_lateness(const std::vector<Job>& schedule); // 返回值修改

// 使用枚举法找到最小化总延迟时间的调度方案
std::vector<Job> find_min_lateness_schedule(const std::vector<Job>& jobs);


//================================随机算例生成================================================

// 随机生成作业列表
std::vector<Job> generate_random_jobs(int num_jobs, double min_p, double max_p, double min_d, double max_d); // 参数修改

#endif // JOB_SCHEDULING_H