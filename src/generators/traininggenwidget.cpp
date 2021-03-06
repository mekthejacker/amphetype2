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

#include "generators/traininggenwidget.h"

#include <QString>
#include <QList>
#include <QChar>
#include <QDateTime>
#include <QDate>

#include "ui_traininggenwidget.h"
#include "generators/traininggenerator.h"
#include "database/db.h"

TrainingGenWidget::TrainingGenWidget(QWidget* parent)
    : QWidget(parent), ui(new Ui::TrainingGenWidget) {
  ui->setupUi(this);

  connect(ui->generateButton, SIGNAL(clicked()), this, SLOT(generate()));

  ui->layoutComboBox->addItem("QWERTY",
                              QVariant::fromValue(amphetype::Layout::QWERTY));
  ui->layoutComboBox->addItem("AZERTY",
                              QVariant::fromValue(amphetype::Layout::AZERTY));
  ui->layoutComboBox->addItem("QWERTZ",
                              QVariant::fromValue(amphetype::Layout::QWERTZ));
  ui->layoutComboBox->addItem("DVORAK",
                              QVariant::fromValue(amphetype::Layout::DVORAK));
  ui->layoutComboBox->addItem("COLEMAK",
                              QVariant::fromValue(amphetype::Layout::COLEMAK));
  ui->layoutComboBox->addItem("WORKMAN",
                              QVariant::fromValue(amphetype::Layout::WORKMAN));
}

TrainingGenWidget::~TrainingGenWidget() { delete ui; }

void TrainingGenWidget::generate() {
  amphetype::Layout layout =
      ui->layoutComboBox->currentData().value<amphetype::Layout>();
  int lessonLength = ui->lessonLengthSpinBox->value();
  int lessonsPerStage = ui->lessonsPerStageSpinBox->value();

  TrainingGenerator tg(layout);

  QStringList bigList;
  QList<QStringList>* x = tg.generate(lessonsPerStage, lessonLength);
  for (QStringList l : *x) {
    for (QString s : l) bigList.append(s);
  }

  QString layoutName;
  switch (layout) {
    case amphetype::Layout::QWERTY:
      layoutName = "QWERTY";
      break;
    case amphetype::Layout::AZERTY:
      layoutName = "AZERTY";
      break;
    case amphetype::Layout::QWERTZ:
      layoutName = "QWERTZ";
      break;
    case amphetype::Layout::COLEMAK:
      layoutName = "COLEMAK";
      break;
    case amphetype::Layout::DVORAK:
      layoutName = "DVORAK";
      break;
    case amphetype::Layout::WORKMAN:
      layoutName = "WORKMAN";
      break;
    default:
      return;
  }

  QDateTime now = QDateTime::currentDateTime();
  QString sourceName =
      layoutName + "::Generated Training " + now.toString("hh:mm:ss.zzz");

  Database db;
  int sourceId = db.getSource(sourceName, amphetype::text_type::Lesson);
  db.addTexts(sourceId, bigList);
  bigList.clear();

  delete x;
  // emit generatedLessons();
  emit newTraining(sourceId);
}
