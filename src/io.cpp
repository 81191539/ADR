/*****************************************************************************
 * io.cpp
 * 
 * Data input/output implementation.
 *****************************************************************************/

#include "io.h"
#include "config.h"
#include "file_utils.h"

#include <iostream>
#include <fstream>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <stdexcept>
#include <filesystem>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_nonfinite_output_warnings = 0;

void warn_nonfinite_output(const char* context, double value)
{
    if (g_nonfinite_output_warnings < 5) {
        std::fprintf(stderr,
                     "Warning: non-finite value (%g) encountered in output (%s)\n",
                     value, context);
    }
    ++g_nonfinite_output_warnings;
}

std::runtime_error invalid_case(int case_number, const std::string& message)
{
    return std::runtime_error("Case " + std::to_string(case_number) +
                              " invalid: " + message);
}

std::string trim(std::string text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

double parse_number(int case_number, const std::string& name, const std::string& text)
{
    try {
        std::size_t parsed = 0;
        const double value = std::stod(text, &parsed);
        if (parsed != text.size()) {
            throw invalid_case(case_number, name + " has trailing characters.");
        }
        return value;
    } catch (const std::invalid_argument&) {
        throw invalid_case(case_number, name + " must be numeric.");
    } catch (const std::out_of_range&) {
        throw invalid_case(case_number, name + " is out of range.");
    }
}

std::map<std::string, double> parse_toml_values(int case_number, std::istream& in)
{
    std::map<std::string, double> values;
    std::string line;
    int line_number = 0;
    std::string section;

    while (std::getline(in, line)) {
        ++line_number;
        const auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line.erase(comment_pos);
        }

        const std::string stripped = trim(line);
        if (stripped.empty()) {
            continue;
        }
        if (stripped.front() == '[' && stripped.back() == ']') {
            section = trim(stripped.substr(1, stripped.size() - 2));
            if (section.empty()) {
                throw invalid_case(case_number,
                                   "invalid TOML line " + std::to_string(line_number) +
                                   ": section name is required.");
            }
            continue;
        }

        const auto eq_pos = stripped.find('=');
        if (eq_pos == std::string::npos) {
            throw invalid_case(case_number,
                               "invalid TOML line " + std::to_string(line_number) +
                               ": expected key = value.");
        }

        std::string key = trim(stripped.substr(0, eq_pos));
        const std::string raw_value = trim(stripped.substr(eq_pos + 1));
        if (key.empty() || raw_value.empty()) {
            throw invalid_case(case_number,
                               "invalid TOML line " + std::to_string(line_number) +
                               ": key and value are required.");
        }
        if (!section.empty()) {
            key = section + "." + key;
        }
        values[key] = parse_number(case_number, key, raw_value);
    }

    return values;
}

struct TomlDocument {
    std::map<std::string, double> numbers;
    std::map<std::string, bool> booleans;
};

bool parse_toml_bool(int case_number, const std::string& name, const std::string& text)
{
    if (text == "true") {
        return true;
    }
    if (text == "false") {
        return false;
    }
    throw invalid_case(case_number, name + " must be true or false.");
}

TomlDocument parse_toml_document(int case_number, std::istream& in)
{
    TomlDocument document;
    std::string line;
    int line_number = 0;
    std::string section;

    while (std::getline(in, line)) {
        ++line_number;
        const auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line.erase(comment_pos);
        }

        const std::string stripped = trim(line);
        if (stripped.empty()) {
            continue;
        }
        if (stripped.front() == '[' && stripped.back() == ']') {
            section = trim(stripped.substr(1, stripped.size() - 2));
            if (section.empty()) {
                throw invalid_case(case_number,
                                   "invalid TOML line " + std::to_string(line_number) +
                                   ": section name is required.");
            }
            continue;
        }

        const auto eq_pos = stripped.find('=');
        if (eq_pos == std::string::npos) {
            throw invalid_case(case_number,
                               "invalid TOML line " + std::to_string(line_number) +
                               ": expected key = value.");
        }

        std::string key = trim(stripped.substr(0, eq_pos));
        const std::string raw_value = trim(stripped.substr(eq_pos + 1));
        if (key.empty() || raw_value.empty()) {
            throw invalid_case(case_number,
                               "invalid TOML line " + std::to_string(line_number) +
                               ": key and value are required.");
        }
        if (!section.empty()) {
            key = section + "." + key;
        }

        if (raw_value == "true" || raw_value == "false") {
            document.booleans[key] = parse_toml_bool(case_number, key, raw_value);
        } else {
            document.numbers[key] = parse_number(case_number, key, raw_value);
        }
    }

    return document;
}

