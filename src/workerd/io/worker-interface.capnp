# Copyright (c) 2017-2022 Cloudflare, Inc.
# Licensed under the Apache 2.0 license found in the LICENSE file or at:
#     https://opensource.org/licenses/Apache-2.0

@0xf7958855f6746344;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("workerd::rpc");
# We do not use `$Cxx.allowCancellation` because runAlarm() currently depends on blocking
# cancellation.

using import "/capnp/compat/http-over-capnp.capnp".HttpMethod;
using import "/capnp/compat/http-over-capnp.capnp".HttpService;
using import "/capnp/compat/byte-stream.capnp".ByteStream;
using import "/workerd/io/outcome.capnp".EventOutcome;
using import "/workerd/io/script-version.capnp".ScriptVersion;

struct Trace @0x8e8d911203762d34 {
  logs @0 :List(Log);
  struct Log {
    timestampNs @0 :Int64;

    logLevel @1 :Level;
    enum Level {
      debug @0 $Cxx.name("debug_");  # avoid collision with macro on Apple platforms
      info @1;
      log @2;
      warn @3;
      error @4;
    }

    message @2 :Text;
  }

  struct LogV2 {
    # Streaming tail workers support an expanded version of Log that supports arbitrary
    # v8 serialized data or a text message. We define this as a separate new
    # struct in order to avoid any possible non-backwards compatible disruption to anything
    # using the existing Log struct in the original trace worker impl. The two structs are
    # virtually identical with the exception that the message field can be v8 serialized data.
    timestampNs @0 :Int64;
    logLevel @1 :Log.Level;
    message :union {
      data @2 :Data;
      # When data is used, the LogV2 message is expected to be a v8 serialized value.
      text @3 :Text;
      # Text would be used, for instance, for simple string outputs (e.g. from things
      # like console.log(...))
    }
    tags @4 :List(Tag);
    # Additional bits of information that are not known to workerd but may be injected
    # by workerd embedders (such as the Cloudflare production environment) or structured
    # logging mechanisms (so called "wide events").
    truncated @5 :Bool;
    # A Log entry might be truncated if it exceeds the maximum size limit configured
    # for the process. Truncation should occur before the data is serialized so it
    # should always be possible to deserialize the data field successfully, regardless
    # of the specific format of the data.
  }

  exceptions @1 :List(Exception);
  struct Exception {
    timestampNs @0 :Int64;
    name @1 :Text;
    message @2 :Text;
    stack @3 :Text;

    detail :group {
      # Additional optional detail accompanying the exception event.
      cause @4 :Exception;
      # If the exception has a cause property, it is serialized here.

      errors @5 :List(Exception);
      # If the exception represents an AggregateError or SupressedError, the
      # errors are serialized here.

      remote @6 :Bool;
      retryable @7 :Bool;
      overloaded @8 :Bool;
      durableObjectReset @9 :Bool;
      tags @10 :List(Tag);
      # Additional metadata fields that are set on some errors originating
      # from the runtime. The remote, retryable, overloaded, and durableObjectReset
      # fields *could* be defined as tags but those are already known to workerd
      # and will be common enough to just represent those separately. If/when new
      # fields are introduced that are not known to workerd, they would be added
      # as tags rather than distinct fields.
    }
  }

  outcome @2 :EventOutcome;
  scriptName @4 :Text;
  scriptVersion @19 :ScriptVersion;
  scriptId @23 :Text;

  eventTimestampNs @5 :Int64;

  eventInfo :union {
    none @3 :Void;
    fetch @6 :FetchEventInfo;
    jsRpc @21 :JsRpcEventInfo;
    scheduled @7 :ScheduledEventInfo;
    alarm @9 :AlarmEventInfo;
    queue @15 :QueueEventInfo;
    custom @13 :CustomEventInfo;
    email @16 :EmailEventInfo;
    trace @18 :TraceEventInfo;
    hibernatableWebSocket @20 :HibernatableWebSocketEventInfo;
  }
  struct FetchEventInfo {
    method @0 :HttpMethod;
    url @1 :Text;
    cfJson @2 :Text;
    # Empty string indicates missing cf blob
    headers @3 :List(Header);
    struct Header {
      name @0 :Text;
      value @1 :Text;
    }
  }

