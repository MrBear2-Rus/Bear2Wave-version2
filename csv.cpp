#include "csv.h"
#include <fstream>
#include <sstream>
#include <iostream>

CSVParser::CSVParser() : m_hasHeaders(false) {
}

bool CSVParser::LoadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return LoadFromString(buffer.str());
}

bool CSVParser::LoadFromString(const std::string& csvData) {
    m_data.clear();
    m_headers.clear();
    m_hasHeaders = false;

    std::istringstream stream(csvData);
    std::string line;
    size_t lineCount = 0;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        std::vector<std::string> row = ParseRow(line);
        if (row.empty()) continue;

        if (lineCount == 0) {
            m_headers = row;
            m_hasHeaders = true;
        } else {
            if (m_hasHeaders && row.size() < m_headers.size()) {
                row.resize(m_headers.size(), "");
            }
            m_data.push_back(row);
        }
        lineCount++;
    }

    return !m_data.empty() || m_hasHeaders;
}

size_t CSVParser::GetRowCount() {
    return m_data.size();
}

size_t CSVParser::GetColumnCount() {
    if (m_hasHeaders && !m_headers.empty()) {
        return m_headers.size();
    } else if (!m_data.empty()) {
        return m_data[0].size();
    }
    return 0;
}

std::vector<std::string> CSVParser::GetHeaders() {
    return m_headers;
}

bool CSVParser::HasHeaders() {
    return m_hasHeaders;
}

std::string CSVParser::GetValue(size_t row, size_t col) {
    if (row < m_data.size() && col < m_data[row].size()) {
        return m_data[row][col];
    }
    return "";
}

std::string CSVParser::GetValue(size_t row, const std::string& header) {
    if (!m_hasHeaders) {
        return "";
    }

    size_t colIndex = -1;
    for (size_t i = 0; i < m_headers.size(); i++) {
        if (m_headers[i] == header) {
            colIndex = i;
            break;
        }
    }

    if (colIndex == -1 || row >= m_data.size() || colIndex >= m_data[row].size()) {
        return "";
    }

    return m_data[row][colIndex];
}

std::vector<std::vector<std::string>> CSVParser::GetData() {
    return m_data;
}

void CSVParser::Clear() {
    m_data.clear();
    m_headers.clear();
    m_hasHeaders = false;
}

bool CSVParser::IsEmpty() {
    return m_data.empty();
}

std::vector<std::string> CSVParser::ParseRow(const std::string& row) {
    std::vector<std::string> fields;
    std::string currentField;
    bool inQuotes = false;

    for (char c : row) {
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            fields.push_back(RemoveQuotes(currentField));
            currentField.clear();
        } else {
            currentField += c;
        }
    }

    if (!currentField.empty()) {
        fields.push_back(RemoveQuotes(currentField));
    }

    return fields;
}

std::string CSVParser::RemoveQuotes(const std::string& str) {
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        return str.substr(1, str.size() - 2);
    }
    return str;
}