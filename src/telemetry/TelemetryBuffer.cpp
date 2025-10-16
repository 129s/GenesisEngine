#include "genesis/telemetry/TelemetryBuffer.hpp"

namespace genesis::telemetry {

TelemetryBuffer::TelemetryBuffer(std::size_t maxEntries)
    : m_maxEntries(maxEntries) {
    if (m_maxEntries == 0) {
        m_maxEntries = 1;
    }
}

void TelemetryBuffer::push(TickTelemetry entry) {
    if (m_entries.size() >= m_maxEntries) {
        m_entries.pop_front();
    }
    m_entries.push_back(std::move(entry));
}

} // namespace genesis::telemetry

