// PulpGain AU v2 entry point
// The adapter source is included directly (it defines PulpAUEffect)
// Factory function: PulpGainAUFactory (must match Info.plist.au factoryFunction)

#include "pulp_gain.hpp"

// Include the adapter implementation (defines pulp::format::au::PulpAUEffect)
#include "../../core/format/src/au_v2_adapter.cpp"

#include <pulp/format/au_v2_entry.hpp>

PULP_AU_PLUGIN(PulpGainAU, pulp::examples::create_pulp_gain)
