#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <map>
#include <set>
#include <random>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <queue>
#include <iomanip> // 用于格式化输出 std::setprecision
#include "InstanceData.h"
#include "BranchBound.h"

std::pair<BatchMap, double> generateInitialSolution(
    const std::vector<int>& parts,
    const std::vector<double>& L,
    const std::vector<double>& W,
    const std::vector<double>& l,
    const std::vector<double>& w,
    const std::vector<double>& v,
    const std::vector<double>& h,
    const std::vector<double>& D,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT
) {
    using Chromosome = std::vector<std::vector<int>>; // 每个染色体是多个批次（每个批次是零件编号）
    const int POP_SIZE = 30;
    const int NUM_GENERATIONS = 100;
    const double MUTATION_RATE = 0.1;

    double machine_area = L[0] * W[0];

    // --- 评估函数：计算总延迟 ---
    auto evaluate = [&](const Chromosome& chrom) -> double {
        std::vector<double> completion(parts.size(), 0.0);
        double time_cursor = 0.0;
        for (const auto& batch : chrom) {
            double vol = 0.0, mh = 0.0;
            for (int p : batch) {
                vol += v[p];
                mh = std::max(mh, h[p]);
            }
            double PT = ST[0] + VT[0] * vol + UT[0] * mh;
            for (int p : batch) {
                completion[p] = time_cursor + PT;
            }
            time_cursor += PT;
        }
        double total = 0.0;
        for (int p : parts) {
            total += std::max(0.0, completion[p] - D[p]);
        }
        return total;
        };

    // --- 初始化种群 ---
    std::vector<Chromosome> population;
    for (int i = 0; i < POP_SIZE; ++i) {
        std::vector<int> shuffled_parts = parts;
        std::random_shuffle(shuffled_parts.begin(), shuffled_parts.end());
        Chromosome chrom;
        std::vector<int> current_batch;
        double current_area = 0.0;

        for (int p : shuffled_parts) {
            double area = l[p] * w[p];
            if (current_area + area <= machine_area) {
                current_batch.push_back(p);
                current_area += area;
            }
            else {
                chrom.push_back(current_batch);
                current_batch = { p };
                current_area = area;
            }
        }
        if (!current_batch.empty()) {
            chrom.push_back(current_batch);
        }
        population.push_back(chrom);
    }

    // --- 遗传算法主循环 ---
    Chromosome best_solution;
    double best_cost = std::numeric_limits<double>::max();

    for (int gen = 0; gen < NUM_GENERATIONS; ++gen) {
        // 评估每个个体
        std::vector<std::pair<double, Chromosome>> scored;
        for (auto& chrom : population) {
            double cost = evaluate(chrom);
            scored.push_back({ cost, chrom });
            if (cost < best_cost) {
                best_cost = cost;
                best_solution = chrom;
            }
        }

        // 选择前50%进入下一代
        std::sort(scored.begin(), scored.end());
        std::vector<Chromosome> new_population;
        for (int i = 0; i < POP_SIZE / 2; ++i) {
            new_population.push_back(scored[i].second);
        }

        // 交叉生成新个体
        while (new_population.size() < POP_SIZE) {
            int p1 = rand() % (POP_SIZE / 2);
            int p2 = rand() % (POP_SIZE / 2);
            Chromosome child;
            std::set<int> used;

            for (const auto& b : scored[p1].second) {
                std::vector<int> new_batch;
                for (int x : b) {
                    if (used.find(x) == used.end()) {
                        new_batch.push_back(x);
                        used.insert(x);
                    }
                }
                if (!new_batch.empty())
                    child.push_back(new_batch);
            }
            for (const auto& b : scored[p2].second) {
                std::vector<int> new_batch;
                for (int x : b) {
                    if (used.find(x) == used.end()) {
                        new_batch.push_back(x);
                        used.insert(x);
                    }
                }
                if (!new_batch.empty())
                    child.push_back(new_batch);
            }

            // 重新合并批次（按面积）
            Chromosome repaired;
            std::vector<int> current_batch;
            double current_area = 0;
            for (const auto& batch : child) {
                for (int p : batch) {
                    double area = l[p] * w[p];
                    if (current_area + area <= machine_area) {
                        current_batch.push_back(p);
                        current_area += area;
                    }
                    else {
                        repaired.push_back(current_batch);
                        current_batch = { p };
                        current_area = area;
                    }
                }
            }
            if (!current_batch.empty()) {
                repaired.push_back(current_batch);
            }

            // 变异
            if ((rand() / (double)RAND_MAX) < MUTATION_RATE) {
                if (!repaired.empty()) {
                    int b = rand() % repaired.size();
                    if (!repaired[b].empty()) {
                        int i = rand() % repaired[b].size();
                        int p = repaired[b][i];
                        repaired[b].erase(repaired[b].begin() + i);
                        int b2 = rand() % repaired.size();
                        repaired[b2].push_back(p);
                    }
                }
            }

            new_population.push_back(repaired);
        }

        population = new_population;
    }

    // 构造 BatchMap 输出
    BatchMap batch_map;
    for (size_t i = 0; i < best_solution.size(); ++i) {
        batch_map[i] = std::set<int>(best_solution[i].begin(), best_solution[i].end());
    }

    return std::make_pair(batch_map, best_cost);
}


