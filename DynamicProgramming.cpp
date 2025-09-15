#include "DynamicProgramming.h"
#include <numeric>
#include <iostream>
#include <limits> 
#include <sstream>



// **确保这里的签名是 double initial_t**
TotalTardinessSolver::TotalTardinessSolver(const std::vector<Job>& jobs, double initial_t)
    : original_jobs(jobs), initial_scheduling_time(initial_t) {
    for (const auto& job : jobs) {
        id_to_job[job.id] = job;
    }
}

// Debugging methods implementation
void TotalTardinessSolver::set_debug_mode(bool enable) {
    debug_enabled = enable;
}

void TotalTardinessSolver::set_debug_depth(int depth) {
    max_debug_depth = depth;
}

std::string TotalTardinessSolver::get_indent() const {
    if (current_debug_depth > max_debug_depth) return "";
    return std::string(current_debug_depth * 2, ' ');
}

std::string TotalTardinessSolver::job_ids_to_string(const std::vector<int>& ids) const {
    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < ids.size(); ++i) {
        oss << ids[i];
        if (i < ids.size() - 1) {
            oss << ", ";
        }
    }
    oss << "}";
    return oss.str();
}


// **确保这里的签名是 const**
int TotalTardinessSolver::find_max_processing_time_job_id(const std::vector<int>& job_ids) const {
    if (job_ids.empty()) {
        return -1;
    }

    double max_p = -1.0;
    int max_p_job_id = -1;

    for (int job_id : job_ids) {
        if (id_to_job.at(job_id).p > max_p) {
            max_p = id_to_job.at(job_id).p;
            max_p_job_id = job_id;
        }
    }
    return max_p_job_id;
}

// **确保这里的参数 t 是 double**
double TotalTardinessSolver::calculate_V(std::vector<int> current_job_ids, double t) {
    std::sort(current_job_ids.begin(), current_job_ids.end());

    DPCacheKey key = { current_job_ids, t };

    // Increment depth for entry
    current_debug_depth++;

    if (debug_enabled && current_debug_depth <= max_debug_depth) {
        std::cout << get_indent() << "Call calculate_V(Jobs: " << job_ids_to_string(current_job_ids) << ", t: " << t << ")\n";
    }

    if (memo.count(key)) {
        double cached_value = memo[key];
        if (debug_enabled && current_debug_depth <= max_debug_depth) {
            std::cout << get_indent() << "Cache Hit: " << cached_value << "\n";
        }
        current_debug_depth--; // Decrement depth for exit
        return cached_value;
    }

    // Base cases
    if (current_job_ids.empty()) {
        if (debug_enabled && current_debug_depth <= max_debug_depth) {
            std::cout << get_indent() << "Base Case (empty set): 0.0\n";
        }
        current_debug_depth--; // Decrement depth for exit
        return memo[key] = 0.0;
    }
    if (current_job_ids.size() == 1) {
        int job_id = current_job_ids[0];
        const Job& job = id_to_job.at(job_id);
        double tardiness = std::max(0.0, t + job.p - job.d);
        if (debug_enabled && current_debug_depth <= max_debug_depth) {
            std::cout << get_indent() << "Base Case (single job " << job_id << "): " << tardiness << "\n";
        }
        current_debug_depth--; // Decrement depth for exit
        return memo[key] = tardiness;
    }

    double min_total_tardiness = std::numeric_limits<double>::max();
    OptimalDecision best_decision;

    int k_prime_id = find_max_processing_time_job_id(current_job_ids);
    const Job& k_prime_job = id_to_job.at(k_prime_id);

    std::vector<int> jobs_without_k_prime;
    for (int job_id : current_job_ids) {
        if (job_id != k_prime_id) {
            jobs_without_k_prime.push_back(job_id);
        }
    }

    if (debug_enabled && current_debug_depth <= max_debug_depth) {
        std::cout << get_indent() << "  Chosen k' (max_p_job): " << k_prime_id << " (p=" << k_prime_job.p << ", d=" << k_prime_job.d << ")\n";
    }

    size_t num_other_jobs = jobs_without_k_prime.size();

    for (size_t i = 0; i < (1ULL << num_other_jobs); ++i) {
        std::vector<int> jobs_before_k_prime;
        std::vector<int> jobs_after_k_prime;

        double current_p_sum_before_k_prime = 0.0;

        for (size_t j = 0; j < num_other_jobs; ++j) {
            if ((i >> j) & 1) {
                jobs_before_k_prime.push_back(jobs_without_k_prime[j]);
                current_p_sum_before_k_prime += id_to_job.at(jobs_without_k_prime[j]).p;
            }
            else {
                jobs_after_k_prime.push_back(jobs_without_k_prime[j]);
            }
        }

        if (debug_enabled && current_debug_depth <= max_debug_depth) {
            std::cout << get_indent() << "  Partition: (Before " << k_prime_id << ": " << job_ids_to_string(jobs_before_k_prime)
                << ", After " << k_prime_id << ": " << job_ids_to_string(jobs_after_k_prime) << ")\n";
        }

        double completion_time_k_prime_at_delta = t + current_p_sum_before_k_prime + k_prime_job.p;

        double tardiness_k_prime = std::max(0.0, completion_time_k_prime_at_delta - k_prime_job.d);

        double tardiness_before = calculate_V(jobs_before_k_prime, t);
        double tardiness_after = calculate_V(jobs_after_k_prime, completion_time_k_prime_at_delta);

        double current_total_tardiness = tardiness_before + tardiness_k_prime + tardiness_after;

        if (debug_enabled && current_debug_depth <= max_debug_depth) {
            std::cout << get_indent() << "    Sub-results: Before=" << tardiness_before
                << ", k'(" << k_prime_id << ")=" << tardiness_k_prime
                << ", After=" << tardiness_after << " -> Total=" << current_total_tardiness << "\n";
        }

        if (current_total_tardiness < min_total_tardiness) {
            min_total_tardiness = current_total_tardiness;
            best_decision.k_prime_id = k_prime_id;
            best_decision.jobs_before_k_prime = jobs_before_k_prime;
            best_decision.jobs_after_k_prime = jobs_after_k_prime;
            if (debug_enabled && current_debug_depth <= max_debug_depth) {
                std::cout << get_indent() << "    New best tardiness for this state: " << min_total_tardiness << "\n";
            }
        }
    }
    path_memo[key] = best_decision;

    if (debug_enabled && current_debug_depth <= max_debug_depth) {
        std::cout << get_indent() << "Return calculate_V(Jobs: " << job_ids_to_string(current_job_ids)
            << ", t: " << t << "): " << min_total_tardiness << "\n";
    }

    current_debug_depth--; // Decrement depth for exit
    return memo[key] = min_total_tardiness;
}

