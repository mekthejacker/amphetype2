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

#include "texts/library.h"

#include <QAction>
#include <QCursor>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QMenu>
#include <QModelIndex>
#include <QProgressBar>
#include <QProgressDialog>
#include <QSettings>
#include <QString>
#include <QUrl>
#include <QXmlSchema>
#include <QXmlSchemaValidator>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <QsLog.h>

#include "database/db.h"
#include "generators/lessongenwidget.h"
#include "texts/edittextdialog.h"
#include "texts/lessonminer.h"
#include "texts/lessonminercontroller.h"
#include "texts/sourcemodel.h"
#include "texts/text.h"
#include "texts/textmodel.h"
#include "ui_library.h"

Library::Library(QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::Library),
      text_model_(new TextModel(this)),
      source_model_(new SourceModel(this)) {
  ui->setupUi(this);

  //QSettings s;

  ui->actionAdd_Text->setEnabled(false);

  //ui->sourcesTable->setModel(source_model_);
  //ui->textsTable->setModel(text_model_);

  connect(ui->actionImport_Texts, &QAction::triggered, this,
          &Library::addFiles);
  connect(ui->actionImport_XML, &QAction::triggered, this,
          &Library::importSource);

  connect(ui->actionAdd_Source, &QAction::triggered, this, &Library::addSource);
  connect(ui->actionAdd_Text, &QAction::triggered, this, &Library::addText);

  connect(ui->textsTable, &QTableView::clicked, this,
          &Library::textsTableClickHandler);

  connect(ui->textsTable, &QTableView::doubleClicked, this,
          &Library::textsTableDoubleClickHandler);

  connect(ui->sourcesTable, &QWidget::customContextMenuRequested, this,
          &Library::sourcesContextMenu);

  connect(ui->textsTable, &QWidget::customContextMenuRequested, this,
          &Library::textsContextMenu);

  connect(ui->actionClose, &QAction::triggered, this, &QWidget::close);

  //connect(ui->sourcesTable->selectionModel(),
  //        &QItemSelectionModel::selectionChanged, this,
  //        &Library::sourceSelectionChanged);

  //source_model_->refresh();
}

Library::~Library() {
  delete ui;
  //delete text_model_;
  //delete source_model_;
}

void Library::sourceSelectionChanged(const QItemSelection& a,
                                     const QItemSelection& b) {
  if (!ui->sourcesTable->selectionModel()->hasSelection()) {
    ui->actionAdd_Text->setEnabled(false);
    TextModel* old = text_model_;
    text_model_ = new TextModel;
    ui->textsTable->setModel(text_model_);
    delete old;
  } else {
    ui->actionAdd_Text->setEnabled(true);
    auto indexes = a.indexes();
    if (indexes.isEmpty()) return;
    int source = indexes[0].data(Qt::UserRole).toInt();
    text_model_->setSource(source);
  }
}

void Library::reload() {
  TextModel* old_tm = text_model_;
  SourceModel* old_sm = source_model_;
  text_model_ = new TextModel(this);
  source_model_ = new SourceModel(this);
  ui->sourcesTable->setModel(source_model_);
  ui->textsTable->setModel(text_model_);
  delete old_tm;
  delete old_sm;

  // Set resize mode on both tables, stretch 1st col, resize the rest
  auto sourceHeader = ui->sourcesTable->horizontalHeader();
  auto textHeader = ui->textsTable->horizontalHeader();
  sourceHeader->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int col = 1; col < sourceHeader->count(); col++)
    sourceHeader->setSectionResizeMode(col, QHeaderView::ResizeToContents);
  textHeader->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int col = 1; col < textHeader->count(); col++)
    textHeader->setSectionResizeMode(col, QHeaderView::ResizeToContents);

  connect(ui->sourcesTable->selectionModel(),
          &QItemSelectionModel::selectionChanged, this,
          &Library::sourceSelectionChanged);

  source_model_->refresh();
}

void Library::sourcesContextMenu(const QPoint& pos) {
  QMenu menu(this);

  auto selectedRows = ui->sourcesTable->selectionModel()->selectedRows();
  int selectedCount = selectedRows.size();

  QAction* deleteAction = menu.addAction("delete");
  connect(deleteAction, &QAction::triggered, this, &Library::deleteSource);

  QAction* enableAction = menu.addAction("enable");
  connect(enableAction, &QAction::triggered, this, &Library::enableSource);

  QAction* disableAction = menu.addAction("disable");
  connect(disableAction, &QAction::triggered, this, &Library::disableSource);

  QAction* exportAction = menu.addAction("export as xml...");
  connect(exportAction, &QAction::triggered, this, &Library::exportSource);

  menu.exec(QCursor::pos());
}

