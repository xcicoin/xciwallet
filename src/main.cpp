// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2016 Karbowanec developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <QApplication>
#include <QCommandLineParser>
#include <QLocale>
#include <QTranslator>
#include <QLockFile>
#include <QMessageBox>
#include <QSplashScreen>
#include <QStyleFactory>

#include "CommandLineParser.h"
#include "CurrencyAdapter.h"
#include "LoggerAdapter.h"
#include "NodeAdapter.h"
#include "Settings.h"
#include "SignalHandler.h"
#include "WalletAdapter.h"

#include "gui/MainWindow.h"



#include <QTextCodec>

#define DEBUG 1

using namespace WalletGui;

int main(int argc, char* argv[]) {

  QApplication app(argc, argv);
  app.setApplicationName(CurrencyAdapter::instance().getCurrencyName() + "wallet");
  app.setApplicationVersion(Settings::instance().getVersion());
  app.setQuitOnLastWindowClosed(false);

#ifndef Q_OS_MAC
  QApplication::setStyle(QStyleFactory::create("Fusion"));
#endif

  CommandLineParser cmdLineParser(nullptr);
  Settings::instance().setCommandLineParser(&cmdLineParser);
  bool cmdLineParseResult = cmdLineParser.process(app.arguments());
  Settings::instance().load();

  QTranslator translator;
  QTranslator translatorQt;

  QString lng = Settings::instance().getLanguage();

  if(!lng.isEmpty()) {
      translator.load(":/languages/" + lng + ".qm");
      translatorQt.load(":/languages/qt_" + lng + ".qm");
    } else {
      translator.load(":/languages/" + QLocale::system().name());
      translatorQt.load(":/languages/qt_" +  QLocale::system().name());
  }
  app.installTranslator(&translator);
  app.installTranslator(&translatorQt);

  QLocale::setDefault(QLocale::c());

  setlocale(LC_ALL, "");

  QFile File(":/skin/default.qss");
  File.open(QFile::ReadOnly);
  QString StyleSheet = QLatin1String(File.readAll());
  qApp->setStyleSheet(StyleSheet);


#ifdef Q_OS_WIN
  if(!cmdLineParseResult) {
    QMessageBox::critical(nullptr, QObject::tr("Error"), cmdLineParser.getErrorText());
    return app.exec();
  } else if (cmdLineParser.hasHelpOption()) {
    QMessageBox::information(nullptr, QObject::tr("Help"), cmdLineParser.getHelpText());
    return app.exec();
  }
#endif

  LoggerAdapter::instance().init();

  QString dataDirPath = Settings::instance().getDataDir().absolutePath();

  if (!QDir().exists(dataDirPath)) {
    QDir().mkpath(dataDirPath);
  }

  QLockFile lockFile(Settings::instance().getDataDir().absoluteFilePath(QApplication::applicationName() + ".lock"));
  if (!lockFile.tryLock()) {
    QMessageBox::warning(nullptr, QObject::tr("Fail"), QObject::tr("%1 wallet already running").arg(CurrencyAdapter::instance().getCurrencyDisplayName()));
    return 0;
  }

  SignalHandler::instance().init();
  QObject::connect(&SignalHandler::instance(), &SignalHandler::quitSignal, &app, &QApplication::quit);

  QSplashScreen* splash = new QSplashScreen(QPixmap(":images/splash"), Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
  if (!splash->isVisible()) {
    splash->show();
  }

  splash->showMessage(QObject::tr("Loading blockchain..."), Qt::AlignLeft | Qt::AlignBottom, Qt::white);
  app.processEvents();
  qRegisterMetaType<CryptoNote::TransactionId>("CryptoNote::TransactionId");
  qRegisterMetaType<quintptr>("quintptr");
  if (!NodeAdapter::instance().init()) {
    return 0;
  }

  splash->finish(&MainWindow::instance());
  MainWindow::instance().show();
  WalletAdapter::instance().open("");
  QObject::connect(QApplication::instance(), &QApplication::aboutToQuit, []() {
    MainWindow::instance().quit();
    if (WalletAdapter::instance().isOpen()) {
      WalletAdapter::instance().close();
    }

    NodeAdapter::instance().deinit();
  });

  return app.exec();
}
