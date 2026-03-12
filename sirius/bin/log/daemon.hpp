#pragma once

#include <functional>

#include "utils/log/shm.hpp"
#include "utils/process/sys.hpp"
#include "utils/time.hpp"

namespace sirius {
namespace bin {
namespace log {
using u_io = utils::Io;
namespace u_log = utils::log;
namespace u_prcs = utils::process;

class Daemon {
 public:
  Daemon()
      : should_leave_(u_prcs::sys_init_type() !=
                        u_prcs::SysInitType::kUnreliableInit &&
                      u_prcs::sys_init_type() != u_prcs::SysInitType::kUnknown),
        log_shm_(u_log::Shm::instance()),
        fd_out_(STDOUT_FILENO),
        fd_err_(STDERR_FILENO) {}

  ~Daemon() = default;

  auto main() -> std::expected<void, UTrace> {
    try {
      u_log::GMutex::LockGuard();
      auto ret = log_shm_.shm_alloc(u_log::MasterType::kDaemon)
                   .and_then([&](auto var) {
                     master_ = std::move(var);
                     return master_->slots_alloc();
                   })
                   .transform_error([&](UTrace &&e) {
                     log_shm_.shm_free();
                     utrace_transform_error(e);
                   });
      if (!ret.has_value())
        return ret;
    } catch (const std::exception &e) {
      return std::unexpected(UTrace(std::move(e.what())));
    }

    auto main_ret = daemon_main().utrace_transform_error_default();
    master_->slots_free();
    log_shm_.shm_free();
    if (main_ret.has_value()) {
      (void)u_log::GMutex::instance().unlock();
    }
    return main_ret;
  }

  void log_write(int level, const void *buffer, size_t size) {
    int fd = level <= SIRIUS_LOG_LEVEL_WARN ? fd_err_ : fd_out_;
    utils_write(fd, buffer, size);
  }

 private:
  bool should_leave_;
  u_log::Shm &log_shm_;
  std::unique_ptr<u_log::Shm::Master> master_;
  int fd_out_;
  int fd_err_;

  class MainStructor {
   public:
    MainStructor(Daemon &parent)
        : parent_(parent), master_(*parent.master_.get()) {
      io_ln_infosp("Daemon startup (PID: {0})", u_prcs::pid());

      thread_crash_ =
        std::jthread([this](std::stop_token st) { parent_.thread_crash(st); });
      thread_consumer_ = std::jthread(
        [this](std::stop_token st) { parent_.thread_consumer(st); });
      thread_monitor_ = std::jthread(
        [this](std::stop_token st) { parent_.thread_monitor(st); });

      master_.get_shm_header()->is_daemon_ready.store(
        true, std::memory_order_relaxed);
    }

    ~MainStructor() {
      master_.get_shm_header()->is_daemon_ready.store(
        false, std::memory_order_relaxed);

      thread_monitor_.request_stop();
      thread_consumer_.request_stop();
      thread_crash_.request_stop();

      if (thread_monitor_.joinable()) {
        thread_monitor_.join();
      }
      if (thread_consumer_.joinable()) {
        thread_consumer_.join();
      }
      if (thread_crash_.joinable()) {
        thread_crash_.join();
      }

      io_ln_infosp("Daemon teardown (PID: {0})", u_prcs::pid());
    }

   private:
    Daemon &parent_;
    u_log::Shm::Master &master_;
    std::jthread thread_crash_ {}, thread_consumer_ {}, thread_monitor_ {};
  };

  /**
   * @note The `Shm::GMutex` will be held if return success.
   */
  auto daemon_main() -> std::expected<void, UTrace> {
    auto main_structor = MainStructor(*this);

    auto header = master_->get_shm_header();
    uint64_t sleep_ms = 500;
    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

      if (auto ret = u_log::GMutex::instance().lock(); !ret.has_value()) {
        utrace_return(ret);
      }

      {
        master_->lock_guard();
        if (header->slot_attached_count <= u_log::kProcessNbDaemon) {
          if (should_leave_) {
            break;
          } else {
            sleep_ms = 2500;
          }
        } else {
          sleep_ms = 500;
        }
      }

      (void)u_log::GMutex::instance().unlock();
    }

