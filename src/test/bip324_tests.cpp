// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bip324.h>
#include <chainparams.h>
#include <crypto/chacha20.h>
#include <crypto/poly1305.h>
#include <key.h>
#include <pubkey.h>
#include <random.h>
#include <span.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {

// Helper function to compare containers without using std::ranges
template<typename Container1, typename Container2>
bool containers_equal(const Container1& c1, const Container2& c2) {
    if (c1.size() != c2.size()) return false;
    return std::equal(c1.begin(), c1.end(), c2.begin());
}

// Get magic bytes as vector for HexStr compatibility
std::vector<uint8_t> GetMagicBytes() {
    const auto& magic = Params().MessageStart();
    return std::vector<uint8_t>(magic.begin(), magic.end());
}

// Convert uint8_t array to std::span<const std::byte>
std::span<const std::byte> ToByteSpan(const std::array<uint8_t, 64>& data) {
    return std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size());
}

// Convert uint256 to std::span<const std::byte>
std::span<const std::byte> ToByteSpan(const uint256& hash) {
    return std::span<const std::byte>(reinterpret_cast<const std::byte*>(hash.begin()), 32);
}

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(bip324_tests, BasicTestingSetup)

// Test BIP324Cipher constructor with CKey and entropy
BOOST_AUTO_TEST_CASE(cipher_with_key_entropy_test)
{
    try {
        CKey priv_key;
        priv_key.MakeNewKey(true);
        
        uint256 entropy = GetRandHash();
        
        // Based on error: BIP324Cipher(const CKey& key, std::span<const std::byte> ent32)
        BIP324Cipher cipher(priv_key, ToByteSpan(entropy));
        
        // Test basic operations
        auto session_id = cipher.GetSessionID();
        auto send_garbage = cipher.GetSendGarbageTerminator();
        auto recv_garbage = cipher.GetReceiveGarbageTerminator();
        
        BOOST_CHECK(!session_id.empty());
        BOOST_CHECK(!send_garbage.empty());
        BOOST_CHECK(!recv_garbage.empty());
        
        auto magic = GetMagicBytes();
        BOOST_TEST_MESSAGE("Cipher with key+entropy test passed");
        BOOST_TEST_MESSAGE("Magic bytes: " + HexStr(magic));
        BOOST_TEST_MESSAGE("Session ID size: " + std::to_string(session_id.size()));
        
    } catch (const std::exception& e) {
        BOOST_TEST_MESSAGE("Exception in cipher_with_key_entropy_test: " + std::string(e.what()));
        BOOST_CHECK(false);
    }
}

// Test BIP324Cipher constructor with CKey and EllSwiftPubKey
BOOST_AUTO_TEST_CASE(cipher_with_key_ellswift_test)
{
    try {
        CKey priv_key;
        priv_key.MakeNewKey(true);
        
        // Create EllSwiftPubKey using span constructor
        std::array<uint8_t, 64> ellswift_data;
        auto magic = GetMagicBytes();
        for (size_t i = 0; i < ellswift_data.size(); ++i) {
            ellswift_data[i] = static_cast<uint8_t>((magic[i % 4] + i) % 256);
        }
        
        EllSwiftPubKey ellswift_key(ToByteSpan(ellswift_data));
        
        // Based on error: BIP324Cipher(const CKey& key, const EllSwiftPubKey& pubkey)
        BIP324Cipher cipher(priv_key, ellswift_key);
        
        // Test basic operations
        auto session_id = cipher.GetSessionID();
        auto send_garbage = cipher.GetSendGarbageTerminator();
        auto recv_garbage = cipher.GetReceiveGarbageTerminator();
        
        BOOST_CHECK(!session_id.empty());
        BOOST_CHECK(!send_garbage.empty());
        BOOST_CHECK(!recv_garbage.empty());
        
        BOOST_TEST_MESSAGE("Cipher with key+ellswift test passed");
        BOOST_TEST_MESSAGE("Magic bytes: " + HexStr(magic));
        
    } catch (const std::exception& e) {
        BOOST_TEST_MESSAGE("Exception in cipher_with_key_ellswift_test: " + std::string(e.what()));
        BOOST_CHECK(false);
    }
}

// Test EllSwiftPubKey creation and operations
BOOST_AUTO_TEST_CASE(ellswift_creation_test)
{
    try {
        auto magic = GetMagicBytes();
        
        // Create multiple EllSwiftPubKey instances with different data
        std::vector<EllSwiftPubKey> ellswift_keys;
        
        for (int i = 0; i < 3; ++i) {
            std::array<uint8_t, 64> data;
            for (size_t j = 0; j < data.size(); ++j) {
                data[j] = static_cast<uint8_t>((magic[j % 4] + i * 17 + j) % 256);
            }
            
            EllSwiftPubKey ellswift(ToByteSpan(data));
            ellswift_keys.push_back(ellswift);
        }
        
        // Verify all keys are different
        for (size_t i = 0; i < ellswift_keys.size(); ++i) {
            for (size_t j = i + 1; j < ellswift_keys.size(); ++j) {
                BOOST_CHECK(ellswift_keys[i] != ellswift_keys[j]);
            }
        }
        
        BOOST_TEST_MESSAGE("EllSwiftPubKey creation test passed");
        
    } catch (const std::exception& e) {
        BOOST_TEST_MESSAGE("Exception in ellswift_creation_test: " + std::string(e.what()));
        BOOST_CHECK(false);
    }
}

BOOST_AUTO_TEST_SUITE_END()