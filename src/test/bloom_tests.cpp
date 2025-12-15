// Copyright (c) 2012-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/bloom.h>

#include <clientversion.h>
#include <common/system.h>
#include <key.h>
#include <key_io.h>
#include <merkleblock.h>
#include <primitives/block.h>
#include <random.h>
#include <serialize.h>
#include <streams.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <vector>

#include <boost/test/unit_test.hpp>

using namespace util::hex_literals;

namespace bloom_tests {
struct BloomTest : public BasicTestingSetup {
    std::vector<unsigned char> RandomData();
};
} // namespace bloom_tests

BOOST_FIXTURE_TEST_SUITE(bloom_tests, BloomTest)

BOOST_AUTO_TEST_CASE(bloom_create_insert_serialize)
{
    CBloomFilter filter(3, 0.01, 0, BLOOM_UPDATE_ALL);

    BOOST_CHECK_MESSAGE( !filter.contains("99108ad8ed9bb6274d3980bab5a85c048f0950c8"_hex_u8), "Bloom filter should be empty!");
    filter.insert("99108ad8ed9bb6274d3980bab5a85c048f0950c8"_hex_u8);
    BOOST_CHECK_MESSAGE( filter.contains("99108ad8ed9bb6274d3980bab5a85c048f0950c8"_hex_u8), "Bloom filter doesn't contain just-inserted object!");
    // One bit different in first byte
    BOOST_CHECK_MESSAGE(!filter.contains("19108ad8ed9bb6274d3980bab5a85c048f0950c8"_hex_u8), "Bloom filter contains something it shouldn't!");

    filter.insert("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"_hex_u8);
    BOOST_CHECK_MESSAGE(filter.contains("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"_hex_u8), "Bloom filter doesn't contain just-inserted object (2)!");

    filter.insert("b9300670b4c5366e95b2699e8b18bc75e5f729c5"_hex_u8);
    BOOST_CHECK_MESSAGE(filter.contains("b9300670b4c5366e95b2699e8b18bc75e5f729c5"_hex_u8), "Bloom filter doesn't contain just-inserted object (3)!");

    DataStream stream{};
    stream << filter;

    constexpr auto expected{"03614e9b050000000000000001"_hex};
    BOOST_CHECK_EQUAL_COLLECTIONS(stream.begin(), stream.end(), expected.begin(), expected.end());

    BOOST_CHECK_MESSAGE( filter.contains("99108ad8ed9bb6274d3980bab5a85c048f0950c8"_hex_u8), "Bloom filter doesn't contain just-inserted object!");
}

