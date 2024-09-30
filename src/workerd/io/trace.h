// Copyright (c) 2017-2022 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

#pragma once

#include "trace-legacy.h"

#include <workerd/io/outcome.capnp.h>
#include <workerd/io/worker-interface.capnp.h>
#include <workerd/jsg/memory.h>
#include <workerd/util/own-util.h>
#include <workerd/util/weak-refs.h>

#include <kj/async.h>
#include <kj/map.h>
#include <kj/one-of.h>
#include <kj/refcount.h>
#include <kj/string.h>
#include <kj/time.h>
#include <kj/vector.h>

namespace kj {
enum class HttpMethod;
class EntropySource;
}  // namespace kj

namespace workerd {

using kj::byte;
using kj::uint;

using Span = trace::Span;

// =======================================================================================

class WorkerTracer;

// A tracer which records traces for a set of stages. All traces for a pipeline's stages and
// possible subpipeline stages are recorded here, where they can be used to call a pipeline's
// trace worker.
class PipelineTracer final: public kj::Refcounted {
public:
  // Creates a pipeline tracer (with a possible parent).
  explicit PipelineTracer(kj::Maybe<kj::Own<PipelineTracer>> parentPipeline = kj::none)
      : parentTracer(kj::mv(parentPipeline)) {}

  ~PipelineTracer() noexcept(false);
  KJ_DISALLOW_COPY_AND_MOVE(PipelineTracer);

  // Returns a promise that fulfills when traces are complete.  Only one such promise can
  // exist at a time.
  kj::Promise<kj::Array<kj::Own<Trace>>> onComplete();

  // Makes a tracer for a subpipeline.
  kj::Own<PipelineTracer> makePipelineSubtracer() {
    return kj::refcounted<PipelineTracer>(kj::addRef(*this));
  }

  // Makes a tracer for a worker stage.
  kj::Own<WorkerTracer> makeWorkerTracer(PipelineLogLevel pipelineLogLevel,
      ExecutionModel executionModel,
      kj::Maybe<kj::String> scriptId,
      kj::Maybe<kj::String> stableId,
      kj::Maybe<kj::String> scriptName,
      kj::Maybe<kj::Own<ScriptVersion::Reader>> scriptVersion,
      kj::Maybe<kj::String> dispatchNamespace,
      kj::Array<kj::String> scriptTags,
      kj::Maybe<kj::String> entrypoint);

  // Adds a trace from the contents of `reader` this is used in sharded workers to send traces back
  // to the host where tracing was initiated.
  void addTrace(rpc::Trace::Reader reader);

private:
  kj::Vector<kj::Own<Trace>> traces;
  kj::Maybe<kj::Own<kj::PromiseFulfiller<kj::Array<kj::Own<Trace>>>>> completeFulfiller;

  kj::Maybe<kj::Own<PipelineTracer>> parentTracer;

  friend class WorkerTracer;
};

// Records a worker stage's trace information.  When all references to the Tracer are released,
// its Trace is considered complete. If the Trace to write to isn't provided (that already exists
// in a PipelineTracer), the trace must by extracted via extractTrace.
class WorkerTracer final: public kj::Refcounted {
public:
  explicit WorkerTracer(kj::Own<PipelineTracer> parentPipeline,
      kj::Own<Trace> trace,
      PipelineLogLevel pipelineLogLevel);
  explicit WorkerTracer(PipelineLogLevel pipelineLogLevel, ExecutionModel executionModel);
  ~WorkerTracer() {
    self->invalidate();
  }
  KJ_DISALLOW_COPY_AND_MOVE(WorkerTracer);

  // Sets info about the event that triggered the trace.  Must not be called more than once.
  void setEventInfo(kj::Date timestamp, trace::EventInfo&&);

  // Sets info about the result of this trace. Can be called more than once, overriding the
  // previous detail.
  void setOutcomeInfo(trace::OutcomeInfo&& info);

