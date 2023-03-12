#include "netplaysessiondialog.h"
#include "common/log.h"
#include <QtCore/qdir.h>
#include <QtWidgets/qfiledialog.h>
#include <mainwindow.h>

Log_SetChannel(NetplaySessionDialog);

NetplaySessionDialog::NetplaySessionDialog(QWidget* parent, EmuThread* emu, Ui::MainWindow* main_window,
                                           const char* disk_filter)
  : QDialog(parent), p_emu(emu), p_main_window(main_window), p_disc_filter(disk_filter)
{
  m_ui.setupUi(this);
  // set some value ranges
  m_ui.sbLocalPort->setRange(0, 65535);
  m_ui.sbRemotePort->setRange(0, 65535);
  m_ui.sbInputDelay->setRange(0, 30);
  m_ui.leRemoteAddr->setMaxLength(15);
  m_ui.lePlayerName->setMaxLength(12);
  QString IpRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
  QRegularExpression IpRegex("^" + IpRange + "(\\." + IpRange + ")" + "(\\." + IpRange + ")" + "(\\." + IpRange + ")$");
  QRegularExpressionValidator* ipValidator = new QRegularExpressionValidator(IpRegex, this);
  m_ui.leRemoteAddr->setValidator(ipValidator);
  // only allow one netplay session at a time
  p_main_window->menuNetplay->setDisabled(true);
  // you start without a session so you cant close it.
  m_ui.btnStopSession->setDisabled(true);
  // cant start a session without selecting a game
  m_ui.btnStartSession->setDisabled(true);
  // cant chat without an active session
  m_ui.btnSendChatMessage->setDisabled(true);
  // ChatView settings
  m_ui.lvChatWindow->setWordWrap(true);
  // fill options
  m_ui.cbLocalPlayer->addItem("Player 1");
  m_ui.cbLocalPlayer->addItem("Player 2");
  // add session events
  connect(m_ui.btnStartSession, &QPushButton::released, this, &NetplaySessionDialog::onStartSession);
  connect(m_ui.btnStopSession, &QPushButton::released, this, &NetplaySessionDialog::onStopSession);
  connect(m_ui.tbChooseGame, &QToolButton::released, this, &NetplaySessionDialog::onSelectingGame);
  connect(m_ui.btnSendChatMessage, &QPushButton::released, this, &NetplaySessionDialog::onSendChatMessage);
  connect(p_emu, &EmuThread::onNetplayMessage, this, &NetplaySessionDialog::onMessageReceived);
}

NetplaySessionDialog::~NetplaySessionDialog()
{
  p_emu->closeNetplaySession();
}

void NetplaySessionDialog::reject()
{
  // close netplay
  p_emu->closeNetplaySession();
  p_main_window->menuNetplay->setEnabled(true);
  accept();
}

void NetplaySessionDialog::onStartSession()
{
  // get session info
  m_player_name = m_ui.lePlayerName->text();
  int localHandle = m_ui.cbLocalPlayer->currentIndex() + 1;
  int localPort = m_ui.sbLocalPort->value();
  auto remoteAddr = m_ui.leRemoteAddr->text();
  int remotePort = m_ui.sbRemotePort->value();
  int inputDelay = m_ui.sbInputDelay->value();
  Log_InfoPrintf("LocalHandle: %d, LocalPort: %d, RemoteAddr: %s, RemotePort: %d, LocalInputDelay: %d", localHandle,
                 localPort, qPrintable(remoteAddr), remotePort, inputDelay);
  // set button states
  m_ui.btnStartSession->setDisabled(true);
  m_ui.btnSendChatMessage->setDisabled(false);
  m_ui.btnStopSession->setDisabled(false);
  m_ui.lePlayerName->setDisabled(true);
  m_ui.cbLocalPlayer->setDisabled(true);
  m_ui.leRemoteAddr->setVisible(false);
  // start netplay
  p_emu->startNetplaySession(localHandle, localPort, remoteAddr, remotePort, inputDelay, m_selected_game_file);
  // make values invisible
}

void NetplaySessionDialog::onStopSession()
{
  // set button states
  m_ui.btnStartSession->setDisabled(false);
  m_ui.btnSendChatMessage->setDisabled(true);
  m_ui.btnStopSession->setDisabled(true);
  m_ui.cbLocalPlayer->setDisabled(false);
  m_ui.leRemoteAddr->setVisible(true);
  // close netplay
  p_emu->closeNetplaySession();
}

void NetplaySessionDialog::onSelectingGame()
{
  QString filename = QDir::toNativeSeparators(
    QFileDialog::getOpenFileName(this, tr("Select Disc Image"), QString(), tr(p_disc_filter), nullptr));
  if (filename.isEmpty())
    return;

  SetSelectedGame(filename);
}

void NetplaySessionDialog::SetSelectedGame(const QString& filename)
{
  m_selected_game_file = filename;
  Log_InfoPrintf("Path: %s", qPrintable(m_selected_game_file));
  // get game name
  // remove path
  auto splitFile = m_selected_game_file.split("\\");
  splitFile = splitFile.value(splitFile.length() - 1).split(".");
  // remove file extension
  splitFile.pop_back();
  // join them back toghether
  auto game = splitFile.join(".");
  // update ui
  m_ui.lblSelectedGame->setText(game);
  m_ui.btnStartSession->setDisabled(false);
}

void NetplaySessionDialog::onMessageReceived(const QString& message)
{
  m_ui.lvChatWindow->addItem(message.trimmed());
}

void NetplaySessionDialog::onSendChatMessage()
{
  // check if message aint empty and the complete message ( message + name + ":" + space) is below 120 characters
  auto msg = m_ui.pteChatbox->toPlainText().trimmed();
  QString completeMsg = m_player_name + ": " + msg;
  if (completeMsg.length() > 120)
    return;
  // add message to list view
  m_ui.lvChatWindow->addItem(completeMsg);
  // clean the chatbox
  m_ui.pteChatbox->clear();
  // send message to other player
  p_emu->sendNetplayMessage(completeMsg);
}
