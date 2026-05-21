#pragma once
#include <string>
#include <vector>

class CSVParser {
private:
    std::vector<std::vector<std::string>> m_data;
    std::vector<std::string> m_headers;
    bool m_hasHeaders;

public:
    CSVParser();
    
    bool LoadFromFile(const std::string& filename);
    bool LoadFromString(const std::string& csvData);
    size_t GetRowCount();
    size_t GetColumnCount();
    std::vector<std::string> GetHeaders();
    bool HasHeaders();
    std::string GetValue(size_t row, size_t col);
    std::string GetValue(size_t row, const std::string& header);
    std::vector<std::vector<std::string>> GetData();
    void Clear();
    bool IsEmpty();

private:
    std::vector<std::string> ParseRow(const std::string& row);
    std::string RemoveQuotes(const std::string& str);
};