  struct JsRpcEventInfo {
    methodName @0 :Text;
  }

  struct ScheduledEventInfo {
    scheduledTime @0 :Float64;
    cron @1 :Text;
  }

  struct AlarmEventInfo {
    scheduledTimeMs @0 :Int64;
  }

  struct QueueEventInfo {
    queueName @0 :Text;
    batchSize @1 :UInt32;
  }

  struct EmailEventInfo {
    mailFrom @0 :Text;
    rcptTo @1 :Text;
    rawSize @2 :UInt32;
  }

  struct TraceEventInfo {
    struct TraceItem {
      scriptName @0 :Text;
    }

    traces @0 :List(TraceItem);
  }

  struct HibernatableWebSocketEventInfo {
    type :union {
      message @0 :Void;
      close :group {
        code @1 :UInt16;
        wasClean @2 :Bool;
      }
      error @3 :Void;
    }
  }

  struct CustomEventInfo { }

  response @8 :FetchResponseInfo;
  struct FetchResponseInfo {
    statusCode @0 :UInt16;
  }

  cpuTime @10 :UInt64;
  wallTime @11 :UInt64;

  dispatchNamespace @12 :Text;
  scriptTags @14 :List(Text);

  entrypoint @22 :Text;

  diagnosticChannelEvents @17 :List(DiagnosticChannelEvent);
  struct DiagnosticChannelEvent {
    timestampNs @0 :Int64;
    channel @1 :Text;
    message @2 :Data;
  }

  truncated @24 :Bool;
  # Indicates that the trace was truncated due to reaching the maximum size limit.

  enum ExecutionModel {
    stateless @0;
    durableObject @1;
    workflow @2;
  }
  executionModel @25 :ExecutionModel;
  # the execution model of the worker being traced. Can be stateless for a regular worker,
  # durableObject for a DO worker or workflow for the upcoming Workflows feature.

  # The following structs are used only in Streaming Traces.

  struct Onset {
    # The Onset struct is always sent as the first event in any trace stream. It
    # contains general metadata about the pipeline that is being traced.
    accountId @0 :UInt32;
    stableId @1 :Text;
    scriptName @2 :Text;
    scriptVersion @3 :ScriptVersion;
    dispatchNamespace @4 :Text;
    scriptId @5 :Text;
    scriptTags @6 :List(Text);
    entrypoint @7 :Text;
    executionModel @8 :ExecutionModel;

    tags @9 :List(Tag);
    # Any additional arbitrary metadata that should be associated with the onset.
    # These are different from the tags that appear in the StreamEvent structure
    # in that those are considered unique for each event in the stream, whereas
    # these are considered part of the onset metadata itself, just like any of
    # the fields above. The goal is to make Onset extensible without requiring
    # schema changes.
  }

  struct Outcome {
    # The Outcome struct is always sent as the last event in any trace stream. It
    # contains the final outcome of the trace including the CPU and wall time for
    # the entire trace steam.
    outcome @0 :EventOutcome;

    tags @1 :List(Tag);
    # Any additional arbitrary metadata that should be associated with the outcome.
  }

  struct ActorFlushInfo {
    # An outcome information object that describes additional detail about the outcome
    # of an Actor/Durable object. Used primarily to identify the ActorFlushReason.
    enum Common {
      reason @0;
      broken @1;
    }
    tags @0 :List(Tag);
  }

  struct Subrequest {
    # A detail event indicating a subrequest that was made during a request. This
    # can be a fetch subrequest, an RPC subrequest, a call out to a KV namespace,
    # etc.
    # TODO(streaming-trace): This needs to be flushed out more.
    id @0 :UInt32;
    # A monotonic sequence number that is unique within the tail. The id is
    # used primarily to correlate with the SubrequestOutcome.
    info :union {
      none @1 :Void;
      fetch @2 :FetchEventInfo;
      jsRpc @3 :JsRpcEventInfo;
    }
  }

