#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vcd.h"
#include <algorithm>
#include <string>
#include <unordered_map>

// 扩展支持的赋值值字符（包含x/z/X/Z）
#define isexpression(c) (strchr("-0123456789zZxXbU", (c)))

#define BUFFER_LENGTH 512

typedef enum {
    BEFORE_MODULE_DEFINITIONS,
    INSIDE_TOP_MODULE,
    INSIDE_INNER_MODULES
} state_t;

extern "C" void signal_free_value_changes(signal_t* sig)
{
    if (!sig)
        return;
    free(sig->value_changes);
    sig->value_changes = nullptr;
    sig->changes_count = 0;
    sig->changes_capacity = 0;
}

void vcd_signal_clear_trace_data(signal_t* sig)
{
    if (!sig)
        return;
    signal_free_value_changes(sig);
    sig->trace_data_loaded = 0;
    sig->trace_loaded_t0 = 0;
    sig->trace_loaded_t1 = 0;
    sig->changes_sorted = 0;
}

extern "C" int vcd_signal_append_change(signal_t* sig, timestamp_t ts, const char* value)
{
    if (!sig || !value)
        return -1;
    if (sig->changes_count >= sig->changes_capacity) {
        size_t ncap = sig->changes_capacity ? (sig->changes_capacity * 2u) : (size_t)VCD_VALUE_CHANGES_INITIAL_CAP;
        value_change_t* nv = (value_change_t*)realloc(sig->value_changes, ncap * sizeof(value_change_t));
        if (!nv)
            return -1;
        sig->value_changes = nv;
        sig->changes_capacity = ncap;
    }
    sig->changes_sorted = 0;
    value_change_t* ch = &sig->value_changes[sig->changes_count++];
    ch->timestamp = ts;
    strncpy(ch->value, value, VCD_SIGNAL_SIZE - 1);
    ch->value[VCD_SIGNAL_SIZE - 1] = '\0';
    return 0;
}

extern "C" int vcd_signal_append_change_lazy(signal_t* sig, timestamp_t ts, const char* value)
{
    if (!sig || !value)
        return -1;
    if (sig->trace_data_loaded) {
        const uint64_t uts = static_cast<uint64_t>(ts);
        if (uts >= sig->trace_loaded_t0 && uts <= sig->trace_loaded_t1)
            return 0;
    }
    return vcd_signal_append_change(sig, ts, value);
}

extern "C" void vcd_signal_shrink_to_fit(signal_t* sig)
{
    if (!sig)
        return;
    if (sig->changes_count == 0) {
        signal_free_value_changes(sig);
        return;
    }
    if (sig->changes_capacity > sig->changes_count) {
        value_change_t* nv = (value_change_t*)realloc(
            sig->value_changes, sig->changes_count * sizeof(value_change_t));
        if (nv) {
            sig->value_changes = nv;
            sig->changes_capacity = sig->changes_count;
        }
    }
}

static vcd_t* new_vcd();
static bool parse_instruction(FILE* file, vcd_t* vcd, state_t* state, std::unordered_map<std::string, signal_t*>* id_map);
static bool parse_timestamp(FILE* file, timestamp_t* timestamp);
static bool parse_assignment(FILE* file, vcd_t* vcd, timestamp_t timestamp, std::unordered_map<std::string, signal_t*>* id_map);
/** FST/部分路径下变化未必按时间追加；排序后缓存与 RawValueAtOrBefore 的假设一致。 */
static void sort_signal_value_changes_chronological(signal_t* sig)
{
    if (!sig || !sig->value_changes || sig->changes_count < 2u)
        return;
    std::stable_sort(
        sig->value_changes,
        sig->value_changes + sig->changes_count,
        [](const value_change_t& a, const value_change_t& b) { return a.timestamp < b.timestamp; });
}
static int get_signal_index(const char* string);
// 新增：日志辅助函数（调试用，可按需删除）
static void log_signal_info(vcd_t* vcd);
// 新增：模块路径管理函数声明（核心新增）
static void update_module_path(vcd_t* vcd, const char* module_name, bool is_upscope);

