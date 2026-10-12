#include "DataRecord.h"
#include <iomanip>
#include <locale>
#include <stdexcept>

//==========================数据记录================================
namespace fs = std::filesystem;
namespace {
const fs::path result_root = "Journal article/Tuning_LBpos_LBpar_LBser_3domi_global/";
}


// 定义全局日志流对象
std::ofstream log_stream;

std::string get_log_filename(const std::string& input_filename, const std::string& result_subfolder) {
    std::string base = fs::path(input_filename).stem().string();  // 提取文件名（不含路径与后缀）
    std::string log_dir = result_root.string();
    if (!result_subfolder.empty()) {
        log_dir = (fs::path(log_dir) / result_subfolder).string() + "/";
    }
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


//===================第一层节点详细数据记录==================

// CSV quoting also handles commas and quotation marks in file paths.
static std::string csv_field(const std::string& value) {
    std::string escaped = "\"";
    for (char c : value) {
        if (c == '"') escaped += '"';
        escaped += c;
    }
    return escaped + "\"";
}

void append_run_summary(const std::string& input_filename, const std::string& log_filename,
    std::size_t n_parts, double alpha_ser, double alpha_pos, double time_limit_seconds,
    double solver_call_seconds, const Stats& stats) {
    fs::create_directories(result_root);
    const fs::path summary = result_root / "experiment_summary.csv";
    const bool write_header = !fs::exists(summary) || fs::file_size(summary) == 0;
    std::ofstream out(summary, std::ios::binary | std::ios::app);
    if (!out) throw std::runtime_error("Cannot append summary CSV: " + summary.string());
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    if (write_header) {
        write_utf8_bom(out);
        out << "instance_id,n_parts,alpha_ser,alpha_pos,solve_status,time_limit_seconds,"
               "optimal_time_seconds,search_time_seconds,solver_call_seconds,final_UB,final_LB,"
               "MIP_gap,MIP_gap_percent,total_nodes,serial_dp_trigger_count,serial_pruning_count,"
               "solver_version,timing_version,input_file,source_log\r\n";
    }
    out << csv_field(fs::path(input_filename).stem().u8string()) << ',' << n_parts << ','
        << alpha_ser << ',' << alpha_pos << ','
        << (stats.proven_optimal ? "OPTIMAL" : "TIME_LIMIT") << ',' << time_limit_seconds << ',';
    if (stats.proven_optimal) out << stats.solve_time_seconds;
    out << ',' << stats.solve_time_seconds << ',' << solver_call_seconds << ','
        << stats.final_ub << ',' << stats.final_lb << ',' << stats.relative_gap << ','
        << 100.0 * stats.relative_gap << ',' << stats.total_nodes << ','
        << stats.serial_dp_trigger_count << ',' << stats.serial_pruning_count << ','
        // Update these labels when the algorithm or timing scope changes.
        << "bb_pending_bounds_v1,search_after_cache_before_cleanup_v1,"
        << csv_field(fs::path(input_filename).u8string()) << ','
        << csv_field(fs::path(log_filename).u8string()) << "\r\n";
    out.flush();
    if (!out) throw std::runtime_error("Failed writing summary CSV: " + summary.string());
}
