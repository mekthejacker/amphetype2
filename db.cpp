#include "db.h"

#include "inc/sqlite3pp.h"

#include <QString>
#include <QStringList>
#include <QSettings>
#include <QApplication>
#include <QDir>
#include <QCryptographicHash>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

QString DB::db_path = QString();

struct agg_median
{
        void step(double x) {
                l.push_back(x);
        }
        double finish() {
                if (l.empty()) return 0;
                std::sort(l.begin(), l.end());
                double median;
                int length = l.size();
                if (length % 2 == 0)
                        median = (l[length / 2] + l[length / 2 - 1]) / 2;
                else
                        median = l[length / 2];
                return median;
        }
        std::vector<double> l;
};

void DB::addFunctions(sqlite3pp::database* db)
{
        try {
                sqlite3pp::ext::aggregate aggr(*db);
                aggr.create<agg_median, double>("agg_median"); 
        } catch (const std::exception& e) {
                std::cout << e.what() << std::endl;
        } 
}

void DB::initDB()
{
        try {
                sqlite3pp::database db(DB::db_path.toUtf8().data());
                db.execute("create table source (name text, disabled integer, "
                    "discount integer)");
                db.execute("create table text (id text primary key, source integer, "
                    "text text, disabled integer)");
                db.execute("create table result (w real, text_id text, source "
                    "integer, wpm real, accuracy real, viscosity real)");
                db.execute("create table statistic (w real, data text, type integer, "
                    "time real, count integer, mistakes integer, viscosity "
                    "real)");
                db.execute("create table mistake (w real, target text, mistake text, "
                            "count integer)");
                db.execute("create view text_source as select "
                            "id,s.name,text,coalesce(t.disabled,s.disabled) from text "
                            "as t left join source as s on (t.source = s.rowid)");           
        } catch (const std::exception& e) {
                std::cout << "cannot create database" <<std::endl;
                std::cout << e.what() <<std::endl;
        }
}

int DB::getSource(const QString& source, int lesson)
{
        try {
                QString sql = QString("select rowid from source where name = \"%1\" limit 1").arg(source);
                QStringList row = DB::getOneRow(sql);
                
                // if the source exists return it
                if(!row.isEmpty())
                        return row[0].toInt();
                
                // source didn't exist. add it
                QVariantList data;

                data << source;
                if (lesson == -1)
                        data << QVariant();
                else
                        data << lesson;

                sql = "insert into source (name, discount) values (?, ?)";
                DB::insertItems(sql, data);
                // try again now that it's in the db
                return getSource(source, lesson);
        } catch (const std::exception& e) {
                std::cout << "error getting source" << std::endl;
                std::cout << e.what() << std::endl;
                return -1;
        }
}

void DB::addTexts(int source, const QStringList& lessons, int lesson, bool update)
{
        try {
                QString sql = "insert into text (id,text,source,disabled) values (?, ?, ?, ?)";
                sqlite3pp::database db(DB::db_path.toStdString().c_str());
                sqlite3pp::transaction xct(db);
                {
                        for (QString text : lessons) {
                                QByteArray txt_id = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha1);
                                txt_id = txt_id.toHex();
                                int dis = ((lesson == 2) ? 1 : 0);

                                QVariantList items;
                                items << txt_id;
                                items << text;
                                items << source;
                                if (dis == 0)
                                        items << QVariant(); // null
                                else
                                        items << dis;
                                DB::insertItems(&db, sql, items);
                        }
                }
                xct.commit();
        } catch (const std::exception& e) {
                std::cout << "error adding text" << std::endl;
                std::cout << e.what() << std::endl;
        }
}

void DB::addResult(const QString& time, const QByteArray& id, int source, double wpm, double acc, double vis)
{
        try {
                QString sql = "insert into result (w,text_id,source,wpm,accuracy,viscosity)"
                                                " values (?, ?, ?, ?, ?, ?)";
                sqlite3pp::database db(DB::db_path.toUtf8().data());
                sqlite3pp::transaction resultTransaction(db);
                {
                        QVariantList items;
                        items << time << id << source << wpm << acc << vis;
                        DB::insertItems(&db, sql, items);
                }
                resultTransaction.commit();
        } catch (const std::exception& e) {
                std::cout << "error adding result" << std::endl;
                std::cout << e.what() << std::endl;
        }
}

