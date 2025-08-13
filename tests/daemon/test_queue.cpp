#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "queue.h"
#include "gpu_guard.h"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

class MockGpuGuard : public GpuGuard {
public:
    MOCK_METHOD(bool, canAcceptWork, (), (override));
};

namespace {

Task makeTask(TaskClass cls, int id) {
    Task t;
    t.id = id;
    t.cls = cls;
    t.work = [] {};
    return t;
}

} // namespace

TEST(QueueTest, EnqueueDequeue) {
    QueueConfig cfg;
    cfg.capacity = 10;
    cfg.classLimits[TaskClass::Interactive] = 2;
    cfg.classLimits[TaskClass::Export] = 1;
    cfg.classLimits[TaskClass::Watch] = 1;

    NiceMock<MockGpuGuard> guard;
    EXPECT_CALL(guard, canAcceptWork()).WillRepeatedly(Return(true));

    TaskQueue queue(cfg, guard);

    queue.enqueue(makeTask(TaskClass::Interactive, 1));
    queue.enqueue(makeTask(TaskClass::Interactive, 2));
    queue.enqueue(makeTask(TaskClass::Export, 3));
    queue.enqueue(makeTask(TaskClass::Watch, 4));
    queue.enqueue(makeTask(TaskClass::Watch, 5));

    auto t1 = queue.dequeue();
    auto t2 = queue.dequeue();
    auto t3 = queue.dequeue();
    auto t4 = queue.dequeue();
    auto t5 = queue.dequeue();

    EXPECT_EQ(t1.id, 1);
    EXPECT_EQ(t2.id, 2);
    EXPECT_EQ(t3.id, 3);
    EXPECT_EQ(t4.id, 4);
    EXPECT_EQ(t5.id, 5);
}

TEST(QueueTest, CapacityLimit) {
    QueueConfig cfg;
    cfg.capacity = 2;
    cfg.classLimits[TaskClass::Interactive] = 1;

    NiceMock<MockGpuGuard> guard;
    EXPECT_CALL(guard, canAcceptWork()).WillRepeatedly(Return(true));

    TaskQueue queue(cfg, guard);

    EXPECT_TRUE(queue.enqueue(makeTask(TaskClass::Interactive, 1)));
    EXPECT_TRUE(queue.enqueue(makeTask(TaskClass::Interactive, 2)));
    EXPECT_FALSE(queue.enqueue(makeTask(TaskClass::Interactive, 3)));

    auto t = queue.dequeue();
    EXPECT_EQ(t.id, 1);
    EXPECT_TRUE(queue.enqueue(makeTask(TaskClass::Interactive, 3)));
}

