#include <gtest/gtest.h>

#include "fork_enums.h"
#include <cstdlib>
#include <memory>
#include <ostream>

#include "../src/machine.h"
#include "../src/platform.h"

#include <gmock/gmock.h>

typedef int Time;
typedef int KeyCode;


// I need archived_event
typedef struct
{
  Time time;
  KeyCode key;
  KeyCode forked;
  bool press;                  /* client type? */
} archived_event;

using testing::Mock;
using testing::Return;
using testing::AnyNumber;

using PlatformEvent = forkNS::PlatformEvent;
// I need Environment which can convert into archived_event
// This is fully under control of our environment:
class TestEvent : public PlatformEvent {
public:
  archived_event *event;

  TestEvent(const Time time, const KeyCode keycode, bool press = true, const KeyCode forked = 0) {
    event = new ::archived_event({time, keycode, forked, press });
  }

};

// I want to mock this:
class testEnvironment final : public forkNS::platformEnvironment<KeyCode, Time, archived_event> {
public:
  // virtual
  MOCK_METHOD(bool, press_p,(const PlatformEvent* event));
  MOCK_METHOD(bool, release_p,(const PlatformEvent* event));
  MOCK_METHOD(Time, time_of,(const PlatformEvent* event));
  MOCK_METHOD(KeyCode, detail_of,(const PlatformEvent* event));

  MOCK_METHOD(bool, ignore_event,(const PlatformEvent *pevent));

  MOCK_METHOD(bool, output_frozen,());
  MOCK_METHOD(void, relay_event,(PlatformEvent* &pevent));
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
  MOCK_METHOD(std::string, fmt_event,(const PlatformEvent *event));

  MOCK_METHOD(void, archive_event,(archived_event& ae, const PlatformEvent* event));
  MOCK_METHOD(void, free_event,(PlatformEvent* pevent), (const));
  MOCK_METHOD(void, rewrite_event,(PlatformEvent* pevent, KeyCode code));

  MOCK_METHOD(std::unique_ptr<forkNS::event_dumper<archived_event>>, get_event_dumper,());
};


using machineRec = forkNS::forkingMachine<KeyCode, Time, archived_event>;
using fork_configuration = forkNS::ForkConfiguration<KeyCode, Time, 256>;

// template instantiation
namespace forkNS {
template Time forkingMachine<KeyCode, Time, archived_event>::accept_event(std::unique_ptr<PlatformEvent> pevent);
template Time forkingMachine<KeyCode, Time, archived_event>::accept_time(const Time);

template bool forkingMachine<KeyCode, Time, archived_event>::create_configs();

template int forkingMachine<KeyCode, Time, archived_event>::configure_key(int type, KeyCode key, int value, bool set);

template int forkingMachine<KeyCode, Time, archived_event>::configure_global(int type, int value, bool set);

template int forkingMachine<KeyCode, Time, archived_event>::configure_twins(int type, KeyCode key, KeyCode twin, int value, bool set);

template int forkingMachine<KeyCode, Time, archived_event>::dump_last_events_to_client(forkNS::event_publisher<archived_event>* publisher, int max_requested);
}
// end template instantiation


class machineTest : public testing::Test {

protected:
    machineTest() : environment(new testEnvironment() ),
                    config (new fork_configuration),
                    fm (new machineRec(environment)) {

      // mmc: I could instead call forking_machine->create_configs();
      config->debug = 0;
      fm->config.reset(config);
    }

  ~machineTest()
  {
    // machine `owns' this:
    // config = nullptr;
    // so don't do this:
    // delete config;

    delete fm;
    delete environment;
  }

  testEnvironment *environment;
  machineRec *fm;
  fork_configuration *config;
};



// When using a fixture, use TEST_F(TestFixtureClassName, TestName)
TEST_F(machineTest, AcceptEvent) {
  auto pevent = std::make_unique<TestEvent>( 100L, 56);

  EXPECT_CALL(*environment, relay_event);

  Time next = fm->accept_event(std::move(pevent));

  // expect calls:
  // so for that EXPECT_CALL: this is necessary? as part of this test:
  Mock::VerifyAndClearExpectations(environment);
  // ::testing::Mock::AllowLeak(environment);
}

TEST_F(machineTest, Configure) {
  KeyCode A = 10;
  KeyCode B = 11;
  fm->configure_key(fork_configure_key_fork, A, B, 1);
  EXPECT_EQ(config->fork_keycode[A], B);

  Mock::VerifyAndClearExpectations(environment);
}

