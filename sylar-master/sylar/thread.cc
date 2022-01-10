#include "thread.h"
#include "log.h"
#include "util.h"

namespace sylar {

static thread_local Thread* t_thread = nullptr;                     //线程局部变量，指向当前线程
static thread_local std::string t_thread_name = "UNKNOW";

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");      //system系统日志

Thread* Thread::GetThis() {
    return t_thread;
}

const std::string& Thread::GetName() {
    return t_thread_name;
}

void Thread::SetName(const std::string& name) {                     //该函数的作用是什么？
    if(name.empty()) {
        return;
    }
    if(t_thread) {                                                  //主线程并不是我们创建的，所以需要分类
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

Thread::Thread(std::function<void()> cb, const std::string& name)
    :m_cb(cb)
    ,m_name(name) {
    if(name.empty()) {
        m_name = "UNKNOW";
    }
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);      //创建一个新线程。因为是静态方法，所以直接把自己传进去
    if(rt) {
        SYLAR_LOG_ERROR(g_logger) << "pthread_create thread fail, rt=" << rt
            << " name=" << name;
        throw std::logic_error("pthread_create error");                 //创建失败，抛出错误
    }
    m_semaphore.wait();                                         //直到线程开始执行时，再出构造函数，sem_wait阻塞
}

Thread::~Thread() {
    if(m_thread) {
        pthread_detach(m_thread);
    }
}

void Thread::join() {
    if(m_thread) {
        int rt = pthread_join(m_thread, nullptr);
        if(rt) {                                                                                    //如果有问题，把问题打出来
            SYLAR_LOG_ERROR(g_logger) << "pthread_join thread fail, rt=" << rt
                << " name=" << m_name;
            throw std::logic_error("pthread_join error");
        }
        m_thread = 0;           //清空
    }
}

void* Thread::run(void* arg) {                                                                     //（子线程）主函数
    Thread* thread = (Thread*)arg;
    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_id = sylar::GetThreadId();
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());           //库函数，用来给线程命名

    std::function<void()> cb;
    cb.swap(thread->m_cb);

    thread->m_semaphore.notify();               //

    cb();
    return 0;
}

}