double require_toml_value(int case_number,
                          const std::map<std::string, double>& values,
                          const std::string& name)
{
    const auto found = values.find(name);
    if (found == values.end()) {
        throw invalid_case(case_number, "missing TOML field: " + name + ".");
    }
    return found->second;
}

long require_toml_integer(int case_number,
                          const std::map<std::string, double>& values,
                          const std::string& name)
{
    const double value = require_toml_value(case_number, values, name);
    if (std::floor(value) != value) {
        throw invalid_case(case_number, name + " must be an integer.");
    }
    return static_cast<long>(value);
}

Params read_toml_parameter(int case_number, std::istream& in)
{
    const auto document = parse_toml_document(case_number, in);
    const auto& values = document.numbers;

    Params p;
    p.lam = require_toml_value(case_number, values, "lam");
    p.Pe = require_toml_value(case_number, values, "Pe");
    p.Pe2 = require_toml_value(case_number, values, "Pe2");
    p.eps = require_toml_value(case_number, values, "eps");
    p.Da = require_toml_value(case_number, values, "Da");
    p.K0 = require_toml_value(case_number, values, "K0");
    p.ny = require_toml_integer(case_number, values, "ny");
    p.xpo_l = require_toml_value(case_number, values, "xpo_l");
    p.xpo_r = require_toml_value(case_number, values, "xpo_r");
    p.endT = require_toml_value(case_number, values, "endT");
    p.total_count = require_toml_integer(case_number, values, "total_count");
    p.coeff_dt = require_toml_value(case_number, values, "coeff_dt");
    p.x_ini_posi = require_toml_value(case_number, values, "x_ini_posi");
    p.alpha = require_toml_value(case_number, values, "alpha");
    const auto sc = values.find("Sc");
    if (sc != values.end()) {
        p.Sc = sc->second;
    }
    p.id = case_number;
    return p;
}

Params read_legacy_parameter(int case_number, std::istream& in)
{
    Params p;
    int unused{};

    if (!(in >> unused
          >> p.lam >> p.Pe >> p.Pe2 >> p.eps >> p.Da >> p.K0 >> p.ny
          >> p.xpo_l >> p.xpo_r >> p.endT >> p.total_count >> p.coeff_dt
          >> p.x_ini_posi >> p.alpha)) {
        throw invalid_case(case_number,
                           "could not parse the expected input parameter record.");
    }

    p.id = case_number;
    return p;
}

void require_finite(int case_number, const char* name, double value)
{
    if (!std::isfinite(value)) {
        throw invalid_case(case_number, std::string(name) + " must be finite.");
    }
}

void validate_parameter(int case_number, const Params& p)
{
    require_finite(case_number, "lam", p.lam);
    require_finite(case_number, "Pe", p.Pe);
    require_finite(case_number, "Pe2", p.Pe2);
    require_finite(case_number, "eps", p.eps);
    require_finite(case_number, "Da", p.Da);
    require_finite(case_number, "K0", p.K0);
    require_finite(case_number, "xpo_l", p.xpo_l);
    require_finite(case_number, "xpo_r", p.xpo_r);
    require_finite(case_number, "endT", p.endT);
    require_finite(case_number, "coeff_dt", p.coeff_dt);
    require_finite(case_number, "x_ini_posi", p.x_ini_posi);
    require_finite(case_number, "alpha", p.alpha);
    require_finite(case_number, "Sc", p.Sc);

    if (p.lam <= 0.0) {
        throw invalid_case(case_number, "lam must be greater than 0.");
    }
    if (p.ny <= 0) {
        throw invalid_case(case_number, "ny must be greater than 0.");
    }
    if (p.K0 <= 0.0) {
        throw invalid_case(case_number, "K0 must be greater than 0.");
    }
    if (p.eps <= 0.0) {
        throw invalid_case(case_number, "eps must be greater than 0.");
    }
    if (std::fabs(p.alpha) < 1e-12) {
        throw invalid_case(case_number,
                           "alpha is too close to 0 (|alpha| < 1e-12).");
    }
    if (p.Sc <= 0.0) {
        throw invalid_case(case_number, "Sc must be greater than 0.");
    }
    if (p.total_count <= 0) {
        throw invalid_case(case_number, "total_count must be greater than 0.");
    }
    if (p.coeff_dt <= 0.0) {
        throw invalid_case(case_number, "coeff_dt must be greater than 0.");
    }
    if (p.endT <= 0.0) {
        throw invalid_case(case_number, "endT must be greater than 0.");
    }
    if (!(0.0 <= p.xpo_l && p.xpo_l < p.xpo_r && p.xpo_r <= 1.0)) {
        throw invalid_case(case_number,
                           "xpo_l and xpo_r must satisfy 0 <= xpo_l < xpo_r <= 1.");
    }
}

}  // namespace

