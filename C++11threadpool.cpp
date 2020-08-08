#pragma once

/* 纸上得来终觉浅，绝知此事要躬行。 */

#include <mutex>
#include <functional>
#include <atomic>
#include <queue>
#include <vector>
#include <future>

using namespace std;

class threadpool
{
    const bool _isAutoIncrement;
    const int _maxThreadNum;

    using Task = function<void()>; //typedef
    atomic<bool> _isRun{true};
    atomic<int> _idleThread{0};
    queue<Task> _taskQueue;
    vector<thread> _threadVec;
    condition_variable _cond;
    mutex _lock;

    threadpool(int maxThreadNum = 16, bool autoIncrement = false) : _maxThreadNum(maxThreadNum), _isAutoIncrement(autoIncrement) { addThread(4); }
    ~threadpool()
    {
        this->_isRun = false;
        this->_cond.notify_all();
        for (thread &thr : this->_threadVec)
            if (thr.joinable())
                thr.join();
    }

public:
    template <class F, class... Args>
    auto commit(F &&f, Args &&... args) -> future<decltype(f(args...))>
    {
        if (!_isRun)
            return;
        using retType = decltype(f(args...));
        auto task = make_shared<packaged_task<retType()>>(bind(forward<F>(f), forward<Args>(args)...));
        future<retType> futureRet = task->get_future();
        {
            unique_lock<mutex> lock(this->_lock);
            this->_taskQueue.emplace([task]() { (*task)(); });
        }
        if (this->_idleThread < 1 && this->_threadVec.size() < this->_maxThreadNum)
            this->addThread(1);
        this->_cond.notify_one();
        return futureRet;
    }

private:
    void addThread(int threadNum)
    {
        for (int i = 0; i < threadNum && this->_threadVec.size() <= this->_maxThreadNum; i++)
        {
            this->_threadVec.emplace_back([this]() {
                while (this->_isRun)
                {
                    Task task;
                    {
                        unique_lock<mutex> lock(this->_lock);
                        _cond.wait(lock, [this]() { return !this->_isRun || !this->_taskQueue.empty(); });

                        if (!this->_isRun && this->_taskQueue.empty())
                            return;

                        task = move(this->_taskQueue.front());
                        this->_taskQueue.pop();
                    }
                    this->_idleThread--;
                    task();
                    this->_idleThread++;
                }
            });
            this->_idleThread++;
        }
    }
};
