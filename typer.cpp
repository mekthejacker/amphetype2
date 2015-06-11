#include "typer.h"
#include "test.h"
#include "boost/date_time/posix_time/posix_time.hpp"

#include <iostream>

#include <QKeyEvent>
#include <QSettings>

Typer::Typer(QWidget* parent) : QTextEdit(parent)
{
        test = 0;

        setPalettes();

        connect(this, SIGNAL(textChanged()), this, SLOT(checkText()));
}

void Typer::setTextTarget(const QString& t)
{
        if (test != 0) 
                delete test;

        test = new Test(t);

        this->clear();
        this->setPalette(palettes.value("inactive"));
        getWaitText();
        this->selectAll();
}

void Typer::setPalettes()
{
        QSettings s("Amphetype2.ini", QSettings::IniFormat);

        palettes.insert("wrong",
                QPalette(Qt::black, Qt::lightGray, Qt::lightGray, Qt::darkGray,
                         Qt::gray, QColor(s.value("quiz_wrong_fg").toString()),
                         Qt::white, QColor(s.value("quiz_wrong_bg").toString()),
                         Qt::yellow));
        palettes.insert("right",
                QPalette(Qt::black, Qt::lightGray, Qt::lightGray, Qt::darkGray,
                         Qt::gray, QColor(s.value("quiz_right_fg").toString()),
                         Qt::yellow,QColor(s.value("quiz_right_bg").toString()),
                         Qt::yellow));
        palettes.insert("inactive",
                QPalette(Qt::black, Qt::lightGray, Qt::lightGray,
                         Qt::darkGray, Qt::gray, Qt::black,
                         Qt::lightGray));
}
void Typer::getWaitText()
{
        this->setText("Press SPACE and then immediately start typing the text\n"
                      "Press ESCAPE to restart with a new text at any time");
}

void Typer::checkText()
{
        QSettings s("Amphetype2.ini", QSettings::IniFormat);

        if (test->text == 0 || test->editFlag)
                return;

        // the text in this QTextEdit
        QString currentText = this->toPlainText();

        if (test->when[0].is_not_a_date_time()) {
                bool space = (currentText.length() > 0) &&
                             (currentText[currentText.length() - 1].isSpace());
                bool req = s.value("req_space").toBool();

                test->editFlag = true;
                if (space) {
                        test->when[0] = boost::posix_time::microsec_clock::local_time();
                        this->clear();
                        this->setPalette(this->palettes.value("right"));
                        emit testStarted(test->length);
                } else if (req) {
                        getWaitText();
                        this->selectAll();
                }

                test->editFlag = false;

                if (req || space)
                        return;
                else
                        test->when[0] = boost::posix_time::neg_infin;
        }

        int pos = std::min(currentText.length(), test->text.length());
        for (pos; pos > -1; pos--) {
                if (QStringRef(&currentText, 0, pos) ==
                    QStringRef(&test->text, 0, pos)) {
                        break;
                }
        }

        QStringRef lcd(&currentText,0,pos);

        test->currentPos = pos;

        if (test->when[pos].is_not_a_date_time() && pos == currentText.length()) {
                // store when we are at this position
                test->when[pos] = boost::posix_time::microsec_clock::local_time(); 
                if (pos > 0) {
                        // store time between keys
                        test->timeBetween[pos-1] = test->when[pos] - test->when[pos-1]; 
                        // time since the beginning
                        boost::posix_time::time_duration timeSinceStart = test->when[pos] - test->when[0];
                        //store wpm
                        test->wpm << 12.0 * (pos / (timeSinceStart.total_milliseconds() / 1000.0));

                        // check for new min/max wpm
                        if (test->wpm.last() > test->maxWPM)
                                test->maxWPM = test->wpm.last();
                        if (test->wpm.last() < test->minWPM)
                                test->minWPM = test->wpm.last();
                        emit newWPM(pos - 1, test->wpm.last());
                }
                if (pos > test->apmWindow) {
                        // time since 1 window ago
                        boost::posix_time::time_duration t = test->when[pos] - test->when[pos-test->apmWindow];
                        
                        test->apm << 12.0 * (test->apmWindow / (t.total_milliseconds() / 1000.0));
                        
                        // check for new min/max apm
                        if (test->apm.last() > test->maxAPM)
                                test->maxAPM = test->apm.last();
                        if (test->apm.last() < test->minAPM)
                                test->minAPM = test->apm.last();
                        emit newAPM(pos-1, test->apm.last());
                }
                
                int min = std::min(test->minWPM, test->minAPM);
                int max = std::max(test->maxWPM, test->maxAPM);
                emit characterAdded(max, min);

        }

        if (lcd == QStringRef(&test->text)) {
                emit done();
                return;
        }

        if (pos < currentText.length() && pos < test->text.length()) {
                //mistake
                test->mistake[pos] = true;
        }

        if (lcd == QStringRef(&currentText))
                this->setPalette(this->palettes.value("right"));
        else
                this->setPalette(this->palettes.value("wrong"));
}

void Typer::keyPressEvent(QKeyEvent* e)
{
        if (e->key() == Qt::Key_Escape)
                emit cancel();

        return QTextEdit::keyPressEvent(e);
}

Test* Typer::getTest() { return test; }