vcd_t* vcd_read_from_path(char* path) {
    FILE* file = fopen(path, "r");
    if (file == NULL)
        return NULL;

    vcd_t* vcd = new_vcd();
    timestamp_t current_timestamp = 0;
    state_t state = BEFORE_MODULE_DEFINITIONS;
    std::unordered_map<std::string, signal_t*> id_to_sig;
    id_to_sig.reserve(4096);

    int character = 0;
    while ((character = fgetc(file)) != EOF) {
        if (character == '$') {
            bool successful = parse_instruction(file, vcd, &state, &id_to_sig);
            if (successful)
                continue;
        }
        else if (character == '#') {
            bool successful = parse_timestamp(file, &current_timestamp);
            if (successful)
                continue;
        }
        else if (isexpression(character)) {
            ungetc(character, file);
            bool successful = parse_assignment(file, vcd, current_timestamp, &id_to_sig);
            if (successful)
                continue;
        }
        else if (isspace(character)) {
            continue; // 跳过空白字符
        }
        else {
            // 原逻辑错误：遇到未知字符直接释放资源返回NULL，改为跳过
            continue;
        }
    }

    for (signal_node_t* node = vcd->signals_head; node; node = node->next)
        vcd_sort_signal_value_changes(&node->signal);

    // 调试：打印解析到的信号信息
    log_signal_info(vcd);

    fclose(file);
    return vcd;
}

signal_t* vcd_get_signal_by_name(vcd_t* vcd, const char* signal_name) {
    if (vcd == NULL || signal_name == NULL) return NULL;
    signal_node_t* node = vcd->signals_head;
    while (node) {
        if (strcmp(node->signal.full_name, signal_name) == 0)
            return &node->signal;
        node = node->next;
    }
    return NULL;
}

char* vcd_signal_get_value_at_timestamp(signal_t* signal, timestamp_t timestamp) {
    if (signal == NULL || !signal->value_changes || signal->changes_count == 0) return NULL;
    /* 与 WaveformPanel::RawValueAtOrBefore 一致：不要求 changes 按时间升序 */
    timestamp_t best_ts = 0;
    char* best_val = NULL;
    bool have = false;
    for (size_t i = 0; i < signal->changes_count; ++i) {
        value_change_t* vc = &signal->value_changes[i];
        if (vc->timestamp > timestamp) continue;
        if (!have || vc->timestamp > best_ts) {
            best_ts = vc->timestamp;
            best_val = vc->value;
            have = true;
        } else if (vc->timestamp == best_ts) {
            best_val = vc->value;
        }
    }
    if (have) return best_val;
    return (char*)"";
}

// 新增：内存释放函数（解决内存泄漏）
/** Implemented in trace_loader.cpp (releases FST session, etc.). */
extern void vcd_trace_session_release(vcd_t* vcd);

void vcd_free(vcd_t* vcd) {
    if (!vcd) return;

    vcd_trace_session_release(vcd);

    signal_node_t* node = vcd->signals_head;
    while (node) {
        signal_node_t* next = node->next;
        signal_free_value_changes(&node->signal);
        free(node);
        node = next;
    }
    free(vcd);
}

vcd_t* new_vcd() {
    vcd_t* vcd = (vcd_t*)calloc(1, sizeof(vcd_t));
    // 初始化默认值，避免空指针
    if (vcd) {
        memset(vcd->date, 0, VCD_DATE_SIZE);
        memset(vcd->version, 0, VCD_VERSION_SIZE);
        memset(vcd->timescale.unit, 0, VCD_TIME_UNIT_SIZE);
        vcd->timescale.scale = 1;
        // 新增：初始化当前模块路径为空
        memset(vcd->current_module_path, 0, VCD_NAME_SIZE);
    }
    return vcd;
}

vcd_t* vcd_alloc_empty(void) { return new_vcd(); }

void vcd_sort_signal_value_changes(signal_t* sig)
{
    if (!sig || sig->changes_sorted)
        return;
    sort_signal_value_changes_chronological(sig);
    sig->changes_sorted = 1;
}

void vcd_ensure_signal_sorted(signal_t* sig)
{
    vcd_sort_signal_value_changes(sig);
}

void vcd_sort_all_value_changes(vcd_t* vcd)
{
    if (!vcd)
        return;
    for (signal_node_t* node = vcd->signals_head; node; node = node->next)
        vcd_sort_signal_value_changes(&node->signal);
}

