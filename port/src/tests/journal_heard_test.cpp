#include <gtest/gtest.h>

#include "journal/heard.hpp"

namespace dc::test {

namespace {

class JournalHeard : public ::testing::Test {
protected:
    void SetUp() override { journal::LoadChunk(nullptr); }

    void TearDown() override { journal::LoadChunk(nullptr); }
};

TEST_F(JournalHeard, AVillagersRequestIsHeardOnceTheySayIt) {
    EXPECT_FALSE(journal::RequestHeard(0, 8));
    journal::NoteTalkLine(0, 8, journal::RequestMessage(8) + 2); // the reply once the request is met
    journal::NoteTalkLine(0, 7, journal::RequestMessage(8));     // another villager's message number
    journal::NoteTalkLine(1, 8, journal::RequestMessage(8));     // the same villager number in another town
    EXPECT_FALSE(journal::RequestHeard(0, 8));
    journal::NoteTalkLine(0, 8, journal::RequestMessage(8));
    EXPECT_TRUE(journal::RequestHeard(0, 8));
    EXPECT_TRUE(journal::RequestHeard(1, 8));
    EXPECT_FALSE(journal::RequestHeard(0, 7));
}

TEST_F(JournalHeard, OutsideTheTownsNothingIsHeard) {
    journal::NoteTalkLine(99, 0, journal::RequestMessage(0));
    journal::NoteTalkLine(0, 16, journal::RequestMessage(16));
    EXPECT_FALSE(journal::RequestHeard(99, 0));
    EXPECT_FALSE(journal::RequestHeard(0, 16));
    EXPECT_FALSE(journal::SaveChunk().has_value());
}

TEST_F(JournalHeard, TheSaveKeepsWhatWasHeardAndANewGameForgetsIt) {
    EXPECT_FALSE(journal::SaveChunk().has_value());
    journal::NoteTalkLine(0, 15, journal::RequestMessage(15));
    journal::NoteTalkLine(4, 3, journal::RequestMessage(3));
    const auto chunk = journal::SaveChunk();
    ASSERT_TRUE(chunk.has_value());

    journal::LoadChunk(nullptr);
    EXPECT_FALSE(journal::RequestHeard(0, 15));

    journal::LoadChunk(&*chunk);
    EXPECT_TRUE(journal::RequestHeard(0, 15));
    EXPECT_TRUE(journal::RequestHeard(4, 3));
    EXPECT_FALSE(journal::RequestHeard(4, 4));
}

TEST_F(JournalHeard, AChunkThisVersionCannotReadHearsNothing) {
    journal::NoteTalkLine(2, 1, journal::RequestMessage(1));
    auto chunk = *journal::SaveChunk();
    chunk.version = 2;
    journal::LoadChunk(&chunk);
    EXPECT_FALSE(journal::RequestHeard(2, 1));
    chunk.version = 1;
    chunk.payload.pop_back();
    journal::LoadChunk(&chunk);
    EXPECT_FALSE(journal::RequestHeard(2, 1));
}

} // namespace

} // namespace dc::test
