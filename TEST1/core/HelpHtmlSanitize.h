#pragma once

#include <wx/string.h>

namespace HelpHtmlSanitize {

/** Make help HTML safer for wxHtmlWindow (limited tag parser). */
inline wxString Prepare(wxString html)
{
    if (html.empty())
        return html;

    html.Replace("<tr><td bgcolor=\"#1e3a5f\" cellpadding=\"14\">", "<tr><td bgcolor=\"#1e3a5f\">");
    html.Replace("<tr><td cellpadding=\"18\">", "<tr><td>");
    html.Replace("<tr><td bgcolor=\"#e2e8f0\" cellpadding=\"8\" align=\"center\">",
        "<tr><td bgcolor=\"#e2e8f0\" align=\"center\">");

    html.Replace("<h2><font color=\"#1565c0\"><b>", "<p><font color=\"#1565c0\" size=\"4\"><b>");
    html.Replace("</b></font></h2>", "</b></font></p>");
    html.Replace("<h3><font color=\"#334155\"><b>", "<p><font color=\"#334155\" size=\"3\"><b>");
    html.Replace("</b></font></h3>", "</b></font></p>");
    html.Replace("<strong>", "<b>");
    html.Replace("</strong>", "</b>");

    if (!html.Lower().Contains("<html"))
        html = "<html><body bgcolor=\"#eef2f7\">" + html + "</body></html>";

    return html;
}

} // namespace HelpHtmlSanitize