  struct SubrequestOutcome {
    id @0 :UInt32;
    info :union {
      none @1 :Void;
      fetch @2 :FetchResponseInfo;
      custom @3 :List(Tag);
    }
    outcome @4 :Span.SpanOutcome;
    # A Subrequest is really a specialist kind of span, so it can have an outcome.
    # just like a span. Unlike regular spans tho, they cannot be transactional,
    # and are not part of the normal span stack.
  }

  struct Span {
    # A span event is sent only at the completion of a span, and includes markers
    # for the start and end times, as well as the start and end sequence numbers.
    # A span event always occurs in the scope of the parent span (or the null span
    # if there is no current). So, for example:
    #
    #  Event 0 - Onset (null span)
    #  Event 1 - Info (null span)
    #  Event 2 - Log (Span 1)  (new span implicitly started)
    #  Event 3 - Log (Span 2)  (new span implicitly started)
    #  Event 4 - Span 2 (Span 1)  (span 2 is complete, span 1 is still open)
    #  Event 5 - Span 1 (null span)  (span 1 is complete)
    #  Event 6 - Outcome (null span)
    #
    # Note that the Span 2 event occurs within Span 1
    id @0 :UInt32;
    parent @1 :UInt32;
    transactional @2 :Bool;

    enum SpanOutcome {
      # A span event may have an outcome field. If, for instance, the span represents
      # events occuring while an output gate is open, and the output gate fails indicating
      # that the events are not longer valid, the outcome field will be used to signal.
      unknown @0;
      ok @1;
      exception @2;
      canceled @3;
    }

    outcome @3 :SpanOutcome;

    info :union {
      # The info field is used to provide additional information about the outcome.
      # These are often dependent on the type of info event that started the span.
      # This is most typically only used for stage spans.
      # For instance, a fetch event will have a fetchResponse info field in the outcome.
      none @4 :Void;
      fetch @5 :FetchResponseInfo;
      actorFlush @6 :ActorFlushInfo;
      custom @7 :List(Tag);
    }

    tags @8 :List(Tag);
    # Any additional metadata specific to the span itself.
  }

  struct Mark {
    # A mark is a special event that simply marks a point of interest in the trace.
    name @0 :Text;
  }

  struct Metric {
    # A metric is a special event that represents a metric value of some form injected
    # into the trace. A metric can be used, for instance, to communicate the current
    # isolate memory usage at a given point in time.

    key :union {
      # The key field can be either arbitrary text or a numeric ID. The ID field is used
      # to identify commonly used metrics defined and injected by the runtime. There is no
      # enum definition for these here as it is expected the actual definitions will
      # come from the internal project and not workerd.
      # Metrics with the same name can appear multiple times, in which case the calculated
      # logical value is the concatenation of all values into a collection. So, for
      # instance, if a metric with name "foo" appears with value 1 and then again with
      # value 2, the logical value of the "foo" metric is [1, 2]. The ordering of such
      # metrics is significant.
      # Multiple appearances of the same metric with the same key can be of different
      # types and units.
      text @0 :Text;
      id @1 :UInt32;
    }
    enum Type {
      counter @0;
      # A counter metric, for instance, the number of times a given event has occurred,
      # the number of requests processed, etc. Counters nearly aways represent a conceptually
      # monotonically increasing value (conceptually because in reality the value may not
      # be strictly increasing unit-by-unit due to sampling frequency, etc).

      gauge @1;
      # A gauge metric, for instance, the current memory heap size, etc. Gauges represent
      # a snapshot of a value at a given point in time and therefore may increase or decrease.
    }
    type @2 :Type;
    value :union {
      float64 @3 :Float64;
      int64 @4 :Int64;
      uint64 @5 :UInt64;
    }
    unit @6 :Text;
    tags @7 :List(Tag);
  }