/*
// 新增：模块路径拼接/回退核心函数
static void update_module_path(vcd_t* vcd, const char* module_name, bool is_upscope) {
    if (vcd == NULL) return;

    if (is_upscope) {
        // 回退路径：如 TOP.full_adder → TOP
        char* last_dot = strrchr(vcd->current_module_path, '.');
        if (last_dot != NULL) {
            *last_dot = '\0'; // 截断最后一个模块名
        }
        else {
            memset(vcd->current_module_path, 0, VCD_NAME_SIZE); // 清空路径
        }
    }
    else {
        // 拼接路径：如 TOP + full_adder → TOP.full_adder
        if (module_name == NULL || *module_name == '\0') return;

        if (strlen(vcd->current_module_path) > 0) {
            snprintf(vcd->current_module_path, VCD_NAME_SIZE,
                "%s.%s", vcd->current_module_path, module_name);
        }
        else {
            strncpy(vcd->current_module_path, module_name, VCD_NAME_SIZE - 1);
        }
        vcd->current_module_path[VCD_NAME_SIZE - 1] = '\0'; // 防止溢出
    }
}
*/
bool parse_instruction(FILE* file, vcd_t* vcd, state_t* state, std::unordered_map<std::string, signal_t*>* id_map) {
    char instruction[BUFFER_LENGTH];
    if (fscanf(file, "%511s", instruction) != 1)
        return false;

    // 忽略无关指令
    if (strcmp(instruction, "end") == 0 ||
        strcmp(instruction, "dumpvars") == 0 ||
        strcmp(instruction, "dumpall") == 0 ||
        strcmp(instruction, "comment") == 0)
    {
        return true;
    }

    // ====================== 进入模块：拼接路径 ======================
    if (strcmp(instruction, "scope") == 0) {
        char module_type[BUFFER_LENGTH];
        char module_name[BUFFER_LENGTH];
        fscanf(file, " %511s %511s", module_type, module_name);

        // 进入子模块，拼接路径
        update_module_path(vcd, module_name, false);
        return true;
    }

    // ====================== 退出模块：回退路径 ======================
    if (strcmp(instruction, "upscope") == 0 ||
        strcmp(instruction, "enddefinitions") == 0)
    {
        // 退出当前模块，路径回退
        update_module_path(vcd, NULL, true);
        return true;
    }

    // ====================== 解析信号（核心修复区） ======================
    if (strcmp(instruction, "var") == 0) {
        // 信号数量上限保护
       

        signal_node_t* new_node = (signal_node_t*)calloc(1, sizeof(signal_node_t));
        if (!new_node) return false;

        signal_t* signal = &new_node->signal;
        memset(signal, 0, sizeof(signal_t));
        signal->fst_var_type = -1;

        char type[BUFFER_LENGTH];
        char signal_id[VCD_NAME_SIZE];
        char signal_name[VCD_NAME_SIZE];

        // 【正确顺序 1】先读取信号的所有信息
        int ret = fscanf(file,
            " %511s %zu %31[^ ] %31[^ $]%*[^$]",
            type,
            &signal->size,
            signal_id,
            signal_name
        );

        // 读取失败 → 释放内存，返回
        if (ret != 4) {
            free(new_node);
            return false;
        }

        // 【正确顺序 2】复制信号名、ID
        strncpy(signal->name, signal_name, VCD_NAME_SIZE - 1);
        signal->name[VCD_NAME_SIZE - 1] = '\0';

        strncpy(signal->signal_id, signal_id, VCD_NAME_SIZE - 1);
        signal->signal_id[VCD_NAME_SIZE - 1] = '\0';

        // 【正确顺序 3】复制当前模块路径（最关键）
        strncpy(signal->module_path, vcd->current_module_path, VCD_NAME_SIZE - 1);
        signal->module_path[VCD_NAME_SIZE - 1] = '\0';

        // 【正确顺序 4】生成完整信号名：模块路径.信号名
        if (strlen(signal->module_path) > 0) {
            snprintf(signal->full_name, VCD_NAME_SIZE,
                "%s.%s",
                signal->module_path,
                signal->name
            );
        }
        else {
            strncpy(signal->full_name, signal->name, VCD_NAME_SIZE - 1);
        }
        signal->full_name[VCD_NAME_SIZE - 1] = '\0';

        // 插入信号链表
        if (vcd->signals_head == NULL) {
            vcd->signals_head = new_node;
        }
        else {
            signal_node_t* p = vcd->signals_head;
            while (p->next) p = p->next;
            p->next = new_node;
        }

        vcd->signals_count++;
        if (id_map)
            (*id_map)[std::string(signal->signal_id)] = signal;
        return true;
    }

    // ====================== 解析文件头信息 ======================
    if (strcmp(instruction, "date") == 0) {
        fscanf(file, "\n%63[^$\n]", vcd->date);
        return true;
    }

    if (strcmp(instruction, "version") == 0) {
        fscanf(file, "\n%63[^$\n]", vcd->version);
        return true;
    }

    if (strcmp(instruction, "timescale") == 0) {
        fscanf(file, "\n%zu%7[^ \n$]", &vcd->timescale.scale, vcd->timescale.unit);
        return true;
    }

    return false;
}

