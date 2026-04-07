#include <sirius/thread/cond.h>

#include <queue>
#include <thread>

#include "inner/utils.h"

namespace {
inline constexpr int kNbProducers = 4;
inline constexpr int kNbConsumers = 4;
inline constexpr int kItemsPerProducer = 10000;
inline constexpr int kExpectedTotal = kNbProducers * kItemsPerProducer;

struct SharedContext {
  ss_mutex_t mutex;
  ss_cond_t cond_not_empty;

  std::queue<int> queue;
  bool finished_producing = false;

  std::atomic<int> total_consumed = 0;

  SharedContext() {
    ss_mutex_init(&mutex, nullptr);
    ss_cond_init(&cond_not_empty, nullptr);
  }

  ~SharedContext() {
    ss_mutex_destroy(&mutex);
    ss_cond_destroy(&cond_not_empty);
  }
};

inline void producer_routine(SharedContext *ctx, int id) {
  for (int i = 0; i < kItemsPerProducer; ++i) {
    ss_mutex_lock(&ctx->mutex);

    ctx->queue.push(1);

    ss_cond_signal(&ctx->cond_not_empty);

    ss_mutex_unlock(&ctx->mutex);
  }

  ss_log_infosp("Producer `%d` completed\n", id);
}

inline void consumer_routine(SharedContext *ctx, int id) {
  int local_consumed = 0;

  while (true) {
    ss_mutex_lock(&ctx->mutex);

    while (ctx->queue.empty() && !ctx->finished_producing) {
      ss_cond_wait(&ctx->cond_not_empty, &ctx->mutex);
    }

    if (ctx->queue.empty() && ctx->finished_producing) {
      ss_mutex_unlock(&ctx->mutex);
      break;
    }

    ctx->queue.pop();
    ++local_consumed;
    ++ctx->total_consumed;

    ss_mutex_unlock(&ctx->mutex);
  }

  ss_log_infosp("Consumer `%d` consumed: %d\n", id, local_consumed);
}

inline int main_impl() {
  SharedContext ctx;
  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;

  for (int i = 0; i < kNbConsumers; ++i) {
    consumers.emplace_back(consumer_routine, &ctx, i);
  }

  for (int i = 0; i < kNbProducers; ++i) {
    producers.emplace_back(producer_routine, &ctx, i);
  }

  for (auto &producer : producers) {
    if (producer.joinable()) {
      producer.join();
    }
  }

  ss_mutex_lock(&ctx.mutex);
  ctx.finished_producing = true;
  ss_cond_broadcast(&ctx.cond_not_empty);
  ss_mutex_unlock(&ctx.mutex);

  for (auto &consumer : consumers) {
    if (consumer.joinable()) {
      consumer.join();
    }
  }

  // Verify the result
  if (ctx.total_consumed.load() == kExpectedTotal) {
    ss_log_infosp("Test passed. Actual total: %d (expected total: %d)\n",
                  ctx.total_consumed.load(), kExpectedTotal);
  } else {
    ss_log_error("Test failed. Counter value: %d (expected value: %d)\n",
                 ctx.total_consumed.load(), kExpectedTotal);
  }

  return (int)!(ctx.total_consumed.load() == kExpectedTotal);
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
