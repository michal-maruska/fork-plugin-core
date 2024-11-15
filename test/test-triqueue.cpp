#include <gtest/gtest.h>

#include "../src/triqueue.h"


class triqueueTest : public testing::Test {
protected:
  triqueue_t<int> q0_{100};
};


TEST_F(triqueueTest, IsEmptyInitially) {

    EXPECT_TRUE(q0_.empty());
  // EXPECT_EQ(q0_.length(), 0);
}


