/* See the file "LICENSE.txt" for the full license governing this code. */

#include <QLoggingCategory>
#include <QApplication>
#include <QtGlobal>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // Set qDebug pattern.
    qSetMessagePattern("[%{time dd.MM.yyyy hh:mm:ss.zzz}] "
                       "[%{if-debug}DEBUG%{endif}%{if-info}INFO%{endif}"
                       "%{if-warning}WARN%{endif}%{if-critical}ERROR%{endif}"
                       "%{if-fatal}FATAL%{endif}] [%{function}] %{message}");

    // Set logging rules.
    QLoggingCategory::setFilterRules("sky::*=true");

    QApplication a(argc, argv);
    a.setStyle("fusion");

    MainWindow w;
    w.show();

    return a.exec();
}