void DB::addStatistics(const QString& time, const QMultiHash<QStringRef, double>& stats, 
                       const QMultiHash<QStringRef, double>& visc, 
                       const QMultiHash<QStringRef, int>& mistakeCount)
{
        try {
                QString sql = "insert into statistic (time,viscosity,w,count,mistakes,type,data) "
                                              "values (?, ?, ?, ?, ?, ?, ?)";
                sqlite3pp::database db(DB::db_path.toUtf8().data());
                sqlite3pp::transaction statisticsTransaction(db);
                {
                        QList<QStringRef> keys = stats.uniqueKeys();
                        for (QStringRef k : keys) {
                                QVariantList items;
                                // median time
                                const QList<double>& timeValues = stats.values(k);
                                if (timeValues.length() > 1)
                                        items << (timeValues[timeValues.length() / 2] + (timeValues[timeValues.length() / 2 - 1])) / 2.0;
                                else if (timeValues.length() == 1)
                                        items << timeValues[timeValues.length() / 2];
                                else
                                        items << timeValues.first();

                                // get the median viscosity
                                const QList<double>& viscValues = visc.values(k);
                                if (viscValues.length() > 1)
                                        items << ((viscValues[viscValues.length() / 2] + viscValues[viscValues.length() / 2 - 1]) / 2.0) * 100.0;
                                else if (viscValues.length() == 1)
                                        items << viscValues[viscValues.length() / 2] * 100.0;
                                else
                                        items << viscValues.first() * 100.0;

                                items << time;
                                items << stats.count(k);
                                items << mistakeCount.count(k);

                                if (k.length() == 1)
                                        items << 0;     // char
                                else if (k.length() == 3)
                                        items << 1;     // tri
                                else 
                                        items << 2;     // word

                                items << k.toString();

                                DB::insertItems(&db, sql, items);
                        }
                }
                statisticsTransaction.commit();
        } catch (const std::exception& e) {
                std::cout << "error adding statistics" << std::endl;
                std::cout << e.what() << std::endl;
        }
}

void DB::addMistakes(const QString& time, const QHash<QPair<QChar, QChar>, int>& m)
{
        try {
                QString sql = "insert into mistake (w,target,mistake,count) values (?, ?, ?, ?)";
                sqlite3pp::database db(DB::db_path.toUtf8().data());
                sqlite3pp::transaction mistakesTransaction(db);
                {
                        QHashIterator<QPair<QChar,QChar>, int> it(m);
                        while (it.hasNext()) {
                                it.next();
                                QVariantList items;
                                items << time
                                      << QString(it.key().first)
                                      << QString(it.key().second)
                                      << it.value();

                                DB::insertItems(&db, sql, items);
                        }
                }
                mistakesTransaction.commit();
        } catch (const std::exception& e) {
                std::cout << "error adding mistakes" << std::endl;
                std::cout << e.what() << std::endl;
        }
}

 std::pair<double,double> DB::getMedianStats(int n)
 {
        QString query = QString(
                "select agg_median(wpm), agg_median(acc) "
                "from (select wpm,100.0*accuracy as acc from result "
                        "order by datetime(w) desc limit %1)").arg(n);
        QStringList cols = getOneRow(query);
        return {cols[0].toDouble(), cols[1].toDouble()};
 }

QList<QStringList> DB::getSourcesData()
 {
        QString sql =
                "select s.rowid, s.name, t.count, r.count, r.wpm, "
                        "nullif(t.dis,t.count) "
                "from source as s "
                "left join (select source,count(*) as count, "
                        "count(disabled) as dis "
                        "from text group by source) as t "
                        "on (s.rowid = t.source) "
                "left join (select source,count(*) as count, "
                        "avg(wpm) as wpm from result group by source) "
                        "as r on (t.source = r.source) "
                "where s.disabled is null "
                "order by s.name";
        QList<QStringList> rows;
        rows = getRows(sql);
        return rows;
 }

QList<QStringList> DB::getTextsData(int source)
{
        QString sql = QString(
                "select t.rowid, substr(t.text,0,30)||' ...', "
                        "length(t.text), r.count, r.m, t.disabled "
                "from (select rowid,* from text where source = %1) as t "
                "left join (select text_id,count(*) as count, avg(wpm) "
                        "as m from result group by text_id) "
                        "as r on (t.id = r.text_id) "
                "order by t.rowid").arg(source);
        QList<QStringList> rows;
        rows = getRows(sql);
        return rows;
}

