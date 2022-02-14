#ifndef DLTMESSAGEFINDER_H
#define DLTMESSAGEFINDER_H

#include <QDebug>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QQueue>
#include <QThread>
#include <QThreadPool>
#include <QSemaphore>
#include <QMutex>
#include <QMutexLocker>
#include <QReadWriteLock>

#include <utility>
#include <memory>

#include "dlt_common.h"
#include "qdlt.h"

template <typename T>
class QSafeList
{
    QList<T> list;
    QReadWriteLock lock;
public:
    QSafeList(){}
    QSafeList(std::initializer_list<T> a){
        list = a;
    }
    QSafeList(const QSafeList<T>&r) = delete;
    QSafeList(const QSafeList<T>&&r){list = std::move(r.list);}
    void append(T& el)
    {
        lock.lockForWrite();
        list.append(el);
        lock.unlock();
    }
    T at(int idx)
    {
        T el;
        lock.lockForRead();
        el = list.at(idx);
        lock.unlock();
        return el;
    }
    int size()
    {
        int size = 0;
        lock.lockForRead();
        size = list.size();
        lock.unlock();
        return size;
    }
    void clear()
    {
        lock.lockForWrite();
        list.clear();
        lock.unlock();
    }
};

typedef std::pair<std::shared_ptr<QDltFile>, QSafeList<int>> qpair_t;

class DltMessageFinder : public QObject
{
    Q_OBJECT

    template<typename T>
    class SafeQQueue
    {
        QQueue<T> queue;
        QMutex    mutex;
    public:
        SafeQQueue(){}
        SafeQQueue(const SafeQQueue&) = delete;
        void enqueue(const T &el)
        {
            QMutexLocker l(&mutex);
            queue.enqueue(el);
        }
        T dequeue()
        {
            QMutexLocker l(&mutex);
            return queue.dequeue();
        }
        int size()
        {
            QMutexLocker l(&mutex);
            return queue.size();
        }
        bool empty()
        {
            QMutexLocker l(&mutex);
            return queue.empty();
        }
        void clear()
        {
            QMutexLocker l(&mutex);
            queue.clear();
        }
    };

    static DltMessageFinder* obj;
    DltMessageFinder(QObject *parent = nullptr);
    void operator=(const DltMessageFinder &) = delete;

    QString m_expression;

    SafeQQueue<QString> q_paths;
    SafeQQueue<std::pair<int, std::shared_ptr<qpair_t>>> results;
    QReadWriteLock running_lock;
    QSemaphore *sem_worker_demand;
    bool is_running = false;

    QSafeList<std::shared_ptr<qpair_t>> m_finalResults;

    static void depth_search_worker();
    static void shallow_search_worker();

public:
    ~DltMessageFinder();

    static DltMessageFinder* getInstance(QObject *parent = nullptr);

    void search(QStringList &paths, const QString &&expression);
    void cancelSearch(bool full_stop = true);
    bool isRunning()
    {
        bool ret_val;

        running_lock.lockForRead();
        ret_val = is_running;
        running_lock.unlock();

        return ret_val;
    }

    QSafeList<std::shared_ptr<qpair_t>>& getResults()
    {
        return m_finalResults;
    }
signals:
    void startedSearch();
    void stoppedSearch(bool full_stop);
    void searchFinished();
    void foundFile(int f_index);
    void resultPartial(int f_index, int index);
};


#endif // DLTMESSAGEFINDER_H
