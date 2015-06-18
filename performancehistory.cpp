#include "performancehistory.h"
#include "ui_performancehistory.h"
#include "db.h"
#include "text.h"

#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/date_time/gregorian/gregorian.hpp"

#include <iostream>

#include <QVariantList>
#include <QSettings>
#include <QByteArray>
#include <QStandardItemModel>

PerformanceHistory::PerformanceHistory(QWidget* parent)
        : QWidget(parent), ui(new Ui::PerformanceHistory),
          modelb(new QStandardItemModel())
{
        ui->setupUi(this);

        QSettings s;

        // set states of checkboxes based on settings file
        if (s.value("chrono_x").toBool())
                ui->timeScaleCheckBox->setCheckState(Qt::Checked);

        if (s.value("show_xaxis").toBool())
                ui->fullRangeYCheckBox->setCheckState(Qt::Checked);

        if (s.value("dampen_graph").toBool())
                ui->dampenCheckBox->setCheckState(Qt::Checked);        

        ui->smaWindowSpinBox->setValue(s.value("dampen_average").toInt());

        // add the 3 graphs we will use
        ui->performancePlot->addGraph();
        ui->performancePlot->addGraph();
        ui->performancePlot->addGraph();
        ui->performancePlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc,3));
        ui->performancePlot->graph(0)->setPen(QPen(QColor(0, 20, 255,100), 1));
        ui->performancePlot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc,3));
        ui->performancePlot->graph(1)->setPen(QPen(QColor(0, 20, 255,100), 1));
        ui->performancePlot->graph(2)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc,3));
        ui->performancePlot->graph(2)->setPen(QPen(QColor(0, 20, 255,100), 1));

        ui->limitNumber->setText(s.value("perf_items").toString());

        refreshSources();

        // double clicking an item in the list
        connect(ui->tableView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(doubleClicked(const QModelIndex&)));
        // settings
        connect(ui->updateButton,   SIGNAL(pressed()),                this, SLOT(writeSettings()));
        connect(ui->sourceComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(writeSettings()));
        connect(ui->plotSelector,   SIGNAL(currentIndexChanged(int)), this, SLOT(showPlot(int)));
        // checkboxes for plot settings
        connect(ui->timeScaleCheckBox,  SIGNAL(stateChanged(int)), this, SLOT(writeSettings()));
        connect(ui->fullRangeYCheckBox, SIGNAL(stateChanged(int)), this, SLOT(writeSettings()));
        connect(ui->dampenCheckBox,     SIGNAL(stateChanged(int)), this, SLOT(writeSettings()));
        connect(ui->smaWindowSpinBox,   SIGNAL(valueChanged(int)), this, SLOT(writeSettings()));
        connect(ui->plotCheckBox,       SIGNAL(stateChanged(int)), this, SLOT(writeSettings()));
}

PerformanceHistory::~PerformanceHistory()
{
        delete ui;
        delete modelb;
}

void PerformanceHistory::togglePlot(int i)
{
        if (i == 0)
                ui->performancePlot->graph(ui->plotSelector->currentIndex())->setVisible(true);
        else
                ui->performancePlot->graph(ui->plotSelector->currentIndex())->setVisible(false);
        ui->performancePlot->replot();
}

// add a graph to the plot that is the SMA of the given graph
// Simple moving average
QCPGraph* PerformanceHistory::dampen(QCPGraph* graph, int n)
{
        QCPDataMap* data = graph->data();
        QCPGraph* newGraph = ui->performancePlot->addGraph();

        if (n > data->size())
                return 0;

        double s = 0;
        QList<double> x;
        QList<double> y;

        QCPDataMapIterator it(*data);
        while (it.hasNext()) {
                it.next();
                x << it.value().key;
                y << it.value().value;
        }

        for (int i = 0; i < n; ++i)
                s += y[i];

        double q = 1.0 / n;
        for (int i = n; i < x.size(); ++i) {
                newGraph->addData(x[i], s * q);
                s += y[i] - y[i - n];
        }

        // style the graph
        //newGraph->setScatterStyle(QCPScatterStyle::ssDisc);
        newGraph->setPen(QPen(QColor(255, 0, 0), 2));

        return newGraph;
}

