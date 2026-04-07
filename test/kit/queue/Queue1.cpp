#include <sirius/kit/queue.h>
#include <sirius/thread/mutex.h>
#include <sirius/thread/thread.h>

#include <vector>

#include "inner/utils.h"

namespace {
inline constexpr int kNbProducers = 32;
inline constexpr int kNbMsgsPerThread = 2048;
inline constexpr int kQueueDepth = 128;
inline constexpr size_t kMsgBufferSize = 256;

struct MessagePayload {
  uint64_t producer_id;
  int seq_num;
  char data[kMsgBufferSize - sizeof(uint64_t) - sizeof(int)];
};

class QueueTestContext {
 public:
  ss_queue_t *q_data = nullptr;
  ss_queue_t *q_free = nullptr;

  std::atomic<size_t> total_produced {0};
  std::atomic<size_t> total_consumed {0};

  std::vector<std::vector<char>> memory_pool {};

  QueueTestContext() {
    ss_queue_args_t qargs {};
    qargs.elem_count = kQueueDepth;
    qargs.queue_type = kSsQueueTypeMutex;

    if (ss_queue_alloc(&q_data, &qargs) != 0 ||
        ss_queue_alloc(&q_free, &qargs) != 0) {
      ss_log_error("ss_queue_alloc\n");
      std::terminate();
    }

    memory_pool.resize(kQueueDepth);
    for (auto &buf : memory_pool) {
      buf.resize(kMsgBufferSize);
      ss_queue_put(q_free, (size_t)buf.data(), kSsTimeoutNoWaiting);
    }
  }

  ~QueueTestContext() {
    if (q_free) {
      ss_queue_free(q_free);
    }
    if (q_data) {
      ss_queue_free(q_data);
    }
  }
};

inline void *producer_routine(void *arg) {
  auto *ctx = static_cast<QueueTestContext *>(arg);
  uint64_t tid = SS_THREAD_ID;

  for (int i = 0; i < kNbMsgsPerThread; ++i) {
    void *buf_ptr = nullptr;
    if (ss_queue_get(ctx->q_free, (size_t *)&buf_ptr, kSsTimeoutInfinite) !=
        0) {
      ss_log_error("ss_queue_get\n");
      break;
    }

    auto *payload = static_cast<MessagePayload *>(buf_ptr);
    payload->producer_id = tid;
    payload->seq_num = i;
    snprintf(payload->data, sizeof(payload->data),
             "Msg %d from Thread %" PRIu64, i, tid);

    if (ss_queue_put(ctx->q_data, (size_t)buf_ptr, kSsTimeoutInfinite) != 0) {
      ss_log_error("ss_queue_put\n");
      break;
    }

    ctx->total_produced.fetch_add(1, std::memory_order_relaxed);
  }

  return nullptr;
}

inline void *consumer_routine(void *arg) {
  auto *ctx = static_cast<QueueTestContext *>(arg);

  while (true) {
    void *buf_ptr = nullptr;

    if (ss_queue_get(ctx->q_data, (size_t *)&buf_ptr, kSsTimeoutInfinite) !=
        0) {
      break;
    }

    if (buf_ptr == nullptr) {
      ss_log_infosp("Consumer received stop signal (Poison Pill)\n");
      break;
    }

    auto *payload = static_cast<MessagePayload *>(buf_ptr);
    ss_log_debugsp("Recv: [TID: %" PRIu64 "] [Seq: %d] %s\n",
                   payload->producer_id, payload->seq_num, payload->data);

    ctx->total_consumed.fetch_add(1, std::memory_order_relaxed);

    if (ss_queue_put(ctx->q_free, (size_t)buf_ptr, kSsTimeoutInfinite) != 0) {
      ss_log_error("ss_queue_put: consumer failed to recycle buffer\n");
    }
  }

  return nullptr;
}

inline int main_impl() {
  ss_log_infosp("Producers: %d. Msgs/Thread: %d. Total Expected: %d\n",
                kNbProducers, kNbMsgsPerThread,
                kNbProducers * kNbMsgsPerThread);

  QueueTestContext ctx;
  std::vector<ss_thread_t> producers(kNbProducers);
  ss_thread_t consumer_thd;

  if (ss_thread_create(&consumer_thd, nullptr, consumer_routine, &ctx) != 0) {
    ss_log_error("Failed to create consumer\n");
    return -1;
  }

  for (int i = 0; i < kNbProducers; ++i) {
    if (ss_thread_create(&producers[i], nullptr, producer_routine, &ctx) != 0) {
      ss_log_error("Failed to create producer `%d`\n", i);
      return -1;
    }
  }

  for (auto producer : producers) {
    ss_thread_join(producer, nullptr);
  }
  ss_log_infosp("All producers finished\n");

  ss_queue_put(ctx.q_data, 0, kSsTimeoutInfinite);

  ss_thread_join(consumer_thd, nullptr);

  size_t produced = ctx.total_produced.load();
  size_t consumed = ctx.total_consumed.load();
  size_t expected = kNbProducers * kNbMsgsPerThread;

  // Verify the result
  ss_log_infosp("Test result:\n");
  ss_log_infosp(" Total produced: %zu\n", produced);
  ss_log_infosp(" Total consumed: %zu\n", consumed);

  size_t free_count = 0;
  ss_queue_nb_cache(ctx.q_free, &free_count);
  ss_log_infosp("Free queue count: %zu (expected: %d)\n", free_count,
                kQueueDepth);

  bool success = true;
  if (produced != expected) {
    ss_log_error("Produced count mismatch\n");
    success = false;
  }
  if (consumed != expected) {
    ss_log_error("Consumed count mismatch\n");
    success = false;
  }
  if (free_count != kQueueDepth) {
    ss_log_warnsp(
      "Memory leak detected or accounting error (free queue not full)\n");
  }

  if (success) {
    ss_log_infosp("Test pass\n");
  } else {
    ss_log_error("Test failed\n");
  }

  return success ? 0 : 1;
}
} // namespace

int main() {
  auto init = utils::Init();

  try {
    return main_impl();
  } catch (const std::exception &e) {
    ss_log_error("%s\n", e.what());
    return -1;
  } catch (...) {
    ss_log_error("`exception`: unknow\n");
    return -1;
  }
}