bool parse_timestamp(FILE* file, timestamp_t* timestamp) {
    unsigned long long v = 0;
    if (fscanf(file, " %llu", &v) != 1)
        return false;
    *timestamp = (timestamp_t)v;
    return true;
}

bool parse_assignment(FILE* file, vcd_t* vcd, timestamp_t timestamp, std::unordered_map<std::string, signal_t*>* id_map)
{
    char buffer[BUFFER_LENGTH];

    if (fgets(buffer, BUFFER_LENGTH, file) == NULL)
        return false;

    char value[VCD_SIGNAL_SIZE] = { 0 };
    char signal_id[VCD_NAME_SIZE] = { 0 };

    bool is_vector = (strchr("01xXzZ", buffer[0]) == NULL);

    int parse_count = 0;
    char scanfmt[48];

    // 解析赋值格式（必须限制宽度，否则宽向量 b... 会写爆 value[] / signal_id[]）
    if (is_vector)
    {
        snprintf(scanfmt, sizeof scanfmt, "%%%d[^ ] %%%d[^\n ]",
            (int)(VCD_SIGNAL_SIZE - 1), (int)(VCD_NAME_SIZE - 1));
        parse_count = sscanf(buffer, scanfmt, value, signal_id);
    }
    else
    {
        snprintf(scanfmt, sizeof scanfmt, "%%1s%%%d[^\n ]", (int)(VCD_NAME_SIZE - 1));
        parse_count = sscanf(buffer, scanfmt, value, signal_id);
    }

    if (parse_count != 2)
        return false;

    value[VCD_SIGNAL_SIZE - 1] = '\0';
    signal_id[VCD_NAME_SIZE - 1] = '\0';

    if (!id_map)
        return true;
    auto it = id_map->find(std::string(signal_id));
    if (it == id_map->end())
        return true;
    signal_t* signal = it->second;
    if (signal && vcd_signal_append_change(signal, timestamp, value) != 0) {
        static int once = 0;
        if (!once) {
            fprintf(stderr, "[VCD] vcd_signal_append_change failed (out of memory?)\n");
            once = 1;
        }
    }
    return true;
}
timestamp_t vcd_get_max_timestamp(vcd_t* vcd) {
    if (vcd == NULL) return 0;

    if (vcd->trace_max_timestamp > 0)
        return (timestamp_t)vcd->trace_max_timestamp;

    timestamp_t max_ts = 0;

    signal_node_t* node = vcd->signals_head;
    while (node) {
        signal_t* signal = &node->signal;
        if (signal->value_changes) {
            for (size_t j = 0; j < signal->changes_count; ++j) {
                value_change_t* change = &signal->value_changes[j];
                if (change->timestamp > max_ts)
                    max_ts = change->timestamp;
            }
        }
        node = node->next;
    }

    return max_ts;
}






// 新增：调试日志 - 打印解析到的所有信号信息（含模块路径）
static void log_signal_info(vcd_t* vcd) {
    if (vcd == NULL) return;
    const char* dbg = getenv("BEAR2WAVE_VCD_DEBUG");
    if (!dbg || dbg[0] == '\0' || dbg[0] == '0')
        return;

    printf("===== VCD解析结果 =====\n");

    // 计算信号总数
    size_t total_signals = 0;
    signal_node_t* node = vcd->signals_head;
    while (node) {
        total_signals++;
        node = node->next;
    }
    printf("信号总数：%zu\n", total_signals);

    // 遍历信号链表打印信息
    size_t i = 0;
    node = vcd->signals_head;
    while (node) {
        signal_t* sig = &node->signal;
        printf("信号[%zu]：原始名称=%s | 模块路径=%s | 完整名称=%s | ID=%s | 位宽=%zu | 变化次数=%zu\n",
            i, sig->name, sig->module_path, sig->full_name, sig->signal_id,
            sig->size, sig->changes_count);
        i++;
        node = node->next;
    }

    printf("========================\n");
}
std::string ParseBusValue(const std::string& v)
{
    if (v.empty()) return "";

    // VCD vector: b1010
    if (v[0] == 'b' || v[0] == 'B')
    {
        return v.substr(1);  // 去掉 'b'
    }

    return v;
}

