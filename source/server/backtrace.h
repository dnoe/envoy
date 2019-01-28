#pragma once

#include "common/common/logger.h"

#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"

namespace Envoy {
#define BACKTRACE_LOG()                                                                            \
  do {                                                                                             \
    BackwardsTrace t;                                                                              \
    t.capture();                                                                                   \
    t.logTrace();                                                                                  \
  } while (0)

/**
 * Use absl::Stacktrace and absl::Symbolize to log resolved symbols
 * stack traces on demand. To use this just do:
 *
 * BackwardsTrace tracer;
 * tracer.capture(); // Trace is captured as of here.
 * tracer.logTrace(); // Output the captured trace to the log.
 *
 * The capture and log steps are separated to enable debugging in the case where
 * you want to capture a stack trace from inside some logic but don't know whether
 * you want to bother logging it until later.
 *
 * For convenience a macro is provided BACKTRACE_LOG() which performs the
 * construction, capture, and log in one shot.
 *
 * If the symbols cannot be resolved by absl::Symbolize then the raw address
 * will be printed instead.
 */
class BackwardsTrace : Logger::Loggable<Logger::Id::backtrace> {
public:
  BackwardsTrace() {}

  /**
   * Capture a stack trace.
   *
   * The trace will begin with the call to capture().
   */
  void capture() {
    // Skip of one means we exclude the last call, which must be to capture().
    stack_depth_ = absl::GetStackTrace(stack_trace_, MaxStackDepth, /* skip_count = */ 1);
  }

  /**
   * Capture a stack trace from a particular context.
   *
   * This can be used to capture a useful stack trace from a fatal signal
   * handler. The context argument should be a pointer to the context passed
   * to a signal handler registered via a sigaction struct.
   *
   * @param context A pointer to ucontext_t obtained from a sigaction handler.
   */
  void captureFrom(const void* context) {
    stack_depth_ =
        absl::GetStackTraceWithContext(stack_trace_, MaxStackDepth, /* skip_count = */ 1, context,
                                       /* min_dropped_frames = */ nullptr);
  }

  /**
   * Log the stack trace.
   */
  void logTrace() {
    ENVOY_LOG(critical, "Backtrace:");

    for (int i = 0; i < stack_depth_; ++i) {
      char out[1024];
      const bool success = absl::Symbolize(stack_trace_[i], out, sizeof(out));
      if (success) {
        ENVOY_LOG(critical, "#{}: {}", i, out);
      } else {
        ENVOY_LOG(critical, "#{}: {}", i, stack_trace_[i]);
      }
    }
  }

  void logFault(const char* signame, const void* addr) {
    ENVOY_LOG(critical, "Caught {}, suspect faulting address {}", signame, addr);
  }

private:
  static constexpr int MaxStackDepth = 64;
  void* stack_trace_[MaxStackDepth];
  int stack_depth_{0};
};
} // namespace Envoy
