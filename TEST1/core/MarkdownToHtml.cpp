#include "core/MarkdownToHtml.h"

#include <cctype>
#include <wx/tokenzr.h>

#include <vector>

namespace {

wxString EscapeHtml(const wxString& text)
{
    wxString out;
    out.reserve(text.length() + 16);
    for (size_t i = 0; i < text.length(); ++i) {
        const wxChar c = text[i];
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c; break;
        }
    }
    return out;
}

bool IsTableRow(const wxString& line)
{
    wxString t = line;
    t.Trim(true).Trim(false);
    return !t.empty() && t[0] == '|' && t.Contains('|');
}

bool IsTableSeparator(const wxString& line)
{
    wxString t = line;
    t.Trim(true).Trim(false);
    if (!IsTableRow(t))
        return false;
    for (size_t i = 0; i < t.length(); ++i) {
        const wxChar c = t[i];
        if (c != '|' && c != '-' && c != ':' && c != ' ' && c != '\t')
            return false;
    }
    return t.Contains('-');
}

wxString FormatInline(const wxString& raw)
{
    wxString out;
    size_t i = 0;
    while (i < raw.length()) {
        if (raw[i] == '`') {
            const size_t end = raw.find('`', i + 1);
            if (end != wxString::npos) {
                out += "<code><font face=\"Consolas, Courier New, monospace\">"
                    + EscapeHtml(raw.substr(i + 1, end - i - 1))
                    + "</font></code>";
                i = end + 1;
                continue;
            }
        }
        if (i + 1 < raw.length() && raw[i] == '*' && raw[i + 1] == '*') {
            const size_t end = raw.find("**", i + 2);
            if (end != wxString::npos) {
                out += "<b>" + FormatInline(raw.substr(i + 2, end - i - 2)) + "</b>";
                i = end + 2;
                continue;
            }
        }
        if (raw[i] == '[') {
            const size_t closeLabel = raw.find(']', i + 1);
            if (closeLabel != wxString::npos && closeLabel + 1 < raw.length() && raw[closeLabel + 1] == '(') {
                const size_t closeUrl = raw.find(')', closeLabel + 2);
                if (closeUrl != wxString::npos) {
                    const wxString label = raw.substr(i + 1, closeLabel - i - 1);
                    const wxString url = raw.substr(closeLabel + 2, closeUrl - closeLabel - 2);
                    out += "<a href=\"" + EscapeHtml(url) + "\">" + FormatInline(label) + "</a>";
                    i = closeUrl + 1;
                    continue;
                }
            }
        }
        out += EscapeHtml(wxString(raw[i]));
        ++i;
    }
    return out;
}

std::vector<wxString> SplitTableCells(const wxString& line)
{
    std::vector<wxString> cells;
    wxString t = line;
    t.Trim(true).Trim(false);
    if (t.StartsWith("|"))
        t = t.Mid(1);
    if (t.EndsWith("|"))
        t = t.substr(0, t.length() - 1);

    wxString cell;
    for (size_t i = 0; i < t.length(); ++i) {
        if (t[i] == '|') {
            cell.Trim(true).Trim(false);
            cells.push_back(cell);
            cell.clear();
        } else {
            cell += t[i];
        }
    }
    cell.Trim(true).Trim(false);
    cells.push_back(cell);
    return cells;
}

wxString RenderTable(const std::vector<wxString>& rows, bool firstRowIsHeader)
{
    if (rows.empty())
        return wxEmptyString;

    wxString html = "<table cellpadding=\"4\" cellspacing=\"0\" border=\"1\" width=\"100%\">";
  bool headerDone = !firstRowIsHeader;
    for (size_t r = 0; r < rows.size(); ++r) {
        if (IsTableSeparator(rows[r]))
            continue;
        const auto cells = SplitTableCells(rows[r]);
        html += "<tr>";
        for (const wxString& c : cells) {
            if (!headerDone) {
                html += "<th bgcolor=\"#e8eef5\"><b>" + FormatInline(c) + "</b></th>";
            } else {
                html += "<td>" + FormatInline(c) + "</td>";
            }
        }
        html += "</tr>";
        headerDone = true;
    }
    html += "</table><br/>";
    return html;
}

wxString HtmlDocumentShell(const wxString& body)
{
    return wxString(
        "<html><head></head>"
        "<body bgcolor=\"#f8f9fb\" text=\"#1a1a1a\" link=\"#1565c0\" vlink=\"#1565c0\">"
        "<font face=\"Segoe UI, Microsoft YaHei UI, sans-serif\" size=\"3\">")
        + body
        + "</font></body></html>";
}

} // namespace

