#pragma once
#ifndef LIBVCD_VCD_H
#define LIBVCD_VCD_H

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
#define VCD_SIGNAL_COUNT 100
#define VCD_VALUE_CHANGE_COUNT 200000
#define VCD_SIGNAL_SIZE 64
#define VCD_NAME_SIZE 32
#define VCD_TIME_UNIT_SIZE 8
#define VCD_VERSION_SIZE 64
#define VCD_DATE_SIZE 64

typedef uint32_t timestamp_t;



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
    value_change_t value_changes[VCD_VALUE_CHANGE_COUNT];
    size_t changes_count;
} signal_t;

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
// �����޸ģ�������C����������extern "C"������C++����
#ifdef __cplusplus
extern "C" {
#endif

    vcd_t* vcd_read_from_path(char* path);

    
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


   
// 修改后：新增
    void vcd_free(vcd_t* vcd);

#ifdef __cplusplus
}
#endif

#endif // LIBVCD_VCD_H