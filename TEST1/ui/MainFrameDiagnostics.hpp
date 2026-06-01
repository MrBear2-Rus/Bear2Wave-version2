#pragma once

class wxString;
class MyFrame;

namespace MainFrameDiagnostics {

/** E3-5: write trace path, backend, env, viewport to a text file. */
bool ExportToFile(MyFrame& frame, const wxString& path);
void PromptExport(MyFrame& frame);

} // namespace MainFrameDiagnostics
