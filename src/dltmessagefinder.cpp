#include <QDateTime>

#include "dltmessagefinder.h"

#define MAIN_WORKER_PRIO    2
 
DltMessageFinder* DltMessageFinder::obj = nullptr;

DltMessageFinder::DltMessageFinder(QObject *parent) :
    QObject(parent)
{
    if (1 == QThreadPool::globalInstance()->maxThreadCount())
    {
        QThreadPool::globalInstance()->setMaxThreadCount(2);
    }
    sem_worker_demand = new QSemaphore(QThreadPool::globalInstance()->maxThreadCount());

}

DltMessageFinder::~DltMessageFinder()
{
    cancelSearch();

    if (nullptr == obj)
       delete obj;
    delete sem_worker_demand;
}

DltMessageFinder *DltMessageFinder::getInstance(QObject *parent)
{
    if (nullptr == obj)
        obj = new DltMessageFinder(parent);
    return obj;
}

void DltMessageFinder::search(QStringList &paths, const QString &&expression)
{
    cancelSearch();
    /* Allow worker threads to do work */
    running_lock.lockForWrite();
    is_running = true;
    running_lock.unlock();

    emit startedSearch();

    m_expression = std::move(expression);

    for (auto &path : paths)
    {
        q_paths.enqueue(path);
    }

    auto th_cnt = QThreadPool::globalInstance()->maxThreadCount();

    for (int i = 0; i < th_cnt/2; i++)
    {
        QThreadPool::globalInstance()->start(&DltMessageFinder::shallow_search_worker, MAIN_WORKER_PRIO);
    }
}

void DltMessageFinder::cancelSearch(bool full_stop)
{
    /* Stop all threads from doing work */
    running_lock.lockForWrite();
    is_running = false;
    running_lock.unlock();

    if (full_stop)
    {
        q_paths.clear();
        results.clear();
        m_finalResults.clear();
    }
#if 0
    qDebug() << QString("[%1] - Canceled search. Full Stop: %2...")
                    .arg(QDateTime::currentMSecsSinceEpoch())
                    .arg(full_stop);
#endif
    emit stoppedSearch(full_stop);
}

void DltMessageFinder::shallow_search_worker()
{
    QRegExp rx(obj->m_expression);
    rx.setCaseSensitivity(Qt::CaseInsensitive);

    obj->setWorkerState(WORKER_STARTED);
#if 0
    qDebug() << QString("[%1] - Started search. Files to look in: %2...")
                    .arg(QDateTime::currentMSecsSinceEpoch())
                    .arg(obj->q_paths.size());
#endif
    while((!obj->q_paths.empty()) && obj->isRunning())
    {
        QString  dlt_path =  obj->q_paths.dequeue();
#if 0
        qDebug() << QString("[%1] Shallow search in: " + dlt_path).arg(QDateTime::currentMSecsSinceEpoch());
#endif
        auto file = std::make_shared<QDltFile>();
        int message_cnt = 0;

        file->open(dlt_path);
        file->createIndex();

        message_cnt = file->getFileMsgNumber();

        for (int i = 0; (i < message_cnt) && obj->isRunning(); i++)
        {
            QDltMsg message;
            file->getMsg(i, message);

            bool found = (rx.indexIn(message.toStringPayload()) >= 0);

            if (found)
            {
                int  index = obj->m_finalResults.size();

                auto result = std::make_shared<qpair_t>(
                            std::make_pair(std::move(file), QSafeList<int>({i}))
                            );

                obj->results.enqueue(std::make_pair(index, result));
                obj->m_finalResults.append(result);

                emit obj->foundFile(index);
#if 0
                 qDebug() << QString("[%1] - parent: %2")
                                 .arg(QDateTime::currentMSecsSinceEpoch())
                                 .arg(QString::number(index));
#endif
                QThreadPool::globalInstance()->start(depth_search_worker, MAIN_WORKER_PRIO * 2);
                break;
            }
        }
    }

    obj->setWorkerState(WORKER_FINISHED);
}

void DltMessageFinder::depth_search_worker(){

    obj->setWorkerState(WORKER_STARTED);

    QRegExp rx(obj->m_expression);
    rx.setCaseSensitivity(Qt::CaseInsensitive);

    if (obj->results.empty())
        return;

    auto result_obj = obj->results.dequeue();
    int  f_index    = result_obj.first;
    auto result     = result_obj.second;
    std::shared_ptr<QDltFile> file   = result->first;

    auto start = result->second.at(0);
    auto name = file->getFileName();

#if 0
    qDebug() << QString("[%1] Worker 2 called, searching in %2, trying to search from index %3")
                        .arg(QString::number(QDateTime::currentMSecsSinceEpoch()), name, QString::number(start));
#endif

    auto message_cnt = file->getFileMsgNumber();

    for (int i = start + 1; (i < message_cnt) && obj->isRunning(); i++)
    {
        QDltMsg message;
        file->getMsg(i, message);

        bool found = (rx.indexIn(message.toStringPayload()) >= 0);

        if (found)
        {
            int index = result->second.size();
            result->second.append(i);

            emit obj->resultPartial(f_index, index);
#if 0
            qDebug() << QString("[%1] - parent: %2 child: %3")
                            .arg(QDateTime::currentMSecsSinceEpoch())
                            .arg(QString::number(f_index),
                                 QString::number(index));
            qDebug() << "Message found: " + message.toStringPayload();
#endif
        }
    }
#if 0
    qDebug() << QString("[%1] Worker 2 done job for %2, total occurences %3")
                        .arg(QString::number(QDateTime::currentMSecsSinceEpoch()),
                             name, QString::number(result->second.size()));
#endif

    obj->setWorkerState(WORKER_FINISHED);
};