// **确保这里的返回类型是 double 且没有参数**
double TotalTardinessSolver::solve() {
    std::vector<int> all_job_ids;
    for (const auto& job : original_jobs) {
        all_job_ids.push_back(job.id);
    }
    return calculate_V(all_job_ids, initial_scheduling_time);
}

// **确保这里的参数 t 是 double**
void TotalTardinessSolver::reconstruct_sequence(std::vector<int> current_job_ids, double t, std::vector<int>& result_sequence) {
    std::sort(current_job_ids.begin(), current_job_ids.end());
    DPCacheKey key = { current_job_ids, t };

    if (current_job_ids.empty()) {
        return;
    }
    if (current_job_ids.size() == 1) {
        result_sequence.push_back(current_job_ids[0]);
        return;
    }

    OptimalDecision decision = path_memo.at(key);

    reconstruct_sequence(decision.jobs_before_k_prime, t, result_sequence);

    result_sequence.push_back(decision.k_prime_id);

    double p_sum_before_k_prime = 0.0;
    for (int job_id : decision.jobs_before_k_prime) {
        p_sum_before_k_prime += id_to_job.at(job_id).p;
    }
    double completion_time_k_prime = t + p_sum_before_k_prime + id_to_job.at(decision.k_prime_id).p;

    reconstruct_sequence(decision.jobs_after_k_prime, completion_time_k_prime, result_sequence);
}

// **确保这里没有参数**
std::vector<int> TotalTardinessSolver::get_optimal_sequence() {
    std::vector<int> all_job_ids;
    for (const auto& job : original_jobs) {
        all_job_ids.push_back(job.id);
    }
    std::vector<int> optimal_sequence;
    reconstruct_sequence(all_job_ids, initial_scheduling_time, optimal_sequence);
    return optimal_sequence;
}

void print_jobs(const std::vector<Job>& jobs) {
    std::cout << "Jobs: (id, p, d)\n";
    for (const auto& job : jobs) {
        std::cout << "(" << job.id << ", " << job.p << ", " << job.d << ")\n";
    }
}