  struct Dropped {
    # The Dropped struct is used to indicate that a trace has dropped a given number of
    # events in the sequence. A Dropped events sequence number must always be greater
    # than the sequence number specified by the end field.
    start @0 :UInt32;
    end @1 :UInt32;
  }

  struct Tag {
    # A Tag is an additional piece of information that can added to each event in a trace.

    key :union {
      # The key field can be either arbitrary text or a numeric ID. The ID field is used
      # to identify commonly used tags defined and injected by the runtime. There is no
      # enum definition for these here as it is expected the actual definitions will
      # come from the internal project and not workerd.
      # Tags with the same key can appear multiple times, in which case the calculated
      # logical value is the concatenation of all values into a collection. So, for
      # instance, if a tag with key "foo" appears with value "bar" and then again with
      # value "baz", the logical value of the "foo" tag is ["bar", "baz"]. The ordering
      # of such tags is significant.
      text @0 :Text;
      id @1 :UInt32;
    }
    value :union {
      bool @2 :Bool;
      int64 @3 :Int64;
      uint64 @4 :UInt64;
      float64 @5 :Float64;
      text @6 :Text;
      data @7 :Data;
    }
  }

  struct StreamEvent {
    id @0 :Text;
    # A unique identifier used to correlate traces across multiple events
    # in a single tail session. Typically this will correlate to a top-level
    # pipeline or specific pipeline stage.

    span :group {
      id @1 :UInt32;
      parent @2 :UInt32;
      transactional @3 :Bool;
      # Some spans may be transactional, meaning that they represent a single
      # atomic operation that may succeed or fail as a whole. When the span
      # event occurs, the span outcome will indicate if the span was successful
      # or failed. If a transaction span outcome indicates failure, then all events
      # within that span should be considered invalidated.
    }
    timestampNs @4 :Int64;
    sequence @5 :UInt32;
    # The sequence order for this event. This is a strictly monotonically
    # increasing sequence number that is unique within the tail. The onset
    # event sequence number will always be 0. The purpose of the sequence
    # is to make it possible to reconstruct the specific ordering of events
    # in the stream.

    event :union {
      onset @6 :Onset;
      # When a tail stream is first created, the first event will always be
      # an onset event.

      outcome @7 :Outcome;
      # The final event in every successfully completed stream be will an outcome
      # event.

      dropped @8 :Dropped;
      # The dropped event is used to identify events that have been dropped from
      # the stream. The start field indicates the sequence number of the first
      # event dropped, and the end field indicates the sequence number of the
      # last event dropped.

      span @9 :Span;
      # Span events mark the ending and outcome of a span.

      info :union {
        # Info events are used at the start of a stage span to identify the kind
        # of trigger that started the span. For instance, a fetch event will have
        # a fetch info event at the start of the span.
        fetch @10 :FetchEventInfo;
        jsRpc @11 :JsRpcEventInfo;
        scheduled @12 :ScheduledEventInfo;
        alarm @13 :AlarmEventInfo;
        queue @14 :QueueEventInfo;
        email @15 :EmailEventInfo;
        trace @16 :TraceEventInfo;
        hibernatableWebSocket @17 :HibernatableWebSocketEventInfo;
        custom @18 :List(Tag);
        # A custom info event is used to enable arbitrary, non-typed extension
        # events to be injected. It is most useful as a way of extending
        # the event stream with new types of events without modifying the
        # schema. This is a tradeoff. Using a custom event is more flexible
        # but there's no schema to verify the data.
      }

      detail :union {
        # Detail events occur throughout a span and may occur many times.
        log @19 :LogV2;
        exception @20 :Exception;
        diagnosticChannel @21 :DiagnosticChannelEvent;
        mark @22 :Mark;
        metrics @23 :List(Metric);
        subrequest @24 :Subrequest;
        subrequestOutcome @25 :SubrequestOutcome;
        custom @26 :List(Tag);
        # A custom detail event is used to enable arbitrary, non-typed extension
        # events to be injected. It is most useful as a way of extending
        # the event stream with new types of events without modifying the
        # schema. This is a tradeoff. Using a custom event is more flexible
        # but there's no schema to verify the data.
      }
    }
  }
}

