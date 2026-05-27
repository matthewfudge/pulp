// Regression for MacAddress parser + formatter (gap-doc Runtime row
// "MACAddress"). Pure value type — no platform calls, no sockets.

#include <catch2/catch_test_macros.hpp>
#include <pulp/runtime/mac_address.hpp>

using pulp::runtime::MacAddress;

TEST_CASE("MacAddress parses canonical colon form", "[runtime][mac]") {
    auto m = MacAddress::parse("aa:bb:cc:dd:ee:ff");
    REQUIRE(m.has_value());
    REQUIRE(m->octets()[0] == 0xaa);
    REQUIRE(m->octets()[5] == 0xff);
    REQUIRE(m->to_string() == "aa:bb:cc:dd:ee:ff");
}

TEST_CASE("MacAddress accepts upper-case hex digits", "[runtime][mac]") {
    auto m = MacAddress::parse("AA:BB:CC:DD:EE:FF");
    REQUIRE(m.has_value());
    REQUIRE(m->to_string() == "aa:bb:cc:dd:ee:ff");
}

TEST_CASE("MacAddress parses hyphen form", "[runtime][mac]") {
    auto m = MacAddress::parse("aa-bb-cc-dd-ee-ff");
    REQUIRE(m.has_value());
    REQUIRE(m->to_string() == "aa:bb:cc:dd:ee:ff");
    REQUIRE(m->to_string_hyphens() == "aa-bb-cc-dd-ee-ff");
}

TEST_CASE("MacAddress parses Cisco dotted-quad form", "[runtime][mac]") {
    auto m = MacAddress::parse("aabb.ccdd.eeff");
    REQUIRE(m.has_value());
    REQUIRE(m->octets()[0] == 0xaa);
    REQUIRE(m->octets()[3] == 0xdd);
}

TEST_CASE("MacAddress parses plain hex form", "[runtime][mac]") {
    auto m = MacAddress::parse("aabbccddeeff");
    REQUIRE(m.has_value());
    REQUIRE(m->to_string() == "aa:bb:cc:dd:ee:ff");
}

TEST_CASE("MacAddress trims surrounding whitespace", "[runtime][mac]") {
    auto m = MacAddress::parse("  aa:bb:cc:dd:ee:ff\n");
    REQUIRE(m.has_value());
    REQUIRE(m->oui() == 0xaabbccu);
}

TEST_CASE("MacAddress rejects bad input", "[runtime][mac]") {
    REQUIRE_FALSE(MacAddress::parse("").has_value());
    REQUIRE_FALSE(MacAddress::parse("aa:bb:cc:dd:ee").has_value());        // 10 hex
    REQUIRE_FALSE(MacAddress::parse("aa:bb:cc:dd:ee:ff:00").has_value()); // 14 hex
    REQUIRE_FALSE(MacAddress::parse("zz:bb:cc:dd:ee:ff").has_value());    // non-hex
    REQUIRE_FALSE(MacAddress::parse("aa:bb:cc-dd:ee:ff").has_value());    // mixed sep
    REQUIRE_FALSE(MacAddress::parse("aa:b:cc:dd:ee:ff").has_value());     // short group
    REQUIRE_FALSE(MacAddress::parse("aa bb cc dd ee ff").has_value());    // embedded ws
    REQUIRE_FALSE(MacAddress::is_valid("hello"));
}

TEST_CASE("MacAddress classifies multicast and broadcast bits",
          "[runtime][mac]") {
    auto unicast = MacAddress::parse("00:11:22:33:44:55");
    REQUIRE(unicast.has_value());
    REQUIRE_FALSE(unicast->is_multicast());
    REQUIRE_FALSE(unicast->is_locally_administered());
    REQUIRE_FALSE(unicast->is_broadcast());
    REQUIRE_FALSE(unicast->is_null());

    auto multicast = MacAddress::parse("01:00:5e:00:00:fb");  // mDNS
    REQUIRE(multicast.has_value());
    REQUIRE(multicast->is_multicast());

    auto local = MacAddress::parse("02:00:00:00:00:00");
    REQUIRE(local.has_value());
    REQUIRE(local->is_locally_administered());

    auto bcast = MacAddress::parse("ff:ff:ff:ff:ff:ff");
    REQUIRE(bcast.has_value());
    REQUIRE(bcast->is_broadcast());
    REQUIRE(bcast->is_multicast());  // bcast is a special case of mcast

    MacAddress zero;
    REQUIRE(zero.is_null());
    REQUIRE_FALSE(zero.is_broadcast());
}

TEST_CASE("MacAddress comparison and OUI", "[runtime][mac]") {
    auto a = MacAddress(0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff);
    auto b = MacAddress::parse("aa:bb:cc:dd:ee:ff");
    REQUIRE(b.has_value());
    REQUIRE(a == *b);
    REQUIRE(a.oui() == 0xaabbccu);

    auto c = MacAddress::parse("00:11:22:33:44:55");
    REQUIRE(c.has_value());
    REQUIRE(a != *c);
}
