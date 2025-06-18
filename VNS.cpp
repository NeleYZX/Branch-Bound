//#include <iostream>
//#include <vector>
//#include <numeric>
//#include <algorithm>
//#include <map>
//#include <set>
//#include <random>
//#include <chrono>
//#include <iomanip> // 用于格式化输出 std::setprecision
//#include "InstanceData.h"
//
//// 使用您代码中的类型别名，以保持一致
//using BatchMap = std::map<int, std::vector<int>>; // 修改为使用 vector
//
//
//// =================================================================
//// 区域 1: VNS 实现代码 (从您的问题中复制)
//// 
//// 在实际项目中, 这些函数通常会放在一个单独的 .h 和 .cpp 文件中
//// 为了演示，我们把它们都放在这里。
//// =================================================================
//
//// 1.1 辅助函数
//std::pair<double, double> calculate_batch_vol_and_mh(
//    const std::set<int>& batch,
//    const std::vector<double>& v,
//    const std::vector<double>& h)
//{
//    double vol = 0.0, mh = 0.0;
//    for (int part_id : batch) {
//        vol += v[part_id];
//        if (h[part_id] > mh) {
//            mh = h[part_id];
//        }
//    }
//    return { vol, mh };
//}
//
//double calculate_total_tardiness(
//    const BatchMap& batches,
//    const std::vector<int>& parts,
//    const std::vector<double>& D,
//    const std::vector<double>& ST,
//    const std::vector<double>& VT,
//    const std::vector<double>& UT,
//    const std::vector<double>& v,
//    const std::vector<double>& h)
//{
//    if (batches.empty()) return std::numeric_limits<double>::infinity();
//
//    std::map<int, double> completion_times;
//    double time_cursor = 0.0;
//
//    for (const auto& pair : batches) {
//        if (pair.second.empty()) continue;
//
//        auto [vol, mh] = calculate_batch_vol_and_mh(pair.second, v, h);
//        double PT = ST[0] + VT[0] * vol + UT[0] * mh;
//        double batch_completion_time = time_cursor + PT;
//
//        for (int part_id : pair.second) {
//            completion_times[part_id] = batch_completion_time;
//        }
//        time_cursor = batch_completion_time;
//    }
//
//    double total_tardiness = 0.0;
//    for (int part_id : parts) {
//        if (completion_times.count(part_id)) {
//            total_tardiness += std::max(0.0, completion_times.at(part_id) - D[part_id]);
//        }
//    }
//
//    return total_tardiness;
//}
//
//// 1.2 VNS 组件
//void local_search_reallocate(
//    BatchMap& current_solution,
//    double& current_tardiness,
//    const std::vector<int>& parts,
//    const std::vector<double>& l,
//    const std::vector<double>& w,
//    const double machine_area,
//    const std::vector<double>& D, const std::vector<double>& ST, const std::vector<double>& VT,
//    const std::vector<double>& UT, const std::vector<double>& v, const std::vector<double>& h
//) {
//    bool improved = true;
//    while (improved) {
//        improved = false;
//
//        int best_part_to_move = -1;
//        int source_batch_id = -1;
//        int target_batch_id = -1;
//        double best_found_tardiness = current_tardiness;
//
//        for (int part_id : parts) {
//            double part_area = l[part_id] * w[part_id];
//            int current_part_batch_id = -1;
//
//            for (const auto& pair : current_solution) {
//                if (pair.second.count(part_id)) {
//                    current_part_batch_id = pair.first;
//                    break;
//                }
//            }
//            if (current_part_batch_id == -1) continue;
//
//            for (const auto& target_pair : current_solution) {
//                if (target_pair.first == current_part_batch_id) continue;
//
//                double target_batch_area = 0;
//                for (int p_in_target : target_pair.second) target_batch_area += l[p_in_target] * w[p_in_target];
//                if (target_batch_area + part_area > machine_area) continue;
//
//                BatchMap candidate_solution = current_solution;
//                candidate_solution.at(current_part_batch_id).erase(part_id);
//                candidate_solution.at(target_pair.first).insert(part_id);
//
//                double candidate_tardiness = calculate_total_tardiness(candidate_solution, parts, D, ST, VT, UT, v, h);
//
//                if (candidate_tardiness < best_found_tardiness) {
//                    best_found_tardiness = candidate_tardiness;
//                    best_part_to_move = part_id;
//                    source_batch_id = current_part_batch_id;
//                    target_batch_id = target_pair.first;
//                }
//            }
//        }
//
//        if (best_part_to_move != -1) {
//            current_solution.at(source_batch_id).erase(best_part_to_move);
//            current_solution.at(target_batch_id).insert(best_part_to_move);
//            if (current_solution.at(source_batch_id).empty()) {
//                current_solution.erase(source_batch_id);
//            }
//            current_tardiness = best_found_tardiness;
//            improved = true;
//        }
//    }
//}
//
//void shake_swap_parts(
//    BatchMap& solution,
//    int k,
//    const std::vector<double>& l,
//    const std::vector<double>& w,
//    const double machine_area,
//    std::mt19937& rng
//) {
//    if (solution.size() < 2) return;
//
//    for (int i = 0; i < k; ++i) {
//        std::vector<int> batch_keys;
//        for (const auto& pair : solution) batch_keys.push_back(pair.first);
//
//        std::shuffle(batch_keys.begin(), batch_keys.end(), rng);
//        if (batch_keys.size() < 2) continue;
//
//        int b_idx1 = batch_keys[0];
//        int b_idx2 = batch_keys[1];
//
//        if (solution.at(b_idx1).empty() || solution.at(b_idx2).empty()) continue;
//
//        std::uniform_int_distribution<size_t> dist_p1(0, solution.at(b_idx1).size() - 1);
//        std::uniform_int_distribution<size_t> dist_p2(0, solution.at(b_idx2).size() - 1);
//
//        auto part_it1 = solution.at(b_idx1).begin();
//        std::advance(part_it1, dist_p1(rng));
//        int part1 = *part_it1;
//
//        auto part_it2 = solution.at(b_idx2).begin();
//        std::advance(part_it2, dist_p2(rng));
//        int part2 = *part_it2;
//
//        double area1_old = 0; for (int p : solution.at(b_idx1)) area1_old += l[p] * w[p];
//        double area2_old = 0; for (int p : solution.at(b_idx2)) area2_old += l[p] * w[p];
//
//        double area1_new = area1_old - (l[part1] * w[part1]) + (l[part2] * w[part2]);
//        double area2_new = area2_old - (l[part2] * w[part2]) + (l[part1] * w[part1]);
//
//        if (area1_new <= machine_area && area2_new <= machine_area) {
//            solution.at(b_idx1).erase(part1);
//            solution.at(b_idx2).erase(part2);
//            solution.at(b_idx1).insert(part2);
//            solution.at(b_idx2).insert(part1);
//        }
//    }
//}
//
//
//// 1.3 VNS 主函数
//std::pair<BatchMap, double> generateInitialSolution(
//    const std::vector<int>& parts,
//    const std::vector<double>& L, const std::vector<double>& W,
//    const std::vector<double>& l, const std::vector<double>& w,
//    const std::vector<double>& v, const std::vector<double>& h,
//    const std::vector<double>& D, const std::vector<double>& ST,
//    const std::vector<double>& VT, const std::vector<double>& UT,
//    int max_iterations = 100, int k_max = 2)
//{
//    // === 步骤 0: 初始解生成 (贪婪算法) ===
//    std::vector<int> sorted_parts = parts;
//    std::sort(sorted_parts.begin(), sorted_parts.end(),
//        [&D](int a, int b) { return D[a] < D[b]; });
//
//    double machine_area = L[0] * W[0];
//    BatchMap best_solution; // 使用 vector<int> 替代 set<int>
//    std::vector<int> current_batch; // 使用 vector
//    double current_batch_area = 0.0;
//    int batch_id = 0;
//
//    for (int p : sorted_parts) {
//        double area = l[p] * w[p];
//        if (current_batch_area + area <= machine_area && !current_batch.empty()) {
//            current_batch.push_back(p); // 使用 push_back
//            current_batch_area += area;
//        }
//        else {
//            if (!current_batch.empty()) {
//                best_solution[batch_id++] = current_batch;
//            }
//            current_batch.clear();
//            current_batch.push_back(p); // 使用 push_back
//            current_batch_area = area;
//        }
//    }
//    if (!current_batch.empty()) {
//        best_solution[batch_id] = current_batch;
//    }
//
//    double best_tardiness = calculate_total_tardiness(best_solution, parts, D, ST, VT, UT, v, h);
//    std::cout << "Initial Greedy Solution Tardiness: " << best_tardiness << std::endl;
//
//    // === VNS 主循环 ===
//    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
//    int iter = 0;
//    while (iter < max_iterations) {
//        int k = 1;
//        while (k <= k_max) {
//            BatchMap current_solution = best_solution;
//            shake_swap_parts(current_solution, k, l, w, machine_area, rng);
//
//            double current_tardiness = calculate_total_tardiness(current_solution, parts, D, ST, VT, UT, v, h);
//            local_search_reallocate(current_solution, current_tardiness, parts, l, w, machine_area, D, ST, VT, UT, v, h);
//
//            if (current_tardiness < best_tardiness) {
//                best_solution = current_solution;
//                best_tardiness = current_tardiness;
//                k = 1;
//            }
//            else {
//                k++;
//            }
//        }
//        iter++;
//    }
//
//    return std::make_pair(best_solution, best_tardiness);
//}
//
//
//// =================================================================
//// 区域 2: 调用代码 (Main Function)
//// =================================================================
//int main() {
//    // --- 1. 从文件读取问题实例 ---
//    MachineInfo machine;
//    std::vector<PartInfo> parts_info; // 存储每个零件对象的详细信息
//    PartLists part_lists;             // 按属性存储所有零件的列表
//
//    std::string filename = "Instance_10parts.txt";
//    if (!readMachineAndParts(filename, machine, parts_info, part_lists)) {
//        std::cerr << "Fatal Error: Failed to read instance file: " << filename << std::endl;
//        return 1;
//    }
//
//    // --- 2. 准备算法所需的参数 ---
//
//    // 构建零件索引 (0, 1, 2, ..., N-1)
//    const int NUM_PARTS = parts_info.size();
//    std::vector<int> part_indices(NUM_PARTS);
//    std::iota(part_indices.begin(), part_indices.end(), 0);
//
//    // 机器参数 (从读取的数据中提取)
//    std::vector<double> L = { machine.length };
//    std::vector<double> W = { machine.width };
//
//    // 批次处理时间参数 (从读取的数据中提取)
//    std::vector<double> ST = { machine.setup_time };
//    std::vector<double> VT = { machine.scanning_speed };
//    std::vector<double> UT = { machine.recoater_speed };
//
//    // 零件几何参数 (直接使用 part_lists)
//    // 注意：变量名在 generateInitialSolution 调用中被映射
//    // l -> part_lists.lengths
//    // w -> part_lists.widths
//    // h -> part_lists.heights
//    // v -> part_lists.volumes
//
//    // 交付日期 (D) - 此数据不在文件中，在此处硬编码
//    std::vector<double> D = { 50, 80, 100, 65, 120, 150, 180, 40, 200, 95 };
//
//    if (D.size() != NUM_PARTS) {
//        std::cerr << "Warning: Number of parts in file (" << NUM_PARTS
//            << ") does not match number of due dates provided (" << D.size() << ")." << std::endl;
//    }
//
//    // --- 3. 调用 VNS 函数 ---
//    std::cout << "--- Running VNS to find a high-quality initial solution ---" << std::endl;
//    std::cout << "Instance loaded from: " << filename << " (" << NUM_PARTS << " parts)" << std::endl;
//
//    // 启动计时器
//    auto start_time = std::chrono::high_resolution_clock::now();
//
//    // 调用核心函数
//    // 使用从文件加载的数据 (part_lists) 和手动定义的交付日期 (D)
//    std::pair<std::map<int, std::vector<int>>, double> result = generateInitialSolution(
//        part_indices,
//        L, W,
//        part_lists.lengths, part_lists.widths, part_lists.volumes, part_lists.heights,
//        D,
//        ST, VT, UT,200,3    );
//
//    // 结束计时器
//    auto end_time = std::chrono::high_resolution_clock::now();
//    std::chrono::duration<double, std::milli> elapsed_ms = end_time - start_time;
//
//    // --- 4. 打印结果 ---
//    auto final_batches = result.first; // 现在是 std::map<int, std::vector<int>>
//    double final_tardiness = result.second;
//
//    std::cout << "\n--- VNS Result ---" << std::endl;
//    std::cout << std::fixed << std::setprecision(2);
//    std::cout << "Execution Time: " << elapsed_ms.count() << " ms" << std::endl;
//    std::cout << "Optimized Total Tardiness: " << final_tardiness << std::endl;
//    std::cout << "Batch Structure (" << final_batches.size() << " batches):" << std::endl;
//
//    for (const auto& pair : final_batches) {
//        std::cout << "  Batch " << pair.first << ": { ";
//        for (size_t i = 0; i < pair.second.size(); ++i) {
//            std::cout << pair.second[i] << (i < pair.second.size() - 1 ? ", " : "");
//        }
//        std::cout << " }" << std::endl;
//    }
//
//    return 0;
//}