struct SendTracesRun @0xde913ebe8e1b82a5 {
  outcome @0 :EventOutcome;
}

struct ScheduledRun @0xd98fc1ae5c8095d0 {
  outcome @0 :EventOutcome;

  retry @1 :Bool;
}

struct AlarmRun @0xfa8ea4e97e23b03d {
  outcome @0 :EventOutcome;

  retry @1 :Bool;
  retryCountsAgainstLimit @2 :Bool = true;
}

struct QueueMessage @0x944adb18c0352295 {
  id @0 :Text;
  timestampNs @1 :Int64;
  data @2 :Data;
  contentType @3 :Text;
  attempts @4 :UInt16;
}

struct QueueRetryBatch {
  retry @0 :Bool;
  union {
    undefined @1 :Void;
    delaySeconds @2 :Int32;
  }
}

struct QueueRetryMessage {
  msgId @0 :Text;
  union {
    undefined @1 :Void;
    delaySeconds @2 :Int32;
  }
}

struct QueueResponse @0x90e98932c0bfc0de {
  outcome @0 :EventOutcome;
  ackAll @1 :Bool;
  retryBatch @2 :QueueRetryBatch;
  # Retry options for the batch.
  explicitAcks @3 :List(Text);
  # List of Message IDs that were explicitly marked as acknowledged.
  retryMessages @4 :List(QueueRetryMessage);
  # List of retry options for messages that were explicitly marked for retry.
}

struct HibernatableWebSocketEventMessage {
  payload :union {
    text @0 :Text;
    data @1 :Data;
    close :group {
      code @2 :UInt16;
      reason @3 :Text;
      wasClean @4 :Bool;
    }
    error @5 :Text;
    # TODO(someday): This could be an Exception instead of Text.
  }
  websocketId @6: Text;
  eventTimeoutMs @7: UInt32;
}

struct HibernatableWebSocketResponse {
  outcome @0 :EventOutcome;
}

interface HibernatableWebSocketEventDispatcher {
  hibernatableWebSocketEvent @0 (message: HibernatableWebSocketEventMessage )
      -> (result :HibernatableWebSocketResponse);
  # Run a hibernatable websocket event
}

enum SerializationTag {
  # Tag values for all serializable types supported by the Workers API.

  invalid @0;
  # Not assigned to anything. Reserved to make things less weird if a zero-valued tag gets written
  # by accident.

  jsRpcStub @1;

  writableStream @2;
  readableStream @3;

  headers @4;
  request @5;
  response @6;

  domException @7;
  domExceptionV2 @8;
  # Keep this value in sync with the DOMException::SERIALIZATION_TAG in
  # /src/workerd/jsg/dom-exception (but we can't actually change this value
  # without breaking things).
}

enum StreamEncoding {
  # Specifies the internal content-encoding of a ReadableStream or WritableStream. This serves an
  # optimization which is not visible to the app: if we end up hooking up streams so that a source
  # is pumped to a sink that has the same encoding, we can avoid a decompression/recompression
  # round trip. However, if the application reads/writes raw bytes, then we must decode/encode
  # them under the hood.

  identity @0;
  gzip @1;
  brotli @2;
}

interface Handle {
  # Type with no methods, but something happens when you drop it.
}

struct JsValue {
  # A serialized JavaScript value being passed over RPC.

  v8Serialized @0 :Data;
  # JS value that has been serialized for network transport.

  externals @1 :List(External);
  # The serialized data may contain "externals" -- references to external resources that cannot
  # simply be serialized. If so, they are placed in this separate list of externals.
  #
  # (We could also call these "capabilities", but that word is pretty overloaded already.)

  struct External {
    union {
      invalid @0 :Void;
      # Invalid default value to reduce confusion if an External wasn't initialized properly.
      # This should never appear in a real JsValue.

