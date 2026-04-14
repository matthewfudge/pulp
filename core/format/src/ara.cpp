#include <pulp/format/ara.hpp>

namespace pulp::format {

bool host_supports_ara() {
#ifdef PULP_HAS_ARA
    // SDK is compiled in; actual host-query lives in the format-adapter
    // companion factories (VST3 / AU / CLAP). Pulp-side availability is
    // gated by those slices landing — workstream 06 slices 6.3..6.5.
    return false;
#else
    return false;
#endif
}

bool ara_sdk_compiled_in() {
#ifdef PULP_HAS_ARA
    return true;
#else
    return false;
#endif
}

}  // namespace pulp::format
