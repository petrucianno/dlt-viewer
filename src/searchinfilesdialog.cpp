#include <QDebug>
#include <QDirIterator>
#include <QCheckBox>
#include <QMenu>
#include <QAction>

#include <QDateTime>

#include "ui_searchinfilesdialog.h"
#include "searchinfilesdialog.h"

#include "dlt_common.h"

SearchInFilesDialog::SearchInFilesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SearchInFilesDialog),
    multiFileSearcher(DltMessageFinder::getInstance(this))
{
    ui->setupUi(this);

    ui->treeWidgetResults->setColumnWidth(0, ui->treeWidgetResults->width() - 10);
    ui->treeWidgetResults->setColumnWidth(1, 10);

    ui->tableWidgetResults->setColumnWidth(4, 500);

    /* enable custom context menu */
    ui->treeWidgetResults->setContextMenuPolicy(Qt::CustomContextMenu);

    /* Make connections */
    /**/
    connect(multiFileSearcher, &DltMessageFinder::startedSearch, this, [this](){
        // set search button disabled
        ui->buttonSearch->setEnabled(false);
        ui->buttonCancel->setText("Stop");
        /* disable textbox */
        ui->directoryTextBox->setEnabled(false);
        ui->tabWidget->setCurrentIndex(1);
    });
    connect(multiFileSearcher, &DltMessageFinder::stoppedSearch, this, [this](bool full_stop){
        // set search button enabled
        ui->buttonSearch->setEnabled(true);
        ui->buttonCancel->setText("Close");
        /* enable textbox */
        ui->directoryTextBox->setEnabled(true);

        if (full_stop)
        {
            ui->treeWidgetResults->clear();
            ui->tableWidgetResults->setRowCount(0);
        }
    });
    connect(multiFileSearcher, &DltMessageFinder::searchFinished, this, [this](){
        // set search button enabled
        ui->buttonSearch->setEnabled(true);
        ui->buttonCancel->setText("Close");
        /* enable textbox */
        ui->directoryTextBox->setEnabled(true);
    });
    connect(multiFileSearcher, &DltMessageFinder::foundFile, this, [this](int index){
        /* get path from results */
        /*
        qDebug() << QString("[%1] + parent: %2")
                        .arg(QDateTime::currentMSecsSinceEpoch())
                        .arg(QString::number(index));
        */
        QTreeWidgetItem* item  = new QTreeWidgetItem;
        QTreeWidgetItem* child = new QTreeWidgetItem;
        ui->treeWidgetResults->insertTopLevelItem(
                    ui->treeWidgetResults->topLevelItemCount(), item);

        auto result = multiFileSearcher->getResults().at(index);
        QString path = result->first->getFileName();


        item->setText(0, path);
        item->setToolTip(0, path);
        item->setText(1, "1");
        
        auto message = QString("index: " ) + QString::number(result->second.at(0));
        child->setText(0, message);
        child->setToolTip(0, "Click to display results at " + message);
        item->addChild(child);

    });
    connect(multiFileSearcher, &DltMessageFinder::resultPartial, this, [this](int f_index, int index){
#if 0
        qDebug() << QString("[%1] + parent: %2 child: %3")
                        .arg(QDateTime::currentMSecsSinceEpoch())
                        .arg(QString::number(f_index),
                             QString::number(index));
#endif
        auto items = ui->treeWidgetResults->topLevelItemCount();

        if (items > 0)
        {
            auto result     = multiFileSearcher->getResults().at(f_index);
            auto tree_item  = ui->treeWidgetResults->topLevelItem(f_index);
            auto sub_item   = new QTreeWidgetItem;

            tree_item->addChild(sub_item);
            tree_item->setText(1, QString::number(result->second.size()));
            auto message = "index: " + QString::number(result->second.at(index));
            sub_item->setText(0, message);
            sub_item->setToolTip(0, "Click to display results at " + message);
        }
    });


}

SearchInFilesDialog::~SearchInFilesDialog()
{
    delete ui;
}

void SearchInFilesDialog::setFolder(const QString &path)
{
    m_currentPath = path;
    ui->directoryTextBox->setText(m_currentPath);
}

void SearchInFilesDialog::dltFilesOpened(const QStringList &files)
{

}


void SearchInFilesDialog::on_buttonSearch_clicked()
{
    if (!QDir(m_currentPath).exists())
    {
        /* Dialog, folder not exists */
        return ;
    }

    /* if there are some, clear previous results */
    multiFileSearcher->cancelSearch(true);
    ui->treeWidgetResults->clear();

    if (!multiFileSearcher->isRunning())
    {
        QDltFilterList filters;
        QTableWidget  *table = ui->tableWidgetPatterns;
        QDirIterator   it_sh(m_currentPath, QStringList() << "*.dlt", QDir::Files, QDirIterator::Subdirectories);
        QStringList    files;
        QStringList    expressions;


        /* Compose search query based on Patterns Table */
        for (int i = 0; i < table->rowCount(); i++)
        {
            /* Check if query is enabled */
            bool is_enabled = dynamic_cast<QCheckBox*>(table->cellWidget(i, 0))->isChecked();

            if (is_enabled)
            {
                ;
                auto text = table->item(i, 1)->text();
                if (!text.isEmpty())
                {
                    /* test if regex expression is valid */
                    expressions << ("("+text+")");
                }
                else
                    qDebug() << "empty";
            }

        }

        if (expressions.empty())
        {
            /* Dialog that query is empty */
            return;
        }

        while (it_sh.hasNext())
        {
            files.append(it_sh.next());
        }
        std::sort(files.begin(), files.end(), [](QString&l, QString&r){
            int l_f = QDir::toNativeSeparators(l).count(QDir::separator());;
            int r_f = QDir::toNativeSeparators(r).count(QDir::separator());;
            return l_f < r_f;
        });

        if (files.size() > 0)
            multiFileSearcher->search(files, std::move(expressions.join("|")));
        else
        {
            /* dialog: no .dlt files found in folder */
        }
    }
}