      rpcTarget @1 :JsRpcTarget;
      # An object that can be called over RPC.

      writableStream :group {
        # A WritableStream. This is much easier to represent that ReadableStream because the bytes
        # flow from the receiver to the sender, and therefore a round trip is obviously necessary
        # before the bytes can begin flowing.

        byteStream @2 :ByteStream;
        encoding @3 :StreamEncoding;
      }

      readableStream :group {
        # A ReadableStream. The sender of the JsValue will use the associated StreamSink to open a
        # stream of type `ByteStream`.

        encoding @4 :StreamEncoding;
        # Bytes read from the stream have this encoding.

        expectedLength :union {
          unknown @5 :Void;
          known @6 :UInt64;
        }
      }

      # TODO(soon): WebSocket, Request, Response
    }
  }

  interface StreamSink {
    # A JsValue may contain streams that flow from the sender to the receiver. We don't want such
    # streams to require a network round trip before the stream can begin pumping. So, we need a
    # place to start sending bytes right away.
    #
    # To that end, JsRpcTarget::call() returns a `paramsStreamSink`. Immediately upon sending the
    # request, the client can use promise pipelining to begin pushing bytes to this object.
    #
    # Similarly, the caller passes a `resultsStreamSink` to the callee. If the response contains
    # any streams, it can start pushing to this immediately after responding.

    startStream @0 (externalIndex :UInt32) -> (stream :Capability);
    # Opens a stream corresponding to the given index in the JsValue's `externals` array. The type
    # of capability returned depends on the type of external. E.g. for `readableStream`, it is a
    # `ByteStream`.
  }
}

interface JsRpcTarget $Cxx.allowCancellation {
  struct CallParams {
    union {
      methodName @0 :Text;
      # Equivalent to `methodPath` where the list has only one element equal to this.

      methodPath @2 :List(Text);
      # Path of properties to follow from the JsRpcTarget itself to find the method being called.
      # E.g. if the application does:
      #
      #     myRpcTarget.foo.bar.baz()
      #
      # Then the path is ["foo", "bar", "baz"].
      #
      # The path can also be empty, which means that the JsRpcTarget itself is being invoked as a
      # function.
    }

    operation :union {
      callWithArgs @1 :JsValue;
      # Call the property as a function. This is a JsValue that always encodes a JavaScript Array
      # containing the arguments to the call.
      #
      # If `callWithArgs` is null (but is still the active member of the union), this indicates
      # that the argument list is empty.

      getProperty @3 :Void;
      # This indicates that we are not actually calling a method at all, but rather retrieving the
      # value of a property. RPC classes are allowed to define properties that can be fetched
      # asynchronously, although more commonly properties will be RPC targets themselves and their
      # methods will be invoked by sending a `methodPath` with more than one element. That is,
      # imagine you have:
      #
      #     myRpcTarget.foo.bar();
      #
      # This code makes a single RPC call with a path of ["foo", "bar"]. However, you could also
      # write:
      #
      #     let foo = await myRpcTarget.foo;
      #     foo.bar();
      #
      # This will make two separate calls. The first call is to "foo" and `getProperty` is used.
      # This returns a new JsRpcTarget. The second call is on that target, invoking the method
      # "bar".
    }

    resultsStreamSink @4 :JsValue.StreamSink;
    # StreamSink used for ReadableStreams found in the results.
  }

  struct CallResults {
    result @0 :JsValue;
    # The returned value.

    callPipeline @1 :JsRpcTarget;
    # Enables promise pipelining on the eventual call result. This is a JsRpcTarget wrapping the
    # result of the call, even if the result itself is a serializable object that would not
    # normally be treated as an RPC target. The caller may use this to initiate speculative calls
    # on this result without waiting for the initial call to complete (using promise pipelining).