namespace MarkdownToHtml {

wxString Convert(const wxString& markdown)
{
    if (markdown.empty())
        return HtmlDocumentShell("<p><i>(empty)</i></p>");

    wxString body;
    wxArrayString lines = wxSplit(markdown, '\n', '\0');

    bool inCode = false;
    wxString codeBlock;
    std::vector<wxString> tableRows;
    bool inUl = false;
    bool inOl = false;
    wxString para;

    auto flushPara = [&]() {
        if (para.empty())
            return;
        para.Trim(true).Trim(false);
        if (!para.empty()) {
            body += "<p>" + FormatInline(para) + "</p>";
            para.clear();
        }
    };

    auto closeLists = [&]() {
        if (inUl) {
            body += "</ul>";
            inUl = false;
        }
        if (inOl) {
            body += "</ol>";
            inOl = false;
        }
    };

    auto flushTable = [&]() {
        if (tableRows.empty())
            return;
        body += RenderTable(tableRows, true);
        tableRows.clear();
    };

    for (size_t i = 0; i < lines.size(); ++i) {
        wxString line = lines[i];
        wxString trimmed = line;
        trimmed.Trim(true).Trim(false);

        if (trimmed.StartsWith("```")) {
            flushPara();
            closeLists();
            flushTable();
            if (inCode) {
                body += "<pre><font face=\"Consolas, Courier New, monospace\" size=\"2\">"
                    + EscapeHtml(codeBlock) + "</font></pre>";
                codeBlock.clear();
                inCode = false;
            } else {
                inCode = true;
            }
            continue;
        }

        if (inCode) {
            codeBlock += line + "\n";
            continue;
        }

        if (IsTableRow(trimmed)) {
            flushPara();
            closeLists();
            tableRows.push_back(trimmed);
            continue;
        }
        flushTable();

        if (trimmed.empty()) {
            flushPara();
            closeLists();
            continue;
        }

        if (trimmed.StartsWith("---") || trimmed.StartsWith("***")) {
            flushPara();
            closeLists();
            body += "<hr/>";
            continue;
        }

        if (trimmed.StartsWith("### ")) {
            flushPara();
            closeLists();
            body += "<h3><font size=\"4\"><b>" + FormatInline(trimmed.Mid(4)) + "</b></font></h3>";
            continue;
        }
        if (trimmed.StartsWith("## ")) {
            flushPara();
            closeLists();
            body += "<h2><font size=\"5\"><b>" + FormatInline(trimmed.Mid(3)) + "</b></font></h2>";
            continue;
        }
        if (trimmed.StartsWith("# ")) {
            flushPara();
            closeLists();
            body += "<h1><font size=\"6\"><b>" + FormatInline(trimmed.Mid(2)) + "</b></font></h1>";
            continue;
        }

        if (trimmed.StartsWith("> ")) {
            flushPara();
            closeLists();
            body += "<blockquote><i>" + FormatInline(trimmed.Mid(2)) + "</i></blockquote>";
            continue;
        }

        if (trimmed.StartsWith("- ") || trimmed.StartsWith("* ")) {
            flushPara();
            if (inOl) {
                body += "</ol>";
                inOl = false;
            }
            if (!inUl) {
                body += "<ul>";
                inUl = true;
            }
            body += "<li>" + FormatInline(trimmed.Mid(2)) + "</li>";
            continue;
        }

        if (trimmed.length() > 2 && isdigit(trimmed[0]) && trimmed.Contains('.')) {
            const size_t dot = trimmed.find('.');
            if (dot != wxString::npos && dot + 1 < trimmed.length() && trimmed[dot + 1] == ' ') {
                bool allDigits = true;
                for (size_t d = 0; d < dot; ++d) {
                    if (!isdigit(trimmed[d])) {
                        allDigits = false;
                        break;
                    }
                }
                if (allDigits) {
                    flushPara();
                    if (inUl) {
                        body += "</ul>";
                        inUl = false;
                    }
                    if (!inOl) {
                        body += "<ol>";
                        inOl = true;
                    }
                    body += "<li>" + FormatInline(trimmed.substr(dot + 2)) + "</li>";
                    continue;
                }
            }
        }

        if (!para.empty())
            para += " ";
        para += trimmed;
    }

    if (inCode && !codeBlock.empty()) {
        body += "<pre><font face=\"Consolas, Courier New, monospace\" size=\"2\">"
            + EscapeHtml(codeBlock) + "</font></pre>";
    }
    flushPara();
    closeLists();
    flushTable();

    return HtmlDocumentShell(body);
}

} // namespace MarkdownToHtml