std::pair<BatchMap, double> generateInitialSolution(
    const std::vector<int>& parts,
    const std::vector<double>& L,
    const std::vector<double>& W,
    const std::vector<double>& l,
    const std::vector<double>& w,
    const std::vector<double>& v,
    const std::vector<double>& h,
    const std::vector<double>& D,
    const std::vector<double>& ST,
    const std::vector<double>& VT,
    const std::vector<double>& UT
);

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr))); // 初始化随机数种子

    // 读取数据
    MachineInfo machine;
    std::vector<PartInfo> parts_info;
    PartLists part_lists;
    std::string filename = "Instance/11part.txt";

    if (!readMachineAndParts(filename, machine, parts_info, part_lists)) {
        std::cerr << "读取数据失败！" << std::endl;
        return 1;
    }

    // 构造 parts 索引向量
    std::vector<int> parts;
    for (std::size_t i = 0; i < parts_info.size(); ++i) {
        parts.push_back(static_cast<int>(i));
    }

    // 构造截止时间向量 D（这里简单设为体积的线性函数，也可自定义）
    //std::vector<double> D = { 8, 22, 31, 29, 28 }; // 5part
    //std::vector<double> D = { 9, 23, 32, 30, 29, 7, 13, 8, 20, 29 }; // 10part 0.796s
    std::vector<double> D = { 13, 9, 21, 12, 36, 33, 35, 29, 18, 11, 36 }; // 11part 3.612s
        //std::vector<double> due_dates = { 13, 9, 21, 12, 36, 33, 35, 29, 18, 11, 36, 6 }; // 12part 15.346s
        //std::vector<double> due_dates = { 13, 9, 21, 12, 36, 33, 35, 29, 18, 11, 36, 6, 29 }; // 13part 133.6s
        //std::vector<double> due_dates = { 14, 42, 10, 22, 13, 37, 34, 36, 30, 19, 12, 37, 7, 30 }; // 14part
    //std::vector<double> due_dates = { 15, 43, 11, 23, 14, 38, 35, 37, 48, 31, 20, 13, 38, 8, 31 }; // 15part

    // 构造参数向量（此处仅简单示例，实际可从机器信息派生或另设）
    std::vector<double> ST = { machine.setup_time };
    std::vector<double> VT = { machine.scanning_speed };
    std::vector<double> UT = { machine.recoater_speed };
    std::vector<double> L = { machine.length };
    std::vector<double> W = { machine.width };

    // 调用变邻域搜索函数
    auto [batches, total_tardiness] = generateInitialSolution(
        parts,
        L,
        W,
        part_lists.lengths,
        part_lists.widths,
        part_lists.volumes,
        part_lists.heights,
        D,
        ST,
        VT,
        UT
    );

    // 输出结果
    std::cout << "最小总延迟（Total Tardiness）: " << total_tardiness << "\n";
    std::cout << "批次数量: " << batches.size() << "\n";
    for (const auto& [batch_id, batch_parts] : batches) {
        std::cout << "批次 " << batch_id << ": ";
        for (int p : batch_parts) {
            std::cout << parts_info[p].id << " ";
        }
        std::cout << "\n";
    }

    return 0;
}