  // Adds log line to trace.  For Spectre, timestamp should only be as accurate as JS Date.now().
  // The isSpan parameter allows for logging spans, which will be emitted after regular logs. There
  // can be at most MAX_USER_SPANS spans in a trace.
  void log(kj::Date timestamp, LogLevel logLevel, kj::String message, bool isSpan = false);
  // Add a span, which will be represented as a log.
  void addSpan(const Span& span, kj::String spanContext);

  // TODO(soon): Eventually:
  //void setMetrics(...) // Or get from MetricsCollector::Request directly?

  void addException(
      kj::Date timestamp, kj::String name, kj::String message, kj::Maybe<kj::String> stack);

  void addDiagnosticChannelEvent(
      kj::Date timestamp, kj::String channel, kj::Array<kj::byte> message);

  // Adds info about the response. Must not be called more than once, and only
  // after passing a FetchEventInfo to setEventInfo().
  void setFetchResponseInfo(trace::FetchResponseInfo&&);

  [[deprecated("use setOutcomeInfo")]] void setOutcome(EventOutcome outcome);

  [[deprecated("use setOutcomeInfo")]] void setCPUTime(kj::Duration cpuTime);

  [[deprecated("use setOutcomeInfo")]] void setWallTime(kj::Duration wallTime);

  // Used only for a Trace in a process sandbox. Copies the content of this tracer's trace to the
  // builder.
  void extractTrace(rpc::Trace::Builder builder);

  // Sets the main trace of this Tracer to match the content of `reader`. This is used in the
  // parent process after receiving a trace from a process sandbox.
  void setTrace(rpc::Trace::Reader reader);

  kj::Own<WeakRef<WorkerTracer>> addWeakRef() {
    return self->addRef();
  }

private:
  PipelineLogLevel pipelineLogLevel;
  kj::Own<Trace> trace;

  // own an instance of the pipeline to make sure it doesn't get destroyed
  // before we're finished tracing
  kj::Maybe<kj::Own<PipelineTracer>> parentPipeline;
  // A weak reference for the internal span submitter. We use this so that the span submitter can
  // add spans while the tracer exists, but does not artifically prolong the lifetime of the tracer
  // which would interfere with span submission (traces get submitted when the worker returns its
  // response, but with e.g. waitUntil() the worker can still be performing tasks afterwards so the
  // span submitter may exist for longer than the tracer).
  kj::Own<WeakRef<WorkerTracer>> self;
};

// =======================================================================================

// Helper function used when setting "truncated_script_id" tags. Truncates the scriptId to 10
// characters.
inline kj::String truncateScriptId(kj::StringPtr id) {
  auto truncatedId = id.first(kj::min(id.size(), 10));
  return kj::str(truncatedId);
}

// =======================================================================================
// Span tracing
//
// TODO(cleanup): As of now, this aspect of tracing is actually not related to the rest of this
//   file. Most of this file defines the interface to feed Trace Workers. Span tracing, however,
//   is currently designed to feed tracing of the Workers Runtime itself for the benefit of the
//   developers of the runtime.
//
//   We might potentially want to give trace workers some access to span tracing as well, but with
//   that the trace worker and span interfaces should still be largely independent of each other;
//   separate span tracing into a separate header.

class SpanBuilder;
class SpanObserver;

// An opaque token which can be used to create child spans of some parent. This is typically
// passed down from a caller to a callee when the caller wants to allow the callee to create
// spans for itself that show up as children of the caller's span, but the caller does not
// want to give the callee any other ability to modify the parent span.
class SpanParent {
public:
  SpanParent(SpanBuilder& builder);

  // Make a SpanParent that causes children not to be reported anywhere.
  SpanParent(decltype(nullptr)) {}

  SpanParent(kj::Maybe<kj::Own<SpanObserver>> observer): observer(kj::mv(observer)) {}

  SpanParent(SpanParent&& other) = default;
  SpanParent& operator=(SpanParent&& other) = default;
  KJ_DISALLOW_COPY(SpanParent);