void PerformanceHistory::writeSettings()
{
        QSettings s;
        s.setValue("perf_items", ui->limitNumber->text().toInt());

        if (ui->timeScaleCheckBox->checkState() == Qt::Unchecked)
                s.setValue("chrono_x", false);
        else
                s.setValue("chrono_x", true);

        if (ui->fullRangeYCheckBox->checkState() == Qt::Unchecked)
                s.setValue("show_xaxis", false);
        else
                s.setValue("show_xaxis", true);

        if (ui->dampenCheckBox->checkState() == Qt::Unchecked)
                s.setValue("dampen_graph", false);
        else
                s.setValue("dampen_graph", true);
        s.setValue("dampen_average", ui->smaWindowSpinBox->value());

        refreshPerformance();
}

void PerformanceHistory::refreshSources()
{
        ui->sourceComboBox->clear();
        ui->sourceComboBox->addItem("<ALL>");
        ui->sourceComboBox->addItem("<LAST TEXT>");
        ui->sourceComboBox->addItem("<ALL TEXTS>");
        ui->sourceComboBox->addItem("<ALL LESSONS>");

        QList<QVariantList> sources;
        DB::getSourcesList(&sources);

        for (QVariantList x : sources)
                ui->sourceComboBox->addItem(x.at(1).toString(), x.at(0));
}

void PerformanceHistory::doubleClicked(const QModelIndex& idx)
{       
        int row = idx.row();
        const QModelIndex& f = modelb->index(row, 0);

        sqlite3pp::database db(DB::db_path.toStdString().c_str());
        QString sql = "select id, source, text from text where id = \""+ f.data().toByteArray() +"\"";
        sqlite3pp::query qry(db, sql.toStdString().c_str());
        for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
                QByteArray _id     = QByteArray((*i).get<char const*>(0));
                int        _source = (*i).get<int>(1);
                QString    _text   = QString((*i).get<char const*>(2));
                Text* t = new Text(_id, _source, _text);
                emit setText(t);
                emit gotoTab(0); 
                return;
        }
}

void PerformanceHistory::refreshPerformance()
{
        QSettings s;

        ui->tableView->hide();
        
        modelb->clear();
        
        QStringList headers;
        headers << "id"
                 << "When"
                 << "Source"
                 << "WPM"
                 << "Accuracy"
                 << "Viscosity";
        modelb->setHorizontalHeaderLabels(headers);
        ui->tableView->setModel(modelb);
        ui->tableView->setSortingEnabled(false);
        ui->tableView->setColumnHidden(0, true);

        QHeaderView* verticalHeader = ui->tableView->verticalHeader();
        verticalHeader->sectionResizeMode(QHeaderView::Fixed);
        verticalHeader->setDefaultSectionSize(24);

        QStringList query;        
        switch (ui->sourceComboBox->currentIndex()) {
        case 0:
                break;
        case 1:
                query << "r.text_id = (select text_id from result order by "
                         "w desc limit 1)";
                break;
        case 2:
                query << "s.discount is null";
                break;
        case 3:
                query << "s.discount is not null";
                break;
        default:
                QString s = ui->sourceComboBox->currentData().toString();
                query << "r.source = " + s;
                break;
        }

        QString where;
        if (query.length() > 0)
                where = "where " + query.join(" and ");
        else
                where = "";

        QString group;

        QString sql;
        int g = s.value("perf_group_by").toInt();
        QString n = ui->limitNumber->text();
        if (g == 0) {
                sql = "select text_id,w,s.name,wpm,100.0*accuracy,viscosity "
                      "from result as r left join source as s on(r.source = "
                      "s.rowid) " +
                      where + " " + group + " order by datetime(w) DESC limit " + n;
        } else {
                sql = "select agg_first(text_id),avg(r.w) as w,count(r.rowid) "
                      "|| ' result(s)',agg_median(r.wpm), 100.0 * "
                      "agg_median(r.accuracy), agg_median(r.viscosity) from "
                      "result as r left join source as s on(r.source = "
                      "s.rowid) " +
                      where + " " + group + " order by datetime(w) DESC limit " + n;
        }

        ////////////////////////////
        sqlite3pp::database db(DB::db_path.toStdString().c_str());
        sqlite3pp::query qry(db, sql.toStdString().c_str());

        // clear the data in the plots
        for (int i = 0; i <ui->performancePlot->graphCount(); ++i)
                ui->performancePlot->graph(i)->clearData();

        QList<QStandardItem*> items;
        double x = -1;
        for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
                // add hash from db
                items << new QStandardItem(QString((*i).get<char const*>(0)));
                // turn the time string from db into a ptime
                // its stored as delimited extended iso
                boost::posix_time::ptime t;
                t = boost::date_time::parse_delimited_time<boost::posix_time::ptime>((*i).get<char const*>(1), 'T');
                // add time. convert it to nicer display first
                items << new QStandardItem(QString::fromStdString(
                                boost::gregorian::to_simple_string(t.date())));

                // add source
                items << new QStandardItem(QString((*i).get<char const*>(2)));

                // add points to each of the plots
                // if chrono x, make the x value seconds since epoch
                if (s.value("chrono_x").toBool()) {
                        boost::posix_time::ptime epoch(boost::gregorian::date(1970,1,1));
                        boost::posix_time::time_duration::sec_type sec = (t - epoch).total_seconds();
                        x = time_t(sec);
                }  
                double wpm = (*i).get<double>(3);
                double acc = (*i).get<double>(4);
                double vis = (*i).get<double>(5);
                ui->performancePlot->graph(0)->addData(x, wpm);
                ui->performancePlot->graph(1)->addData(x, acc);
                ui->performancePlot->graph(2)->addData(x, vis);

                // add wpm,acc,vis, 1 sigificant digit
                items << new QStandardItem(QString::number(wpm, 'f', 1));
                items << new QStandardItem(QString::number(acc, 'f', 1));
                items << new QStandardItem(QString::number(vis, 'f', 1));

                for (QStandardItem* item : items)
                        item->setFlags(Qt::ItemIsEnabled);
                modelb->appendRow(items);
                items.clear();

                --x;
        }
        //////////////////

        ui->tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        ui->tableView->resizeColumnsToContents();

        ui->tableView->show();
        showPlot(ui->plotSelector->currentIndex());
}