void Library::textsContextMenu(const QPoint& pos) {
  QMenu menu(this);

  auto selectedRows = ui->textsTable->selectionModel()->selectedRows();
  int selectedCount = selectedRows.size();

  if (selectedCount == 1) {
    QAction* testAction = menu.addAction("Send to Typer");
    testAction->setData(selectedRows[0].data(Qt::UserRole));
    connect(testAction, &QAction::triggered, this, &Library::actionSendToTyper);

    QAction* editAction = menu.addAction("edit");
    connect(editAction, &QAction::triggered, this, &Library::actionEditText);
  }

  QAction* deleteAction = menu.addAction("delete");
  connect(deleteAction, &QAction::triggered, this, &Library::actionDeleteTexts);

  menu.exec(QCursor::pos());
}

void Library::exportSource() {
  if (!ui->sourcesTable->selectionModel()->hasSelection()) return;
  auto indexes = ui->sourcesTable->selectionModel()->selectedRows();

  QString fileName = QFileDialog::getSaveFileName(this, tr("Export XML"), ".",
                                                  tr("XML files (*.xml)"));
  QLOG_DEBUG() << fileName;

  Database db;

  QFile file(fileName);
  if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) return;

  QXmlStreamWriter stream(&file);
  stream.setAutoFormatting(true);
  stream.writeStartDocument();
  stream.writeStartElement("sources");
  for (const auto& index : indexes) {
    int source = index.data(Qt::UserRole).toInt();
    auto sourceData = db.getSourceData(source);
    stream.writeStartElement("source");
    stream.writeAttribute("name", sourceData[1].toString());
    if (sourceData[6].toInt() == 1) stream.writeAttribute("type", "lesson");
    QStringList texts = db.getAllTexts(source);
    for (const QString& text : texts) {
      stream.writeTextElement("text", text);
    }
    stream.writeEndElement();
  }
  stream.writeEndElement();
  stream.writeEndDocument();

  this->validateXml(&file);
}

void Library::importSource() {
  QString fileName = QFileDialog::getOpenFileName(
      this, tr("Import"), ".", tr("amphetype2 source xml (*.xml)"));
  QFile file(fileName);

  if (!this->validateXml(&file)) return;
  file.open(QIODevice::ReadOnly | QIODevice::Text);

  Database db;
  QXmlStreamReader xml(&file);
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.name() == "source") {
      if (xml.attributes().value("name").isEmpty()) {
        continue;
      }

      QStringList texts;
      int type = 0;
      int discount = -1;

      if (xml.attributes().value("type") == "lesson") {
        type = 1;
        discount = 1;
      }

      int source = db.getSource(xml.attributes().value("name").toString(),
                                discount, type);
      while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement()) {
          if (!texts.isEmpty()) {
            db.addTexts(source, texts);
          }
          break;
        }
        if (xml.name() == "text") {
          QString text(xml.readElementText());
          if (!text.isEmpty()) {
            texts << text;
          }
        }
      }
    }
  }

  if (xml.hasError()) {
    return;
  }

  refreshSources();
  emit sourcesChanged();
}

void Library::actionEditText(bool checked) {
  auto indexes = ui->textsTable->selectionModel()->selectedRows();
  if (!ui->textsTable->selectionModel()->hasSelection() || indexes.size() > 1)
    return;
  int id = indexes[0].data(Qt::UserRole).toInt();

  Database db;
  auto t = db.getText(id);

  EditTextDialog dialog(tr("Edit Text:"), t->text());

  if (dialog.exec() == QDialog::Accepted) {
    db.updateText(id, dialog.text());
    QLOG_DEBUG() << id << dialog.text();
    text_model_->refreshText(indexes[0]);
    emit textsChanged(QList<int>() << id);
  }
}

void Library::actionDeleteTexts(bool checked) {
  if (!ui->textsTable->selectionModel()->hasSelection()) return;
  auto indexes = ui->textsTable->selectionModel()->selectedRows();

  QList<int> text_ids;
  for (const auto& index : indexes) {
    text_ids << index.data(Qt::UserRole).toInt();
  }
  Database db;
  db.deleteText(text_ids);

  if (ui->sourcesTable->selectionModel()->hasSelection()) {
    auto sourceIndex = ui->sourcesTable->selectionModel()->selectedRows()[0];
    source_model_->refreshSource(sourceIndex);
    emit sourceChanged(sourceIndex.data(Qt::UserRole).toInt());
  }

  text_model_->removeIndexes(indexes);
  emit textsDeleted(text_ids);
}

