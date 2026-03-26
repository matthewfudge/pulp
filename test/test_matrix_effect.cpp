#include "test_matrix.hpp"
#include "pulp_effect.hpp"

using namespace pulp;
using namespace pulp::test;

static format::ProcessorFactory factory = examples::create_pulp_effect;

TEST_CASE("PulpEffect matrix: silence finite", "[matrix][effect]") {
    verify_silence_produces_finite(factory);
}

TEST_CASE("PulpEffect matrix: impulse finite", "[matrix][effect]") {
    verify_impulse_produces_finite(factory);
}

TEST_CASE("PulpEffect matrix: DC stable", "[matrix][effect]") {
    verify_dc_offset_stable(factory);
}

TEST_CASE("PulpEffect matrix: state round-trip", "[matrix][effect]") {
    verify_state_round_trip(factory);
}

TEST_CASE("PulpEffect matrix: zero-length buffer", "[matrix][effect]") {
    verify_zero_length_buffer(factory);
}