int get_signal_index(const char* string) {
    if (string == NULL || *string == '\0') return -1;
    int id = -1;
    if (isdigit((unsigned char)*string)) {
        id = *string - '0';
    }
    else if (islower((unsigned char)*string)) {
        id = *string - 'a' + 10;
    }
    else if (isupper((unsigned char)*string)) {
        id = *string - 'A' + 36;
    }
    else {
        id = *string - '!';
    }
    return (id >= 0 ) ? id : -1;
}

// vcd.cpp 替换原有 update_module_path 函数
static void update_module_path(vcd_t* vcd, const char* module_name, bool is_upscope) {
    if (!vcd) return;

    /* current_module_path 在 vcd_t 中只有 VCD_NAME_SIZE 字节，勿用 VCD_SIGNAL_SIZE */
    const size_t cap = VCD_NAME_SIZE;

    // 回退路径（upscope/enddefinitions）
    if (is_upscope) {
        char* last_dot = strrchr(vcd->current_module_path, '.');
        if (last_dot != NULL) {
            *last_dot = '\0'; // 截断最后一个模块（如 TOP.full → TOP）
        }
        else {
            memset(vcd->current_module_path, 0, cap);
        }
        return;
    }

    // 拼接路径（scope指令）
    if (!module_name || *module_name == '\0' || strlen(module_name) >= cap - 1) {
        return; // 过滤空/超长模块名
    }

    size_t curr_len = strlen(vcd->current_module_path);
    size_t new_name_len = strlen(module_name);
    if (curr_len + new_name_len + 2 > cap) {
        fprintf(stderr, "模块路径过长：%s + %s\n", vcd->current_module_path, module_name);
        return;
    }

    if (curr_len > 0) {
        snprintf(vcd->current_module_path + curr_len, cap - curr_len, ".%s", module_name);
    }
    else {
        strncpy(vcd->current_module_path, module_name, cap - 1);
    }
    vcd->current_module_path[cap - 1] = '\0';
}

// 新增：模块路径切割工具函数（供信号树构建用）
void split_module_path(const char* full_path, std::vector<std::string>& out_parts) {
    out_parts.clear();
    if (!full_path || *full_path == '\0') return;

    std::string path_str(full_path);
    size_t start = 0;
    size_t pos = path_str.find('.');
    while (pos != std::string::npos) {
        out_parts.push_back(path_str.substr(start, pos - start));
        start = pos + 1;
        pos = path_str.find('.', start);
    }
    // 最后一段
    if (start < path_str.size()) {
        out_parts.push_back(path_str.substr(start));
    }
}

// 新增：路径格式校验（避免非法字符）
bool validate_module_path(const char* path) {
    if (!path) return false;
    for (const char* p = path; *p; p++) {
        // 仅允许 字母/数字/下划线/点
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '.') {
            return false;
        }
    }
    return true;
}
// vcd.cpp 新增信号树构建逻辑
SignalGroup* BuildSignalTree(vcd_t* vcd) {
    if (!vcd || !vcd->signals_head) {
        return nullptr;
    }

    // 根节点
    SignalGroup* root = new SignalGroup();
    root->name = "ROOT";

    // 遍历所有信号，按模块路径构建树
    signal_node_t* node = vcd->signals_head;
    while (node) {
        signal_t* sig = &node->signal;
        std::vector<std::string> path_parts;
        // 切割模块路径（如 "TOP.full_adder" → ["TOP", "full_adder"]）
        split_module_path(sig->module_path, path_parts);

        // 从根节点开始，逐层查找/创建子分组
        SignalGroup* current_group = root;
        for (const auto& part : path_parts) {
            SignalGroup* sub_group = current_group->find_subgroup(part);
            if (!sub_group) {
                // 不存在则创建新分组
                sub_group = new SignalGroup();
                sub_group->name = part;
                sub_group->parent = current_group;
                current_group->subGroups.push_back(sub_group);
            }
            current_group = sub_group;
        }

        // 将当前信号添加到最终的分组中
        current_group->signals.push_back(sig);
        node = node->next;
    }

    return root;
}

// 新增：释放信号树内存（对外暴露）
void FreeSignalTree(SignalGroup* root) {
    if (root) {
        delete root; // 触发析构函数，递归释放子节点
    }
}