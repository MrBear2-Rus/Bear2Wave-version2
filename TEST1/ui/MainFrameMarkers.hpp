#pragma once

class wxCommandEvent;
class MyFrame;

namespace MainFrameMarkers {

void OnShowChangeMarkerData(MyFrame& f, wxCommandEvent&);
void OnDropNamedMarker(MyFrame& f, wxCommandEvent&);
void OnCollectNamedMarker(MyFrame& f, wxCommandEvent&);
void OnCollectAllNamedMarkers(MyFrame& f, wxCommandEvent&);
void OnCopyPrimaryToBMarker(MyFrame& f, wxCommandEvent&);
void OnDeletePrimaryMarker(MyFrame& f, wxCommandEvent&);
void OnFindPreviousEdge(MyFrame& f, wxCommandEvent&);
void OnFindNextEdge(MyFrame& f, wxCommandEvent&);
void OnAlternateWheelMode(MyFrame& f, wxCommandEvent&);
void OnWaveScrolling(MyFrame& f, wxCommandEvent&);
void OnLocking(MyFrame& f, wxCommandEvent&);

} // namespace MainFrameMarkers