    return {};
  }

  void thread_crash(std::stop_token stop_token) {
    if (auto ret = master_->mutex_crash_lock(); !ret.has_value()) {
      io_ln_error("{}", ret.error().join_self_all());
      return;
    }

    for (uint64_t i = 0; !stop_token.stop_requested();
         std::this_thread::sleep_for(std::chrono::milliseconds(1500))) {
      if (++i % 60 == 0) {
        io_ln_infosp("Daemon is alive (PID: {0})", u_prcs::pid());
      }
    }

    (void)master_->mutex_crash_unlock();
  }

  void thread_consumer(std::stop_token stop_token) {
    auto header = master_->get_shm_header();
    uint32_t idle_counter = 0;

    while (true) {
      uint64_t index_rd = header->read_index.load(std::memory_order_acquire);
      uint64_t index_wr;

    label_load_write:
      index_wr = header->write_index.load(std::memory_order_acquire);
      if (index_rd >= index_wr) {
        if (stop_token.stop_requested())
          break;
        ++idle_counter;
        if (idle_counter < 200) {
          std::this_thread::yield();
        } else if (idle_counter < 500) {
          std::this_thread::sleep_for(std::chrono::microseconds(800));
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        goto label_load_write;
      }
      idle_counter = 0;

      uint64_t retry_times = u_log::kShmCapacity - (index_wr - index_rd) + 10;
      if (auto opt_slot = get_slot(index_rd, retry_times); opt_slot) {
        u_log::ShmSlot &slot = opt_slot->get();
        u_log::ShmBuf &buffer = slot.buffer;
        if (buffer.type == u_log::ShmBufDataType::kLog) [[likely]] {
          auto &data = buffer.data.log;
          log_write(buffer.level, (void *)data.buf, data.buf_size);
        } else {
          /* ----------------------------------------- */
          /* ---------------- Not Yet ---------------- */
          /* ----------------------------------------- */
        }
        slot.state.store(u_log::ShmSlotState::kFree, std::memory_order_release);
      } else if (stop_token.stop_requested()) {
        break;
      }

      header->read_index.fetch_add(1, std::memory_order_release);
    }
  }

  void thread_monitor(std::stop_token stop_token) {
    auto header = master_->get_shm_header();
    auto slots = master_->get_shm_slots();

    while (!stop_token.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(300));

      uint64_t now = utils::time::get_monotonic_steady_ms();
      for (size_t i = 0; i < u_log::kShmCapacity; ++i) {
        u_log::ShmSlotState s = slots[i].state.load(std::memory_order_acquire);
        if (s == u_log::ShmSlotState::kWaiting) {
          uint64_t ts = slots[i].timestamp_ms.load(std::memory_order_relaxed);
          if (now - ts > u_log::kShmSlotResetTimeoutMs) {
            io_ln_warnsp("Recovering stuck slot: {0}", i);

            /**
             * @note Construct a forged log indicating data loss.
             */
            auto es = io_str_warnsp("Slot recovered/skipped due to timeout");
            slots[i].buffer.type = u_log::ShmBufDataType::kLog;
            slots[i].buffer.level = SIRIUS_LOG_LEVEL_ERROR;
            auto &log = slots[i].buffer.data.log;
            log.buf_size = es.size();
            std::memcpy(log.buf, es.c_str(), log.buf_size + 1);

            /**
             * @note Let it be `kReady`, so that consumers can read it, thereby
             * advancing the index.
             */
            slots[i].state.store(u_log::ShmSlotState::kReady,
                                 std::memory_order_release);
          }
        }
      }
    }
  }

  [[nodiscard]] std::optional<std::reference_wrapper<u_log::ShmSlot>>
  get_slot(const uint64_t read_index, const int retry_times) {
    size_t slot_idx = read_index & (u_log::kShmCapacity - 1);
    u_log::ShmSlot &slot = master_->get_shm_slots()[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) !=
           u_log::ShmSlotState::kReady) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (++retries > retry_times) {
        io_ln_warnsp("Skip corrupted slot index: {0}", read_index);
        return std::nullopt;
      }
    }

    return slot;
  }
};
} // namespace log
} // namespace bin
} // namespace sirius
