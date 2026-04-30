/*****************************************************************************
 * checkpoint.cpp
 * 
 * 检查点功能实现
 *****************************************************************************/

#include "checkpoint.h"
#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "file_utils.h"

namespace fs = std::filesystem;

//-----------------------------------------------------------------------------
// 检查点文件名生成
//-----------------------------------------------------------------------------
std::string get_checkpoint_filename(int case_number)
{
    char buf[128];
    if (config::CHECKPOINT_DIR.empty()) {
        std::snprintf(buf, sizeof(buf), "checkpoint_%d.bin", case_number);
    } else {
        // 确保目录存在
        if (!fs::exists(config::CHECKPOINT_DIR)) {
            fs::create_directories(config::CHECKPOINT_DIR);
        }
        std::snprintf(buf, sizeof(buf), "%s/checkpoint_%d.bin", 
                      config::CHECKPOINT_DIR.c_str(), case_number);
    }
    return std::string(buf);
}

//-----------------------------------------------------------------------------
// 检查检查点文件是否存在
//-----------------------------------------------------------------------------
bool checkpoint_exists(int case_number)
{
    std::string filename = get_checkpoint_filename(case_number);
    return fs::exists(filename);
}

//-----------------------------------------------------------------------------
// 删除检查点文件
//-----------------------------------------------------------------------------
void delete_checkpoint(int case_number)
{
    std::string filename = get_checkpoint_filename(case_number);
    if (fs::exists(filename)) {
        fs::remove(filename);
    }
}

//-----------------------------------------------------------------------------
// 检查是否强制重新开始
//-----------------------------------------------------------------------------
bool check_force_restart(int case_number)
{
    char filename[64];
    std::snprintf(filename, sizeof(filename), "force_restart_%d.txt", case_number);
    
    if (fs::exists(filename)) {
        // 删除标志文件（只触发一次）
        fs::remove(filename);
        return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// 保存检查点
//-----------------------------------------------------------------------------
bool save_checkpoint(const char* filename, const Checkpoint& chkpt,
                     const Field2D& cc, const Field1D& ee, long nx, long ny)
{
    try {
        SafeFile fp(filename, "wb");
        
        uint32_t magic = Checkpoint::MAGIC;
        uint32_t version = Checkpoint::VERSION;
        
        fp.write(&magic, sizeof(magic), 1);
        fp.write(&version, sizeof(version), 1);
        fp.write(&nx, sizeof(nx), 1);
        fp.write(&ny, sizeof(ny), 1);
        fp.write(&chkpt.iteration, sizeof(chkpt.iteration), 1);
        fp.write(&chkpt.sim_time, sizeof(chkpt.sim_time), 1);
        fp.write(&chkpt.dt, sizeof(chkpt.dt), 1);
        fp.write(&chkpt.output_count, sizeof(chkpt.output_count), 1);
        fp.write(&chkpt.dt_adjustments, sizeof(chkpt.dt_adjustments), 1);
        fp.write(&chkpt.eta_ave, sizeof(chkpt.eta_ave), 1);
        fp.write(&chkpt.old_eta, sizeof(chkpt.old_eta), 1);
        fp.write(&chkpt.case_number, sizeof(chkpt.case_number), 1);
        fp.write(&chkpt.coeff_dt, sizeof(chkpt.coeff_dt), 1);
        
        for (long i = 0; i <= nx; ++i) {
            fp.write(cc.physical_row_data(i), sizeof(double), ny + 1);
        }

        fp.write(ee.physical_data(), sizeof(double), nx + 1);
        
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Checkpoint save error: %s\n", e.what());
        return false;
    }
}
//-----------------------------------------------------------------------------
// 加载检查点
//-----------------------------------------------------------------------------
bool load_checkpoint(const char* filename, Checkpoint& chkpt,
                     Field2D& cc, Field1D& ee, long nx, long ny)
{
    try {
        SafeFile fp(filename, "rb");
        
        uint32_t magic, version;
        fp.read(&magic, sizeof(magic), 1);
        fp.read(&version, sizeof(version), 1);
        
        if (magic != Checkpoint::MAGIC) {
            std::fprintf(stderr, "Invalid checkpoint file (bad magic number)\n");
            return false;
        }
        if (version != Checkpoint::VERSION) {
            std::fprintf(stderr, "Incompatible checkpoint version\n");
            return false;
        }
        
        long file_nx, file_ny;
        fp.read(&file_nx, sizeof(file_nx), 1);
        fp.read(&file_ny, sizeof(file_ny), 1);
        
        if (file_nx != nx || file_ny != ny) {
            std::fprintf(stderr, "Grid size mismatch\n");
            return false;
        }
        
        fp.read(&chkpt.iteration, sizeof(chkpt.iteration), 1);
        fp.read(&chkpt.sim_time, sizeof(chkpt.sim_time), 1);
        fp.read(&chkpt.dt, sizeof(chkpt.dt), 1);
        fp.read(&chkpt.output_count, sizeof(chkpt.output_count), 1);
        fp.read(&chkpt.dt_adjustments, sizeof(chkpt.dt_adjustments), 1);
        fp.read(&chkpt.eta_ave, sizeof(chkpt.eta_ave), 1);
        fp.read(&chkpt.old_eta, sizeof(chkpt.old_eta), 1);
        fp.read(&chkpt.case_number, sizeof(chkpt.case_number), 1);
        fp.read(&chkpt.coeff_dt, sizeof(chkpt.coeff_dt), 1);
        
        for (long i = 0; i <= nx; ++i) {
            fp.read(cc.physical_row_data(i), sizeof(double), ny + 1);
        }

        fp.read(ee.physical_data(), sizeof(double), nx + 1);
        
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Checkpoint load error: %s\n", e.what());
        return false;
    }
}