    hasDisposer @2 :Bool;
    # If `hasDisposer` is true, the server side returned a serializable object (not a stub) with a
    # disposer (Symbol.dispose). The disposer itself is not included in the object's serialization,
    # but dropping the `callPipeline` will invoke it.
    #
    # On the client side, when an RPC returns a plain object, a disposer is added to it. In order
    # to avoid confusion, we want the server-side disposer to be invoked only after the client-side
    # disposer is invoked. To that end, when `hasDisposer` is true, the client should hold on to
    # `callPipeline` until the disposer is invoked. If `hasDisposer` is false, `callPipeline` can
    # safely be dropped immediately.

    paramsStreamSink @3 :JsValue.StreamSink;
    # StreamSink used for ReadableStreams found in the params. The caller begins sending bytes for
    # these streams immediately using promise pipelining.
  }

  call @0 CallParams -> CallResults;
  # Runs a Worker/DO's RPC method.
}

interface EventDispatcher @0xf20697475ec1752d {
  # Interface used to deliver events to a Worker's global event handlers.

  getHttpService @0 () -> (http :HttpService) $Cxx.allowCancellation;
  # Gets the HTTP interface to this worker (to trigger FetchEvents).

  sendTraces @1 (traces :List(Trace)) -> (result :SendTracesRun) $Cxx.allowCancellation;
  # Deliver a trace event to a trace worker. This always completes immediately; the trace handler
  # runs as a "waitUntil" task.

  prewarm @2 (url :Text) $Cxx.allowCancellation;

  runScheduled @3 (scheduledTime :Int64, cron :Text) -> (result :ScheduledRun)
      $Cxx.allowCancellation;
  # Runs a scheduled worker. Returns a ScheduledRun, detailing information about the run such as
  # the outcome and whether the run should be retried. This does not complete immediately.


  runAlarm @4 (scheduledTime :Int64, retryCount :UInt32) -> (result :AlarmRun);
  # Runs a worker's alarm.
  # scheduledTime is a unix timestamp in milliseconds for when the alarm should be run
  # retryCount indicates the retry count, if it's a retry. Else it'll be 0.
  # Returns an AlarmRun, detailing information about the run such as
  # the outcome and whether the run should be retried. This does not complete immediately.
  #
  # TODO(cleanup): runAlarm()'s implementation currently relies on *not* allowing cancellation.
  #   It would be cleaner to handle that inside the implementation so we could mark the entire
  #   interface (and file) with allowCancellation.

  queue @8 (messages :List(QueueMessage), queueName :Text) -> (result :QueueResponse)
      $Cxx.allowCancellation;
  # Delivers a batch of queue messages to a worker's queue event handler. Returns information about
  # the success of the batch, including which messages should be considered acknowledged and which
  # should be retried.

  jsRpcSession @9 () -> (topLevel :JsRpcTarget) $Cxx.allowCancellation;
  # Opens a JS rpc "session". The call does not return until the session is complete.
  #
  # `topLevel` is the top-level RPC target, on which exactly one method call can be made. This
  # call must be made using pipelining since `jsRpcSession()` won't return until after the call
  # completes.
  #
  # If, through the one top-level call, new capabilities are exchanged between the client and
  # server, then `jsRpcSession()` won't return until all those capabilities have been dropped.
  #
  # In C++, we use `WorkerInterface::customEvent()` to dispatch this event.

  obsolete5 @5();
  obsolete6 @6();
  obsolete7 @7();
  # Deleted methods, do not reuse these numbers.

  # Other methods might be added to handle other kinds of events, e.g. TCP connections, or maybe
  # even native Cap'n Proto RPC eventually.
}

interface WorkerdBootstrap {
  # Bootstrap interface exposed by workerd when serving Cap'n Proto RPC.

  startEvent @0 () -> (dispatcher :EventDispatcher);
  # Start a new event. Exactly one event should be delivered to the returned EventDispatcher.
  #
  # TODO(someday): Pass cfBlobJson? Currently doesn't matter since the cf blob is only present for
  #   HTTP requests which can be delivered over regular HTTP instead of capnp.
}
