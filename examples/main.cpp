#include "../include/taskqueue/TaskQueue.h"


void taskExample(const char *taskName, unsigned int sleep = 10)
{
    std::thread::id threadId = std::this_thread::get_id();
    // printf("{%s} started by thread [%u] \n", taskName, threadId);
    std::this_thread::sleep_for(std::chrono::seconds(sleep));
};


int main()
{

    TaskQueue::WorkerPool pool(3);

    pool.PostTask(taskExample, "task-example", 20);
    pool.PostTask(taskExample, "task-example", 15);

    printf("Total Worker: %d \n", pool.GetTotalWorker());
    std::this_thread::sleep_for(std::chrono::seconds(10));

    printf("Active Worker: %d \n", pool.GetActiveWorker());

    pool.Sync();

    std::this_thread::sleep_for(std::chrono::seconds(10));

    return 0;
}
