#include <gtest/gtest.h>

#include "../src/splitqueue.h"
#define HAS_STD_LIBRARY 1
#include "../src/circular.h"


class testEnvironment {
public:
  // static void fmt_event(const char* message, int item) {};

  void log(const char* format ...) const {
#if 0
    va_list argptr;
    va_start(argptr, format);
    // VErrorF(format, argptr);
    va_end(argptr);
#endif
  }
};


class splitqueueTest : public testing::Test {
public:
    using value_t = int;
protected:
  // testEnvironment environment;
    SplitQueue<value_t, circular_buffer> q0_{100};

  void SetUp () override {
    // static variable:
    //  nothing to do
  }
};


TEST_F(splitqueueTest, IsEmptyInitially) {

  EXPECT_TRUE(q0_.empty());
  EXPECT_TRUE(q0_.front_empty());
  EXPECT_TRUE(q0_.back_empty());
  // EXPECT_EQ(q0_.length(), 0);
}

TEST_F(splitqueueTest, Push) {

  q0_.push(50); // goes in the back

  EXPECT_FALSE(q0_.empty());
  EXPECT_TRUE(q0_.front_empty());
  EXPECT_FALSE(q0_.back_empty());

  EXPECT_EQ(q0_.peek_back(), 50);

  // note: the single value is visible from the front too:
  EXPECT_EQ(q0_.peek_front(), 50);
}


TEST_F(splitqueueTest, Push_And_Move) {
  q0_.push(50);
  q0_.move_to_front();

  EXPECT_FALSE(q0_.empty());
  EXPECT_FALSE(q0_.front_empty());
  EXPECT_TRUE(q0_.back_empty());

  EXPECT_EQ(q0_.peek_front(), 50);
}

TEST_F(splitqueueTest, Push_Move_Pop) {
  q0_.push(50);
  q0_.move_to_front();
  splitqueueTest::value_t value = q0_.pop();

  EXPECT_EQ(value, 50);
  // all regions empty again:
  EXPECT_TRUE(q0_.empty());
  EXPECT_TRUE(q0_.front_empty());
  EXPECT_TRUE(q0_.back_empty());
}

// we don't need this functionality
TEST_F(splitqueueTest, Push_And_overwrite_move) {
    q0_.push(50); // value_A
  q0_.move_to_front();

  // overwrite, since we get reference
  const splitqueueTest::value_t new_value = 20; // value_B
  q0_.peek_front() = new_value;

  // only front has values  (x)()()
  EXPECT_FALSE(q0_.empty());
  EXPECT_FALSE(q0_.front_empty());
  EXPECT_TRUE(q0_.back_empty());

  // the values is overwritten:
  EXPECT_EQ(q0_.peek_front(), new_value);

  // pop:
  EXPECT_EQ(q0_.pop(), new_value);
  EXPECT_TRUE(q0_.empty());
  EXPECT_TRUE(q0_.front_empty());
  EXPECT_TRUE(q0_.back_empty());
}

TEST_F(splitqueueTest, PopOrderIsFifo) {
  q0_.push(1);
  q0_.push(2);
  q0_.push(3);

  EXPECT_EQ(q0_.peek_back(), 1);

  q0_.move_to_front();
  EXPECT_EQ(q0_.peek_front(), 1);
  EXPECT_EQ(q0_.peek_back(), 2);

  q0_.move_to_front();
  EXPECT_FALSE(q0_.back_empty()); // back=[3]

  EXPECT_EQ(q0_.pop(), 1);
  EXPECT_EQ(q0_.pop(), 2);
  EXPECT_TRUE(q0_.front_empty());

  // we can feed it again:
  q0_.push(4);
  q0_.move_to_front();
  EXPECT_EQ(q0_.pop(), 3);

  q0_.move_to_front();
  EXPECT_EQ(q0_.pop(), 4);

  EXPECT_TRUE(q0_.empty());
  EXPECT_TRUE(q0_.front_empty());
  EXPECT_TRUE(q0_.back_empty());
}


TEST_F(splitqueueTest, Rewind) {
  q0_.push(1);
  q0_.push(2);

  q0_.move_to_front();
  EXPECT_FALSE(q0_.front_empty());

  q0_.rewind();
  // everything back in the "back" part:
  EXPECT_TRUE(q0_.front_empty());
  EXPECT_FALSE(q0_.back_empty());
  EXPECT_EQ(q0_.peek_back(), 1);

  // and we can go again:
  q0_.move_to_front();
  EXPECT_EQ(q0_.pop(), 1);

  EXPECT_TRUE(q0_.front_empty());
  EXPECT_EQ(q0_.peek_back(), 2);
}

TEST_F(splitqueueTest, DropFromBack) {
  q0_.push(1);
  q0_.push(2);
  q0_.push(3);
  EXPECT_FALSE(q0_.back_empty());

  q0_.move_to_front();
  EXPECT_FALSE(q0_.front_empty()); // front=[1]

  q0_.drop_from_back(); // drops 3
  EXPECT_EQ(q0_.peek_back(), 2);
  EXPECT_EQ(q0_.peek_front(), 1); // unchanged!

  q0_.drop_from_back(); // drops 2
  EXPECT_TRUE(q0_.back_empty());
  EXPECT_EQ(q0_.peek_front(), 1); // unchanged!
  EXPECT_FALSE(q0_.empty());

  EXPECT_EQ(q0_.pop(), 1);
  EXPECT_TRUE(q0_.empty());
}

TEST_F(splitqueueTest, MoveToFrontWithoutBackIsNoOp) {
  q0_.push(5);

  q0_.move_to_front();
  EXPECT_TRUE(q0_.back_empty());

  // BUG case: ignored, no shift:
  q0_.move_to_front();
  EXPECT_FALSE(q0_.front_empty());
  EXPECT_EQ(q0_.peek_front(), 5);

  EXPECT_EQ(q0_.pop(), 5);
  EXPECT_TRUE(q0_.empty());
}

TEST_F(splitqueueTest, PushBeyondCapacityOverwritesOldest) {
    SplitQueue<int, circular_buffer> q{3};

  q.push(1);
  q.push(2);
  q.push(3);
  q.push(4); // overwrites the oldest, 1

  // front part unaffected by the overwrite:
  EXPECT_TRUE(q.front_empty());
  EXPECT_EQ(q.peek_back(), 2);

  q.move_to_front();
  EXPECT_EQ(q.pop(), 2);
  EXPECT_EQ(q.peek_back(), 3);
}
