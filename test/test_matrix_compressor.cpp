#include "test_matrix.hpp"
#include "pulp_compressor.hpp"

using namespace pulp;
using namespace pulp::test;

static format::ProcessorFactory factory = examples::create_pulp_compressor;

TEST_CASE("PulpCompressor matrix: silence finite", "[matrix][compressor]") {
    verify_silence_produces_finite(factory);
}

TEST_CASE("PulpCompressor matrix: impulse finite", "[matrix][compressor]") {
    verify_impulse_produces_finite(factory);
}

TEST_CASE("PulpCompressor matrix: DC stable", "[matrix][compressor]") {
    verify_dc_offset_stable(factory);
}

TEST_CASE("PulpCompressor matrix: state round-trip", "[matrix][compressor]") {
    verify_state_round_trip(factory);
}
