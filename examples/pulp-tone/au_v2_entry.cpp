// PulpTone AU v2 instrument entry point
// Uses MusicDeviceBase for synth (aumu) type
// Factory function: PulpToneAUFactory (must match Info.plist.au)

#include "pulp_tone.hpp"
#include "../../core/format/src/au_v2_instrument.cpp"
#include <pulp/format/au_v2_instrument_entry.hpp>

PULP_AU_INSTRUMENT(PulpToneAU, pulp::examples::create_pulp_tone)
