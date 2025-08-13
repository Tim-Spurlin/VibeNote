#include <array>
#include <condition_variable>
#include <cstdint>
#include <queue>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "gpu_guard.h"

namespace vibenote {

enum class TaskType { kWatch, kInteractive, kExport };

enum class TaskPriority { kHigh = 0, kNormal = 1, kLow = 2, kCount };

struct Task {
  std::uint64_t id{};
  TaskType type{TaskType::kInteractive};
  TaskPriority priority{TaskPriority::kNormal};
  std::string prompt;
  std::function<void(const std::string &)> callback;
};

struct QueueConfig {
  std::size_t max_queue_depth{128};
  std::unordered_map<TaskType, std::size_t> max_concurrent;
};

}  // namespace vibenote

namespace std {
template <>
struct hash<vibenote::TaskType> {
  std::size_t operator()(const vibenote::TaskType &t) const noexcept {
    return static_cast<std::size_t>(t);
  }
};
}  // namespace std

namespace vibenote {

class TaskQueue {
 public:
  TaskQueue(GpuGuard *guard, QueueConfig cfg);

  bool enqueue(Task task);
  Task dequeue();
  void taskCompleted(std::uint64_t id);
  void setPaused(bool paused);

  struct Stats {
    std::array<std::size_t, static_cast<std::size_t>(TaskPriority::kCount)> queued{};
    std::unordered_map<TaskType, std::size_t> running;
  };

  Stats getStats() const;

 private:
  bool canRunUnlocked() const;
  std::optional<Task> popNextTaskUnlocked();
  std::optional<Task> findReadyTask(std::deque<Task> &q) const;
  std::size_t totalQueuedUnlocked() const;

  mutable std::mutex mutex_;
  std::condition_variable cv_;

  std::array<std::deque<Task>, static_cast<std::size_t>(TaskPriority::kCount)> queues_;
  std::unordered_map<TaskType, std::size_t> running_;
  std::unordered_map<std::uint64_t, TaskType> inflight_;

  GpuGuard *guard_;
  QueueConfig config_;
  bool paused_{false};
  std::size_t rr_index_{static_cast<std::size_t>(TaskPriority::kNormal)};  // rotate normal/low
};

TaskQueue::TaskQueue(GpuGuard *guard, QueueConfig cfg)
    : guard_(guard), config_(std::move(cfg)) {
  running_[TaskType::kWatch] = 0;
  running_[TaskType::kInteractive] = 0;
  running_[TaskType::kExport] = 0;

  if (guard_) {
    guard_->throttleStateChanged([this](bool paused) { setPaused(paused); });
  }
}

bool TaskQueue::enqueue(Task task) {
  std::lock_guard lock(mutex_);
  if (totalQueuedUnlocked() >= config_.max_queue_depth) {
    return false;
  }
  auto idx = static_cast<std::size_t>(task.priority);
  queues_[idx].push_back(std::move(task));
  cv_.notify_one();
  return true;
}

Task TaskQueue::dequeue() {
  std::unique_lock lock(mutex_);
  cv_.wait(lock, [this] { return !paused_ && canRunUnlocked(); });
  auto task_opt = popNextTaskUnlocked();
  // canRunUnlocked guarantees task
  Task task = std::move(*task_opt);
  running_[task.type]++;
  inflight_[task.id] = task.type;
  return task;

}

void TaskQueue::taskCompleted(std::uint64_t id) {
  std::lock_guard lock(mutex_);
  auto it = inflight_.find(id);
  if (it != inflight_.end()) {
    auto type = it->second;
    auto run_it = running_.find(type);
    if (run_it != running_.end() && run_it->second > 0) {
      run_it->second--;
    }
    inflight_.erase(it);
  }
  cv_.notify_all();
}

void TaskQueue::setPaused(bool paused) {
  {
    std::lock_guard lock(mutex_);
    paused_ = paused;
  }
  cv_.notify_all();
}

TaskQueue::Stats TaskQueue::getStats() const {
  std::lock_guard lock(mutex_);
  Stats stats;
  for (std::size_t i = 0; i < static_cast<std::size_t>(TaskPriority::kCount); ++i) {
    stats.queued[i] = queues_[i].size();
  }
  stats.running = running_;
  return stats;
}

bool TaskQueue::canRunUnlocked() const {
  if (paused_) {
    return false;
  }
  if (!guard_ || !guard_->canAcceptWork()) {
    return false;
  }
  for (const auto &q : queues_) {
    for (const auto &task : q) {
      auto max_it = config_.max_concurrent.find(task.type);
      std::size_t limit =
          max_it == config_.max_concurrent.end() ? 0 : max_it->second;
      if (running_.at(task.type) < limit) {
        return true;
      }
    }
  }
  return false;
}

std::optional<Task> TaskQueue::popNextTaskUnlocked() {
  if (auto t = findReadyTask(queues_[static_cast<std::size_t>(TaskPriority::kHigh)])) {
    return t;
  }

  for (std::size_t i = 0; i < 2; ++i) {
    auto idx = rr_index_;
    rr_index_ = idx == static_cast<std::size_t>(TaskPriority::kNormal)
                    ? static_cast<std::size_t>(TaskPriority::kLow)
                    : static_cast<std::size_t>(TaskPriority::kNormal);
    if (auto t = findReadyTask(queues_[idx])) {
      return t;
    }
  }
  return std::nullopt;
}

std::optional<Task> TaskQueue::findReadyTask(std::deque<Task> &q) const {
  for (auto it = q.begin(); it != q.end(); ++it) {
    auto max_it = config_.max_concurrent.find(it->type);
    std::size_t limit =
        max_it == config_.max_concurrent.end() ? 0 : max_it->second;
    if (running_.at(it->type) < limit) {
      Task t = std::move(*it);
      q.erase(it);
      return t;
    }
  }
  return std::nullopt;
}

std::size_t TaskQueue::totalQueuedUnlocked() const {
  std::size_t total = 0;
  for (const auto &q : queues_) {
    total += q.size();
  }
  return total;
}

}  // namespace vibenote

