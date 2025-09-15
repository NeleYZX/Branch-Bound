// TotalTardiness.h
#ifndef TOTAL_TARDINESS_H
#define TOTAL_TARDINESS_H

#include <vector>
#include <map>
#include <algorithm>
#include <set>
#include <tuple>
#include <cmath>
#include <limits>
#include <string> // For std::string

const double EPSILON = 1e-9;

struct Job {
    int id;
    double p;
    double d;
};

struct JobDueProcessTimeComparator {
    bool operator()(const Job& a, const Job& b) const {
        if (a.d != b.d) {
            return a.d < b.d;
        }
        return a.p < b.p;
    }
};

struct OptimalDecision {
    int k_prime_id = -1;
    std::vector<int> jobs_before_k_prime;
    std::vector<int> jobs_after_k_prime;
};

struct DPCacheKey {
    std::vector<int> job_ids;
    double start_time;

    bool operator<(const DPCacheKey& other) const {
        if (job_ids != other.job_ids) {
            return job_ids < other.job_ids;
        }
        if (std::fabs(start_time - other.start_time) < EPSILON) {
            return false;
        }
        return start_time < other.start_time;
    }

    bool operator==(const DPCacheKey& other) const {
        return job_ids == other.job_ids && std::fabs(start_time - other.start_time) < EPSILON;
    }
};


class TotalTardinessSolver {
public:
    TotalTardinessSolver(const std::vector<Job>& jobs, double initial_t = 0.0);

    double solve();
    std::vector<int> get_optimal_sequence();

    // Debugging methods
    void set_debug_mode(bool enable);
    void set_debug_depth(int depth);

private:
    std::vector<Job> original_jobs;
    std::map<int, Job> id_to_job;
    double initial_scheduling_time;

    std::map<DPCacheKey, double> memo;
    std::map<DPCacheKey, OptimalDecision> path_memo;

    // Debugging members
    bool debug_enabled = false;
    int max_debug_depth = 10; // Default max depth for printing
    mutable int current_debug_depth = 0; // mutable because it's modified in const functions (e.g., in print_indent)

    // Helper functions
    std::string get_indent() const;
    std::string job_ids_to_string(const std::vector<int>& ids) const;

    double calculate_V(std::vector<int> current_job_ids, double t);
    int find_max_processing_time_job_id(const std::vector<int>& job_ids) const;
    void reconstruct_sequence(std::vector<int> current_job_ids, double t, std::vector<int>& result_sequence);
};

void print_jobs(const std::vector<Job>& jobs);

#endif // TOTAL_TARDINESS_H