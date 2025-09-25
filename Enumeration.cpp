#include "DynamicProgramming.h"
#include <iostream>
#include <limits> // 用于 std::numeric_limits

//==================================================枚举函数实现===============================================

// Job 结构体中的 ostream << 运算符的定义
std::ostream& operator<<(std::ostream& os, const Job& job) {
    os << "(ID:" << job.id << ", p:" << job.p << ", d:" << job.d << ")";
    return os;
}

// 计算给定调度方案的总延迟
double calculate_total_lateness(const std::vector<Job>& schedule) {
    double current_time = 0.0;
    double total_lateness = 0.0;
    for (const auto& job : schedule) {
        current_time += job.p;
        total_lateness += std::max(0.0, current_time - job.d);
    }
    return total_lateness;
}

// 查找所有最小化总延迟的调度方案
std::vector<std::vector<Job>> find_all_min_lateness_schedules(const std::vector<Job>& jobs) {
    std::vector<Job> current_permutation = jobs;
    std::vector<std::vector<Job>> best_schedules; // 存储所有最优调度方案
    double min_lateness = std::numeric_limits<double>::max();

    // 初始排序，确保 std::next_permutation 从第一个排列开始
    std::sort(current_permutation.begin(), current_permutation.end(), [](const Job& a, const Job& b) {
        return a.id < b.id; // 假设ID唯一且作为排序基准
        });

    do {
        double current_lateness = calculate_total_lateness(current_permutation);

        if (current_lateness < min_lateness) {
            // 找到了更小的总延迟
            min_lateness = current_lateness;
            best_schedules.clear();
            best_schedules.push_back(current_permutation);
        }
        else if (current_lateness == min_lateness) {
            // 找到了相同最小总延迟的方案
            best_schedules.push_back(current_permutation);
        }
    } while (std::next_permutation(current_permutation.begin(), current_permutation.end(), [](const Job& a, const Job& b) {
        return a.id < b.id; // 保持与初始排序一致的比较器
        }));

    return best_schedules;
}


// 辅助函数：打印调度方案
void print_schedule(const std::vector<Job>& schedule) {
    std::cout << "["; // 添加方括号，使输出更清晰
    for (size_t i = 0; i < schedule.size(); ++i) {
        std::cout << schedule[i].id; // 只打印 Job 的 id
        if (i < schedule.size() - 1) {
            std::cout << ", "; // 使用逗号和空格分隔
        }
    }
    std::cout << "]" << std::endl; // 结束方括号并换行
}




//==================================================随机算例生成实现================================================

std::vector<Job> generate_random_jobs(int num_jobs, double min_p, double max_p, double min_d, double max_d) {
    std::vector<Job> jobs;
    // 使用 std::default_random_engine 和系统时间作为种子
    std::default_random_engine generator(std::chrono::system_clock::now().time_since_epoch().count());

    // 使用 std::uniform_real_distribution 生成浮点数
    std::uniform_real_distribution<double> p_distribution(min_p, max_p);
    std::uniform_real_distribution<double> d_distribution(min_d, max_d);

    for (int i = 0; i < num_jobs; ++i) {
        jobs.push_back({ i + 1, p_distribution(generator), d_distribution(generator) });
    }
    return jobs;
}