//-----------------------------------------------------------------------------
// Return a path prefixed with the output directory.
//-----------------------------------------------------------------------------
static std::string get_output_path(const std::string& filename)
{
    if (config::OUTPUT_DIR.empty()) {
        return filename;
    }
    if (!fs::exists(config::OUTPUT_DIR)) {
        fs::create_directories(config::OUTPUT_DIR);
    }
    return config::OUTPUT_DIR + "/" + filename;
}

static fs::path find_input_parameter_file(int case_number)
{
    const std::vector<std::string> patterns = {
        "input_parameter_%04d.toml",
        "input_parameter_%d.toml",
        "input_parameter_%03d.toml",
        "input_parameter_%02d.toml",
        "input_parameter_%05d.toml",
        "input_parameter_%d.txt",
        "input_parameter_%03d.txt",
        "input_parameter_%02d.txt",
        "input_parameter_%04d.txt",
        "input_parameter_%05d.txt",
    };

    for (const auto& pat : patterns) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), pat.c_str(), case_number);

        const fs::path filepath = config::INPUT_DIR.empty()
                                ? fs::path(buf)
                                : fs::path(config::INPUT_DIR) / buf;
        std::ifstream in(filepath);
        if (in.is_open()) {
            return filepath;
        }
    }

    throw invalid_case(case_number, "input parameter file was not found.");
}

//-----------------------------------------------------------------------------
// Read input parameters.
//-----------------------------------------------------------------------------
Params read_parameter(int case_number)
{
    const fs::path filepath = find_input_parameter_file(case_number);
    std::ifstream in(filepath);

    Params p = fs::path(filepath).extension() == ".toml"
             ? read_toml_parameter(case_number, in)
             : read_legacy_parameter(case_number, in);
    validate_parameter(case_number, p);
    
    return p;
}

static bool toml_number(const TomlDocument& document,
                        const std::string& key,
                        double& value)
{
    const auto found = document.numbers.find(key);
    if (found == document.numbers.end()) {
        return false;
    }
    value = found->second;
    return true;
}

static bool toml_bool(const TomlDocument& document,
                      const std::string& key,
                      bool& value)
{
    const auto found = document.booleans.find(key);
    if (found == document.booleans.end()) {
        return false;
    }
    value = found->second;
    return true;
}

static long checked_positive_long(int case_number,
                                  const std::string& key,
                                  double value)
{
    if (!std::isfinite(value) || value <= 0.0 || std::floor(value) != value) {
        throw invalid_case(case_number, key + " must be a positive integer.");
    }
    return static_cast<long>(value);
}

static int checked_nonnegative_int(int case_number,
                                   const std::string& key,
                                   double value)
{
    if (!std::isfinite(value) || value < 0.0 || std::floor(value) != value) {
        throw invalid_case(case_number, key + " must be a non-negative integer.");
    }
    return static_cast<int>(value);
}

static double checked_nonnegative_double(int case_number,
                                         const std::string& key,
                                         double value)
{
    if (!std::isfinite(value) || value < 0.0) {
        throw invalid_case(case_number, key + " must be a non-negative number.");
    }
    return value;
}

