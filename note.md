# C++11摸索笔记——std::promise/std::future

`std::promise`与`std::future`是C++11新增的类，这一对类在代码里基本上是会一块儿出现的，是解决多线程时主线程与子线程的通信问题的。

当然有小朋友会说：啊这，可以用指针来传递数据呀，为啥非得要用这玩意儿？？？

像是`pthread_create()`函数就是用第四个参数的指针来传参的。

```c++
/* 用pthread_create举例 */
int pthread_create(pthread_t *__restrict__ __newthread, const pthread_attr_t *__restrict__ __attr, void *(*__start_routine)(void *), void *__restrict__ __arg)
```

仔细想想也不是不行，但是这样子会有一些问题：

1. 你在父进程传入一个指针到子进程，同时进入条件等待状态，子进程对指针设置数据以后，通知条件变量唤醒主进程读取数据，这样子一通信过程就完成了。这个过程里你将会用到一个条件变量，一个mutex锁，一个指针。要是子进程需要在多个不同的时间分别返回数据，这个过程就会变得很麻烦。
2. 