void SearchInFilesDialog::on_buttonCancel_clicked()
{
    if (multiFileSearcher->isRunning())
    {/* This will also re-enable the search button */
        multiFileSearcher->cancelSearch(false);/* Stop the search, but keep results */
    }
    else
    {
        multiFileSearcher->cancelSearch(true); /* Clear cache memeory, also */
        close();
    }
}


void SearchInFilesDialog::on_buttonAddPattern_clicked()
{
    auto rowIdx = ui->tableWidgetPatterns->rowCount();
    auto checkBox = new QCheckBox(ui->tableWidgetPatterns);
    auto table = ui->tableWidgetPatterns;

    checkBox->setCheckState(Qt::Checked);
    checkBox->setStyleSheet("margin-left:15%;");
    table->insertRow(rowIdx);
    table->setCellWidget(rowIdx, 0, checkBox);

    auto item = new QTableWidgetItem("");
    item->setText("");
//    item->
    table->setItem(rowIdx, 1, item);

}


void SearchInFilesDialog::on_buttonRemovePattern_clicked()
{
    auto table = ui->tableWidgetPatterns;
    auto sel_m = table->selectionModel();

    foreach(auto index, sel_m->selectedRows())
        table->removeRow(index.row());
}

void SearchInFilesDialog::on_directoryTextBox_textChanged(const QString &arg1)
{
    m_currentPath = arg1;
}


void SearchInFilesDialog::on_directoryTextBox_returnPressed()
{
}

void SearchInFilesDialog::showEvent(QShowEvent *event)
{
    (void)event;
    ui->tabWidget->setCurrentIndex(0);
}


void SearchInFilesDialog::on_treeWidgetResults_itemClicked(QTreeWidgetItem *item, int column)
{
    (void)column;
    int topLvlIndex = ui->treeWidgetResults->indexOfTopLevelItem(item);

    if (-1 == topLvlIndex)
    {/* it means, a child was clicked */
        auto parent = item->parent();
        auto childIndex = 0;

        topLvlIndex = ui->treeWidgetResults->indexOfTopLevelItem(parent);
        childIndex  = parent->indexOfChild(item);

        auto resultsTable = ui->tableWidgetResults;
        auto results      = multiFileSearcher->getResults().at(topLvlIndex);
        /* Get .dlt file content for current index */
        auto dltFile      = results->first;

        {/*  */
            QTableWidgetItem* ourItem = nullptr;
            long ourItemIndex = 0;
            long result_idx   = results->second.at(childIndex);
            long msgNumber    = dltFile->getFileMsgNumber();
            long start        = result_idx - 100;
            long end          = result_idx + 100;

            start = (start >= 0)?start:0;
            end   = (end <= msgNumber)?end:msgNumber;

            /* Delete results table items */
            if (resultsTable->rowCount() > 0)
                resultsTable->setRowCount(0);

            for (long idx = 0, msg_idx = start; msg_idx < end; idx++, msg_idx++)
            {
                auto item = new QTableWidgetItem;
                QDltMsg message;

                dltFile->getMsg(msg_idx, message);
                resultsTable->insertRow(idx);

                /* payload */
                item->setText(message.toStringPayload());
                resultsTable->setItem(idx, 4, item);

                /* index */
                item = new QTableWidgetItem;
                item->setText(QString::number(msg_idx));
                resultsTable->setItem(idx, 0, item);

                if (result_idx == msg_idx)
                {
                    ourItem      = item;
                    ourItemIndex = idx;
                }

                /* ecuid */
                item = new QTableWidgetItem;
                item->setText(message.getEcuid());
                resultsTable->setItem(idx, 1, item);

                /* apid */
                item = new QTableWidgetItem;
                item->setText(message.getApid());
                resultsTable->setItem(idx, 2, item);

                /* ctid */
                item = new QTableWidgetItem;
                item->setText(message.getCtid());
                resultsTable->setItem(idx, 3, item);

                ;
                resultsTable->setRowHeight(resultsTable->row(item), 12);
            }

            /* Scroll to message idx: at(childIndex) */
            resultsTable->setFocus();
            resultsTable->scrollToItem(ourItem, QAbstractItemView::PositionAtCenter);
            resultsTable->setRangeSelected({ourItemIndex, 0, ourItemIndex, 4}, true);
        }
    }
}


void SearchInFilesDialog::on_treeWidgetResults_customContextMenuRequested(const QPoint &pos)
{
    qDebug() << __FUNCTION__;

    if (multiFileSearcher->getResults().size() <= 0)
        return;

    auto resultsTree  = ui->treeWidgetResults;
    auto item         = resultsTree->itemAt(pos);
    int  topLvlIndex  = resultsTree->indexOfTopLevelItem(item);
    int  childIndex   = 0;

    if (-1 == topLvlIndex)
    {/* item is a treeChild */
        auto parent = item->parent();

        childIndex  = parent->indexOfChild(item);
        topLvlIndex = resultsTree->indexOfTopLevelItem(parent);
    }

    auto results = multiFileSearcher->getResults().at(topLvlIndex);
    auto dltFile = results->first->getFileName();

    QMenu menu(ui->treeWidgetResults);
    auto action = new QAction("&Load", this);
    connect(action, &QAction::triggered, this, [this, dltFile](){
        emit openDltFiles(QStringList() << dltFile);
    });
    menu.addAction(action);

    menu.exec(resultsTree->mapToGlobal(pos));
}






