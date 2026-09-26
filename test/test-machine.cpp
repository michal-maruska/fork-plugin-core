#include "gmock/gmock.h"
#include <config.h>
#include <gtest/gtest.h>

#include "fork_enums.h"
#include <cstdlib>
#include <memory>
#include <ostream>
#include <thread>
#include <vector>
#include <atomic>

#include "../src/machine.h"
#include "../src/platform.h"
#include "empty_last.h"

#include <gmock/gmock.h>

typedef int Time;
typedef int KeyCode;


// I need archived_event
struct test_archived_event
{
  Time time; // never used
  KeyCode key;
  KeyCode forked;
  bool press;                  /* client type? */
};
// typedef

using testing::Mock;
using testing::Return;
using testing::AnyNumber;

// I need Environment which can convert into test_archived_event
// This is fully under control of our environment:
class TestEvent : public test_archived_event {
public:

  TestEvent(const Time time, const KeyCode keycode, bool press = true, const KeyCode forked = 0) :
    test_archived_event{time, keycode, forked, press} {}

  ~TestEvent() {}

  /* todo:
  operator=();
  */
private:
  // copy ctor:
  // when we store in the triqueue, we _copy_
  // TestEvent(TestEvent& copy) = delete; // we need the builtin one
};

// I want to mock this:
class testEnvironment final : public forkNS::platformEnvironment<KeyCode, Time,
                                                                 test_archived_event,
                                                                 TestEvent>{
public:
  // virtual
  MOCK_METHOD(bool, press_p, (const TestEvent& event), (const));
  MOCK_METHOD(bool, release_p,(const TestEvent& event), (const));
  MOCK_METHOD(Time, time_of,(const TestEvent& event), (const));
  MOCK_METHOD(KeyCode, detail_of,(const TestEvent& event), (const));

  MOCK_METHOD(bool, ignore_event,(const TestEvent &pevent));

  MOCK_METHOD(bool, output_frozen,());
  MOCK_METHOD(void, relay_event,(const TestEvent &pevent), (const));
  MOCK_METHOD(void, push_time,(Time now));

  // MOCK_METHOD(void, vlog,(const char* format, va_list argptr));

  void vlog(const char* format, va_list argptr) const override {
    vprintf(format, argptr);
  }

  void log(const char* format...) const override
  {
    va_list argptr;
    va_start(argptr, format);
    vprintf(format, argptr);
    va_end(argptr);
  };
  MOCK_METHOD(void, fmt_event,(const char* message, const TestEvent &event), (const));

  MOCK_METHOD(void, archive_event,(test_archived_event& ae, const TestEvent& event));
  MOCK_METHOD(void, free_event,(TestEvent* pevent), (const));
  MOCK_METHOD(void, rewrite_event,(TestEvent& pevent, KeyCode code));

  MOCK_METHOD(std::unique_ptr<forkNS::event_dumper<test_archived_event>>, get_event_dumper,());
};


using last_events_t = empty_last_events_t<test_archived_event>;
using machineRec = forkNS::forkingMachine<KeyCode, Time,
                                          TestEvent, testEnvironment,
                                          test_archived_event, last_events_t>;
using fork_configuration = machineRec::fork_configuration;

// template instantiation
namespace forkNS {
  // explicit template instantiation
  template class forkingMachine<KeyCode, Time, TestEvent, testEnvironment, test_archived_event, last_events_t>;
}


class machineTest : public testing::Test {

protected:
    machineTest() : environment(new testEnvironment() ),
                    // config (new fork_configuration),
                    fm (new machineRec(environment)) {

      fm->create_configs();
    }

  ~machineTest()
  {
    // machine owns `config` and `environment` via std::unique_ptr
    delete fm;
  }

  testEnvironment *environment;
  machineRec *fm;
  // fork_configuration *config;
};



// When using a fixture, use TEST_F(TestFixtureClassName, TestName)
TEST_F(machineTest, AcceptEvent) {
  int keycode = 67;
  TestEvent pevent(100L, keycode);

  EXPECT_CALL(*environment, relay_event);
  EXPECT_CALL(*environment, detail_of(testing::_))
    .Times(AnyNumber())
    .WillRepeatedly(testing::Return(keycode));
  EXPECT_CALL(*environment, time_of).Times(AnyNumber());
  EXPECT_CALL(*environment, press_p).Times(AnyNumber());
  EXPECT_CALL(*environment, release_p).Times(AnyNumber());

  EXPECT_CALL(*environment, output_frozen).Times(AnyNumber()).WillRepeatedly(Return(false));

  Time next = fm->accept_event(pevent); // this hands over ownership?
  UNUSED(next);
  // expect calls:
  // so for that EXPECT_CALL: this is necessary? as part of this test:
  Mock::VerifyAndClearExpectations(environment);
  // ::testing::Mock::AllowLeak(environment);
}

