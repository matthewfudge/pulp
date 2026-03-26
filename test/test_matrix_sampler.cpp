#include "test_matrix.hpp"
#include "pulp_sampler.hpp"

using namespace pulp;
using namespace pulp::test;

// PulpSampler needs a sample loaded to produce meaningful output.
// For matrix tests we just verify it doesn't crash without one.
static format::ProcessorFactory factory = examples::create_pulp_sampler;

TEST_CASE("PulpSampler matrix: silence finite", "[matrix][sampler]") {
    verify_silence_produces_finite(factory);
}

TEST_CASE("PulpSampler matrix: state round-trip", "[matrix][sampler]") {
    verify_state_round_trip(factory);
}

TEST_CASE("PulpSampler matrix: zero-length buffer", "[matrix][sampler]") {
    verify_zero_length_buffer(factory);
}
