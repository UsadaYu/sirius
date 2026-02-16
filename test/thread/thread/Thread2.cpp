#include <sirius/thread/thread.h>

#include <algorithm>
#include <array>
#include <set>

#include "internal/utils.h"

static constexpr int kNbThreads = 6;

using HashPair = std::pair<std::string_view, std::string_view>;
using IndexToHash = std::array<HashPair, kNbThreads>;

static const IndexToHash kHashes = {
  {
   {"97ce467c-534d-408c-a4c6-4548e5f0fbf0",
     "36ad79d2-94e3-4172-9421-092a52f7ef98"},
   {"cbab5212-bbbe-4730-9b3a-c069147e1c13",
     "b4790ab4-da08-4888-a1ce-6ac88bf9c35b"},
   {"e3aeed55-390a-4c3f-83d8-f1f28a937e07",
     "b4f0e0c9-5d32-476d-8824-5c41ed2e3f75"},
   {"10d48fa2-0180-4ef8-a18e-c15ffa3b7bf8",
     "fea50995-f535-498f-8738-6303bd2c90c3"},
   {"c43cc2fd-7498-4ecf-8a97-7230f32b55fe",
     "f778e46c-fe1c-4da5-b653-2dda7087df6b"},
   {"4ecc5c75-a008-4a35-8262-eb241e1c7b94",
     "3f58a037-1cd6-49f4-92a0-df71d59b27e2"},
   }
};

struct Arg {
  int index;
  std::string_view string;
};

void *foo(void *arg) {
  auto *content = (Arg *)arg;
  int index = content->index;
  auto hash1 = kHashes[index].first;
  auto hash2 = kHashes[index].second;

  if (content->string == hash1) {
    sirius_infosp(
      "Sub-thread (index: %d). The `argument` was successfully verified\n",
      index);
  } else {
    std::string es;
    es = std::format(LOG_RED
                     "\n"
                     "  Sub-thread (TID: {0})\n"
                     "  Fail to verifiy the `argument`:\n"
                     "    Actual string:   {1}\n"
                     "    Expected string: {2}\n" LOG_COLOR_NONE,
                     SIRIUS_THREAD_ID, content->string, hash1);
    throw std::runtime_error(es);
  }

  sirius_thread_exit((void *)hash2.data());

  return nullptr;
}

int main() {
  auto init = Utils::Init();

  int ret;
  std::string es;
  sirius_thread_attr_t attr {};
  sirius_thread_t threads[kNbThreads];
  Arg args[kNbThreads] {};
  void *stackaddr = nullptr;

  attr.inherit_sched = kSiriusThreadExplicitSched;
  attr.scope = kSiriusThreadScopeSystem;
  attr.stackaddr = stackaddr;
  attr.guardsize = 4096;

  for (int i = 0; i < kNbThreads; ++i) {
    args[i].index = i;
    args[i].string = kHashes[i].first;
    ret = sirius_thread_create(threads + i, &attr, foo, (void *)(args + i));
    if (ret) {
      es = std::format(LOG_RED
                       "\n"
                       "  Main-Join (creating the index: {0})\n"
                       "  `sirius_thread_create` error: {1}\n" LOG_COLOR_NONE,
                       i, ret);
      throw std::runtime_error(es);
    }
  }

  const char *retval = nullptr;
  for (int i = 0; i < kNbThreads; ++i) {
    ret = sirius_thread_join(threads[i], (void **)&retval);
    if (ret) {
      es = std::format(LOG_RED
                       "\n"
                       "  Main-Join (joining the index: {0})\n"
                       "  `sirius_thread_join` error: {1}\n" LOG_COLOR_NONE,
                       i, ret);
      throw std::runtime_error(es);
    }

    auto hash2 = kHashes[i].second;
    if (retval == hash2.data()) {
      sirius_infosp(
        "Main-Join (joining the index: %d). The `retval` was successfully "
        "verified\n",
        i);
    } else {
      es = std::format(LOG_RED
                       "\n"
                       "  Main-Join (joining the index: {0})\n"
                       "  Fail to verifiy the `retval`:\n"
                       "    Actual string:   {1}\n"
                       "    Expected string: {2}\n" LOG_COLOR_NONE,
                       i, retval, hash2);
      throw std::runtime_error(es);
    }

    retval = nullptr;
  }

  return 0;
}
