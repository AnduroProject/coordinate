// Copyright (c) 2024-2026 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * PQHD Test Vector Verification
 *
 * Validates the PQHD key derivation engine against the official BIP-360
 * test vectors from p2mr_pqhd_derivation_paths.json.
 *
 * This file can be compiled standalone or integrated into the test suite.
 * Compile: g++ -std=c++20 -I src/ pqhd_test.cpp wallet/pqhd.cpp ... -o pqhd_test
 */

#include <wallet/pqhd.h>
#include <util/strencodings.h>
#include <logging.h>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace wallet;

// BIP39 test mnemonic: "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"
// with empty passphrase produces this 64-byte seed:
static const char* BIP39_TEST_SEED_HEX =
    "5eb00bbddcf069084889a8ab9155568165f5c453ccb85e70811aaed6f6da5fc1"
    "9a5ac40b389cd370d086206dec8aa6c43daea6690f20ad3d8d48b2d2ce9e38e4";

struct TestVector {
    std::string id;
    std::string path;
    std::string expected_child_seed;
    std::string expected_chain_code;
    std::string expected_secret_key;
    std::string expected_public_key;  // xonly for schnorr, raw for slh-dsa
};

static const std::vector<TestVector> test_vectors = {
    {
        "schnorr_m_360_0_0_340_0_0",
        "m/360'/0'/0'/340'/0'/0",
        "b4e4bd8eea76c1f080e45a88be3ef68351d3817b1882a5caa7cbc64833ec9d34",
        "470a546a3d44684e66d11962bc8650b41e9a09f567f8ec33003dab8b9c2c92be",
        "b4e4bd8eea76c1f080e45a88be3ef68351d3817b1882a5caa7cbc64833ec9d34",
        "ac65830ba524dd01a36d9023f073f823ba5fcc7ec2d0a7f2327e654a77f3dab6"
    },
    {
        "schnorr_m_360_0_0_340_0_1",
        "m/360'/0'/0'/340'/0'/1",
        "ec8ced710207d36facc72e4e2fded40051b947216061530ff44b7c8b7f295690",
        "a915902cdcd022fd106b73636cf72a9cf025a7de1cb9e40958be8e5d35296439",
        "ec8ced710207d36facc72e4e2fded40051b947216061530ff44b7c8b7f295690",
        "376459967d6f266d0afad3f1436e59a4996483796e8c0711a9be9323c1fd80b5"
    },
    {
        "slh_dsa_m_360_0_0_205_0_0",
        "m/360'/0'/0'/205'/0'/0",
        "399bbbb52c3efa319e2eca9eb27aaeabb67335438838aa707a719d75eb4626a0",
        "cc184e74f25ed50d85d05e9190a827ee4d14272513c9c5c8ea9fad21f066e14a",
        "66956a2328f4d0d618f447fe356226dd507d22ba29fc83c6ae0dd320bdfe8576"
        "f38969a30c0045999d7f850b8215a825ae8f6243b8b1d1a86d7c35194c6f7794",
        "f38969a30c0045999d7f850b8215a825ae8f6243b8b1d1a86d7c35194c6f7794"
    }
};

