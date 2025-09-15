// TotalTardiness.h
#ifndef TOTAL_TARDINESS_H
#define TOTAL_TARDINESS_H

#include <vector>
#include <map>
#include <algorithm>
#include <set>
#include <tuple>
#include <cmath> // For std::fabs and comparison with epsilon
#include <limits> // For std::numeric_limits


// Define a small epsilon for floating-point comparisons
const double EPSILON = 1e-9;

// Structure to represent a job
struct Job {
    int id;
    double p; // processing time (changed to double)
    double d; // due date (changed to double)
};

// Custom comparator (not strictly used for sorting jobs in the DP state, but generally useful)
struct JobDueProcessTimeComparator {
    bool operator()(const Job& a, const Job& b) const {
        if (a.d != b.d) {
            return a.d < b.d;
        }
        return a.p < b.p;
    }
};

// Structure to store the decision that led to the optimal value
// 必须在 DPCacheKey 之后，TotalTardinessSolver 之前定义，因为它在 TotalTardinessSolver 中被使用。
// 并且在 DPCacheKey 内部并不直接引用 OptimalDecision。
// 错误提示“未定义标识符 "OptimalDecision"”是因为它在 TotalTardinessSolver 声明中被使用了，
// 但在那个点它还没被定义。
struct OptimalDecision {
    int k_prime_id = -1; // 初始化为-1以避免未初始化警告
    std::vector<int> jobs_before_k_prime;
    std::vector<int> jobs_after_k_prime;
};

// Key for the memoization map: {sorted_job_ids_tuple, start_time}
struct DPCacheKey {
    std::vector<int> job_ids; // Sorted job IDs representing the subset J
    double start_time;       // Changed to double

    bool operator<(const DPCacheKey& other) const {
        if (job_ids != other.job_ids) {
            return job_ids < other.job_ids;
        }
        // Custom comparison for double to avoid precision issues
        // We consider two start_times equal if their difference is within EPSILON
        if (std::fabs(start_time - other.start_time) < EPSILON) {
            return false; // They are considered equal
        }
        return start_time < other.start_time;
    }

    // Optional: equality operator if using unordered_map or just for clarity
    bool operator==(const DPCacheKey& other) const {
        return job_ids == other.job_ids && std::fabs(start_time - other.start_time) < EPSILON;
    }
};


class TotalTardinessSolver {
public:
    TotalTardinessSolver(const std::vector<Job>& jobs, double initial_t = 0.0);

    double solve();
    std::vector<int> get_optimal_sequence();

private:
    std::vector<Job> original_jobs;
    std::map<int, Job> id_to_job;
    double initial_scheduling_time;

    std::map<DPCacheKey, double> memo;
    std::map<DPCacheKey, OptimalDecision> path_memo; // OptimalDecision 现在已经定义

    double calculate_V(std::vector<int> current_job_ids, double t);

    int find_max_processing_time_job_id(const std::vector<int>& job_ids) const;

    void reconstruct_sequence(std::vector<int> current_job_ids, double t, std::vector<int>& result_sequence);
};

void print_jobs(const std::vector<Job>& jobs);

#endif // TOTAL_TARDINESS_H