#include "trace-streaming.h"

#include <workerd/jsg/jsg-test.h>

#include <kj/compat/http.h>
#include <kj/test.h>

namespace workerd {
namespace {

static auto idFactory = StreamingTrace::IdFactory::newUuidIdFactory();

struct MockTimeProvider final: public StreamingTrace::TimeProvider {
  kj::Date getNow() const override {
    return 0 * kj::MILLISECONDS + kj::UNIX_EPOCH;
  }
};
MockTimeProvider mockTimeProvider;

KJ_TEST("We can create a simple, empty streaming trace session with implicit unknown outcome") {
  trace::Onset onset;
  // In this test we are creating a simple trace with no events or spans.
  // The delegate should be called exactly twice, once with the onset and
  // once with an implicit unknown outcome (since we're not explicitly calling
  // setOutcome ourselves)
  int callCount = 0;
  kj::String id = nullptr;
  {
    auto streamingTrace = workerd::StreamingTrace::create(
        *idFactory, kj::mv(onset), [&callCount, &id](workerd::StreamEvent&& event) {
      KJ_EXPECT(event.span.id == 0, "the root span should have id 0");
      KJ_EXPECT(event.span.parent == 0, "the root span should have no parent");
      switch (callCount++) {
        case 0: {
          id = kj::str(event.id);
          KJ_EXPECT(id.size() > 0, "there should be a non-empty id. we don't care what it is.");
          KJ_EXPECT(event.sequence == 0, "the sequence should be 0");
          auto& onset = KJ_ASSERT_NONNULL(
              event.event.tryGet<trace::Onset>(), "the event should be an onset event");
          auto& info = KJ_ASSERT_NONNULL(onset.info, "the onset event should have info");
          KJ_ASSERT_NONNULL(
              info.tryGet<trace::FetchEventInfo>(), "the onset event should have fetch info");
          break;
        }
        case 1: {
          KJ_EXPECT(event.id == id, "the event id should be the same as the onset id");
          KJ_EXPECT(event.sequence == 1, "the sequence should have been incremented");
          auto& outcome = KJ_ASSERT_NONNULL(
              event.event.tryGet<trace::Outcome>(), "the event should be an outcome event");
          KJ_EXPECT(outcome.outcome == EventOutcome::UNKNOWN, "the outcome should be unknown");
          break;
        }
      }
    }, mockTimeProvider);
    // The onset event will not be sent until the event info is set.
    streamingTrace->setEventInfo(
        trace::FetchEventInfo(kj::HttpMethod::GET, kj::String(), kj::String(), nullptr));
  }
  KJ_EXPECT(callCount == 2);
}

KJ_TEST("We can create a simple, empty streaming trace session with expicit canceled outcome") {
  trace::Onset onset;
  // In this test we are creating a simple trace with no events or spans.
  // The delegate should be called exactly twice, once with the onset and
  // once with an explicit canceled outcome.
  int callCount = 0;
  kj::String id = nullptr;
  auto streamingTrace = workerd::StreamingTrace::create(
      *idFactory, kj::mv(onset), [&callCount, &id](workerd::StreamEvent&& event) {
    KJ_EXPECT(event.span.id == 0, "the root span should have id 0");
    KJ_EXPECT(event.span.parent == 0, "the root span should have no parent");
    switch (callCount++) {
      case 0: {
        id = kj::str(event.id);
        KJ_EXPECT(id.size() > 0, "there should be a non-empty id. we don't care what it is.");
        KJ_EXPECT(event.sequence == 0, "the sequence should be 0");
        KJ_ASSERT_NONNULL(event.event.tryGet<trace::Onset>(), "the event should be an onset event");
        break;
      }
      case 1: {
        KJ_EXPECT(event.id == id, "the event id should be the same as the onset id");
        KJ_EXPECT(event.sequence == 1, "the sequence should have been incremented");
        auto& outcome = KJ_ASSERT_NONNULL(
            event.event.tryGet<trace::Outcome>(), "the event should be an outcome event");
        KJ_EXPECT(outcome.outcome == EventOutcome::CANCELED, "the outcome should be canceled");
        break;
      }
    }
  }, mockTimeProvider);
  // The onset event will not be sent until the event info is set.
  streamingTrace->setEventInfo(
      trace::FetchEventInfo(kj::HttpMethod::GET, kj::String(), kj::String(), nullptr));
  streamingTrace->setOutcome(trace::Outcome(EventOutcome::CANCELED));
  KJ_EXPECT(callCount == 2);
}

KJ_TEST("We can create a simple, streaming trace session with a single explicitly canceled trace") {
  trace::Onset onset;
  // In this test we are creating a simple trace with no events or spans.
  // The delegate should be called exactly five times.
  int callCount = 0;
  kj::String id = nullptr;
  {
    auto streamingTrace = workerd::StreamingTrace::create(
        *idFactory, kj::mv(onset), [&callCount, &id](workerd::StreamEvent&& event) {
      switch (callCount++) {
        case 0: {
          KJ_EXPECT(event.span.id == 0, "the root span should have id 0");
          KJ_EXPECT(event.span.parent == 0, "the root span should have no parent");
          id = kj::str(event.id);
          KJ_EXPECT(id.size() > 0, "there should be a non-empty id. we don't care what it is.");
          KJ_EXPECT(event.sequence == 0, "the sequence should be 0");
          KJ_ASSERT_NONNULL(
              event.event.tryGet<trace::Onset>(), "the event should be an onset event");
          break;
        }
        case 1: {
          KJ_EXPECT(event.span.id == 1, "the mark event should have span id 1");
          KJ_EXPECT(event.span.parent == 0, "the parent span should be the root span");
          KJ_EXPECT(event.id == id);
          KJ_EXPECT(event.sequence == 1);
          auto& mark = KJ_ASSERT_NONNULL(event.event.tryGet<trace::Mark>());
          KJ_EXPECT(mark.name == "bar"_kj);
          break;
        }
        case 2: {
          KJ_EXPECT(event.span.id == 1, "the spanclose should have span id 1");
          KJ_EXPECT(event.span.parent == 0, "the parent span should be the root span");
          KJ_EXPECT(event.sequence, 2);
          KJ_EXPECT(event.id == id);
          auto& span = KJ_ASSERT_NONNULL(
              event.event.tryGet<trace::SpanClose>(), "the event should be a spanclose event");
          KJ_EXPECT(span.outcome == trace::SpanClose::Outcome::CANCELED);
          break;
        }
        case 3: {
          KJ_EXPECT(event.span.id == 0, "the root span should have id 0");
          KJ_EXPECT(event.span.parent == 0, "the root span should have no parent");
          KJ_EXPECT(event.id == id, "the event id should be the same as the onset id");
          KJ_EXPECT(event.sequence == 3, "the sequence should have been incremented");
          auto& outcome = KJ_ASSERT_NONNULL(
              event.event.tryGet<trace::Outcome>(), "the event should be an outcome event");
          KJ_EXPECT(outcome.outcome == EventOutcome::CANCELED, "the outcome should be canceled");
          break;
        }
      }
    }, mockTimeProvider);

    streamingTrace->setEventInfo(trace::FetchEventInfo(
        kj::HttpMethod::GET, kj::str("http://example.com"), kj::String(), {}));
    auto span = KJ_ASSERT_NONNULL(streamingTrace->newChildSpan());
    span->addMark("bar");
    // Intentionally not calling setOutcome on the span.
    streamingTrace->setOutcome(trace::Outcome(EventOutcome::CANCELED));

    // Once the outcome is set, no more events should be emitted but calling the methods on
    // the span shouldn't crash or error.
    span->addMark("foo");
  }
  KJ_EXPECT(callCount == 4);
}

// ======================================================================================

jsg::V8System v8System;

struct TestContext: public jsg::Object, public jsg::ContextGlobal {
  JSG_RESOURCE_TYPE(TestContext) {}
};
JSG_DECLARE_ISOLATE_TYPE(TestIsolate, TestContext);

static kj::Maybe<kj::StringPtr> nullNameProvider(uint32_t, trace::NameProviderContext) {
  return kj::none;
}

KJ_TEST("JS Serialization of StreamEvent") {
  jsg::test::Evaluator<TestContext, TestIsolate> e(v8System);
  e.getIsolate().runInLockScope([&](TestIsolate::Lock& isolateLock) {
    JSG_WITHIN_CONTEXT_SCOPE(isolateLock,
        isolateLock.newContext<TestContext>().getHandle(isolateLock), [&](jsg::Lock& js) {
      StreamEvent streamEvent(kj::str("foo"),
          StreamEvent::Span{
            .id = 1,
            .parent = 2,
          },
          0 * kj::MILLISECONDS + kj::UNIX_EPOCH, 0, trace::Onset());
      auto obj = streamEvent.toObject(js, nullNameProvider);
      auto ser = js.serializeJson(obj);
      KJ_EXPECT(ser ==
          "{\"id\":\"foo\",\"span\":{\"id\":1,\"parent\":2},\"timestampNs\":\"1970-01-01T00:00:00.000Z\",\"sequence\":0,\"event\":{\"type\":\"onset\",\"scriptTags\":[],\"executionModel\":\"stateless\"}}");
    });
  });
}

}  // namespace
}  // namespace workerd
