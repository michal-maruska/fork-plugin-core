#include <gtest/gtest.h>

#include <cstdlib>

#include "../src/triqueue.h"


class testEnvironment {
public:
  void fmt_event(const char* message, int item) const {};

  void log(const char* format ...) const {
    va_list argptr;
#if 0
    va_start(argptr, format);
    // VErrorF(format, argptr);
    va_end(argptr);
#endif
  }
};


class triqueueTest : public testing::Test {
protected:
  testEnvironment environment;
  triqueue_t<int, testEnvironment> q0_{100};

  void SetUp ()
  {
    // static variable:
    triqueue_t<int, testEnvironment>::env = &environment;
  }
};



TEST_F(triqueueTest, IsEmptyInitially) {

  EXPECT_TRUE(q0_.empty());
  EXPECT_TRUE(q0_.middle_empty());
  EXPECT_TRUE(q0_.third_empty());
  // EXPECT_EQ(q0_.length(), 0);
  EXPECT_FALSE(q0_.can_pop());
}

TEST_F(triqueueTest, Push) {

  q0_.push(50); // goes in the the 3rd
  EXPECT_FALSE(q0_.empty());
  // ??
  EXPECT_TRUE(q0_.middle_empty());
  //
  EXPECT_FALSE(q0_.third_empty());
  EXPECT_FALSE(q0_.can_pop());

  EXPECT_EQ(*q0_.first(), 50);
}


TEST_F(triqueueTest, Push_And_Move) {
  q0_.push(50);

  q0_.move_to_second();
  EXPECT_FALSE(q0_.empty());
  EXPECT_FALSE(q0_.middle_empty());
  EXPECT_TRUE(q0_.third_empty());
  EXPECT_FALSE(q0_.can_pop());

}

TEST_F(triqueueTest, Push_And_overwrite_move) {
  q0_.push(50);
  q0_.move_to_second();

  // overwrite?
  const int new_value = 20;
  q0_.head() = new_value;

  q0_.move_to_first();

  // only first has values  (x)()()
  EXPECT_FALSE(q0_.empty());
  EXPECT_TRUE(q0_.middle_empty());
  EXPECT_TRUE(q0_.third_empty());
  EXPECT_TRUE(q0_.can_pop());

  // the values is overwritten:
  EXPECT_EQ(*q0_.first(), new_value);

  // pop:
  EXPECT_EQ(q0_.pop(), new_value);
  EXPECT_FALSE(q0_.can_pop());
  EXPECT_TRUE(q0_.empty());
  EXPECT_TRUE(q0_.middle_empty());
  EXPECT_TRUE(q0_.third_empty());

  EXPECT_EQ(q0_.first(), nullptr);
}
