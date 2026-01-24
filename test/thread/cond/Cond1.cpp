#include <sirius/thread/cond.h>

#include <queue>
#include <thread>

#include "internal/utils.h"

const int NB_PRODUCERS = 4;
const int NB_CONSUMERS = 4;
const int ITEMS_PER_PRODUCER = 10000;
const int EXPECTED_TOTAL = NB_PRODUCERS * ITEMS_PER_PRODUCER;

struct SharedContext {
  sirius_mutex_t mutex;
  sirius_cond_t cond_not_empty;

  std::queue<int> queue;
  bool finished_producing = false;

  std::atomic<int> total_consumed = 0;

  SharedContext() {
    sirius_mutex_init(&mutex, nullptr);
    sirius_cond_init(&cond_not_empty, nullptr);
  }

  ~SharedContext() {
    sirius_mutex_destroy(&mutex);
    sirius_cond_destroy(&cond_not_empty);
  }
};

static void producer_routine(SharedContext *ctx, int id) {
  for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
    sirius_mutex_lock(&ctx->mutex);

    ctx->queue.push(1);

    sirius_cond_signal(&ctx->cond_not_empty);

    sirius_mutex_unlock(&ctx->mutex);
  }

  sirius_infosp("Producer `%d` completed\n", id);
}

static void consumer_routine(SharedContext *ctx, int id) {
  int local_consumed = 0;

  while (true) {
    sirius_mutex_lock(&ctx->mutex);

    while (ctx->queue.empty() && !ctx->finished_producing) {
      sirius_cond_wait(&ctx->cond_not_empty, &ctx->mutex);
    }

    if (ctx->queue.empty() && ctx->finished_producing) {
      sirius_mutex_unlock(&ctx->mutex);
      break;
    }

    ctx->queue.pop();
    local_consumed++;
    ctx->total_consumed++;

    sirius_mutex_unlock(&ctx->mutex);
  }

  sirius_infosp("Consumer `%d` consumed: %d\n", id, local_consumed);
}

int main() {
  auto init = Utils::Init();

  SharedContext ctx;
  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;

  for (int i = 0; i < NB_CONSUMERS; ++i) {
    consumers.emplace_back(consumer_routine, &ctx, i);
  }

  for (int i = 0; i < NB_PRODUCERS; ++i) {
    producers.emplace_back(producer_routine, &ctx, i);
  }

  for (auto &t : producers) {
    if (t.joinable())
      t.join();
  }

  sirius_mutex_lock(&ctx.mutex);
  ctx.finished_producing = true;
  sirius_cond_broadcast(&ctx.cond_not_empty);
  sirius_mutex_unlock(&ctx.mutex);

  for (auto &t : consumers) {
    if (t.joinable())
      t.join();
  }

  // Verify the result
  if (ctx.total_consumed.load() == EXPECTED_TOTAL) {
    sirius_infosp("Test passed. Actual total: %d (expected total: %d)\n",
                  ctx.total_consumed.load(), EXPECTED_TOTAL);
  } else {
    sirius_error("Test failed. Counter value: %d (expected value: %d)\n",
                 ctx.total_consumed.load(), EXPECTED_TOTAL);
  }

  return (int)!(ctx.total_consumed.load() == EXPECTED_TOTAL);
}