// create event, non-forkable, pass, and expect it's got back, and freed?
TEST_F(machineTest, EventFreed) {

  KeyCode A = 10;
  Time a_time = 156;
  Time next_time = 201;
  auto A_pevent = std::make_unique<TestEvent>(a_time, A);

  KeyCode B = 11;
  config->debug = 1;
  fm->configure_key(fork_configure_key_fork, A, B, 1);
  // EXPECT_EQ(config->fork_keycode[A], B);

  // Emulate next plugin:
  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(Return(false));


  ON_CALL(*environment, ignore_event(A_pevent.get())).WillByDefault(Return(false));
  EXPECT_CALL(*environment, time_of(A_pevent.get())).WillRepeatedly(Return(a_time));

  // many times:
  TestEvent* a = A_pevent.get();
  EXPECT_CALL(*environment, detail_of(a)).Times(AnyNumber()).WillRepeatedly(Return(A));
  ON_CALL(*environment, press_p(A_pevent.get())).WillByDefault(Return(true));
  // archive_event
  // fmt_event
  // ON_CALL(*environment,rewrite_event).
  //  EXPECT_CALL(*environment, push_time(a_time));
  EXPECT_CALL(*environment, push_time(a_time + next_time));

  EXPECT_CALL(*environment,rewrite_event)
    .WillOnce([](PlatformEvent* pevent, KeyCode b) {
      auto event = static_cast<TestEvent*>(pevent)->event;
      event->key = b;
    });

  fm->accept_event(std::move(A_pevent));
#if 1
  // we lost A_pevent
  EXPECT_CALL(*environment, relay_event); // (a)
  // this drop leaks ^^^
  EXPECT_CALL(*environment, free_event(a)); // (nullptr)
#endif

  fm->accept_time(a_time + next_time);

  Mock::VerifyAndClearExpectations(environment);
}


TEST_F(machineTest, ForkBySecond) {
  // configure
  KeyCode A = 10;
  Time a_time = 156;
  auto A_pevent = std::make_unique<TestEvent>(a_time, A);
  KeyCode F = 11;

  Time b_time = a_time + 50;
  KeyCode B = 60;
  auto B_pevent = std::make_unique<TestEvent>(b_time, B);

  Time b_release_time = b_time + 50;
  auto B_release_pevent = std::make_unique<TestEvent>(b_release_time, B, false);

  config->debug = 1;
  fm->configure_key(fork_configure_key_fork, A, F, 1); // 1 means SET
  // EXPECT_EQ(config->fork_keycode[A], F);

  TestEvent* a = A_pevent.get();
  TestEvent* b = B_pevent.get();

  std::cout << "A: " << a << " b:" << b << " br:" << B_release_pevent.get() << std::endl;

  // return:
  EXPECT_CALL(*environment, output_frozen).WillRepeatedly(Return(false));

  // many times:
  EXPECT_CALL(*environment, ignore_event(A_pevent.get())).WillRepeatedly(Return(false));
  EXPECT_CALL(*environment, time_of(A_pevent.get())).WillRepeatedly(Return(a_time));
  EXPECT_CALL(*environment, detail_of(a)).WillRepeatedly(Return(A));
  EXPECT_CALL(*environment, press_p(A_pevent.get())).WillRepeatedly(Return(true));

  EXPECT_CALL(*environment, time_of(B_pevent.get())).WillRepeatedly(Return(b_time));
  EXPECT_CALL(*environment, ignore_event(B_pevent.get())).WillRepeatedly(Return(false));
  EXPECT_CALL(*environment, detail_of(b)).WillRepeatedly(Return(B));
  EXPECT_CALL(*environment, press_p(B_pevent.get())).WillRepeatedly(Return(true));

  EXPECT_CALL(*environment, detail_of(B_release_pevent.get())).WillRepeatedly(Return(B));
  EXPECT_CALL(*environment, time_of(B_release_pevent.get())).WillRepeatedly(Return(b_release_time));
  EXPECT_CALL(*environment, press_p(B_release_pevent.get())).WillRepeatedly(Return(false));
  // EXPECT_CALL(*environment, release_p(B_release_pevent.get())).WillRepeatedly(Return(true));
  // archive_event
  // fmt_event
  // ON_CALL(*environment,rewrite_event).
#if 0
  EXPECT_CALL(*environment,push_time(a_time));
  EXPECT_CALL(*environment,push_time(b_time));
  EXPECT_CALL(*environment,rewrite_event)
    .WillOnce([](PlatformEvent* pevent, KeyCode b) {
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



// Thus your main() function must return the value of RUN_ALL_TESTS().
// Calling it more than once conflicts with some advanced GoogleTest features (e.g., thread-safe death tests)
// RUN_ALL_TESTS()

// gtest_main (as opposed to with gtest