  SpanParent addRef();

  // Create a new child span.
  //
  // `operationName` should be a string literal with infinite lifetime.
  SpanBuilder newChild(
      kj::ConstString operationName, kj::Date startTime = kj::systemPreciseCalendarClock().now());

  // Useful to skip unnecessary code when not observed.
  bool isObserved() {
    return observer != kj::none;
  }

  // Get the underlying SpanObserver representing the parent span.
  //
  // This is needed in particular when making outbound network requests that must be annotated with
  // trace IDs in a way that is specific to the trace back-end being used. The caller must downcast
  // the `SpanObserver` to the expected observer type in order to extract the trace ID.
  kj::Maybe<SpanObserver&> getObserver() {
    return observer;
  }

private:
  kj::Maybe<kj::Own<SpanObserver>> observer;
};

// Interface for writing a span. Essentially, this is a mutable interface to a `Span` object,
// given only to the code which is meant to create the span, whereas code that merely collects
// and reports spans gets the `Span` type.
//
// The reason we use a separate builder type rather than rely on constness is so that the methods
// can be no-ops when there is no observer, avoiding unnecessary allocations. To allow for this,
// SpanBuilder is designed to be write-only -- you cannot read back the content. Only the
// observer (if there is one) receives the content.
class SpanBuilder {
public:
  // Create a new top-level span that will report to the given observer. If the observer is null,
  // no data is collected.
  //
  // `operationName` should be a string literal with infinite lifetime, or somehow otherwise be
  // attached to the observer observing this span.
  explicit SpanBuilder(kj::Maybe<kj::Own<SpanObserver>> observer,
      kj::ConstString operationName,
      kj::Date startTime = kj::systemPreciseCalendarClock().now()) {
    if (observer != kj::none) {
      this->observer = kj::mv(observer);
      span.emplace(kj::mv(operationName), startTime);
    }
  }

  // Make a SpanBuilder that ignores all calls. (Useful if you want to assign it later.)
  SpanBuilder(decltype(nullptr)) {}

  SpanBuilder(SpanBuilder&& other) = default;
  SpanBuilder& operator=(SpanBuilder&& other);  // ends the existing span and starts a new one
  KJ_DISALLOW_COPY(SpanBuilder);

  ~SpanBuilder() noexcept(false);

  // Finishes and submits the span. This is done implicitly by the destructor, but sometimes it's
  // useful to be able to submit early. The SpanBuilder ignores all further method calls after this
  // is invoked.
  void end();

  // Useful to skip unnecessary code when not observed.
  bool isObserved() {
    return observer != kj::none;
  }

  // Get the underlying SpanObserver representing the span.
  //
  // This is needed in particular when making outbound network requests that must be annotated with
  // trace IDs in a way that is specific to the trace back-end being used. The caller must downcast
  // the `SpanObserver` to the expected observer type in order to extract the trace ID.
  kj::Maybe<SpanObserver&> getObserver() {
    return observer;
  }

  // Create a new child span.
  //
  // `operationName` should be a string literal with infinite lifetime.
  SpanBuilder newChild(
      kj::ConstString operationName, kj::Date startTime = kj::systemPreciseCalendarClock().now());

  // Change the operation name from what was specified at span creation.
  //
  // `operationName` should be a string literal with infinite lifetime.
  void setOperationName(kj::ConstString operationName);

  using TagValue = Span::TagValue;
  // `key` must point to memory that will remain valid all the way until this span's data is
  // serialized.
  void setTag(kj::ConstString key, TagValue value);

  // `key` must point to memory that will remain valid all the way until this span's data is
  // serialized.
  //
  // The differences between this and `setTag()` is that logs are timestamped and may have
  // duplicate keys.
  void addLog(kj::Date timestamp, kj::ConstString key, TagValue value);

private:
  kj::Maybe<kj::Own<SpanObserver>> observer;
  // The under-construction span, or null if the span has ended.
  kj::Maybe<Span> span;

