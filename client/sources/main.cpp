#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QFont>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QApplication::setApplicationName("月夜议会");
    QApplication::setOrganizationName("WolfGame");
    a.setStyle("Fusion");

    QFont appFont;
    appFont.setFamilies({"Noto Sans CJK SC", "Microsoft YaHei UI", "sans-serif"});
    appFont.setPointSize(10);
    a.setFont(appFont);

    QFile themeFile(":/styles/mobile_theme.qss");
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        a.setStyleSheet(QString::fromUtf8(themeFile.readAll()));
    }

    MainWindow w;
#ifdef Q_OS_ANDROID
    w.showMaximized();
#else
    w.resize(430, 860);
    w.show();
#endif
    return a.exec();
}