void PerformanceHistory::showPlot(int p)
{
        if (p >= ui->performancePlot->graphCount())
                return;

        QSettings s;

        // hide all the plots
        for (int i = 0; i < ui->performancePlot->graphCount(); i++)
                ui->performancePlot->graph(i)->setVisible(false);

        // show the plot we want
        if (ui->plotCheckBox->checkState() == 0)
                ui->performancePlot->graph(p)->setVisible(true);

        // set correct y axis label
        /*
        switch (p) {
        case 0:
                ui->performancePlot->yAxis->setLabel("Words per Minute (wpm)");
                break;
        case 1:
                ui->performancePlot->yAxis->setLabel("Accuracy (%)");
                break;
        case 2:
                ui->performancePlot->yAxis->setLabel("viscosity");
                break;
        }*/

        // axis properties dependent on time scaling or not
        if (s.value("chrono_x").toBool()) {
                ui->performancePlot->xAxis->setTickLabelType(QCPAxis::ltDateTime);
                ui->performancePlot->xAxis->setDateTimeFormat("M/dd");
                ui->performancePlot->xAxis->setAutoTickStep(true);
                //ui->performancePlot->xAxis->setLabel("Time of Result");
        } else {
                ui->performancePlot->xAxis->setTickLabelType(QCPAxis::ltNumber);
                ui->performancePlot->xAxis->setAutoTickStep(false);
                ui->performancePlot->xAxis->setTickStep(ui->performancePlot->graph(p)->data()->size()/10);
                ui->performancePlot->xAxis->setSubTickCount(ui->performancePlot->graph(p)->data()->size()/100);   
                //ui->performancePlot->xAxis->setLabel("Test Result #");
        }

        // make a SMA graph out of the current one and show it
        if (s.value("dampen_graph").toBool()) {
                QCPGraph* sma = dampen(ui->performancePlot->graph(p), s.value("dampen_average").toInt()); 
                if (sma != 0)
                        sma->setVisible(true); 
        }

        ui->performancePlot->rescaleAxes(true);

        // set the min or max of the y axis if needed
        if (s.value("show_xaxis").toBool()) {
                if (p == 1) // accuracy plot
                        ui->performancePlot->yAxis->setRangeUpper(100);
                else
                        ui->performancePlot->yAxis->setRangeLower(0);
        }

        ui->performancePlot->replot();
}