TEST_F(machineTest, AcceptEventFlushesWithoutDeadlock) {
  TestEvent pevent(100L, 56);

  EXPECT_CALL(*environment, relay_event);
  EXPECT_CALL(*environment, detail_of(testing::_))
    .WillRepeatedly(testing::Return(56));
  EXPECT_CALL(*environment, time_of).WillRepeatedly(testing::Return(100L));
  EXPECT_CALL(*environment, press_p).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*environment, release_p).WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*environment, ignore_event).WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*environment, push_time(testing::_)).Times(testing::AtLeast(1));

  Time next = fm->accept_event(pevent);
  UNUSED(next);

  // Calling accept_time should also execute without deadlock
  Time next_time = fm->accept_time(200L);
  UNUSED(next_time);

  Mock::VerifyAndClearExpectations(environment);
}

TEST_F(machineTest, Configure) {
  KeyCode A = 10;
  KeyCode B = 11;
  fm->configure_key(fork_configure_key_fork, A, B, 1);
  // fixme: EXPECT_EQ(config->fork_keycode[A], B);

  Mock::VerifyAndClearExpectations(environment);
}

TEST_F(machineTest, ConcurrentAccess) {
  EXPECT_CALL(*environment, relay_event).Times(AnyNumber());
  EXPECT_CALL(*environment, push_time).Times(AnyNumber());
  EXPECT_CALL(*environment, detail_of(testing::_))
    .WillRepeatedly(testing::Return(56));
  EXPECT_CALL(*environment, time_of).WillRepeatedly(testing::Return(100L));
  EXPECT_CALL(*environment, press_p).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*environment, release_p).WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*environment, ignore_event).WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(Return(false));

  std::atomic<bool> start_flag{false};
  constexpr int num_threads = 4;
  constexpr int iterations = 100;

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, &start_flag, i]() {
      while (!start_flag) {
        std::this_thread::yield();
      }
      for (int j = 0; j < iterations; ++j) {
        if (i % 2 == 0) {
          fm->accept_event(TestEvent(100L + j, 56));
          fm->accept_time(100L + j);
        } else {
          fm->configure_key(fork_configure_key_fork, 10, 20, 1);
          fm->configure_twins(fork_configure_total_limit, 10, 11, 150, true);
          fm->configure_global(fork_configure_debug, j % 2, true);
        }
      }
    });
  }

  start_flag = true;
  for (auto& t : threads) {
    t.join();
  }

  Mock::VerifyAndClearExpectations(environment);
}

