#include "statisticswidget.h"
#include "ui_statisticswidget.h"
#include "db.h"

#include <vector>
#include <algorithm>
#include <string>

#include <QStandardItemModel>
#include <QSettings>

#include "boost/date_time/posix_time/posix_time.hpp"
namespace bpt = boost::posix_time;

StatisticsWidget::StatisticsWidget(QWidget *parent) :
        QWidget(parent),
        ui(new Ui::StatisticsWidget),
        model(new QStandardItemModel)
{
        ui->setupUi(this);

        QSettings s;

        ui->limitSpinBox->        setValue(s.value("ana_many").toInt());
        ui->minCountSpinBox->     setValue(s.value("ana_count").toInt());
        ui->orderComboBox->setCurrentIndex(s.value("ana_which").toInt());
        ui->typeComboBox-> setCurrentIndex(s.value("ana_what").toInt());

        connect(ui->orderComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(writeSettings()));
        connect(ui->typeComboBox,  SIGNAL(currentIndexChanged(int)), this, SLOT(writeSettings()));
        connect(ui->updateButton,  SIGNAL(pressed()),                this, SLOT(writeSettings()));
}

StatisticsWidget::~StatisticsWidget()
{
        delete ui;
        delete model;
}

void StatisticsWidget::writeSettings()
{
        QSettings s;
        s.setValue("ana_which", ui->orderComboBox->currentIndex());
        s.setValue("ana_what",  ui->typeComboBox->currentIndex());
        s.setValue("ana_many",  ui->limitSpinBox->value());
        s.setValue("ana_count", ui->minCountSpinBox->value());
        populateStatistics();
}

void StatisticsWidget::populateStatistics()
{
        QSettings s;

        model->clear();

        QStringList headers;
        headers << "Item"
                << "Speed"
                << "Accuracy"
                << "Viscosity"
                << "Count"
                << "Mistakes"
                << "Impact";
        model->setHorizontalHeaderLabels(headers);
        ui->tableView->setModel(model);
        ui->tableView->setSortingEnabled(false);

        ui->tableView->verticalHeader()->sectionResizeMode(QHeaderView::Fixed);
        ui->tableView->verticalHeader()->setDefaultSectionSize(24);

        int ord   = ui->orderComboBox->currentIndex();
        int cat   = ui->typeComboBox->currentIndex();
        int limit = ui->limitSpinBox->value();
        int count = ui->minCountSpinBox->value();

        bpt::ptime history = bpt::microsec_clock::local_time();
        history = history - bpt::seconds(s.value("history").toInt()*86400);   
        QString historyString = QString::fromStdString(bpt::to_iso_extended_string(history));

        QFont font("Monospace");
        font.setStyleHint(QFont::Monospace);

        QList<QStringList> rows = DB::getStatisticsData(historyString, cat, count, ord, limit);
        for (QStringList row : rows) {
                QList<QStandardItem*> items;
                // item: key/trigram/word
                QString data(row[0]);
                data.replace(" ",  "␣"); // UNICODE U+2423 'OPEN BOX'
                data.replace('\n', "↵"); // UNICODE U+23CE 'RETURN SYMBOL'
                items << new QStandardItem(data);
                items.last()->setFont(font);
                // speed
                items << new QStandardItem(QString::number(row[1].toDouble(), 'f', 1) + " wpm");
                // accuracy
                items << new QStandardItem(QString::number(row[2].toDouble(), 'f', 1) + "%");
                // viscosity
                items << new QStandardItem(QString::number(row[3].toDouble(), 'f', 1));
                //count
                items << new QStandardItem(row[4]);
                //mistakes
                items << new QStandardItem(row[5]);
                //impact
                items << new QStandardItem(QString::number(row[6].toDouble(), 'f', 1));     

                for (QStandardItem* item : items)
                        item->setFlags(Qt::ItemIsEnabled);
                model->appendRow(items);           
        }

        ui->tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        ui->tableView->resizeColumnsToContents();       
}
