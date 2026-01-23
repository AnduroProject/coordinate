// Copyright (c) 2014-2023 Daniel Kraft
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <auxpow.h>
#include <chainparams.h>
#include <consensus/merkle.h>
#include <primitives/block.h>
#include <script/script.h>
#include <uint256.h>

#include <test/util/setup_common.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(auxpow_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(init_auxpow_creates_valid_structure)
{
    // Create a block header
    CBlockHeader header;
    header.nVersion = 1;
    header.hashPrevBlock = uint256{};
    header.hashMerkleRoot = uint256{};
    header.nTime = 1234567890;
    header.nBits = 0x1d00ffff;
    header.nNonce = 12345;
    
    // Initialize auxpow - this sets the auxpow flag and creates the structure
    CPureBlockHeader& parent = CAuxPow::initAuxPow(header);
    
    // Check that auxpow flag is set
    BOOST_CHECK(header.IsAuxpow());
    
    // Check that auxpow object was created
    BOOST_CHECK(header.auxpow != nullptr);
    
    // Check that parent block was created
    BOOST_CHECK_EQUAL(parent.nVersion, 1);
    
    // Check that parent block hash can be retrieved
    uint256 parentHash = header.auxpow->getParentBlockHash();
    BOOST_CHECK(!parentHash.IsNull());
}

BOOST_AUTO_TEST_CASE(create_auxpow_creates_minimal_structure)
{
    // Create a block header with auxpow flag already set
    CBlockHeader header;
    header.nVersion = 1;
    header.hashPrevBlock = uint256{};
    header.hashMerkleRoot = uint256{};
    header.nTime = 1234567890;
    header.nBits = 0x1d00ffff;
    header.nNonce = 12345;
    header.SetAuxpowVersion(true);
    
    // Create minimal auxpow
    auto auxpow = CAuxPow::createAuxPow(header);
    
    // Check that auxpow was created
    BOOST_CHECK(auxpow != nullptr);
    
    // Check that parent block exists
    BOOST_CHECK_EQUAL(auxpow->getParentBlock().nVersion, 1);
    
    // Check that parent hash can be retrieved
    uint256 parentHash = auxpow->getParentBlockHash();
    BOOST_CHECK(!parentHash.IsNull());
}

BOOST_AUTO_TEST_CASE(expected_index_is_deterministic)
{
    // Test that getExpectedIndex returns consistent values
    const uint32_t nonce = 12345;
    const int chainId = 1;
    const unsigned height = 4;
    
    int index1 = CAuxPow::getExpectedIndex(nonce, chainId, height);
    int index2 = CAuxPow::getExpectedIndex(nonce, chainId, height);
    
    // Same inputs should give same output
    BOOST_CHECK_EQUAL(index1, index2);
    
    // Index should be within valid range
    const uint32_t maxIndex = (1u << height);
    BOOST_CHECK(index1 >= 0 && index1 < (int)maxIndex);
}

BOOST_AUTO_TEST_CASE(expected_index_varies_with_nonce)
{
    const int chainId = 1;
    const unsigned height = 4;
    
    // Different nonces should generally give different indices
    int index1 = CAuxPow::getExpectedIndex(1000, chainId, height);
    int index2 = CAuxPow::getExpectedIndex(2000, chainId, height);
    int index3 = CAuxPow::getExpectedIndex(3000, chainId, height);
    
    // At height 4, we have 16 possible positions, so likely different
    BOOST_CHECK(index1 != index2 || index1 != index3);
}

BOOST_AUTO_TEST_CASE(expected_index_varies_with_chain_id)
{
    const uint32_t nonce = 12345;
    const unsigned height = 4;
    
    // Different chain IDs should generally give different indices
    int index1 = CAuxPow::getExpectedIndex(nonce, 1, height);
    int index2 = CAuxPow::getExpectedIndex(nonce, 2, height);
    int index3 = CAuxPow::getExpectedIndex(nonce, 3, height);
    
    // At height 4, we have 16 possible positions, so likely different
    BOOST_CHECK(index1 != index2 || index1 != index3);
}

