// PulpPluck AU v2 instrument entry point
// MUST use MusicDeviceBase (not AUEffectBase) — instruments have no audio input
#include "pulp_pluck.hpp"
#include "../../core/format/src/au_v2_instrument.cpp"
#include <pulp/format/au_v2_instrument_entry.hpp>

PULP_AU_INSTRUMENT(PulpPluckAU, pulp::examples::create_pulp_pluck)
