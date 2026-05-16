#include <gtest/gtest.h>

#include <boost/circular_buffer.hpp>
#include <ostream>

class circular_bufferTest : public testing::Test {
  // there is a reason: derived classes:
 protected:
  circular_bufferTest() {
     // q0_ remains empty
     // q1_.push_back(1);
     // q2_.Enqueue(2);
     // q2_.Enqueue(3);
  }

  // If necessary, write a default constructor or SetUp() function to prepare
  // override void SetUp() {};
  void SetUp() override {};

  // write a destructor or TearDown()
  // throwing in a destructor leads to undefined behavior and usually will kill your program right away.
  // you shouldn’t use GoogleTest assertions in a destructor if your code could run on such a platform.
  void TearDown() override {};


  // ~QueueTest() override = default;

  boost::circular_buffer<int> q0_;
  // Queue<int> q1_;
  // Queue<int> q2_;
};


//
TEST_F(circular_bufferTest, IsEmptyInitially) {
  EXPECT_EQ(q0_.size(), 0);
}


TEST_F(circular_bufferTest, push_backWorks) {
  // int* n =
  q0_.set_capacity(1);
  q0_.push_back(1);
  EXPECT_EQ(q0_.size(), 1);

#if 0
  n = q1_.Dequeue();
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(*n, 1);
  EXPECT_EQ(q1_.size(), 0);
  delete n;

  n = q2_.Dequeue();
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(*n, 2);
  EXPECT_EQ(q2_.size(), 1);
  delete n;
#endif

}

class event_dumper {
private:

public:
  int count = 0;

  void operator() (const int& item) {
    std::cerr << item << " " // std::endl
              << ++count << " "
              << static_cast<void*>(this)
              << std::endl;
  }
};



TEST_F(circular_bufferTest, for_each) {
  const int cap = 30;
  const int n = 20;


  q0_.set_capacity(cap);

  for(int i = 0; i< n; i++) {
    q0_.push_back(i);
  }
  // q0_.push_back(2);

  auto dumper = event_dumper{};

  for(auto i = q0_.begin(); i !=q0_.end(); i++) {
    dumper(*i);
  };

  EXPECT_EQ(dumper.count, n);
  std::cerr << "count: " << dumper.count << std::endl;


  dumper.count = 0;
  for_each(q0_.begin(),
           q0_.end(),
           // bug: copies!
           dumper);
  // it copies, so the count raises in the clone, not in our
  // object. (does not propagate back)

  std::cerr << "count: " << dumper.count << " " << &dumper << std::endl;
  // n
  EXPECT_EQ(dumper.count, 0);
  // delete dumper;
}




int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