BOOST_AUTO_TEST_CASE(expected_index_at_height_zero)
{
    // At height 0, there's only one position (index must be 0)
    const uint32_t nonce = 12345;
    const int chainId = 1;
    const unsigned height = 0;
    
    int index = CAuxPow::getExpectedIndex(nonce, chainId, height);
    
    BOOST_CHECK_EQUAL(index, 0);
}

BOOST_AUTO_TEST_CASE(auxpow_parent_block_has_valid_properties)
{
    CBlockHeader header;
    header.nVersion = 1;
    header.hashPrevBlock = uint256{};
    header.hashMerkleRoot = uint256{};
    header.nTime = 1234567890;
    header.nBits = 0x1d00ffff;
    header.nNonce = 12345;
    
    // Initialize auxpow
    CPureBlockHeader& parent = CAuxPow::initAuxPow(header);
    
    // Parent should have valid version
    BOOST_CHECK_EQUAL(parent.nVersion, 1);
    
    // Parent hash merkle root should not be null (it has a coinbase tx)
    BOOST_CHECK(!parent.hashMerkleRoot.IsNull());
    
    // Parent hash should be computable
    uint256 parentHash = parent.GetHash();
    BOOST_CHECK(!parentHash.IsNull());
}

BOOST_AUTO_TEST_CASE(multiple_auxpow_init_creates_different_parents)
{
    // Create two different headers
    CBlockHeader header1;
    header1.nVersion = 1;
    header1.nNonce = 11111;
    
    CBlockHeader header2;
    header2.nVersion = 1;
    header2.nNonce = 22222;
    
    // Initialize auxpow for both
    CPureBlockHeader& parent1 = CAuxPow::initAuxPow(header1);
    CPureBlockHeader& parent2 = CAuxPow::initAuxPow(header2);
    
    // Both should have auxpow
    BOOST_CHECK(header1.IsAuxpow());
    BOOST_CHECK(header2.IsAuxpow());
    
    // Parent hashes should be different (different block hashes committed)
    BOOST_CHECK(parent1.GetHash() != parent2.GetHash());
}

BOOST_AUTO_TEST_CASE(auxpow_fails_with_wrong_block_hash)
{
    const Consensus::Params& params = Params().GetConsensus();
    const int chainId = params.nAuxpowChainId;
    
    // Create first header
    CBlockHeader header1;
    header1.nVersion = 1;
    header1.nNonce = 11111;
    CAuxPow::initAuxPow(header1);
    const uint256 blockHash1 = header1.GetHash();
    
    // Create second header with different nonce
    CBlockHeader header2;
    header2.nVersion = 1;
    header2.nNonce = 22222;
    header2.SetAuxpowVersion(true);
    const uint256 blockHash2 = header2.GetHash();
    
    // auxpow from header1 should not validate with blockHash2
    BOOST_CHECK(!header1.auxpow->check(blockHash2, chainId, params));
}

BOOST_AUTO_TEST_CASE(auxpow_parent_hash_is_consistent)
{
    CBlockHeader header;
    header.nVersion = 1;
    header.nTime = 1234567890;
    header.nBits = 0x1d00ffff;
    header.nNonce = 12345;
    
    // Initialize auxpow
    CPureBlockHeader& parent = CAuxPow::initAuxPow(header);
    
    // Get parent hash from reference and from auxpow
    uint256 parentHashFromRef = parent.GetHash();
    uint256 parentHashFromAuxpow = header.auxpow->getParentBlockHash();
    
    // Both should be the same
    BOOST_CHECK_EQUAL(parentHashFromRef, parentHashFromAuxpow);
}

BOOST_AUTO_TEST_CASE(create_auxpow_requires_auxpow_flag)
{
    // Create header WITHOUT auxpow flag
    CBlockHeader header;
    header.nVersion = 1;
    
    // createAuxPow expects the flag to be set (will assert in debug mode)
    // In release mode, behavior is undefined
    // This test just documents the requirement
    
    // Correct usage: set flag before calling createAuxPow
    header.SetAuxpowVersion(true);
    auto auxpow = CAuxPow::createAuxPow(header);
    BOOST_CHECK(auxpow != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()