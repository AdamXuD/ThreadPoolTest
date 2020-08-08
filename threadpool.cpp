
/* This's C code. */

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define LL_ADD(item, list)     \
    do                         \
    {                          \
        item->prev = NULL;     \
        item->next = list;     \
        if (list != NULL)      \
            list->prev = item; \
        list = item;           \
    } while (0)
//双向链表头插法的C实现 （C++可用模板）

#define LL_REMOVE(item, list)              \
    do                                     \
    {                                      \
        if (item->prev != NULL)            \
            item->prev->next = item->next; \
        if (item->next != NULL)            \
            item->next->prev = item->prev; \
        if (list == item)                  \
            list = item->next;             \
        item->prev = item->next = NULL;    \
    } while (0)

struct NWORKER
{
    pthread_t thread;
    struct NMANAGER *pool; //每一个线程都包含指向一个池子 方便操作

    int terminate; //停止工作flag

    NWORKER *next;
    NWORKER *prev;
};

struct NJOB
{
    void (*func)(void *arg);
    void *user_data;

    NJOB *prev;
    NJOB *next;
};

struct NMANAGER
{
    NWORKER *workers;
    NJOB *jobs;

    pthread_cond_t jobs_cond;   //条件（没满足某条件 即挂起 等待任务）
    pthread_mutex_t jobs_mutex; //公共资源需要加mutex锁
};

typedef struct NMANAGER nThreadPool;

static void *nThreadCallback(void *arg)
{
    NWORKER *worker = (NWORKER *)arg;

    // worker有两个状态 第一个是在执行 第二个是在等待
    // 任务队列为空 即在等待

    while (1)
    {
        pthread_mutex_lock(&worker->pool->jobs_mutex); //条件等待前需要加锁

        while (worker->pool->jobs == NULL)
        {
            if (worker->terminate)
                break; //要是想使该线程退出 则把terminate置为1即可 同下
            pthread_cond_wait(&worker->pool->jobs_cond, &worker->pool->jobs_mutex);
            /*
             * 条件等待函数开头有个加锁操作
             * 条件等待结束时有个解锁操作
             * 条件等待这个过程是在锁外面的
             * （剩下待会儿查一下）
             */
        }

        if (worker->terminate)
        {
            pthread_mutex_unlock(&worker->pool->jobs_mutex); //条件等待完毕后需要解锁
            break;                                           //要是想使该线程退出 则把terminate置为1即可 同上
        }

        NJOB *job = worker->pool->jobs;
        if (job != NULL) //该判断可不加
        {
            LL_REMOVE(job, worker->pool->jobs);
        }
        //跳出循环后取出一个job

        pthread_mutex_unlock(&worker->pool->jobs_mutex); //条件等待完毕后需要解锁

        if (job == NULL)
            continue; //该判断也可不加

        job->func((NJOB *)job->user_data);
    }

    free(worker); //释放相应的资源 线程退出
    pthread_exit(NULL);
    // 区别于pthread_cancel()，执行pthread_exit()后直接退出相应的进程
    // pthread_cancel()，则是在外部退出pthread的相应进程
    // pthread_detach()，父子进程无关联
}

// Thread Pool Create, API
int nThreadPoolCreate(nThreadPool *pool, int numWorkers)
{
    // 写函数先对参数进行校验
    // 保证后续代码运行不会出问题

    if (numWorkers < 1)
        numWorkers = 1;
    if (pool == NULL)
        return -1;
    memset(pool, 0, sizeof(nThreadPool));

    pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
    memcpy(&pool->jobs_cond, &blank_cond, sizeof(pthread_cond_t));

    pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
    memcpy(&pool->jobs_mutex, &blank_mutex, sizeof(pthread_mutex_t));

    for (int i = 0; i < numWorkers; i++)
    {
        NWORKER *worker = (NWORKER *)malloc(sizeof(NWORKER));
        if (worker == NULL)
        {
            perror("malloc");
            return -2;
        }
        //开辟一块新的内存

        memset(worker, 0, sizeof(NWORKER));
        worker->pool = pool;
        //置零脏内存 （好习惯）

        int ret = pthread_create(&worker->thread, NULL, nThreadCallback, worker); //nThreadCallback 有几种情况 两个状态
        if (ret)
        {
            perror("pthread_create");
            free(worker);
            return -3;
        }
        //创建线程

        LL_ADD(worker, pool->workers);
        //加入到线程队列
    }
    //NMANAGER 每一项都应该初始化
}

// 向线程池中添加任务
void nThreadPoolPush(nThreadPool *pool, NJOB *job)
{
    /* 
     * 添加任务的流程：
     * 首先是将任务加入到队列中
     * 其次就是唤醒线程池中的线程（在无任务时线程是处于休眠（挂起）状态的）
     */
    pthread_mutex_lock(&pool->jobs_mutex);
    //加任务前要先加锁

    LL_ADD(job, pool->jobs);
    pthread_cond_signal(&pool->jobs_cond);

    //加完任务后要解锁
    pthread_mutex_unlock(&pool->jobs_mutex);
}

//销毁线程池
int nThreadPoolDestroy(nThreadPool *pool)
{
    for (NWORKER *worker = pool->workers; worker != NULL; worker = worker->next)
    {
        worker->terminate = 1;
    }

    //唤醒所有线程
    pthread_mutex_lock(&pool->jobs_mutex);
    pthread_cond_broadcast(&pool->jobs_cond);
    pthread_mutex_unlock(&pool->jobs_mutex);
}

#if 1 //debug

// 0 -- 1000,
// task -->
static void print(void *arg)
{
    int i = *(int *)arg;
    printf("%d\n", i);
    free(arg);
}

int main()
{
    nThreadPool *pool = (nThreadPool *)malloc(sizeof(nThreadPool));
    nThreadPoolCreate(pool, 4);
    for (int i = 1; i <= 100; i++)
    {
        NJOB *job = (NJOB *)malloc(sizeof(NJOB));
        int *num = (int *)malloc(sizeof(int));
        *num = i;
        job->func = print;
        job->user_data = num;
        nThreadPoolPush(pool, job);
        usleep(100);
    }
    return 0;
}

#endif

/*
 * 思考：
 * 1. 线程不够时如何增加线程池？
 * 2. 如何减少线程？
 * 3. 对于线程，增加和减少的策略是怎么样的？
 */