  friend class SpanParent;
};

// Abstract interface for observing trace spans reported by the runtime. Different
// implementations might support different tracing back-ends, e.g. Trace Workers, Jaeger, or
// whatever infrastructure you prefer to use for this.
//
// A new SpanObserver is created at the start of each Span. The observer is used to report the
// span data at the end of the span, as well as to construct child observers.
class SpanObserver: public kj::Refcounted {
public:
  // Allocate a new child span.
  //
  // Note that children can be created long after a span has completed.
  virtual kj::Own<SpanObserver> newChild() = 0;

  // Report the span data. Called at the end of the span.
  //
  // This should always be called exactly once per observer.
  virtual void report(const Span& span) = 0;
};

inline SpanParent::SpanParent(SpanBuilder& builder): observer(mapAddRef(builder.observer)) {}

inline SpanParent SpanParent::addRef() {
  return SpanParent(mapAddRef(observer));
}

inline SpanBuilder SpanParent::newChild(kj::ConstString operationName, kj::Date startTime) {
  return SpanBuilder(observer.map([](kj::Own<SpanObserver>& obs) { return obs->newChild(); }),
      kj::mv(operationName), startTime);
}

inline SpanBuilder SpanBuilder::newChild(kj::ConstString operationName, kj::Date startTime) {
  return SpanBuilder(observer.map([](kj::Own<SpanObserver>& obs) { return obs->newChild(); }),
      kj::mv(operationName), startTime);
}

// TraceContext to keep track of user tracing/existing tracing better
// TODO(o11y): When creating user child spans, verify that operationName is within a set of
// supported operations. This is important to avoid adding spans to the wrong tracing system.

// Interface to track trace context including both Jaeger and User spans.
// TODO(o11y): Consider fleshing this out to make it a proper class, support adding tags/child spans
// to both,... We expect that tracking user spans will not needed in all places where we have the
// existing spans, so synergies will likely be limited.
struct TraceContext {
  TraceContext(SpanBuilder span, SpanBuilder userSpan)
      : span(kj::mv(span)),
        userSpan(kj::mv(userSpan)) {}
  TraceContext(TraceContext&& other) = default;
  TraceContext& operator=(TraceContext&& other) = default;
  KJ_DISALLOW_COPY(TraceContext);

  SpanBuilder span;
  SpanBuilder userSpan;
};

// TraceContext variant tracking span parents instead. This is useful for code interacting with
// IoChannelFactory::SubrequestMetadata, which often needs to pass through both spans together
// without modifying them. In particular, add functions like newUserChild() here to make it easier
// to add a span for the right parent.
struct TraceParentContext {
  TraceParentContext(TraceContext& tracing)
      : parentSpan(tracing.span),
        userParentSpan(tracing.userSpan) {}
  TraceParentContext(SpanParent span, SpanParent userSpan)
      : parentSpan(kj::mv(span)),
        userParentSpan(kj::mv(userSpan)) {}
  TraceParentContext(TraceParentContext&& other) = default;
  TraceParentContext& operator=(TraceParentContext&& other) = default;
  KJ_DISALLOW_COPY(TraceParentContext);

  SpanParent parentSpan;
  SpanParent userParentSpan;
};

// RAII object that measures the time duration over its lifetime. It tags this duration onto a
// given request span using a specified tag name. Ideal for automatically tracking and logging
// execution times within a scoped block.
class ScopedDurationTagger {
public:
  explicit ScopedDurationTagger(
      SpanBuilder& span, kj::ConstString key, const kj::MonotonicClock& timer);
  ~ScopedDurationTagger() noexcept(false);
  KJ_DISALLOW_COPY_AND_MOVE(ScopedDurationTagger);

private:
  SpanBuilder& span;
  kj::ConstString key;
  const kj::MonotonicClock& timer;
  const kj::TimePoint startTime;
};

}  // namespace workerd
