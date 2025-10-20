#include "DataRecord.h"

//==========================数据记录================================
namespace fs = std::filesystem;

// 定义全局日志流对象
std::ofstream log_stream;

std::string get_log_filename(const std::string& input_filename) {
    std::string base = fs::path(input_filename).stem().string();  // 提取文件名（不含路径与后缀）
    std::string log_dir = "logs_LBupdate_test_DP_adaptive_Ptcompare_4-7/";
    fs::create_directories(log_dir);  // 创建 logs 目录（若不存在）

    int count = 1;
    std::string log_filename;
    do {
        log_filename = log_dir + base + "_log_" + std::to_string(count) + ".txt";
        count++;
    } while (fs::exists(log_filename));

    return log_filename;
}

void write_utf8_bom(std::ofstream& stream) {
    // 写入 UTF-8 BOM: EF BB BF
    stream << "\xEF\xBB\xBF";
}


//=================================bound数据记录============================
void export_bound_statistics(const std::string& log_filename, const Stats& stats) {
    std::string bound_csv = log_filename;
    size_t pos = bound_csv.find_last_of(".");
    if (pos != std::string::npos) {
        bound_csv = bound_csv.substr(0, pos);
    }
    bound_csv += "_bounds.csv";

    std::ofstream out(bound_csv);
    if (!out.is_open()) {
        std::cerr << "无法打开文件用于写入 Bound 统计信息: " << bound_csv << std::endl;
        return;
    }

    out << "Time,UB,LB\n";

    size_t i = 0, j = 0;
    double last_ub = -1.0;
    double last_lb = -1.0;

    while (i < stats.UB_updates.size() || j < stats.LB_convergence.size()) {
        double t_ub = (i < stats.UB_updates.size()) ? stats.UB_updates[i].first : std::numeric_limits<double>::infinity();
        double t_lb = (j < stats.LB_convergence.size()) ? stats.LB_convergence[j].first : std::numeric_limits<double>::infinity();

        if (t_ub < t_lb) {
            last_ub = stats.UB_updates[i].second;
            out << std::fixed << std::setprecision(6) << t_ub << "," << last_ub << "," << last_lb << "\n";
            ++i;
        }
        else if (t_lb < t_ub) {
            last_lb = stats.LB_convergence[j].second;
            out << std::fixed << std::setprecision(6) << t_lb << "," << last_ub << "," << last_lb << "\n";
            ++j;
        }
        else {
            last_ub = stats.UB_updates[i].second;
            last_lb = stats.LB_convergence[j].second;
            out << std::fixed << std::setprecision(6) << t_ub << "," << last_ub << "," << last_lb << "\n";
            ++i;
            ++j;
        }
    }

    out.close();
    log_and_cout("上下界记录已导出至: " + bound_csv + "\n");
}
//======================深度剪枝节点数据记录=====================
int export_pruned_depth_info(
    const std::string& log_filename,
    const std::unordered_map<int, int>& pruned_nodes_per_depth
) {
    std::string csv_filename = log_filename;
    size_t pos = csv_filename.find_last_of(".");
    if (pos != std::string::npos) {
        csv_filename = csv_filename.substr(0, pos);
    }
    csv_filename += "_pruned_per_depth.csv";

    std::ofstream csv_file(csv_filename);
    if (!csv_file.is_open()) {
        std::cerr << "无法打开文件用于写入剪枝信息: " << csv_filename << std::endl;
        return 1;
    }

    csv_file << "Depth,PrunedNodes\n";
    for (const auto& [depth, count] : pruned_nodes_per_depth) {
        csv_file << depth << "," << count << "\n";
    }

    csv_file.close();
    std::cout << "剪枝深度信息已导出至: " << csv_filename << std::endl;

    return 0;
}

//================第一层节点数据记录=======================
void export_first_level_lbs(
    const std::string& log_filename,
    const std::vector<std::pair<std::string, double>>& first_level_node_lbs
) {
    std::string csv_filename = log_filename;
    size_t pos = csv_filename.find_last_of(".");
    if (pos != std::string::npos) {
        csv_filename = csv_filename.substr(0, pos);
    }
    csv_filename += "_first_level_lbs.csv"; // 命名方式与其他文件类似

    std::ofstream csv_file(csv_filename);
    if (!csv_file.is_open()) {
        std::cerr << "无法打开文件用于写入第一层子节点 LB 信息: " << csv_filename << std::endl;
        return;
    }

    csv_file << "Node Name,Lower Bound\n"; // CSV 文件头
    for (const auto& entry : first_level_node_lbs) {
        // 使用 std::fixed 和 std::setprecision 保持数值精度
        csv_file << entry.first << "," << std::fixed << std::setprecision(6) << entry.second << "\n";
    }

    csv_file.close();
    log_and_cout("第一层子节点 LB 信息已导出至: " + csv_filename + "\n");
}


//================第一层节点未分配零件和LB计算时间数据记录=======================
void export_first_level_unassigned_parts_and_lb_time(
    const std::string& log_filename,
    const std::vector<std::tuple<std::string, int, double>>& first_level_node_unassigned_parts_and_lb_time
) {
    std::string csv_filename = log_filename;
    size_t pos = csv_filename.find_last_of(".");
    if (pos != std::string::npos) {
        csv_filename = csv_filename.substr(0, pos);
    }
    csv_filename += "_first_level_unassigned_lb_time.csv"; // 明确的文件名

    std::ofstream csv_file(csv_filename);
    if (!csv_file.is_open()) {
        std::cerr << "无法打开文件用于写入第一层子节点未分配零件和 LB 计算时间信息: " << csv_filename << std::endl;
        return;
    }

    // CSV 文件头
    csv_file << "Node Name,Unassigned Parts Count,LB Calculation Time (s)\n";
    for (const auto& entry : first_level_node_unassigned_parts_and_lb_time) {
        // 使用 std::get<N>(entry) 访问 tuple 元素
        csv_file << std::get<0>(entry) << ","
            << std::get<1>(entry) << ","
            << std::fixed << std::setprecision(6) << std::get<2>(entry) << "\n";
    }

    csv_file.close();
    log_and_cout("第一层子节点未分配零件和 LB 计算时间信息已导出至: " + csv_filename + "\n");
}