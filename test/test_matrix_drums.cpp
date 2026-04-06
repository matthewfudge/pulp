#include "test_matrix.hpp"
#include "pulp_drums.hpp"

using namespace pulp;
using namespace pulp::test;

static format::ProcessorFactory factory = examples::create_pulp_drums;

TEST_CASE("PulpDrums matrix: silence finite", "[matrix][drums]") {
    verify_silence_produces_finite(factory);
}

TEST_CASE("PulpDrums matrix: state round-trip", "[matrix][drums]") {
    verify_state_round_trip(factory);
}

TEST_CASE("PulpDrums matrix: zero-length buffer", "[matrix][drums]") {
    verify_zero_length_buffer(factory);
}