TEST_F(machineTest, AcceptTimeForksOnTimeout) {
  KeyCode A = 10;
  KeyCode F = 11;
  Time a_time = 100;

  fm->configure_key(fork_configure_key_fork, A, F, 1);

  TestEvent A_pevent(a_time, A);

  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(Return(false));
  EXPECT_CALL(*environment, ignore_event(testing::_)).WillRepeatedly(Return(false));
  EXPECT_CALL(*environment, time_of(testing::_)).WillRepeatedly(Return(a_time));
  EXPECT_CALL(*environment, detail_of(testing::_)).WillRepeatedly(Return(A));
  EXPECT_CALL(*environment, press_p(testing::_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*environment, release_p(testing::_)).WillRepeatedly(Return(false));

  Time decision_time = fm->accept_event(A_pevent);
  EXPECT_GT(decision_time, a_time);

  // Now call accept_time with time >= decision_time
  EXPECT_CALL(*environment, rewrite_event(testing::_, F));
  EXPECT_CALL(*environment, relay_event(testing::_));
  EXPECT_CALL(*environment, push_time(decision_time));

  Time next = fm->accept_time(decision_time);
  EXPECT_EQ(next, 0);

  Mock::VerifyAndClearExpectations(environment);
}

TEST_F(machineTest, AcceptTimeMovedBackwardsDoesNotDeadlock) {
  // First set current time to 100
  EXPECT_CALL(*environment, push_time(100));
  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(Return(false));

  fm->accept_time(100);

  // Now pass a time earlier than 100 (time moving backwards)
  // This triggers the branch `if (mCurrent_time > now)` in `accept_time()`,
  // which previously called `next_decision_time()` while holding `mLock`,
  // causing a double-lock deadlock on std::mutex.
  Time next = fm->accept_time(50);
  EXPECT_EQ(next, 0);

  Mock::VerifyAndClearExpectations(environment);
}

class ConcreteTestEnvironment : public forkNS::platformEnvironment<KeyCode, Time,
                                                                   test_archived_event,
                                                                   TestEvent> {
public:
    bool press_p(const TestEvent& event) const override { return false; }
    bool release_p(const TestEvent& event) const override { return true; }
    Time time_of(const TestEvent& event) const override { return 100; }
    KeyCode detail_of(const TestEvent& event) const override { return 20; }
    bool ignore_event(const TestEvent &pevent) override { return false; }
    bool output_frozen() override { return false; }
    void relay_event(const TestEvent &pevent) const override {}
    void push_time(Time now) override {}
    void vlog(const char* format, va_list argptr) const override {}
    void log(const char* format...) const override {}
    void fmt_event(const char* message, const TestEvent &event) const override {}
    void archive_event(test_archived_event& ae, const TestEvent& event) override {}
    void free_event(TestEvent* pevent) const override {}
    void rewrite_event(TestEvent& pevent, KeyCode code) override {}
};

using concreteMachineRec = forkNS::forkingMachine<KeyCode, Time,
                                                  TestEvent, ConcreteTestEnvironment,
                                                  test_archived_event, last_events_t>;

TEST(machineConcurrentTest, ConcurrentLocking) {
  auto env = new ConcreteTestEnvironment();
  auto fm = std::make_unique<concreteMachineRec>(env);
  /*
  auto cfg = std::make_unique<concreteMachineRec::fork_configuration>();
  cfg->debug = 0;
  fm->config = std::move(cfg);
  */
  fm->create_configs();
  fm->configure_global(fork_configure_debug, 1, true);

  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back([&fm, i]() {
      for (int j = 0; j < 20; ++j) {
        fm->configure_key(fork_configure_key_fork, 10 + (i % 5), 20 + (i % 5), 1);
        fm->configure_global(fork_configure_repeat_limit, 200 + j, true);
        TestEvent pevent(100 + j, 20);
        fm->accept_event(pevent);
        fm->accept_time(100 + j);
        fm->accept_confirmation();
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
}

TEST_F(machineTest, ThreadSafety) {
  TestEvent pevent(100L, 56);

  EXPECT_CALL(*environment, relay_event).Times(AnyNumber());

  EXPECT_CALL(*environment, detail_of(testing::_)).WillRepeatedly(Return(56));
  EXPECT_CALL(*environment, time_of(testing::_)).WillRepeatedly(Return(100L));
  // (testing::_) ???

  EXPECT_CALL(*environment, press_p).WillRepeatedly(Return(true));
  EXPECT_CALL(*environment, release_p).WillRepeatedly(Return(false));
  EXPECT_CALL(*environment, ignore_event).WillRepeatedly(Return(false));
  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(Return(false));

  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([this, pevent]() {
      for (int j = 0; j < 50; ++j) {
        TestEvent event = pevent;
        event.time += j;

        fm->accept_event(pevent);
        fm->accept_time(100 + j);
        fm->accept_confirmation();
        fm->configure_key(fork_configure_key_fork, 56, 57, 1);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  Mock::VerifyAndClearExpectations(environment);
}

TEST_F(machineTest, StopFlagPreventsEventProcessing) {
  TestEvent pevent(100L, 56);
  fm->stop();

  // After stop(), accept_event should return 0 and not call relay_event or state machine transitions
  EXPECT_CALL(*environment, relay_event).Times(0);
  Time next = fm->accept_event(pevent);
  EXPECT_EQ(next, 0);

  Mock::VerifyAndClearExpectations(environment);
}

#if 0
// fixme: I need equal_to()

// create event, non-forkable, pass, and expect it's got back, and freed?
TEST_F(machineTest, EventFreed) {

  KeyCode A = 10;
  Time a_time = 156;
  Time next_time = 201;
  TestEvent A_pevent(a_time, A);

  KeyCode B = 11;
  fm->create_configs()
  fm->configure_global(fork_configure_debug, 1, true);
  fm->configure_key(fork_configure_key_fork, A, B, 1);
  // EXPECT_EQ(config->fork_keycode[A], B);

  // Emulate next plugin:
  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(Return(false));


  ON_CALL(*environment, ignore_event(A_pevent)).WillByDefault(Return(false));
  EXPECT_CALL(*environment, time_of(A_pevent)).WillRepeatedly(Return(a_time));

  // many times:
  TestEvent& a = A_pevent;
  EXPECT_CALL(*environment, detail_of(a)).Times(AnyNumber()).WillRepeatedly(Return(A));
  ON_CALL(*environment, press_p(A_pevent)).WillByDefault(Return(true));
  // archive_event
  // fmt_event
  // ON_CALL(*environment,rewrite_event).
  //  EXPECT_CALL(*environment, push_time(a_time));
  EXPECT_CALL(*environment, push_time(a_time + next_time));

  EXPECT_CALL(*environment,rewrite_event)
    .WillOnce([](TestEvent& pevent, KeyCode b) {
      auto event = static_cast<TestEvent&>(pevent).event;
      event->key = b;
    });

  fm->accept_event(A_pevent);
#if 1
  // we lost A_pevent
  EXPECT_CALL(*environment, relay_event); // (a)
  // this drop leaks ^^^
  EXPECT_CALL(*environment, free_event(&a)); // (nullptr)
#endif

  fm->accept_time(a_time + next_time);

  Mock::VerifyAndClearExpectations(environment);
}

TEST_F(machineTest, ForkBySecond) {
  // configure
  KeyCode A = 10;
  Time a_time = 156;
  TestEvent A_pevent(a_time, A);
  KeyCode F = 11;

  Time b_time = a_time + 50;
  KeyCode B = 60;
  TestEvent B_pevent(b_time, B);

  Time b_release_time = b_time + 50;
  TestEvent B_release_pevent (b_release_time, B, false);

  fm->configure_global(fork_configure_debug, 1, true);
  fm->configure_key(fork_configure_key_fork, A, F, 1); // 1 means SET
  // EXPECT_EQ(config->fork_keycode[A], F);

  TestEvent& a = A_pevent;
  TestEvent& b = B_pevent;

  std::cout << "A: " << &a << " b:" << &b << " br:" << &B_release_pevent << std::endl;

  // return:
  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(Return(false));

  // many times:
  EXPECT_CALL(*environment, ignore_event(A_pevent)).WillRepeatedly(Return(false));
  EXPECT_CALL(*environment, time_of(A_pevent)).WillRepeatedly(Return(a_time));
  EXPECT_CALL(*environment, detail_of(a)).WillRepeatedly(Return(A));
  EXPECT_CALL(*environment, press_p(A_pevent)).WillRepeatedly(Return(true));

  EXPECT_CALL(*environment, time_of(B_pevent)).WillRepeatedly(Return(b_time));
  EXPECT_CALL(*environment, ignore_event(B_pevent)).WillRepeatedly(Return(false));
  EXPECT_CALL(*environment, detail_of(b)).WillRepeatedly(Return(B));
  EXPECT_CALL(*environment, press_p(B_pevent)).WillRepeatedly(Return(true));

  EXPECT_CALL(*environment, detail_of(B_release_pevent)).WillRepeatedly(Return(B));
  EXPECT_CALL(*environment, time_of(B_release_pevent)).WillRepeatedly(Return(b_release_time));
  EXPECT_CALL(*environment, press_p(B_release_pevent)).WillRepeatedly(Return(false));
  // EXPECT_CALL(*environment, release_p(B_release_pevent)).WillRepeatedly(Return(true));
  // archive_event
  // fmt_event
  // ON_CALL(*environment,rewrite_event).
#if 0
  EXPECT_CALL(*environment,push_time(a_time));
  EXPECT_CALL(*environment,push_time(b_time));
  EXPECT_CALL(*environment,rewrite_event)
    .WillOnce([](TestEvent* pevent, KeyCode b) {
      auto event = static_cast<TestEvent*>(pevent)->event;
      event->key = b;
    });
#endif

#if 0
  // we lost A_pevent
  EXPECT_CALL(*environment, relay_event); // todo: check it's F
  // this drop leaks ^^^
  EXPECT_CALL(*environment, free_event(a)); // (nullptr)
#endif

  fm->accept_event(std::move(A_pevent));
  fm->accept_event(std::move(B_pevent));
  fm->accept_event(std::move(B_release_pevent));

  Mock::VerifyAndClearExpectations(environment);
}

#endif


// Thus your main() function must return the value of RUN_ALL_TESTS().
// Calling it more than once conflicts with some advanced GoogleTest features (e.g., thread-safe death tests)
// RUN_ALL_TESTS()

// gtest_main (as opposed to with gtest