void Library::actionSendToTyper(bool checked) {
  auto sender = reinterpret_cast<QAction*>(this->sender());
  const QVariant& id = sender->data();

  if (!id.isValid()) return;

  QLOG_DEBUG() << id.toInt();
  Database db;
  auto t = db.getText(id.toInt());
  emit setText(t);
}

void Library::textsTableDoubleClickHandler(const QModelIndex& index) {
  QVariant text_id = index.data(Qt::UserRole);
  if (!text_id.isValid()) return;

  QLOG_DEBUG() << text_id.toInt();

  Database db;
  auto t = db.getText(text_id.toInt());
  emit setText(t);
}

void Library::textsTableClickHandler(const QModelIndex& index) {
  QLOG_DEBUG() << index.data(Qt::UserRole);
}

void Library::enableSource() {
  auto indexes = ui->sourcesTable->selectionModel()->selectedRows();
  for (const auto& index : indexes) source_model_->enableSource(index);
}

void Library::disableSource() {
  auto indexes = ui->sourcesTable->selectionModel()->selectedRows();
  for (const QModelIndex& index : indexes) source_model_->disableSource(index);
}

void Library::addText() {
  auto indexes = ui->sourcesTable->selectionModel()->selectedRows();
  if (indexes.isEmpty()) return;

  bool ok;
  QString text = QInputDialog::getMultiLineText(this, tr("Add Text:"),
                                                tr("Text:"), "", &ok);

  if (ok && !text.isEmpty()) {
    int source = indexes[0].data(Qt::UserRole).toInt();
    Database db;
    db.addText(source, text);
    source_model_->refreshSource(indexes[0]);
    text_model_->refresh();
  }
}

void Library::addSource() {
  bool ok;
  QString sourceName = QInputDialog::getText(
      this, tr("Add Source:"), tr("Source name:"), QLineEdit::Normal, "", &ok);

  if (ok && !sourceName.isEmpty()) {
    source_model_->addSource(sourceName);
    emit sourcesChanged();
  }
}

void Library::deleteSource() {
  auto indexes = ui->sourcesTable->selectionModel()->selectedRows();
  QList<int> sources;
  for (const auto& index : indexes) sources << index.data(Qt::UserRole).toInt();

  source_model_->removeIndexes(indexes);
  emit sourcesDeleted(sources);
  emit sourcesChanged();
}

void Library::refreshSources() { source_model_->refresh(); }

void Library::refreshSource(int source) {
  source_model_->refreshSource(source);
}

void Library::selectSource(int source) {
  for (int i = 0; i < source_model_->rowCount(); i++) {
    auto index = source_model_->index(i, 0);
    if (index.data(Qt::UserRole) == source) {
      ui->sourcesTable->setCurrentIndex(index);
      return;
    }
  }
}

void Library::addFiles() {
  files_ = QFileDialog::getOpenFileNames(
      this, tr("Import"), ".", tr("UTF-8 text files (*.txt);;All files (*)"));
  if (files_.isEmpty()) return;

  lmc_ = new LessonMinerController;
  connect(lmc_, SIGNAL(workDone()), this, SLOT(processNextFile()));

  progress_ = new QProgressDialog("Importing...", "Abort", 0, 100);
  progress_->setMinimumDuration(0);
  progress_->setAutoClose(false);
  connect(lmc_, &LessonMinerController::progressUpdate, progress_,
          &QProgressDialog::setValue);

  processNextFile();
}

void Library::processNextFile() {
  if (files_.isEmpty()) {
    refreshSources();
    emit sourcesChanged();
    delete progress_;
    delete lmc_;
    return;
  }
  progress_->setLabelText(files_.front());
  lmc_->operate(files_.front());
  files_.pop_front();
}

bool Library::validateXml(QFile* file) {
  if (file->isOpen()) file->close();

  if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
    QLOG_DEBUG() << "validateXml: can't open file:" << file->fileName();
    return false;
  }

  QXmlSchema schema;
  QFile schemaFile(":/sources.xsd");
  schemaFile.open(QIODevice::ReadOnly);
  schema.load(&schemaFile, QUrl::fromLocalFile(schemaFile.fileName()));
  bool v = false;
  if (schema.isValid()) {
    QXmlSchemaValidator validator(schema);
    v = validator.validate(file, QUrl::fromLocalFile(file->fileName()));
    if (v)
      QLOG_DEBUG() << "validateXml: document is valid." << file->fileName();
    else
      QLOG_DEBUG() << "validateXml: document is invalid." << file->fileName();
  } else {
    QLOG_DEBUG() << "validateXml: schema is invalid." << schemaFile.fileName();
  }
  file->close();
  return v;
}
