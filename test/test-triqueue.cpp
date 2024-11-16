#include <gtest/gtest.h>

#include <cstdlib>

#include "../src/triqueue.h"


class testEnvironment {
public:
  virtual void log(const char* format ...) const {
    va_list argptr;
#if 0
    va_start(argptr, format);
    // VErrorF(format, argptr);
    va_end(argptr);
#endif
  }
};

testEnvironment environment;

class triqueueTest : public testing::Test {
protected:
  triqueue_t<int, testEnvironment> q0_{100};

};


TEST_F(triqueueTest, IsEmptyInitially) {

  triqueue_t<int, testEnvironment>::env = &environment;

    EXPECT_TRUE(q0_.empty());
    EXPECT_TRUE(q0_.middle_empty());
    EXPECT_TRUE(q0_.third_empty());
  // EXPECT_EQ(q0_.length(), 0);
    EXPECT_FALSE(q0_.can_pop());

    q0_.push(50);
    EXPECT_FALSE(q0_.empty());
    EXPECT_FALSE(q0_.middle_empty());
    EXPECT_FALSE(q0_.third_empty());
    EXPECT_FALSE(q0_.can_pop());

    EXPECT_EQ(*q0_.first(), 50);


    q0_.move_to_second();
    EXPECT_FALSE(q0_.empty());
    EXPECT_FALSE(q0_.middle_empty());
    EXPECT_TRUE(q0_.third_empty());
    EXPECT_FALSE(q0_.can_pop());

    const int new_value = 20;
    q0_.rewrite_head() = new_value;

    q0_.move_to_first();

    EXPECT_FALSE(q0_.empty());
    EXPECT_TRUE(q0_.middle_empty());
    EXPECT_TRUE(q0_.third_empty());

    EXPECT_TRUE(q0_.can_pop());

    EXPECT_EQ(*q0_.first(), new_value);

    EXPECT_EQ(q0_.pop(), new_value);
    EXPECT_FALSE(q0_.can_pop());
    EXPECT_TRUE(q0_.empty());
    EXPECT_TRUE(q0_.middle_empty());
    EXPECT_TRUE(q0_.third_empty());

    EXPECT_EQ(q0_.first(), nullptr);
}


