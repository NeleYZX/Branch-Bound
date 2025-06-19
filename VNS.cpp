//#include <iostream>
//#include <vector>
//#include <numeric>
//#include <algorithm>
//#include <map>
//#include <set>
//#include <random>
//#include <chrono>
//#include <limits>
//#include <unordered_map>
//#include <unordered_set>
//#include <string>
//#include <queue>
//#include <iomanip> // 用于格式化输出 std::setprecision
//#include "InstanceData.h"
//#include "BranchBound.h"
//
//std::pair<BatchMap, double> generateInitialSolution(
//    const std::vector<int>& parts,
//    const std::vector<double>& L,
//    const std::vector<double>& W,
//    const std::vector<double>& l,
//    const std::vector<double>& w,
//    const std::vector<double>& v,
//    const std::vector<double>& h,
//    const std::vector<double>& D,
//    const std::vector<double>& ST,
//    const std::vector<double>& VT,
//    const std::vector<double>& UT
//) {
//    auto computeTardiness = [&](const BatchMap& batches) -> double {
//        std::vector<double> completion(parts.size(), 0.0);
//        double time_cursor = 0.0;
//        for (const auto& batch : batches) {
//            double vol = 0.0, mh = 0.0;
//            for (int p : batch.second) {
//                vol += v[p];
//                mh = std::max(mh, h[p]);
//            }
//            double PT = ST[0] + VT[0] * vol + UT[0] * mh;
//            for (int p : batch.second) {
//                completion[p] = time_cursor + PT;
//            }
//            time_cursor += PT;
//        }
//
//        double total = 0.0;
//        for (int p : parts) {
//            total += std::max(0.0, completion[p] - D[p]);
//        }
//        return total;
//        };
//
//    // Step 1: 初始解（贪心法）
//    std::vector<int> sorted_parts = parts;
//    std::sort(sorted_parts.begin(), sorted_parts.end(), [&D](int a, int b) { return D[a] < D[b]; });
//
//    double machine_area = L[0] * W[0];
//    BatchMap current;
//    std::vector<int> current_batch;
//    double current_area = 0;
//    int batch_id = 0;
//
//    for (int p : sorted_parts) {
//        double area = l[p] * w[p];
//        if (current_area + area <= machine_area) {
//            current_batch.push_back(p);
//            current_area += area;
//        }
//        else {
//            current[batch_id++] = std::set<int>(current_batch.begin(), current_batch.end());
//            current_batch = { p };
//            current_area = area;
//        }
//    }
//    if (!current_batch.empty()) {
//        current[batch_id] = std::set<int>(current_batch.begin(), current_batch.end());
//    }
//
//    double best_cost = computeTardiness(current);
//    BatchMap best_solution = current;
//
//    // Step 2: 变邻域搜索（基本结构）
//    const int MAX_ITER = 100;
//    for (int iter = 0; iter < MAX_ITER; ++iter) {
//        BatchMap neighbor = best_solution;
//
//        // 定义邻域操作1：随机从两个批次交换一个零件
//        auto it1 = neighbor.begin();
//        auto it2 = neighbor.begin();
//        std::advance(it1, rand() % neighbor.size());
//        std::advance(it2, rand() % neighbor.size());
//
//        if (it1 != it2 && !it1->second.empty() && !it2->second.empty()) {
//            int idx1 = rand() % it1->second.size();
//            int idx2 = rand() % it2->second.size();
//
//            auto it1_elem = it1->second.begin();
//            std::advance(it1_elem, idx1);
//            int part1 = *it1_elem;
//
//            auto it2_elem = it2->second.begin();
//            std::advance(it2_elem, idx2);
//            int part2 = *it2_elem;
//
//            // 检查是否可交换（满足面积约束）
//            double area1 = l[part1] * w[part1];
//            double area2 = l[part2] * w[part2];
//
//            double total_area1 = 0, total_area2 = 0;
//            for (int p : it1->second) total_area1 += l[p] * w[p];
//            for (int p : it2->second) total_area2 += l[p] * w[p];
//
//            total_area1 = total_area1 - area1 + area2;
//            total_area2 = total_area2 - area2 + area1;
//
//            if (total_area1 <= machine_area && total_area2 <= machine_area) {
//                it1->second.erase(part1);
//                it2->second.erase(part2);
//                it1->second.insert(part2);
//                it2->second.insert(part1);
//            }
//        }
//
//        double new_cost = computeTardiness(neighbor);
//        if (new_cost < best_cost) {
//            best_cost = new_cost;
//            best_solution = neighbor;
//        }
//    }
//
//    return std::make_pair(best_solution, best_cost);
//}
//
//std::pair<BatchMap, double> generateInitialSolution(
//    const std::vector<int>& parts,
//    const std::vector<double>& L,
//    const std::vector<double>& W,
//    const std::vector<double>& l,
//    const std::vector<double>& w,
//    const std::vector<double>& v,
//    const std::vector<double>& h,
//    const std::vector<double>& D,
//    const std::vector<double>& ST,
//    const std::vector<double>& VT,
//    const std::vector<double>& UT
//);
//
//int main() {
//    std::srand(static_cast<unsigned>(std::time(nullptr))); // 初始化随机数种子
//
//    // 读取数据
//    MachineInfo machine;
//    std::vector<PartInfo> parts_info;
//    PartLists part_lists;
//    std::string filename = "Instance/11part.txt";
//
//    if (!readMachineAndParts(filename, machine, parts_info, part_lists)) {
//        std::cerr << "读取数据失败！" << std::endl;
//        return 1;
//    }
//
//    // 构造 parts 索引向量
//    std::vector<int> parts;
//    for (std::size_t i = 0; i < parts_info.size(); ++i) {
//        parts.push_back(static_cast<int>(i));
//    }
//
//    // 构造截止时间向量 D（这里简单设为体积的线性函数，也可自定义）
//    //std::vector<double> D = { 8, 22, 31, 29, 28 }; // 5part
//    //std::vector<double> D = { 9, 23, 32, 30, 29, 7, 13, 8, 20, 29 }; // 10part 0.796s
//    std::vector<double> D = { 13, 9, 21, 12, 36, 33, 35, 29, 18, 11, 36 }; // 11part 3.612s
//        //std::vector<double> due_dates = { 13, 9, 21, 12, 36, 33, 35, 29, 18, 11, 36, 6 }; // 12part 15.346s
//        //std::vector<double> due_dates = { 13, 9, 21, 12, 36, 33, 35, 29, 18, 11, 36, 6, 29 }; // 13part 133.6s
//        //std::vector<double> due_dates = { 14, 42, 10, 22, 13, 37, 34, 36, 30, 19, 12, 37, 7, 30 }; // 14part
//    //std::vector<double> due_dates = { 15, 43, 11, 23, 14, 38, 35, 37, 48, 31, 20, 13, 38, 8, 31 }; // 15part
//
//    // 构造参数向量（此处仅简单示例，实际可从机器信息派生或另设）
//    std::vector<double> ST = { machine.setup_time };
//    std::vector<double> VT = { machine.scanning_speed };
//    std::vector<double> UT = { machine.recoater_speed };
//    std::vector<double> L = { machine.length };
//    std::vector<double> W = { machine.width };
//
//    // 调用变邻域搜索函数
//    auto [batches, total_tardiness] = generateInitialSolution(
//        parts,
//        L,
//        W,
//        part_lists.lengths,
//        part_lists.widths,
//        part_lists.volumes,
//        part_lists.heights,
//        D,
//        ST,
//        VT,
//        UT
//    );
//
//    // 输出结果
//    std::cout << "最小总延迟（Total Tardiness）: " << total_tardiness << "\n";
//    std::cout << "批次数量: " << batches.size() << "\n";
//    for (const auto& [batch_id, batch_parts] : batches) {
//        std::cout << "批次 " << batch_id << ": ";
//        for (int p : batch_parts) {
//            std::cout << parts_info[p].id << " ";
//        }
//        std::cout << "\n";
//    }
//
//    return 0;
//}