TEST(QueueTest, ConcurrencyLimits) {
    QueueConfig cfg;
    cfg.capacity = 10;
    cfg.classLimits[TaskClass::Interactive] = 1;

    NiceMock<MockGpuGuard> guard;
    EXPECT_CALL(guard, canAcceptWork()).WillRepeatedly(Return(true));

    TaskQueue queue(cfg, guard);

    queue.enqueue(makeTask(TaskClass::Interactive, 1));
    queue.enqueue(makeTask(TaskClass::Interactive, 2));

    auto first = std::async(std::launch::async, [&] { return queue.dequeue(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_EQ(first.wait_for(std::chrono::milliseconds(0)), std::future_status::ready);

    auto second = std::async(std::launch::async, [&] { return queue.dequeue(std::chrono::milliseconds(50)); });
    EXPECT_EQ(second.wait_for(std::chrono::milliseconds(30)), std::future_status::timeout);

    queue.finish(first.get());
    EXPECT_EQ(second.get().id, 2);
}

TEST(QueueTest, GpuThrottling) {
    QueueConfig cfg;
    cfg.capacity = 10;
    cfg.classLimits[TaskClass::Interactive] = 1;

    MockGpuGuard guard;
    TaskQueue queue(cfg, guard);

    queue.enqueue(makeTask(TaskClass::Interactive, 1));

    EXPECT_CALL(guard, canAcceptWork())
        .WillOnce(Return(false))
        .WillOnce(Return(true));

    auto fut = std::async(std::launch::async, [&] { return queue.dequeue(); });
    EXPECT_EQ(fut.wait_for(std::chrono::milliseconds(30)), std::future_status::timeout);

    queue.notifyGpuAvailable();
    EXPECT_EQ(fut.get().id, 1);
}

TEST(QueueTest, FairScheduling) {
    QueueConfig cfg;
    cfg.capacity = 10;
    cfg.classLimits[TaskClass::Interactive] = 2;
    cfg.classLimits[TaskClass::Export] = 1;
    cfg.classLimits[TaskClass::Watch] = 1;

    NiceMock<MockGpuGuard> guard;
    EXPECT_CALL(guard, canAcceptWork()).WillRepeatedly(Return(true));

    TaskQueue queue(cfg, guard);

    for (int i = 0; i < 3; ++i) {
        queue.enqueue(makeTask(TaskClass::Interactive, 100 + i));
        queue.enqueue(makeTask(TaskClass::Export, 200 + i));
        queue.enqueue(makeTask(TaskClass::Watch, 300 + i));
    }

    std::vector<int> order;
    for (int i = 0; i < 9; ++i) {
        order.push_back(queue.dequeue().id);
    }

    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(order[i * 3 + 0], 100 + i);
        EXPECT_EQ(order[i * 3 + 1], 200 + i);
        EXPECT_EQ(order[i * 3 + 2], 300 + i);
    }
}

TEST(QueueTest, ThreadSafety) {
    QueueConfig cfg;
    cfg.capacity = 100;
    cfg.classLimits[TaskClass::Interactive] = 10;

    NiceMock<MockGpuGuard> guard;
    EXPECT_CALL(guard, canAcceptWork()).WillRepeatedly(Return(true));

    TaskQueue queue(cfg, guard);

    std::atomic<int> counter{0};
    auto producer = [&] {
        for (int i = 0; i < 100; ++i) {
            queue.enqueue(makeTask(TaskClass::Interactive, i));
        }
    };

    auto consumer = [&] {
        for (int i = 0; i < 100; ++i) {
            auto t = queue.dequeue();
            counter.fetch_add(t.id >= 0 ? 1 : 0);
            queue.finish(t);
        }
    };

    std::thread p1(producer), p2(producer);
    std::thread c1(consumer), c2(consumer);

    p1.join();
    p2.join();
    c1.join();
    c2.join();

    EXPECT_EQ(counter.load(), 200);
    EXPECT_EQ(queue.size(), 0U);
}

TEST(QueueTest, TimeoutAndCancellation) {
    QueueConfig cfg;
    cfg.capacity = 2;
    cfg.classLimits[TaskClass::Interactive] = 1;

    NiceMock<MockGpuGuard> guard;
    EXPECT_CALL(guard, canAcceptWork()).WillRepeatedly(Return(true));

    TaskQueue queue(cfg, guard);

    auto empty = queue.dequeue(std::chrono::milliseconds(50));
    EXPECT_FALSE(empty.has_value());

    queue.enqueue(makeTask(TaskClass::Interactive, 1));
    auto token = queue.enqueue(makeTask(TaskClass::Interactive, 2));
    queue.cancel(token);

    auto t = queue.dequeue();
    EXPECT_EQ(t.id, 1);
    EXPECT_FALSE(queue.dequeue(std::chrono::milliseconds(10)).has_value());
}

TEST(QueueTest, SignalEmission) {
    QueueConfig cfg;
    cfg.capacity = 2;
    cfg.classLimits[TaskClass::Interactive] = 1;

    NiceMock<MockGpuGuard> guard;
    EXPECT_CALL(guard, canAcceptWork()).WillRepeatedly(Return(true));

    TaskQueue queue(cfg, guard);

    testing::MockFunction<void(const Task&)> enqueued;
    testing::MockFunction<void(const Task&)> dequeued;
    queue.onEnqueued(enqueued.AsStdFunction());
    queue.onDequeued(dequeued.AsStdFunction());

    EXPECT_CALL(enqueued, Call(_)).Times(1);
    EXPECT_CALL(dequeued, Call(_)).Times(1);

    auto token = queue.enqueue(makeTask(TaskClass::Interactive, 1));
    auto t = queue.dequeue();
    queue.finish(t);
}

TEST(QueueTest, EdgeCases) {
    QueueConfig cfg;
    cfg.capacity = 1;
    cfg.classLimits[TaskClass::Interactive] = 1;

    NiceMock<MockGpuGuard> guard;
    EXPECT_CALL(guard, canAcceptWork()).WillRepeatedly(Return(true));

    TaskQueue queue(cfg, guard);

    EXPECT_FALSE(queue.dequeue(std::chrono::milliseconds(10)).has_value());

    queue.enqueue(makeTask(TaskClass::Interactive, 42));
    auto t = queue.dequeue();
    EXPECT_EQ(t.id, 42);
    EXPECT_FALSE(queue.dequeue(std::chrono::milliseconds(10)).has_value());
}