void apply_runtime_config_from_case(int case_number, ExecutionConfig& exec_config)
{
    const fs::path filepath = find_input_parameter_file(case_number);
    if (filepath.extension() != ".toml") {
        return;
    }

    std::ifstream in(filepath);
    const TomlDocument document = parse_toml_document(case_number, in);

    double number = 0.0;
    bool boolean = false;

    if (!exec_config.runtime_overrides.stats_interval &&
        toml_number(document, "runtime.stats_interval", number)) {
        exec_config.stats_interval =
            checked_positive_long(case_number, "runtime.stats_interval", number);
    }
    if (!exec_config.runtime_overrides.stability_check_interval &&
        toml_number(document, "runtime.stability_check_interval", number)) {
        exec_config.stability_check_interval =
            checked_positive_long(case_number, "runtime.stability_check_interval", number);
    }
    if (!exec_config.runtime_overrides.checkpoint_interval &&
        toml_number(document, "runtime.checkpoint_interval", number)) {
        exec_config.checkpoint_interval =
            checked_positive_long(case_number, "runtime.checkpoint_interval", number);
    }
    if (!exec_config.runtime_overrides.dense_dump_start &&
        toml_number(document, "runtime.dense_dump_start", number)) {
        exec_config.dense_dump_start =
            checked_nonnegative_double(case_number, "runtime.dense_dump_start", number);
    }
    if (!exec_config.runtime_overrides.dense_dump_count &&
        toml_number(document, "runtime.dense_dump_count", number)) {
        exec_config.dense_dump_count =
            checked_nonnegative_int(case_number, "runtime.dense_dump_count", number);
    }
    if (!exec_config.runtime_overrides.convergence_threshold &&
        toml_number(document, "runtime.convergence_threshold", number)) {
        exec_config.convergence_threshold =
            checked_nonnegative_double(case_number, "runtime.convergence_threshold", number);
    }
    if (!exec_config.runtime_overrides.enable_dense_dump &&
        toml_bool(document, "runtime.enable_dense_dump", boolean)) {
        exec_config.enable_dense_dump = boolean;
    }
    if (!exec_config.runtime_overrides.output_matlab &&
        toml_bool(document, "runtime.output_matlab", boolean)) {
        exec_config.output_matlab = boolean;
    }
    if (!exec_config.runtime_overrides.output_tecplot &&
        toml_bool(document, "runtime.output_tecplot", boolean)) {
        exec_config.output_tecplot = boolean;
    }
}

//-----------------------------------------------------------------------------
// Ensure the directory exists.
//-----------------------------------------------------------------------------
void ensure_dir(const std::string& dir)
{
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
}

