#pragma once
#ifndef LIBVCD_VCD_H
#define LIBVCD_VCD_H

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
#define VCD_SIGNAL_COUNT 100
/** Initial capacity when appending value changes (grows exponentially). */
#define VCD_VALUE_CHANGES_INITIAL_CAP 64
#define VCD_SIGNAL_SIZE 64
#define VCD_NAME_SIZE 32
#define VCD_TIME_UNIT_SIZE 8
#define VCD_VERSION_SIZE 64
#define VCD_DATE_SIZE 64

typedef uint64_t timestamp_t;



typedef struct {
    timestamp_t timestamp;
    char value[VCD_SIGNAL_SIZE];
} value_change_t;

typedef struct signal_node signal_node_t;

typedef struct {
    char name[VCD_NAME_SIZE];
    char module_path[VCD_SIGNAL_SIZE]; // 新增：存储信号所属模块路径（如TOP/TOP.full_adder）
    char full_name[VCD_SIGNAL_SIZE]; // 新增：存储完整信号名（如TOP.a/TOP.full_adder.u1.sum）
    char signal_id[VCD_NAME_SIZE];
    size_t size;
    /** From FST hierarchy (fstVarType); -1 if unknown / VCD-only / GEOM fallback. */
    int32_t fst_var_type;
    value_change_t* value_changes;
    size_t changes_count;
    size_t changes_capacity;
    /** 1 if value_changes timestamps are sorted (enables binary probe). */
    uint8_t changes_sorted;
    /** 1 if lazy backend has loaded VC data for this signal. */
    uint8_t trace_data_loaded;
    /** Inclusive loaded time span (valid when trace_data_loaded). */
    uint64_t trace_loaded_t0;
    uint64_t trace_loaded_t1;
} signal_t;

/** Trace storage backend attached to vcd_t (see trace_loader / *\_loader). */
enum vcd_trace_backend_t {
    VCD_TRACE_BACKEND_NONE = 0,
    VCD_TRACE_BACKEND_FST_LAZY = 1,
    VCD_TRACE_BACKEND_VZT_LAZY = 2,
    VCD_TRACE_BACKEND_LXT2_LAZY = 3,
    VCD_TRACE_BACKEND_GHW_LAZY = 4
};

/** Full-file load window for trace_load_signals. */
#define TRACE_LOAD_T0_FULL 0u
#define TRACE_LOAD_T1_FULL UINT64_MAX

typedef struct {
    char unit[VCD_TIME_UNIT_SIZE];
    size_t scale;
} timescale_t;

typedef struct signal_node {
    signal_t signal;
    struct signal_node* next;
} signal_node_t;

typedef struct {
    size_t signals_count;
    signal_node_t* signals_head;  // 头指针
    char date[VCD_DATE_SIZE];
    char version[VCD_VERSION_SIZE];
    timescale_t timescale;
    char current_module_path[VCD_NAME_SIZE];
    /** Opaque session (e.g. FstTraceSession*); freed in vcd_free. */
    void* trace_session;
    uint8_t trace_backend;
    /** File end time when hierarchy-only open (FST lazy); 0 = derive from changes. */
    uint64_t trace_max_timestamp;
} vcd_t;

struct SignalGroup {
    std::string name;
    std::vector<SignalGroup*> subGroups;
    std::vector<signal_t*> signals;
    bool collapsed = false; // UI 折叠用
    SignalGroup* parent = nullptr; // 新增：父节点引用，便于回溯

    // 新增：析构函数，递归释放子节点（避免内存泄漏）
    ~SignalGroup() {
        for (auto sg : subGroups) {
            delete sg;
        }
        subGroups.clear();
        signals.clear();
    }

    // 新增：查找子分组（按名称）
    SignalGroup* find_subgroup(const std::string& group_name) {
        for (auto sg : subGroups) {
            if (sg->name == group_name) {
                return sg;
            }
        }
        return nullptr;
    }
};
#ifdef __cplusplus
extern "C" {
#endif

    vcd_t* vcd_read_from_path(char* path);

    /** Allocate empty vcd_t (same as internal new_vcd); used by FST loader. */
    vcd_t* vcd_alloc_empty(void);

    /** Free dynamic value-change buffer for one signal (heap CSV signals, etc.). */
    void signal_free_value_changes(signal_t* sig);

    /** Append one change; grows buffer as needed. Returns 0 on success, -1 on OOM / invalid args. */
    int vcd_signal_append_change(signal_t* sig, timestamp_t ts, const char* value);

    /** Append only if ts is outside sig's loaded lazy span (incremental trace load). */
    int vcd_signal_append_change_lazy(signal_t* sig, timestamp_t ts, const char* value);

    /** Realloc value_changes to exact count after load (reduces memory overhead). */
    void vcd_signal_shrink_to_fit(signal_t* sig);

    
    signal_t* vcd_get_signal_by_name(vcd_t* vcd, const char* signal_name);

    char* vcd_signal_get_value_at_timestamp(signal_t* signal, timestamp_t timestamp);

    timestamp_t vcd_get_max_timestamp(vcd_t* vcd);
    // 原代码：无内存释放函数声明
    std::string ParseBusValue(const std::string& v);

    //int get_signal_index(const char* string);
    // 新增：解析模块路径为层级列表（如 "TOP.full_adder" → ["TOP", "full_adder"]）
    void split_module_path(const char* full_path, std::vector<std::string>& out_parts);
    // 新增：校验模块路径格式（避免非法字符）
    bool validate_module_path(const char* path);

    SignalGroup* BuildSignalTree(vcd_t* vcd);
    // 释放信号树内存
    void FreeSignalTree(SignalGroup* root);

    void vcd_free(vcd_t* vcd);

    /** Sort one signal's value_changes by timestamp; no-op if already sorted. */
    void vcd_sort_signal_value_changes(signal_t* sig);

    /** Sort every signal's value_changes by timestamp. */
    void vcd_sort_all_value_changes(vcd_t* vcd);

    /** Ensure sorted before binary search / draw (sorts once per signal). */
    void vcd_ensure_signal_sorted(signal_t* sig);

    /** Clear value changes and lazy-load metadata for one signal. */
    void vcd_signal_clear_trace_data(signal_t* sig);

#ifdef __cplusplus
}
#endif

#endif // LIBVCD_VCD_H