bool RunPQHDTests()
{
    std::cout << "=== PQHD Derivation Test Vector Verification ===" << std::endl;

    // Parse test seed
    std::vector<unsigned char> bip39_seed = ParseHex(BIP39_TEST_SEED_HEX);
    assert(bip39_seed.size() == 64);

    // Initialize derivation engine
    PQHDKeyDerivation engine;
    if (!engine.SetSeed(bip39_seed)) {
        std::cerr << "FAIL: SetSeed failed" << std::endl;
        return false;
    }

    bool all_passed = true;

    for (const auto& tv : test_vectors) {
        std::cout << "\nTest: " << tv.id << std::endl;
        std::cout << "  Path: " << tv.path << std::endl;

        // Parse path
        auto path_opt = PQHDPath::Parse(tv.path);
        if (!path_opt) {
            std::cerr << "  FAIL: Could not parse path" << std::endl;
            all_passed = false;
            continue;
        }

        // Derive through path
        PQHDNode node;
        if (!engine.DerivePath(*path_opt, node)) {
            std::cerr << "  FAIL: DerivePath failed" << std::endl;
            all_passed = false;
            continue;
        }

        // Check child seed
        std::string got_seed = HexStr(node.seed);
        if (got_seed != tv.expected_child_seed) {
            std::cerr << "  FAIL: childSeed mismatch" << std::endl;
            std::cerr << "    expected: " << tv.expected_child_seed << std::endl;
            std::cerr << "    got:      " << got_seed << std::endl;
            all_passed = false;
        } else {
            std::cout << "  PASS: childSeed" << std::endl;
        }

        // Check chain code
        std::string got_cc = HexStr(std::vector<unsigned char>(
            node.chaincode.begin(), node.chaincode.end()));
        if (got_cc != tv.expected_chain_code) {
            std::cerr << "  FAIL: chainCode mismatch" << std::endl;
            std::cerr << "    expected: " << tv.expected_chain_code << std::endl;
            std::cerr << "    got:      " << got_cc << std::endl;
            all_passed = false;
        } else {
            std::cout << "  PASS: chainCode" << std::endl;
        }

        // Generate keypair
        PQHDKeyPair keypair;
        if (!engine.GenerateKeyPair(node, path_opt->algorithm, keypair)) {
            std::cerr << "  FAIL: GenerateKeyPair failed" << std::endl;
            all_passed = false;
            continue;
        }

        // Check secret key
        std::string got_sk = HexStr(keypair.secret_key);
        if (got_sk != tv.expected_secret_key) {
            std::cerr << "  FAIL: secretKey mismatch" << std::endl;
            std::cerr << "    expected: " << tv.expected_secret_key << std::endl;
            std::cerr << "    got:      " << got_sk << std::endl;
            all_passed = false;
        } else {
            std::cout << "  PASS: secretKey (" << keypair.secret_key.size() << " bytes)" << std::endl;
        }

        // Check public key
        std::string got_pk = HexStr(keypair.public_key);
        if (got_pk != tv.expected_public_key) {
            std::cerr << "  FAIL: publicKey mismatch" << std::endl;
            std::cerr << "    expected: " << tv.expected_public_key << std::endl;
            std::cerr << "    got:      " << got_pk << std::endl;
            all_passed = false;
        } else {
            std::cout << "  PASS: publicKey (" << keypair.public_key.size() << " bytes)" << std::endl;
        }
    }

    // Test PQHDPath parsing roundtrip
    std::cout << "\nTest: Path parsing roundtrip" << std::endl;
    {
        auto p = PQHDPath::Parse("m/360'/0'/0'/340'/0'/42");
        assert(p.has_value());
        assert(p->purpose == 360);
        assert(p->coin_type == 0);
        assert(p->account == 0);
        assert(p->algorithm == 340);
        assert(p->change == 0);
        assert(p->index == 42);
        assert(p->ToString() == "m/360'/0'/0'/340'/0'/42");
        std::cout << "  PASS: Schnorr path roundtrip" << std::endl;
    }
    {
        auto p = PQHDPath::Parse("m/360'/0'/0'/205'/1'/5");
        assert(p.has_value());
        assert(p->algorithm == 205);
        assert(p->change == 1);
        assert(p->index == 5);
        std::cout << "  PASS: SLH-DSA path roundtrip" << std::endl;
    }
    {
        auto p = PQHDPath::Parse("m/44'/0'/0'/0/0");
        assert(!p.has_value()); // Not BIP-360
        std::cout << "  PASS: Rejects non-BIP360 path" << std::endl;
    }
    {
        auto p = PQHDPath::Parse("m/360'/0'/0'/999'/0'/0");
        assert(!p.has_value()); // Unknown algorithm
        std::cout << "  PASS: Rejects unknown algorithm" << std::endl;
    }

    // Test DeriveP2MRKeyPairs convenience method
    std::cout << "\nTest: DeriveP2MRKeyPairs" << std::endl;
    {
        PQHDKeyPair schnorr, slhdsa;
        if (!engine.DeriveP2MRKeyPairs(0, 0, 0, 0, schnorr, slhdsa)) {
            std::cerr << "  FAIL: DeriveP2MRKeyPairs returned false" << std::endl;
            all_passed = false;
        } else {
            // Schnorr should match test vector 0
            std::string schnorr_pk = HexStr(schnorr.public_key);
            assert(schnorr_pk == "ac65830ba524dd01a36d9023f073f823ba5fcc7ec2d0a7f2327e654a77f3dab6");
            std::cout << "  PASS: Schnorr pubkey matches" << std::endl;

            // SLH-DSA should match test vector 2
            std::string slhdsa_pk = HexStr(slhdsa.public_key);
            assert(slhdsa_pk == "f38969a30c0045999d7f850b8215a825ae8f6243b8b1d1a86d7c35194c6f7794");
            std::cout << "  PASS: SLH-DSA pubkey matches" << std::endl;

            assert(schnorr.algorithm == PQHD_ALGO_SCHNORR);
            assert(slhdsa.algorithm == PQHD_ALGO_SLH_DSA);
            std::cout << "  PASS: Algorithm types correct" << std::endl;
        }
    }

    std::cout << "\n=== Results: " << (all_passed ? "ALL PASSED" : "SOME FAILED") << " ===" << std::endl;
    return all_passed;
}
