#include "test_matrix.hpp"
#include "pulp_tone.hpp"

using namespace pulp;
using namespace pulp::test;

static format::ProcessorFactory factory = examples::create_pulp_tone;

TEST_CASE("PulpTone matrix: silence finite", "[matrix][tone]") {
    verify_silence_produces_finite(factory);
}

TEST_CASE("PulpTone matrix: state round-trip", "[matrix][tone]") {
    verify_state_round_trip(factory);
}

TEST_CASE("PulpTone matrix: zero-length buffer", "[matrix][tone]") {
    verify_zero_length_buffer(factory);
}
