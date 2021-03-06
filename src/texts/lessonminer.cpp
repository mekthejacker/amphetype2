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

#include "texts/lessonminer.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QSettings>
#include <QString>
#include <QStringRef>
#include <QTextStream>


#include <list>

#include "database/db.h"

LessonMiner::LessonMiner(QObject* parent) : QObject(parent) {
  QSettings s;
  min_chars = s.value("min_chars", 220).toInt();

  // things to ignore as sentence enders.
  // ie "Mr. Smith." is shouldn't be 2 sentences
  abbr << "jr"
       << "mr"
       << "mrs"
       << "ms"
       << "dr"
       << "prof"
       << "sr"
       << "sen"
       << "rep"
       << "sens"
       << "reps"
       << "gov"
       << "attys"
       << "atty"
       << "supt"
       << "det"
       << "rev"
       << "col"
       << "gen"
       << "lt"
       << "cmdr"
       << "adm"
       << "capt"
       << "sgt"
       << "cpl"
       << "maj"
       << "dept"
       << "univ"
       << "assn"
       << "bros"
       << "inc"
       << "ltd"
       << "co"
       << "corp"
       << "arc"
       << "al"
       << "ave"
       << "blvd"
       << "bld"
       << "cl"
       << "ct"
       << "cres"
       << "dr"
       << "expy"
       << "exp"
       << "dist"
       << "mt"
       << "ft"
       << "fwy"
       << "fy"
       << "hway"
       << "hwy"
       << "la"
       << "pde"
       << "pd"
       << "pl"
       << "plz"
       << "rd"
       << "st"
       << "tce"
       << "Ala"
       << "Ariz"
       << "Ark"
       << "Cal"
       << "Calif"
       << "Col"
       << "Colo"
       << "Conn"
       << "Del"
       << "Fed"
       << "Fla"
       << "Ga"
       << "Ida"
       << "Id"
       << "Ill"
       << "Ind"
       << "Ia"
       << "Kan"
       << "Kans"
       << "Ken"
       << "Ky"
       << "La"
       << "Me"
       << "Md"
       << "Is"
       << "Mass"
       << "Mich"
       << "Minn"
       << "Miss"
       << "Mo"
       << "Mont"
       << "Neb"
       << "Nebr"
       << "Nev"
       << "Mex"
       << "Okla"
       << "Ok"
       << "Ore"
       << "Penna"
       << "Penn"
       << "Pa"
       << "Dak"
       << "Tenn"
       << "Tex"
       << "Ut"
       << "Vt"
       << "Va"
       << "Wash"
       << "Wis"
       << "Wisc"
       << "Wy"
       << "Wyo"
       << "USAFA"
       << "Alta"
       << "Man"
       << "Ont"
       << "Qué"
       << "Sask"
       << "Yuk"
       << "jan"
       << "feb"
       << "mar"
       << "apr"
       << "may"
       << "jun"
       << "jul"
       << "aug"
       << "sep"
       << "oct"
       << "nov"
       << "dec"
       << "sept"
       << "vs"
       << "etc"
       << "no"
       << "esp"
       << "eg"
       << "ie"
       << "1"
       << "2"
       << "3"
       << "4"
       << "5"
       << "6"
       << "7"
       << "8"
       << "9"
       << "10"
       << "11"
       << "12"
       << "avg"
       << "viz"
       << "m"
       << "mme";
}

LessonMiner::~LessonMiner() {}

void LessonMiner::doWork(const QString& fname) {
  // open the file
  QFile file(fname);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

  // parse the file into a list of lists. each QStringList is a paragraph,
  // each QString in the QStringList is a sentence.
  QList<QStringList> paragraphs;
  fileToParagraphs(&file, &paragraphs);

  // turn that list into lessons
  QStringList lessons;
  makeLessons(paragraphs, &lessons);

  // add the lessons to the database
  QFileInfo fi(fname);

  Database db;
  int id = db.getSource(fi.fileName());
  db.addTexts(id, lessons);

  // done
  emit resultReady();
}

void LessonMiner::fileToParagraphs(QFile* f, QList<QStringList>* paragraphs) {
  QStringList paragraph;
  QTextStream in(f);

  while (!in.atEnd()) {
    // QTextStream readline trims leading/trailing whitespace
    QString line = in.readLine();

    if (!line.isEmpty()) {
      // add the line to the current paragraph
      paragraph << line;
    } else if (paragraph.size() > 0) {
      // paragraph is done, dump it into the list.
      *paragraphs << sentenceSplitter(paragraph.join(" "));
      paragraph.clear();
    }
  }
  if (paragraph.size() > 0) {
    *paragraphs << sentenceSplitter(paragraph.join(" "));
  }
}

// Splits a QString representing a paragraph into a QStringList of its sentences
QStringList LessonMiner::sentenceSplitter(const QString& text) {
  QRegularExpression re(
      "(?:(?: |^)[^\\w. ]*(?P<pre>\\w+)"
      "[^ .]*\\.+|[?!]+)['\"]?(?= +(?:[^ a-z]|$))|$",
      QRegularExpression::UseUnicodePropertiesOption);

  int start = 0;
  QStringList list;

  QRegularExpressionMatchIterator i = re.globalMatch(text);
  while (i.hasNext()) {
    QRegularExpressionMatch match = i.next();
    // if there is a pre match, and it's an abbreviation, skip to
    // the next
    // match because it's not a valid sentence ending
    bool a = match.captured("pre").isNull();
    bool b = abbr.contains(match.captured("pre"), Qt::CaseInsensitive);
    if (!a && b) continue;

    // the position of the character at the end of the match
    int end = match.capturedEnd();
    // add substring from start to end, to the list
    list << QStringRef(&text, start, end - start).toString().trimmed();
    start = end;
  }

  return list;
}

// turn the split paragraphs/sentences into lessons
void LessonMiner::makeLessons(const QList<QStringList>& pgs,
                              QStringList* lessons) {
  lessons->clear();
  QStringList backlog;
  int backlen = 0;
  double i = 0.0;
  QList<QStringList>::const_iterator p;
  for (p = pgs.constBegin(); p != pgs.constEnd(); ++p) {
    if (backlog.size() > 0) backlog << QString("");
    QStringList::const_iterator s;
    for (s = (*p).constBegin(); s != (*p).constEnd(); ++s) {
      backlog << (*s);
      backlen += (*s).length();
      if (backlen >= min_chars) {
        (*lessons) << popFormat(&backlog);
        backlen = 0;
      }
    }
    i += 1.0;
    emit progress(static_cast<int>(100 * (i / pgs.size())));
  }
  if (backlen > 0) (*lessons) << popFormat(&backlog);
}

// joins the backlog list into a qstring, with \n between sentences of different
// paragraphs
QString LessonMiner::popFormat(QStringList* lst) {
  QStringList ret;
  QStringList p;

  while (lst->size() > 0) {
    QString s = lst->first();
    lst->pop_front();
    if (!s.isEmpty()) {
      p << s;
    } else {
      ret << p.join(" ");
      p.clear();
    }
  }
  if (p.size() > 0) ret << p.join(" ");

  return ret.join("\n");
}
