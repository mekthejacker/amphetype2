// Copyright (C) 2016  Cory Parsons
//
// This file is part of amphetype2.
//
// amphetype2 is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// amphetype2 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with amphetype2.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef SRC_TEXTS_SOURCEMODEL_H_
#define SRC_TEXTS_SOURCEMODEL_H_

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QVariant>
#include <QVector>

#include "database/db.h"

class SourceItem {
  friend class SourceModel;

 public:
  SourceItem(Database *, int, const QString &, int, int, int, int, int, double);
  int id() { return id_; };
  void refresh();
  void deleteFromDb();
  void enable();
  void disable();

 private:
  Database *db_;
  int id_;
  QString name_;
  int disabled_;
  int discount_;
  int type_;
  int text_count_;
  int results_;
  double wpm_;
};

class SourceModel : public QAbstractTableModel {
  Q_OBJECT

 public:
  explicit SourceModel(QObject *parent = Q_NULLPTR);
  ~SourceModel();

  void clear();
  void refresh();
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  int rowCount(const QModelIndex &parent = QModelIndex()) const;
  int columnCount(const QModelIndex &parent = QModelIndex()) const;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

  void removeIndexes(const QModelIndexList &indexes);
  void refreshSource(const QModelIndex &index);
  void refreshSource(int source);
  void deleteSource(const QModelIndex &index);
  void enableSource(const QModelIndex &index);
  void disableSource(const QModelIndex &index);
  void addSource(const QString &name);

 private:
  Database db_;
  QVector<SourceItem *> items;
};

#endif  // SRC_TEXTS_SOURCEMODEL_H_