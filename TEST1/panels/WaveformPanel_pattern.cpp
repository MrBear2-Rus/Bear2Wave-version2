#include "panels/WaveformPanel.hpp"

#include "core/pattern_search.h"

#include <algorithm>

std::vector<signal_t*> WaveformPanel::GetPatternSearchSignals() const
{
    std::vector<signal_t*> out;
    if (m_selectedSignalIndex >= 0 && m_selectedSignalIndex < (int)m_displayedSignals2.size()) {
        signal_t* sig = m_displayedSignals2[m_selectedSignalIndex];
        if (sig && m_rowComments.find(m_selectedSignalIndex) == m_rowComments.end())
            out.push_back(sig);
        if (!out.empty())
            return out;
    }

    out.reserve(m_displayedSignals2.size());
    for (size_t i = 0; i < m_displayedSignals2.size(); ++i) {
        if (m_rowComments.find((int)i) != m_rowComments.end())
            continue;
        signal_t* sig = m_displayedSignals2[i];
        if (sig)
            out.push_back(sig);
        if (out.size() >= 500)
            break;
    }
    return out;
}

void WaveformPanel::SetLastPatternSpec(const PatternSearchSpec& spec)
{
    m_lastPatternSpec = spec;
}

void WaveformPanel::SetPatternSearchRepeatCount(int count)
{
    m_patternSearchRepeatCount = std::max(1, count);
}

void WaveformPanel::ClearPatternMarks()
{
    m_patternMarkTimes.clear();
    m_lastPatternMarkCount = 0;
    Refresh(false);
}

size_t WaveformPanel::ApplyPatternMarks(const PatternSearchSpec& spec)
{
    m_lastPatternSpec = spec;
    const uint64_t t0 = static_cast<uint64_t>(std::max(0LL, m_timeOffset));
    const uint64_t t1 = static_cast<uint64_t>(std::max(0LL, m_timeOffset + m_displayTimeRange));
    std::vector<uint64_t> hits;
    pattern_collect_matches(spec, t0, t1, hits);
    m_patternMarkTimes.assign(hits.begin(), hits.end());
    m_lastPatternMarkCount = hits.size();
    Refresh(false);
    return hits.size();
}

long long WaveformPanel::FindNextEdgeWithRepeat(long long t) const
{
    long long cur = t;
    for (int i = 0; i < m_patternSearchRepeatCount; ++i) {
        const long long next = FindNextEdge(cur);
        if (next <= cur)
            return cur;
        cur = next;
    }
    return cur;
}

long long WaveformPanel::FindPrevEdgeWithRepeat(long long t) const
{
    long long cur = t;
    for (int i = 0; i < m_patternSearchRepeatCount; ++i) {
        const long long prev = FindPrevEdge(cur);
        if (prev >= cur)
            return cur;
        cur = prev;
    }
    return cur;
}

bool WaveformPanel::PatternFind(bool forward)
{
    if (m_lastPatternSpec.empty())
        return false;

    const uint64_t from = forward
        ? static_cast<uint64_t>(std::max(0LL, m_currentTimestamp))
        : static_cast<uint64_t>(std::max(0LL, m_currentTimestamp));
    const uint64_t t0 = 0;
    const uint64_t t1 = static_cast<uint64_t>(std::max(0LL, m_maxTimestamp));
    uint64_t hit = 0;
    if (!pattern_find_from(m_lastPatternSpec, from, forward, m_patternSearchRepeatCount, hit, t0, t1))
        return false;
    SetCurrentTimestamp(static_cast<long long>(hit), true);
    return true;
}