QList<QStringList> DB::getPerformanceData(int w, int source, int limit)
{
        QSettings s;
        int g = s.value("perf_group_by").toInt();
        bool grouping = (g == 0) ? false : true;
        QStringList query;
        switch (w) {
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
                query << "r.source = " + QString::number(source);
                break;
        }
        QString where;
        if (query.length() > 0)
                where = "where " + query.join(" and ");
        else
                where = "";

        // TODO: add grouping
        QString group;
        int gn = s.value("def_group_by").toInt();
        //

        QString sql;
        if (!grouping) {
                sql = QString(
                        "select text_id,w,s.name,wpm,100.0*accuracy,viscosity "
                        "from result as r "
                        "left join source as s on(r.source = s.rowid) "
                        "%1 %2 order by datetime(w) DESC limit %3")
                        .arg(where).arg(group).arg(limit);
        } else {
                sql = QString(
                        "select agg_first(text_id),"
                               "avg(r.w) as w,"
                               "count(r.rowid) || ' result(s)',"
                               "agg_median(r.wpm),"
                               "100.0 * agg_median(r.accuracy),"
                               "agg_median(r.viscosity) "
                        "from result as r "
                        "left join source as s on(r.source = s.rowid) "
                        "%1 %2 order by datetime(w) DESC limit %3")
                      .arg(where).arg(group).arg(limit);
        }
        QList<QStringList> rows;
        rows = getRows(sql);
        return rows;     
}

QList<QStringList> DB::getStatisticsData(const QString& when, int type, int count, const QString& order, int limit)
{
        QString sql = QString("select data, "
                  "12.0/time as wpm, "
                  "100.0-100.0*misses/cast(total as real) as accuracy, "
                  "viscosity, "
                  "total, "
                  "misses, "
                  "total*time*time*(1.0+misses/total) as damage "
                "from (select data, "
                       "agg_median(time) as time, "
                       "agg_median(viscosity) as viscosity, "
                       "sum(count) as total, "
                       "sum(mistakes) as misses "
                       "from statistic "
                       "where datetime(w) >= datetime(\"%1\") "
                        "and type = %2 group by data) "
                "where total >= %3 "
                "order by %4 limit %5").arg(when).arg(type).arg(count).arg(order).arg(limit);

        QList<QStringList> rows;
        rows = getRows(sql);
        return rows;
}



QList<QStringList> DB::getSourcesList()
{
        QString sql = "select rowid, name from source order by name";
        QList<QStringList> rows;
        rows = getRows(sql);
        return rows;
}

QStringList DB::getOneRow(const QString& sql)
{
        try {
                sqlite3pp::database db(DB::db_path.toUtf8().data());
                DB::addFunctions(&db);

                sqlite3pp::query qry(db, sql.toUtf8().data());
                QStringList row;
                for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
                        for (int j = 0; j < qry.column_count(); ++j)
                                row << (*i).get<char const*>(j);
                }
                return row;
        } catch (const std::exception& e) {
                std::cout << "error running query: " << sql.toStdString() << std::endl;
                std::cout << e.what() << std::endl;
                return QStringList();
        }
}

QList<QStringList> DB::getRows(const QString& sql)
{
        try {
                sqlite3pp::database db(DB::db_path.toUtf8().data());
                DB::addFunctions(&db);

                sqlite3pp::query qry(db, sql.toUtf8().data());
                QList<QStringList> rows;
                for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
                        QStringList row;
                        for (int j = 0; j < qry.column_count(); ++j)
                                row << (*i).get<char const*>(j);
                        rows << row;
                }
                return rows;
        } catch (const std::exception& e) {
                std::cout << "error running query: " << sql.toStdString() << std::endl;
                std::cout << e.what() << std::endl;
                return QList<QStringList>();
        }
}

void DB::insertItems(const QString& sql, const QVariantList& values)
{
        try {
                sqlite3pp::database db(DB::db_path.toUtf8().data());
                sqlite3pp::command cmd(db, sql.toUtf8().data());

                QList<QByteArray> items;
                // whatever the bind value is can't go out of scope before the execute()
                for (int i = 0; i < values.length(); ++i) {
                        items << values[i].toByteArray();
                        if (items[i].isEmpty())
                                cmd.bind(i + 1);
                        else
                                cmd.bind(i + 1, items[i].data());
                }
                cmd.execute();
        } catch (const std::exception& e) {
                std::cout << "error inserting data: " << sql.toStdString() << std::endl;
                std::cout << e.what() << std::endl;
        }
}

void DB::insertItems(sqlite3pp::database* db, const QString& sql, const QVariantList& values)
{
        try {
                sqlite3pp::command cmd(*db, sql.toUtf8().data());

                QList<QByteArray> items;
                // whatever the bind value is can't go out of scope before the execute()
                for (int i = 0; i < values.length(); ++i) {
                        items << values[i].toByteArray();
                        if (items[i].isEmpty())
                                cmd.bind(i + 1);
                        else
                                cmd.bind(i + 1, items[i].data());
                }
                cmd.execute();
        } catch (const std::exception& e) {
                std::cout << "error inserting data: " << sql.toStdString() << std::endl;
                std::cout << e.what() << std::endl;
        }
}
