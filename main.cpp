#include <iostream>
#include <iomanip>
#include <ctime>
#include <fstream>
#include <locale>
#include <codecvt>
#include <numeric>
#include <chrono>
#include <vector>
#include <algorithm>
#include "InstanceData.h"
#include "BranchBound.h"
#include "DataRecord.h"
#include "DynamicProgramming.h"
#include <thread> //用于等待


//-------------含初始解的调用-------------------------

#ifdef _WIN32
#include <windows.h>
static void stabilize_process() {
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    // 固定到前 12 个逻辑 CPU（通常是 P-core 超线程）
    DWORD_PTR mask = 0xFFF;
    SetProcessAffinityMask(GetCurrentProcess(), mask);
    SetThreadAffinityMask(GetCurrentThread(), 0x1); // 也可固定到单核，更稳
}
#endif

void run_single_instance(const std::string& filename) {
#ifdef _WIN32
    stabilize_process();
#endif
    std::clock_t start_time = std::clock();
    MachineInfo machine;
    std::vector<PartInfo> parts;
    PartLists part_lists;

    std::string log_filename = get_log_filename(filename);

    // 确保流的状态是清空的，防止上一次运行的标志位影响本次
    log_stream.clear();
    log_stream.open(log_filename, std::ios::out | std::ios::binary);

    // 打印正在运行的文件名到控制台，方便监视进度
    std::cout << ">> Reading instance: " << filename << std::endl;

    if (!log_stream.is_open()) {
        std::cerr << "Can't not open the log file:" << log_filename << std::endl;
        return ; // 原为 return 1; 改为 return; 以退出当前函数但不退出程序
    }
    write_utf8_bom(log_stream);
    if (!readMachineAndParts(filename, machine, parts, part_lists)) {
        log_and_cout("fail to read the instance\n");
        return ; // 原为 return 1; 改为 return;
    }


    // 构建零件索引
    std::vector<int> part_indices;
    for (size_t i = 0; i < parts.size(); ++i) {
        part_indices.push_back(static_cast<int>(i));
    }

    // 将读取到的数据赋值给本地变量，替换原本的手动输入
    std::vector<double> due_dates = part_lists.due_dates;

    // 机器尺寸
    std::vector<double> L = { machine.length };
    std::vector<double> W = { machine.width };

    // 时间参数
    std::vector<double> ST = { machine.setup_time };
    std::vector<double> VT = { machine.scanning_speed }; // 体积系数，请替换为实际值
    std::vector<double> UT = { machine.recoater_speed }; // 高度系数，请替换为实际值

    // 输出机器信息
    log_and_cout("Machine Info:\n");
    log_and_cout("  ID: " + std::to_string(machine.id) + ", Num: " + std::to_string(machine.num) + "\n");
    log_and_cout("  Scanning speed: " + std::to_string(machine.scanning_speed) + "\n");
    log_and_cout("  Recoater speed: " + std::to_string(machine.recoater_speed) + "\n");
    log_and_cout("  Setup time: " + std::to_string(machine.setup_time) + "\n");
    log_and_cout("  Size: " + std::to_string(machine.length) + " x "
        + std::to_string(machine.width) + " x "
        + std::to_string(machine.height) + "\n\n");

    log_and_cout("Parts Summary:\n");
    for (size_t i = 0; i < part_lists.volumes.size(); ++i) {
        log_and_cout("  Part " + std::to_string(i) + ": "
            + std::to_string(part_lists.lengths[i]) + " x "
            + std::to_string(part_lists.widths[i]) + " x "
            + std::to_string(part_lists.heights[i]) + ", Vol = "
            + std::to_string(part_lists.volumes[i]) + ", Support = "
            + std::to_string(part_lists.supports[i]) + "\n");
    }



    // 调用初始解生成函数
    std::pair<BatchMap, double> result = generateInitialSolution(
        part_indices, L, W,
        part_lists.lengths,
        part_lists.widths,
        part_lists.volumes,
        part_lists.heights,
        due_dates,
        ST, VT, UT
    );

    log_and_cout("\nThe total tardiness:" + std::to_string(result.second) + "\n");

    // 构造初始空解（对应新版签名）
    std::unordered_map<int, std::vector<int>> init_S;
    // 若有已知不可行批次，用 vector<vector<int>> 填入
    std::vector<std::set<int>> infeasible_batches;

    double UB = result.second;
    double time_limit = 1800.0; // 限制最大搜索时间，单位：秒
    std::string output_path = "search_log.txt"; // 用于保存搜索日志

    // ====== 分支过程追踪（观察 Type I / Type II 分支）======
// 想看分支过程就保持 true；不想看就改成 false。
// 建议只对小算例（n 较小）开启，否则节点太多会刷屏（已用 max_nodes 限制上限）。
    const bool PRINT_BRANCH_TRACE = false;
    if (PRINT_BRANCH_TRACE) {
        std::cout << "\n############ 分支过程追踪: " << filename << " ############\n";
        trace_branch_and_bound(
            part_indices, due_dates, ST, VT, UT, L, W,
            part_lists.lengths, part_lists.widths,
            part_lists.heights, part_lists.volumes,
            UB,
            std::cout,      // 输出到控制台；想存到日志文件就改成 log_stream
            2000            // 最多打印多少个节点（防止刷屏，可调大/调小）
        );
        log_stream.close();
        //return;             // 只追踪、不再跑完整搜索；若想“追踪+正常搜索都做”，删掉这行 return
    }
    // ====== 追踪代码结束 ======

    log_and_cout("\n============== Start Branch and Cut Search ==============\n\n");

    // 调用 branch_and_cut（新版签名）
    std::pair<Node, Stats> bc_result = branch_and_cut(
        part_indices,
        due_dates,
        infeasible_batches,
        ST, VT, UT,
        L, W,
        part_lists.lengths,
        part_lists.widths,
        part_lists.heights,
        part_lists.volumes,
        init_S,
        UB,
        time_limit,
        output_path
    );

    export_bound_statistics(log_filename, bc_result.second); //上下界记录
    export_pruned_depth_info(log_filename, bc_result.second.pruned_nodes_per_depth);//深度剪枝节点数记录
    //export_first_level_lbs(log_filename, bc_result.second.first_level_node_lbs);//第一层节点LB记录
    // 导出新增的详细数据
    export_first_level_detailed_info(log_filename, bc_result.second.first_level_details);



    log_and_cout("\n=============== Best Solution ===============\n");
    log_and_cout(bc_result.first);
    log_and_cout("\n============= The search Info =============\n");
    log_and_cout("The handled total nodes(pop):" + std::to_string(bc_result.second.total_nodes) + "\n");
    log_and_cout("The number of generated nodes:" + std::to_string(bc_result.second.generated_nodes) + "\n");
    log_and_cout("The number of leaf nodes:" + std::to_string(bc_result.second.leaf_nodes) + "\n");
    log_and_cout("The times of updated best solution:" + std::to_string(bc_result.second.updated_solutions) + "\n");
    log_and_cout("pruned times-Area:" + std::to_string(bc_result.second.area_pruned_nodes) + "\n");
    log_and_cout("pruned times-LB:" + std::to_string(bc_result.second.LB_pruned_nodes) + "\n");
    log_and_cout("pruned times-Infeasible:" + std::to_string(bc_result.second.U_pruned_nodes) + "\n");
    log_and_cout("pruned times-Dominance:" + std::to_string(bc_result.second.dominance_pruned_nodes) + "\n");

    log_and_cout("total_V_calls:" + std::to_string(bc_result.second.total_V_calls) + "\n");
    log_and_cout("local_memo_hits :" + std::to_string(bc_result.second.local_memo_hits) + "\n");
    log_and_cout("global_memo_hits:" + std::to_string(bc_result.second.global_memo_hits) + "\n");
    log_and_cout("computed_states :" + std::to_string(bc_result.second.computed_states) + "\n");
    log_and_cout("delta_trigger_count :" + std::to_string(bc_result.second.delta_trigger_count) + "\n");
    log_and_cout("serial_pruning_count :" + std::to_string(bc_result.second.serial_pruning_count) + "\n");
    log_and_cout("serial_missing_count :" + std::to_string(bc_result.second.serial_missing_count) + "\n");


    double elapsed_seconds = double(std::clock() - start_time) / CLOCKS_PER_SEC;
    log_and_cout("time:" + std::to_string(elapsed_seconds) + "s\n");

    log_stream.close();


    return ;
}