BOOST_AUTO_TEST_CASE(bloom_create_insert_serialize_with_tweak)
{
    // Same test as bloom_create_insert_serialize, but we add a nTweak of 100
    CBloomFilter filter(3, 0.01, 2147483649UL, BLOOM_UPDATE_ALL);

    filter.insert("99108ad8ed9bb6274d3980bab5a85c048f0950c8"_hex_u8);
    BOOST_CHECK_MESSAGE( filter.contains("99108ad8ed9bb6274d3980bab5a85c048f0950c8"_hex_u8), "Bloom filter doesn't contain just-inserted object!");
    // One bit different in first byte
    BOOST_CHECK_MESSAGE(!filter.contains("19108ad8ed9bb6274d3980bab5a85c048f0950c8"_hex_u8), "Bloom filter contains something it shouldn't!");

    filter.insert("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"_hex_u8);
    BOOST_CHECK_MESSAGE(filter.contains("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"_hex_u8), "Bloom filter doesn't contain just-inserted object (2)!");

    filter.insert("b9300670b4c5366e95b2699e8b18bc75e5f729c5"_hex_u8);
    BOOST_CHECK_MESSAGE(filter.contains("b9300670b4c5366e95b2699e8b18bc75e5f729c5"_hex_u8), "Bloom filter doesn't contain just-inserted object (3)!");

    DataStream stream{};
    stream << filter;

    constexpr auto expected{"03ce4299050000000100008001"_hex};
    BOOST_CHECK_EQUAL_COLLECTIONS(stream.begin(), stream.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(bloom_create_insert_key)
{
    std::string strSecret = std::string("5Kg1gnAjaLfKiwhhPpGS3QfRg2m6awQvaj98JCZBZQ5SuS2F15C");
    CKey key = DecodeSecret(strSecret);
    CPubKey pubkey = key.GetPubKey();
    std::vector<unsigned char> vchPubKey(pubkey.begin(), pubkey.end());

    CBloomFilter filter(2, 0.001, 0, BLOOM_UPDATE_ALL);
    filter.insert(vchPubKey);
    uint160 hash = pubkey.GetID();
    filter.insert(hash);

    DataStream stream{};
    stream << filter;

    constexpr auto expected{"038fc16b080000000000000001"_hex};
    BOOST_CHECK_EQUAL_COLLECTIONS(stream.begin(), stream.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(bloom_match)
{
    // Random real transaction (0f2e9924b1e175a5fc55d26f38b1d50a52380eebaa12c6484920a2c7f5065818)
    DataStream stream{
        "020000000001018eee3117ad6619f59b5ffe82469fac06568b509d6589b08c0cff39f8bb374fb0000000000000fdffffff0200e1f505000000001600149cda0de94e384ca38c057124a489523b4d92637cc0ca4403000000001600141a0a0776e138371bb68034648589f2940fc7e43f0247304402204e96d50850737750de5bfbc797cdac06045657945dc487b5cb83233e77d40f19022010db62f9daa3eecabdb543c81d39636b71e77228f5dbfd3adc66e59b7ab8763101210263f7df4d9b8ba0035e28325e2643b762d97185ff147df713fc58dceb6746544fe8030000"_hex,
    };
    CTransaction tx(deserialize, TX_WITH_WITNESS, stream);


    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(uint256{"0f2e9924b1e175a5fc55d26f38b1d50a52380eebaa12c6484920a2c7f5065818"});
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match tx hash");
}

BOOST_AUTO_TEST_CASE(merkle_block_1)
{
    CBlock block = getBlock13b8a();
    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the last transaction
    filter.insert(uint256{"c7718378c699827024e99795289ddfc97157462ff96aacfe9569336213d78bf2"});

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK_EQUAL(merkleBlock.header.GetHash().GetHex(), block.GetHash().GetHex());

    BOOST_CHECK_EQUAL(merkleBlock.vMatchedTxn.size(), 1U);
}

BOOST_AUTO_TEST_CASE(merkle_block_2)
{
    // Random real block (000000005a4ded781e667e06ceefafb71410b511fe0d5adc3e5a27ecbec34ae6)
    // With 4 txes
    CBlock block = getBlock13b8a();

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the first transaction
    filter.insert(uint256{"99a6e318a9a82bfc07c0dc21a2cc9f1e71cff56763422ba3d505efd9ddc9583c"});

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256{"99a6e318a9a82bfc07c0dc21a2cc9f1e71cff56763422ba3d505efd9ddc9583c"});
}

BOOST_AUTO_TEST_CASE(merkle_block_2_with_update_none)
{
    // Random real block (000000005a4ded781e667e06ceefafb71410b511fe0d5adc3e5a27ecbec34ae6)
    // With 4 txes
    CBlock block = getBlock13b8a();

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_NONE);
    // Match the first transaction
    filter.insert(uint256{"99a6e318a9a82bfc07c0dc21a2cc9f1e71cff56763422ba3d505efd9ddc9583c"});

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());
}


std::vector<unsigned char> BloomTest::RandomData()
{
    uint256 r = m_rng.rand256();
    return std::vector<unsigned char>(r.begin(), r.end());
}

BOOST_AUTO_TEST_CASE(rolling_bloom)
{
    SeedRandomForTest(SeedRand::ZEROS);

    // last-100-entry, 1% false positive:
    CRollingBloomFilter rb1(100, 0.01);

    // Overfill:
    static const int DATASIZE=399;
    std::vector<unsigned char> data[DATASIZE];
    for (int i = 0; i < DATASIZE; i++) {
        data[i] = RandomData();
        rb1.insert(data[i]);
    }
    // Last 100 guaranteed to be remembered:
    for (int i = 299; i < DATASIZE; i++) {
        BOOST_CHECK(rb1.contains(data[i]));
    }

    // false positive rate is 1%, so we should get about 100 hits if
    // testing 10,000 random keys. We get worst-case false positive
    // behavior when the filter is as full as possible, which is
    // when we've inserted one minus an integer multiple of nElement*2.
    unsigned int nHits = 0;
    for (int i = 0; i < 10000; i++) {
        if (rb1.contains(RandomData()))
            ++nHits;
    }
    // Expect about 100 hits
    BOOST_CHECK_EQUAL(nHits, 71U);

    BOOST_CHECK(rb1.contains(data[DATASIZE-1]));
    rb1.reset();
    BOOST_CHECK(!rb1.contains(data[DATASIZE-1]));

    // Now roll through data, make sure last 100 entries
    // are always remembered:
    for (int i = 0; i < DATASIZE; i++) {
        if (i >= 100)
            BOOST_CHECK(rb1.contains(data[i-100]));
        rb1.insert(data[i]);
        BOOST_CHECK(rb1.contains(data[i]));
    }

    // Insert 999 more random entries:
    for (int i = 0; i < 999; i++) {
        std::vector<unsigned char> d = RandomData();
        rb1.insert(d);
        BOOST_CHECK(rb1.contains(d));
    }
    // Sanity check to make sure the filter isn't just filling up:
    nHits = 0;
    for (int i = 0; i < DATASIZE; i++) {
        if (rb1.contains(data[i]))
            ++nHits;
    }
    // Expect about 5 false positives
    BOOST_CHECK_EQUAL(nHits, 3U);

    // last-1000-entry, 0.01% false positive:
    CRollingBloomFilter rb2(1000, 0.001);
    for (int i = 0; i < DATASIZE; i++) {
        rb2.insert(data[i]);
    }
    // ... room for all of them:
    for (int i = 0; i < DATASIZE; i++) {
        BOOST_CHECK(rb2.contains(data[i]));
    }
}

BOOST_AUTO_TEST_SUITE_END()
