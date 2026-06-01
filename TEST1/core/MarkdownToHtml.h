#pragma once

#include <wx/string.h>

namespace MarkdownToHtml {

/** Convert a Markdown subset (headings, lists, tables, code, links) to wxHtml-compatible HTML. */
wxString Convert(const wxString& markdown);

} // namespace MarkdownToHtml
