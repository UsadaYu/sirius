#include <sirius/kit/queue.h>
#include <sirius/thread/mutex.h>
#include <sirius/thread/thread.h>

#include <vector>

#include "internal/utils.h"

static constexpr int kNbProducers = 32;
static constexpr int kNbMsgsPerThread = 2048;
static constexpr int kQueueDepth = 128;
static constexpr size_t kMsgBufferSize = 256;

struct MessagePayload {
  uint64_t producer_id;
  int seq_num;
  char data[kMsgBufferSize - sizeof(uint64_t) - sizeof(int)];
};

class QueueTestContext {
 public:
  sirius_queue_t *q_data = nullptr;
  sirius_queue_t *q_free = nullptr;

  std::atomic<size_t> total_produced {0};
  std::atomic<size_t> total_consumed {0};

  std::vector<std::vector<char>> memory_pool;

  QueueTestContext() {
    sirius_queue_args_t qargs {};
    qargs.elem_count = kQueueDepth;
    qargs.queue_type = kSiriusQueueTypeMutex;

    if (sirius_queue_alloc(&q_data, &qargs) != 0 ||
        sirius_queue_alloc(&q_free, &qargs) != 0) {
      sirius_error("sirius_queue_alloc\n");
      std::terminate();
    }

    memory_pool.resize(kQueueDepth);
    for (auto &buf : memory_pool) {
      buf.resize(kMsgBufferSize);
      sirius_queue_put(q_free, (size_t)buf.data(), SIRIUS_TIMEOUT_NO_WAITING);
    }
  }

  ~QueueTestContext() {
    if (q_free)
      sirius_queue_free(q_free);
    if (q_data)
      sirius_queue_free(q_data);
  }
};

void *producer_routine(void *arg) {
  auto *ctx = static_cast<QueueTestContext *>(arg);
  uint64_t tid = SIRIUS_THREAD_ID;

  for (int i = 0; i < kNbMsgsPerThread; ++i) {
    void *buf_ptr = nullptr;
    if (sirius_queue_get(ctx->q_free, (size_t *)&buf_ptr,
                         SIRIUS_TIMEOUT_INFINITE) != 0) {
      sirius_error("sirius_queue_get\n");
      break;
    }

    auto *payload = static_cast<MessagePayload *>(buf_ptr);
    payload->producer_id = tid;
    payload->seq_num = i;
    snprintf(payload->data, sizeof(payload->data),
             "Msg %d from Thread %" PRIu64, i, tid);

    if (sirius_queue_put(ctx->q_data, (size_t)buf_ptr,
                         SIRIUS_TIMEOUT_INFINITE) != 0) {
      sirius_error("sirius_queue_put\n");
      break;
    }

    ctx->total_produced.fetch_add(1, std::memory_order_relaxed);
  }

  return nullptr;
}

void *consumer_routine(void *arg) {
  auto *ctx = static_cast<QueueTestContext *>(arg);

  while (true) {
    void *buf_ptr = nullptr;

    if (sirius_queue_get(ctx->q_data, (size_t *)&buf_ptr,
                         SIRIUS_TIMEOUT_INFINITE) != 0) {
      break;
    }

    if (buf_ptr == nullptr) {
      sirius_infosp("Consumer received stop signal (Poison Pill)\n");
      break;
    }

    auto *payload = static_cast<MessagePayload *>(buf_ptr);
    sirius_debugsp("Recv: [TID: %" PRIu64 "] [Seq: %d] %s\n",
                   payload->producer_id, payload->seq_num, payload->data);

    ctx->total_consumed.fetch_add(1, std::memory_order_relaxed);

    if (sirius_queue_put(ctx->q_free, (size_t)buf_ptr,
                         SIRIUS_TIMEOUT_INFINITE) != 0) {
      sirius_error("sirius_queue_put: consumer failed to recycle buffer\n");
    }
  }

  return nullptr;
}

int main() {
  auto init = Utils::Init();

  sirius_infosp("Producers: %d. Msgs/Thread: %d. Total Expected: %d\n",
                kNbProducers, kNbMsgsPerThread,
                kNbProducers * kNbMsgsPerThread);

  QueueTestContext ctx;
  std::vector<sirius_thread_t> producers(kNbProducers);
  sirius_thread_t consumer_thd;

  if (sirius_thread_create(&consumer_thd, nullptr, consumer_routine, &ctx) !=
      0) {
    sirius_error("Failed to create consumer\n");
    return -1;
  }

  for (int i = 0; i < kNbProducers; ++i) {
    if (sirius_thread_create(&producers[i], nullptr, producer_routine, &ctx) !=
        0) {
      sirius_error("Failed to create producer `%d`\n", i);
      return -1;
    }
  }

  for (int i = 0; i < kNbProducers; ++i) {
    sirius_thread_join(producers[i], nullptr);
  }
  sirius_infosp("All producers finished\n");

  sirius_queue_put(ctx.q_data, 0, SIRIUS_TIMEOUT_INFINITE);

  sirius_thread_join(consumer_thd, nullptr);

  size_t produced = ctx.total_produced.load();
  size_t consumed = ctx.total_consumed.load();
  size_t expected = kNbProducers * kNbMsgsPerThread;

  // Verify the result
  sirius_infosp("Test result:\n");
  sirius_infosp(" Total produced: %zu\n", produced);
  sirius_infosp(" Total consumed: %zu\n", consumed);

  size_t free_count = 0;
  sirius_queue_nb_cache(ctx.q_free, &free_count);
  sirius_infosp("Free queue count: %zu (expected: %d)\n", free_count,
                kQueueDepth);

  bool success = true;
  if (produced != expected) {
    sirius_error("Produced count mismatch\n");
    success = false;
  }
  if (consumed != expected) {
    sirius_error("Consumed count mismatch\n");
    success = false;
  }
  if (free_count != kQueueDepth) {
    sirius_warnsp(
      "Memory leak detected or accounting error (free queue not full)\n");
  }

  if (success) {
    sirius_infosp("Test pass\n");
  } else {
    sirius_error("Test failed\n");
  }

  return success ? 0 : 1;
}