//-----------------------------------------------------------------------------
// Output concentration field and surface coverage data in MATLAB format.
//-----------------------------------------------------------------------------
void print_data(const Field2D& phi, const Field1D& eta, const Field1D& xx,
                int count, const char* buf,
                long nx, long ny)
{
    std::string full_dir = get_output_path(buf);
    
    if (!fs::exists(full_dir)) {
        fs::create_directories(full_dir);
    }

    char buffer_phi[128], buffer_eta[128];
    std::snprintf(buffer_phi, sizeof(buffer_phi), "%s/cc_%d.m", full_dir.c_str(), count);
    std::snprintf(buffer_eta, sizeof(buffer_eta), "%s/ee_%d.m", full_dir.c_str(), count);

    try {
        SafeFile fphi(buffer_phi, "w");
        for (long i = 0; i <= nx; ++i) {
            for (long j = 0; j <= ny; ++j) {
                const double value = phi(i, j);
                if (!std::isfinite(value)) {
                    warn_nonfinite_output("cc", value);
                }
                fphi.printf("%s%16.14f ", " ", value);
            }
            fphi.puts("\n");
        }

        SafeFile feta(buffer_eta, "w");
        for (long i = 0; i <= nx; ++i) {
            const double eta_value = eta(i);
            if (!std::isfinite(eta_value)) {
                warn_nonfinite_output("eta", eta_value);
            }
            feta.printf(" %16.14f  %16.14f\n", xx(i), eta_value);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error in print_data: %s\n", e.what());
    }
}

//-----------------------------------------------------------------------------
// Output Tecplot format data.
//-----------------------------------------------------------------------------
void print_tecplot_data(const Field2D& cc, int count, const char* buf,
                        long nx, long ny, double h,
                        double xleft, double yleft)
{
    std::string full_dir = get_output_path(buf);
    
    if (!fs::exists(full_dir)) {
        fs::create_directories(full_dir);
    }

    char mid[128];
    std::snprintf(mid, sizeof(mid), "%s/cc_%d.dat", full_dir.c_str(), count);
    
    try {
        SafeFile out(mid, "w");

        out.puts(" \"IJK - Ordered Data\"\n");
        out.puts("VARIABLES = \"x\",\"y\",\"cc\"\n");
        out.puts("ZONE T = \"immerseBoundary\"\n");
        out.puts("STRANDID = 0, SOLUTIONTIME = 0\n");
        out.printf("I = %ld, J = %ld, K = 1, ZONETYPE = Ordered\n", nx + 1, ny + 1);
        out.puts("DATAPACKING = BLOCK\n");
        out.puts("DT = ( SINGLE SINGLE SINGLE )\n");

        for (long j = 0; j <= ny; ++j) {
            for (long i = 0; i <= nx; ++i) {
                out.printf(" %8.5e ", xleft + i * h);
            }
            out.puts("\n");
        }

        for (long j = 0; j <= ny; ++j) {
            for (long i = 0; i <= nx; ++i) {
                out.printf(" %8.5e ", yleft + j * h);
            }
            out.puts("\n");
        }

        for (long j = 0; j <= ny; ++j) {
            for (long i = 0; i <= nx; ++i) {
                out.printf(" %8.5e ", cc(i, j));
            }
            out.puts("\n");
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error in print_tecplot_data: %s\n", e.what());
    }
}

//-----------------------------------------------------------------------------
// Unified data output interface.
//-----------------------------------------------------------------------------
void output_data(const Field2D& phi, const Field1D& eta, const Field1D& xx,
                 int count, const char* buf,
                 long nx, long ny, double h,
                 double xleft, double yleft,
                 bool output_matlab,
                 bool output_tecplot)
{
    if (output_matlab) {
        print_data(phi, eta, xx, count, buf, nx, ny);
    }
    
    if (output_tecplot) {
        print_tecplot_data(phi, count, buf, nx, ny, h, xleft, yleft);
    }
}

//-----------------------------------------------------------------------------
// Write detailed runtime log.
//-----------------------------------------------------------------------------
void write_detailed_log(const char* fname_log, int case_number,
                        const Params& p, const GridInfo& grid,
                        const PhysicsParams& phys, const AdsorptionZone& zone,
                        const RunLog& log, double dt_initial,
                        AdvectionScheme advection_scheme,
                        bool output_matlab,
                        bool output_tecplot)
{
    std::string full_path = get_output_path(fname_log);
    
    try {
        SafeFile fp(full_path, "w");
        
        std::time_t now = std::time(nullptr);
        char time_buf[64];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        
        fp.puts("%%=============================================================\n");
        fp.puts("%%  DIFFUSION-CONVECTION SIMULATION LOG\n");
        fp.printf("%%  Case Number: %d\n", case_number);
        fp.printf("%%  Generated:   %s\n", time_buf);
        fp.puts("%%=============================================================\n\n");
        
        fp.puts("%% -------------------- INPUT PARAMETERS --------------------\n");
        fp.printf("case_number = %d;\n\n", case_number);
        
        fp.puts("%% Geometry and mesh\n");
        fp.printf("lam = %.6g;        %% aspect ratio (H/L)\n", p.lam);
        fp.printf("ny  = %ld;         %% grid points in y-direction\n", p.ny);
        fp.printf("nx  = %ld;         %% grid points in x-direction (computed)\n", grid.nx);
        fp.printf("h   = %.6e;   %% grid spacing\n", grid.h);
        fp.puts("\n");
        
        fp.puts("%% Computational domain\n");
        fp.printf("xleft  = %.6g;\n", grid.xleft);
        fp.printf("xright = %.6g;\n", grid.xright);
        fp.printf("yleft  = %.6g;\n", grid.yleft);
        fp.printf("yright = %.6g;\n", grid.yright);
        fp.printf("domain_length = %.6g;  %% L = xright - xleft\n", grid.xright - grid.xleft);
        fp.printf("domain_height = %.6g;  %% H = yright - yleft\n", grid.yright - grid.yleft);
        fp.puts("\n");
        
        fp.puts("%% Flow parameters\n");
        fp.printf("Pe    = %.6g;      %% Peclet number (steady flow)\n", phys.Pe);
        fp.printf("Pe2   = %.6g;      %% Peclet number (oscillatory flow)\n", phys.Pe2);
        fp.printf("alpha = %.6g;      %% Womersley number\n", phys.alpha);
        fp.printf("Sc    = %.6g;      %% Schmidt number\n", phys.Sc);
        fp.puts("\n");
        
        fp.puts("%% Reaction parameters\n");
        fp.printf("Da  = %.6g;        %% Damkohler number\n", phys.Da);
        fp.printf("K0  = %.6g;        %% equilibrium constant\n", phys.K0);
        fp.printf("eps = %.6g;        %% surface capacity parameter\n", phys.eps);
        fp.printf("c0  = %.6g;        %% inlet concentration\n", phys.c0);
        fp.puts("\n");
        
        fp.puts("%% Adsorption zone (dimensionless, then physical)\n");
        fp.printf("xpo_l_rel = %.6g;  %% relative position (0-1)\n", p.xpo_l);
        fp.printf("xpo_r_rel = %.6g;  %% relative position (0-1)\n", p.xpo_r);
        fp.printf("xpo_l = %.6g;      %% physical left boundary\n", zone.xpo_l);
        fp.printf("xpo_r = %.6g;      %% physical right boundary\n", zone.xpo_r);
        fp.printf("adsorption_length = %.6g;  %% length of adsorption zone\n", zone.xpo_r - zone.xpo_l);
        fp.puts("\n");
        
        fp.puts("%% Time integration\n");
        fp.printf("endT        = %.6g;    %% target end time\n", p.endT);
        fp.printf("coeff_dt    = %.6g;    %% dt coefficient (dt = coeff_dt * h^2)\n", p.coeff_dt);
        fp.printf("dt_initial  = %.6e;    %% initial time step\n", dt_initial);
        fp.printf("total_count = %ld;      %% planned output count\n", p.total_count);
        fp.printf("advection_scheme = '%s';\n", advection_scheme_name(advection_scheme));
        fp.puts("\n");
        
        fp.puts("%% Initial condition\n");
        fp.printf("x_ini_posi = %.6g;  %% initial concentration front position\n", p.x_ini_posi);
        fp.puts("\n");
        
        fp.puts("%% -------------------- THEORETICAL VALUES --------------------\n");
        double eta_eq = phys.K0 / (1.0 + phys.K0);
        double u_max = std::abs(phys.Pe) * 0.25 + std::abs(phys.Pe2);
        double dt_diff = 0.25 * grid.h * grid.h;
        double dt_conv = (u_max > 1e-10) ? (grid.h / u_max) : 1e10;
        
        fp.printf("eta_eq = %.10g;     %% equilibrium coverage = K0/(1+K0)\n", eta_eq);
        fp.printf("u_max_estimate = %.6g;  %% estimated maximum velocity\n", u_max);
        fp.puts("\n");
        
        fp.puts("%% CFL stability estimates\n");
        fp.printf("dt_diffusion_limit  = %.6e;  %% h^2/4 (2D explicit)\n", dt_diff);
        fp.printf("dt_convection_limit = %.6e;  %% h/u_max\n", dt_conv);
        fp.printf("dt_cfl_recommended  = %.6e;  %% 0.4 * min(dt_diff, dt_conv)\n", 
                 0.4 * std::min(dt_diff, dt_conv));
        fp.puts("\n");
        
        fp.puts("%% -------------------- MEMORY ESTIMATE --------------------\n");
        long total_cells = (grid.nx + 3) * (grid.ny + 3);
        double mem_matrix = total_cells * sizeof(double) / (1024.0 * 1024.0);
        double mem_vector = (grid.nx + 3) * sizeof(double) / (1024.0 * 1024.0);
        double mem_total = 3 * mem_matrix + 5 * mem_vector;
        
        fp.printf("total_grid_cells = %ld;  %% including ghost cells\n", total_cells);
        fp.printf("memory_per_matrix_MB = %.3f;\n", mem_matrix);
        fp.printf("memory_per_vector_MB = %.6f;\n", mem_vector);
        fp.printf("memory_total_estimate_MB = %.3f;  %% 3 matrices + 5 vectors\n", mem_total);
        fp.puts("\n");
        
        fp.puts("%% -------------------- RUNTIME EVENTS --------------------\n");
        fp.printf("instability_events = %d;  %% non-finite, negative, or too-large concentration events\n",
                  log.instability_events);
        fp.printf("nan_events = %d;  %% legacy alias for instability-triggered restarts\n",
                  log.nan_events);
        fp.printf("resumed_from_checkpoint = %d;  %% 1=yes, 0=no\n", 
                 log.resumed_from_checkpoint ? 1 : 0);
        if (log.resumed_from_checkpoint) {
            fp.printf("resumed_at_iteration = %ld;\n", log.resumed_at_iteration);
        }
        fp.puts("\n");
        
        if (!log.dt_history.empty()) {
            fp.puts("%% dt adjustment history (iteration, old_dt, new_dt, sim_time)\n");
            fp.puts("dt_adjustments = [\n");
            for (const auto& adj : log.dt_history) {
                fp.printf("    %8ld, %.6e, %.6e, %.6e;  %% iter, old, new, time\n",
                        adj.iteration, adj.old_dt, adj.new_dt, adj.sim_time);
            }
            fp.puts("];\n\n");
        } else {
            fp.puts("%% No dt adjustments were needed (stable throughout)\n\n");
        }
        
        fp.puts("%% -------------------- CONVERGENCE HISTORY --------------------\n");
        if (!log.convergence_history.empty()) {
            fp.puts("%% Key convergence milestones (iteration, time, eta, rel_error)\n");
            fp.puts("convergence_milestones = [\n");
            for (const auto& pt : log.convergence_history) {
                fp.printf("    %8ld, %.6e, %.10g, %.6e;\n",
                        pt.iteration, pt.sim_time, pt.eta_ave, pt.rel_err);
            }
            fp.puts("];\n");
            fp.puts("convergence_milestones_headers = {'iteration', 'sim_time', 'eta_ave', 'rel_error'};\n\n");
        }
        
        fp.puts("%% -------------------- PERFORMANCE STATISTICS --------------------\n");
        fp.printf("actual_iterations = %ld;\n", log.actual_iterations);
        fp.printf("output_file_count = %d;\n", log.output_count);
        fp.puts("\n");
        
        fp.puts("%% Timing breakdown (seconds)\n");
        fp.printf("time_initialization = %.3f;\n", log.time_init);
        fp.printf("time_computation    = %.3f;\n", log.time_compute);
        fp.printf("time_io             = %.3f;\n", log.time_io);
        fp.printf("time_total          = %.3f;\n", log.time_total);
        fp.puts("\n");
        
        if (log.actual_iterations > 0 && log.time_compute > 0) {
            double time_per_iter = log.time_compute / log.actual_iterations * 1000.0;
            double iters_per_sec = log.actual_iterations / log.time_compute;
            fp.printf("time_per_iteration_ms = %.4f;\n", time_per_iter);
            fp.printf("iterations_per_second = %.1f;\n", iters_per_sec);
            fp.puts("\n");
        }
        
        fp.puts("%% -------------------- FINAL RESULTS --------------------\n");
        fp.printf("converged = %d;  %% 1=yes, 0=no (reached max_it)\n", log.converged ? 1 : 0);
        fp.printf("final_sim_time = %.10g;\n", log.final_sim_time);
        fp.printf("final_eta_ave  = %.10g;\n", log.final_eta);
        fp.printf("final_rel_error = %.6e;\n", log.final_rel_err);
        fp.puts("\n");
        
        if (log.converged) {
            fp.puts("%% Convergence achieved!\n");
            fp.printf("%% eta_ave reached within 1%% of eta_eq = %.10g\n", eta_eq);
        } else {
            fp.puts("%% WARNING: Simulation ended without convergence\n");
            fp.puts("%% Consider: (1) increase endT, (2) check parameters\n");
        }
        fp.puts("\n");
        
        fp.puts("%% -------------------- OUTPUT FILES --------------------\n");
        fp.printf("%% Data files are in: data_%d/\n", case_number);
        if (output_matlab) {
            fp.puts("%%   cc_N.m  - concentration field (MATLAB format)\n");
            fp.puts("%%   ee_N.m  - surface coverage (MATLAB format)\n");
        }
        if (output_tecplot) {
            fp.puts("%%   cc_N.dat - concentration field (Tecplot format)\n");
        }
        fp.puts("%% \n");
        fp.printf("%% Time series: eta_ave_%d.m\n", case_number);
        fp.puts("%%   Format: [time, eta_average, d(eta)/dt]\n");
        fp.puts("\n");
        
        fp.puts("%%=============================================================\n");
        fp.puts("%%  END OF LOG\n");
        fp.puts("%%=============================================================\n");
        
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error in write_detailed_log: %s\n", e.what());
    }
}
