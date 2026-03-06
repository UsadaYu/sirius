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
namespace ul_shm = u_log::shm;
namespace u_prcs = utils::process;

class Daemon {
 public:
  Daemon()
      : should_leave_(u_prcs::sys::check_init_type() !=
                        u_prcs::sys::InitType::kUnreliableInit &&
                      u_prcs::sys::check_init_type() !=
                        u_prcs::sys::InitType::kUnknown),
        log_shm_(ul_shm::Shm::instance()),
        fd_out_(STDOUT_FILENO),
        fd_err_(STDERR_FILENO) {}

  ~Daemon() = default;

  auto main() -> std::expected<void, std::string> {
    bool main_ret = false;
    std::string ret_msg;

    if (auto ret = ul_shm::GMutex::instance().lock(); !ret.has_value()) {
      return ret;
    }
    if (auto ret = log_shm_.shm_alloc(u_log::MasterType::kDaemon);
        !ret.has_value()) {
      ret_msg = ret.error();
      goto label_free1;
    } else {
      master_ = std::move(ret.value());
    }
    if (auto ret = master_->slots_alloc(); !ret.has_value()) {
      ret_msg = ret.error();
      goto label_free2;
    }
    (void)ul_shm::GMutex::instance().unlock();

    if (auto ret = daemon_main(); !ret.has_value()) {
      ret_msg = ret.error();
    } else {
      main_ret = true;
    }

    master_->slots_free();
  label_free2:
    master_.reset();
    log_shm_.shm_free();
  label_free1:
    (void)ul_shm::GMutex::instance().unlock();

    return main_ret ? std::expected<void, std::string> {}
                    : std::unexpected(ret_msg);
  }

  void log_write(int level, const void *buffer, size_t size) {
    int fd = level <= SIRIUS_LOG_LEVEL_WARN ? fd_err_ : fd_out_;

    utils_write(fd, buffer, size);
  }

 private:
  bool should_leave_;
  ul_shm::Shm &log_shm_;
  std::unique_ptr<ul_shm::Shm::Master> master_;
  int fd_out_;
  int fd_err_;

  std::jthread thread_crash_, thread_consumer_, thread_monitor_;

  void daemon_setup() {
    utils::io_outln(IO_ISP("Daemon startup (PID: {0})", u_prcs::pid()));

    thread_crash_ = std::jthread(std::bind_front(&Daemon::thread_crash, this));
    thread_consumer_ =
      std::jthread(std::bind_front(&Daemon::thread_consumer, this));
    thread_monitor_ =
      std::jthread(std::bind_front(&Daemon::thread_monitor, this));

    master_->get_shm_header()->is_daemon_ready.store(true,
                                                     std::memory_order_relaxed);
  }

  void daemon_teardown() {
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

    utils::io_outln(IO_ISP("Daemon teardown (PID: {0})", u_prcs::pid()));
  }

  /**
   * @note After this function ends, the `Shm::GMutex` will be held.
   */
  auto daemon_main() -> std::expected<void, std::string> {
    auto header = master_->get_shm_header();

    daemon_setup();

    uint64_t sleep_ms = 500;
    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

      (void)ul_shm::GMutex::instance().lock();

      {
        master_->lock_guard();

        if (header->attached_count <= u_log::kProcessNbDaemon) {
          if (should_leave_) {
            header->is_daemon_ready.store(false, std::memory_order_relaxed);
            break;
          } else {
            sleep_ms = 2500;
          }
        } else {
          sleep_ms = 500;
        }
      }

      (void)ul_shm::GMutex::instance().unlock();
    }

    daemon_teardown();

    return {};
  }

  void thread_crash(std::stop_token stop_token) {
    if (auto ret = master_->mutex_crash_lock(); !ret.has_value()) {
      utils::io_errln(
        ret.error().append(u_io::row_gs("\n{}", utils_pretty_func)));
      return;
    }

    for (uint64_t i = 0; !stop_token.stop_requested();
         std::this_thread::sleep_for(std::chrono::milliseconds(1500))) {
      if (++i % 60 == 0) {
        utils::io_outln(IO_DSP("Daemon is alive (PID: {0})", u_prcs::pid()));
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
        ul_shm::Slot &slot = opt_slot->get();
        ul_shm::Buffer &buffer = slot.buffer;
        if (buffer.type == ul_shm::DataType::kLog) [[likely]] {
          auto &data = buffer.data.log;
          log_write(buffer.level, (void *)data.buf, data.buf_size);
        } else {
          /* ----------------------------------------- */
          /* ---------------- Not Yet ---------------- */
          /* ----------------------------------------- */
        }
        slot.state.store(ul_shm::SlotState::kFree, std::memory_order_release);
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
        ul_shm::SlotState s = slots[i].state.load(std::memory_order_acquire);

        if (s == ul_shm::SlotState::kWaiting) {
          uint64_t ts = slots[i].timestamp_ms.load(std::memory_order_relaxed);
          if (now - ts > u_log::kShmSlotResetTimeoutMilliseconds) {
            utils::io_errln(IO_WSP("Recovering stuck slot: {0}", i));

            /**
             * @note Construct a forged log indicating data loss.
             */
            auto es =
              IO_WSP("Slot recovered/skipped due to timeout").append("\n");
            slots[i].buffer.type = ul_shm::DataType::kLog;
            slots[i].buffer.level = SIRIUS_LOG_LEVEL_ERROR;

            auto &log = slots[i].buffer.data.log;
            log.buf_size = es.size();
            std::memcpy(log.buf, es.c_str(), log.buf_size + 1);

            /**
             * @note Let it be `kReady`, so that consumers can read it, thereby
             * advancing the index.
             */
            slots[i].state.store(ul_shm::SlotState::kReady,
                                 std::memory_order_release);
          }
        }
      }
    }
  }

  [[nodiscard]] std::optional<std::reference_wrapper<ul_shm::Slot>>
  get_slot(const uint64_t read_index, const int retry_times) {
    size_t slot_idx = read_index & (u_log::kShmCapacity - 1);
    ul_shm::Slot &slot = master_->get_shm_slots()[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) !=
           ul_shm::SlotState::kReady) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (++retries > retry_times) {
        utils::io_errln(IO_WSP("Skip corrupted slot index: {0}", read_index));
        return std::nullopt;
      }
    }

    return slot;
  }
};
} // namespace log
} // namespace bin
} // namespace sirius