// 简单的辅助函数：检查文件是否存在
bool file_exists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

int main() {
    // 1. 定义实验参数范围
    // 根据你的截图，规模是从 10 到 17 (如果还有更大规模，修改这里的结束值，比如 20)
    std::vector<int> instance_sizes;
    for (int n = 10; n <= 15; ++n) {
        instance_sizes.push_back(n);
    }

    // 定义 TF 和 RDD 的参数组合
    std::vector<std::string> tf_values = { "0.6" };
    std::vector<std::string> rdd_values = { "0.6"  };

    // 定义算例文件所在的文件夹路径
    // 注意：如果文件在当前目录，留空 ""；如果在 GeneratedResults 文件夹，填 "GeneratedResults/"
    // 根据你之前的代码，我假设在 "GeneratedResults/"，如果找不到文件，请改为空字符串 ""
    std::string folder_path = "GeneratedResults/";

    int total_runs_per_instance = 1; // 每个算例运行 1 次

    // 2. 三层嵌套循环生成所有文件名
    for (int size : instance_sizes) {
        for (const auto& tf : tf_values) {
            for (const auto& rdd : rdd_values) {

                // 3. 动态构建文件名
                // 格式：10part_TF=0.3_RDD=0.6.txt
                std::string filename_base = std::to_string(size) + "part_TF=" + tf + "_RDD=" + rdd + ".txt";
                std::string full_path = folder_path + filename_base;

                // 4. 检查文件是否存在（防止程序因为找不到某个文件而崩溃）
                if (!file_exists(full_path)) {
                    // 如果在指定文件夹找不到，尝试在当前目录找一下（容错）
                    if (file_exists(filename_base)) {
                        full_path = filename_base;
                    }
                    else {
                        std::cout << "[Skip] File not found: " << full_path << std::endl;
                        continue; // 跳过当前循环，继续下一个
                    }
                }

                std::cout << "\n##################################################\n";
                std::cout << "Starting Batch for File: " << full_path << "\n";
                std::cout << "##################################################\n";

                // 5. 执行算例
                for (int i = 0; i < total_runs_per_instance; ++i) {
                    std::cout << "\n----------------------------------------\n";
                    std::cout << "File: " << filename_base << " | Run " << (i + 1) << "\n";
                    std::cout << "----------------------------------------\n";

                    run_single_instance(full_path);

                    // 运行间隙稍作休息，防止CPU过热或IO冲突
                    if (i < total_runs_per_instance - 1) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    }
                }

                std::cout << ">>> Finished: " << filename_base << "\n" << std::endl;
                // 文件切换间隙
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    std::cout << "\nAll instances finished.\n";
    // 等待用户按键退出，防止窗口直接关闭
    std::cout << "Press Enter to exit...";
    std::cin.get();

    return 0;
}



//
//======================================DP独立运行的main函数=======================================
void print_jobs(const std::vector<Job>& jobs) {
    std::cout << "作业列表:\n";
    std::cout << "ID\tP\tD\n";
    for (const auto& job : jobs) {
        std::cout << job.id << "\t" << job.p << "\t" << job.d << "\n";
    }
    std::cout << std::endl;
}

// Helper function to print an optimal sequence
void print_optimal_sequence(const std::vector<int>& sequence) {
    std::cout << "最优序列: ";
    for (size_t i = 0; i < sequence.size(); ++i) {
        std::cout << sequence[i];
        if (i < sequence.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << std::endl;
}

//int main() {
//    // 记录程序开始时间
//    auto start_time = std::chrono::high_resolution_clock::now();
//    // 示例数据，来自文献 Example 3.4.5
//    // 注意：这里的作业ID是1-indexed，但在all_jobs中它们将被存储为0-indexed。
//    // 所以 job 1 对应 all_jobs[0]，job 2 对应 all_jobs[1]，以此类推。
//    std::vector<Job> example_jobs = {
//        //{2, 121, 260}, // Job 1 (index 0)
//        //{1, 79,  266}, // Job 2 (index 1)
//        //{4, 147, 266}, // Job 3 (index 2)
//        //{3, 83,  336}, // Job 4 (index 3)
//        //{5, 130, 337}  // Job 5 (index 4)
//        //{1, 4.722939, 12.417969},
//        //{2, 3.225740, 8.269357},
//        //{3, 3.553233, 5.913514},
//        //{4, 1.942039, 8.127329},
//        //{1, 82.845041, 112.776038},
//        //{2, 65.375992, 55.804702},
//        //{3, 28.594138, 90.429218}
//            {1, 105, 340},
//    {2, 90, 260},
//    {3, 135, 410},
//    {4, 70, 280},
//    {5, 125, 390},
//    {6, 85, 300},
//    {7, 155, 470},
//    {8, 95, 330},
//    {9, 115, 420},
//    {10, 145, 510}
//
//
//    };
//    double initial_time = 0.0;
//
//    print_jobs(example_jobs);
//
//    std::cout << "开始计算最小总延迟...\n";
//    std::vector<int> optimal_sequence_result;
//    double min_tardiness = minimize_total_tardiness(example_jobs, initial_time, optimal_sequence_result);
//
//    std::cout << "\n计算完成。\n";
//    std::cout << "最小总延迟为: " << min_tardiness << std::endl; // 预期结果 370
//    print_optimal_sequence(optimal_sequence_result); // 打印最优序列
//
//    // 记录程序结束时间
//    auto end_time = std::chrono::high_resolution_clock::now();
//
//    // 计算运行时间
//    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
//
//    std::cout << "程序运行时间: " << duration.count() << " 微秒 ("
//        << static_cast<double>(duration.count()) / 1000000.0 << " 秒)\n";
//    return 0;
//}

//======================================枚举检验函数的main函数=========================================
//int main() {
//    // 示例作业列表
//    std::vector<Job> jobs = {
//                {1, 121, 260}, // Job 1 (index 0)
//                {2, 79,  266}, // Job 2 (index 1)
//                {3, 147.9, 266}, // Job 3 (index 2)
//                {4, 83,  336}, // Job 4 (index 3)
//                {5, 130, 337}  // Job 5 (index 4)
//    };
//
//    std::cout << "原始作业列表:" << std::endl;
//    for (const auto& job : jobs) {
//        std::cout << job.to_string() << std::endl;
//    }
//    std::cout << std::endl;
//
//    // 找到最小化总延迟时间的调度方案
//    std::vector<Job> best_schedule = find_min_lateness_schedule(jobs);
//
//    std::cout << "最小化总延迟时间的调度方案:" << std::endl;
//    for (const auto& job : best_schedule) {
//        std::cout << job.to_string() << std::endl;
//    }
//    std::cout << std::endl;
//
//    // 计算最佳方案的总延迟时间
//    double min_lateness = calculate_total_lateness(best_schedule);
//    std::cout << "最小总延迟时间: " << min_lateness << std::endl;
//
//
//    return 0;
//}


//==========================================合并检验函数========================================
//int main() {
//    // 1. 生成随机算例
//    int num_random_jobs = 3; // 注意：DP算法对于大量作业会非常慢 (指数级复杂度)
//    double min_p = 10.0, max_p = 100.0;
//    double min_d = 5.0, max_d = 150.0;
//    std::vector<Job> random_jobs = generate_random_jobs(num_random_jobs, min_p, max_p, min_d, max_d);
//
//    std::cout << "Generated " << num_random_jobs << " Random Jobs:" << std::endl;
//    for (const auto& job : random_jobs) {
//        std::cout << "  " << job.to_string() << std::endl;
//    }
//
//    std::vector<int> optimal_sequence_random_dp;
//    double min_tardiness_random_dp = minimize_total_tardiness(random_jobs,0.0, optimal_sequence_random_dp);
//
//    std::cout << "\nDynamic Programming Results for Random Jobs:" << std::endl;
//    std::cout << "  Minimum Total Tardiness: " << min_tardiness_random_dp << std::endl;
//    std::cout << "  Optimal Sequence (Job IDs): ";
//    print_optimal_sequence(optimal_sequence_random_dp);
//    std::cout << std::endl;
//
//    // 验证DP结果的序列
//    std::vector<Job> sequenced_jobs_random_dp;
//    for (int job_id : optimal_sequence_random_dp) {
//        for (const auto& job : random_jobs) {
//            if (job.id == job_id) {
//                sequenced_jobs_random_dp.push_back(job);
//                break;
//            }
//        }
//    }
//    std::cout << "  Calculated tardiness for DP sequence: " << calculate_total_lateness(sequenced_jobs_random_dp) << std::endl;
//
//    // --- 示例 3: 枚举算法 (适用于小规模问题，与DP比较) ---
//    std::cout << "\n--- Example 3: Enumeration Algorithm (for small scale, with Manual Jobs) ---" << std::endl;
//    // 枚举算法对作业数量非常敏感，通常只适用于 n <= 10 左右
//    if (random_jobs.size() <= 10) { // 限制枚举的作业数量
//        std::vector<Job> best_schedule_enum = find_min_lateness_schedule(random_jobs);
//        double min_lateness_enum = calculate_total_lateness(best_schedule_enum);
//
//        std::cout << "Enumeration Results (using random jobs):" << std::endl;
//        std::cout << "  Minimum Total Tardiness: " << min_lateness_enum << std::endl;
//        std::cout << "  Optimal Sequence (Job IDs): ";
//        for (size_t i = 0; i < best_schedule_enum.size(); ++i) {
//            std::cout << best_schedule_enum[i].id << (i == best_schedule_enum.size() - 1 ? "" : " -> ");
//        }
//        std::cout << std::endl;
//    }
//    else {
//        std::cout << "  Skipping enumeration for large number of jobs (" << random_jobs.size() << ") due to high complexity." << std::endl;
//    }
//
//
//    std::cout << "\n--- End of Program ---" << std::endl;
//
//    return 0;
//}
