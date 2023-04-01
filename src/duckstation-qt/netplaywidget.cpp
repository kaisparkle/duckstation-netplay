#include "netplaywidget.h"
#include "ui_netplaywidget.h"
#include <QtGui/QClipboard>
#include <QtNetwork/QNetworkDatagram>
#include <QtWidgets/qmessagebox.h>
#include <common/log.h>
#include <core/controller.h>
#include <qthost.h>

Log_SetChannel(NetplayWidget);

NetplayWidget::NetplayWidget(QWidget* parent) : QDialog(parent), m_ui(new Ui::NetplayWidget)
{
  m_ui->setupUi(this);
  FillGameList();
  SetupConnections();
  SetupConstraints();
  CheckControllersSet();
}

NetplayWidget::~NetplayWidget()
{
  StopSession();
  delete m_ui;
}

void NetplayWidget::FillGameList()
{
  // Get all games and fill the list later to know which game to boot.
  s32 numGames = GameList::GetEntryCount();
  for (s32 i = 0; i < numGames; i++)
  {
    const auto& entry = GameList::GetEntryByIndex(i);
    std::string baseFilename = entry->path.substr(entry->path.find_last_of("/\\") + 1);
    m_ui->cbSelectedGame->addItem(
      QString::fromStdString("[" + entry->serial + "] " + entry->title + " | " + baseFilename));
    m_available_games.push_back(entry->path);
  }
}

void NetplayWidget::SetupConnections()
{
  // connect netplay window messages
  connect(g_emu_thread, &EmuThread::onNetplayMessage, this, &NetplayWidget::OnMsgReceived);
  // connect sending messages when the chat button has been pressed
  connect(m_ui->btnSendMsg, &QPushButton::pressed, [this]() {
    // check if message aint empty and the complete message ( message + name + ":" + space) is below 120 characters
    auto msg = m_ui->tbNetplayChat->toPlainText().trimmed();
    QString completeMsg = m_ui->lePlayerName->text().trimmed() + ": " + msg;
    if (completeMsg.length() > 120)
      return;
    m_ui->lwChatWindow->addItem(completeMsg);
    m_ui->tbNetplayChat->clear();
    if (!g_emu_thread)
      return;
    g_emu_thread->sendNetplayMessage(completeMsg);
  });

  // switch between DIRECT IP and traversal options
  connect(m_ui->cbConnMode, &QComboBox::currentIndexChanged, [this]() {
    // zero is DIRECT IP mode
    const bool action = (m_ui->cbConnMode->currentIndex() == 0 ? true : false);
    m_ui->frDirectIP->setVisible(action);
    m_ui->frDirectIP->setEnabled(action);
    m_ui->btnStartSession->setEnabled(action);
    m_ui->tabTraversal->setEnabled(!action);
    m_ui->btnTraversalJoin->setEnabled(!action);
    m_ui->btnTraversalHost->setEnabled(!action);
  });

  // actions to be taken when stopping a session.
  auto fnOnStopSession = [this]() {
    m_ui->cbConnMode->setEnabled(true);
    m_ui->btnSendMsg->setEnabled(false);
    m_ui->tbNetplayChat->setEnabled(false);
    m_ui->btnStopSession->setEnabled(false);
    m_ui->btnStartSession->setEnabled(true);
    m_ui->btnTraversalHost->setEnabled(true);
    m_ui->btnTraversalJoin->setEnabled(true);
    m_ui->lblHostCodeResult->setText("XXXXXXXXX-");
    StopSession();
  };

  // check session when start button pressed if there is the needed info depending on the connection mode
  auto fnCheckValid = [this, fnOnStopSession]() {
    const bool action = (m_ui->cbConnMode->currentIndex() == 0 ? true : false);
    if (CheckInfoValid(action))
    {
      m_ui->cbConnMode->setEnabled(false);
      m_ui->btnSendMsg->setEnabled(true);
      m_ui->tbNetplayChat->setEnabled(true);
      m_ui->btnStopSession->setEnabled(true);
      m_ui->btnStartSession->setEnabled(false);
      m_ui->btnTraversalHost->setEnabled(false);
      m_ui->btnTraversalJoin->setEnabled(false);
      if (!StartSession(action))
        fnOnStopSession();
    }
  };
  connect(m_ui->btnStartSession, &QPushButton::pressed, fnCheckValid);
  connect(m_ui->btnTraversalJoin, &QPushButton::pressed, fnCheckValid);
  connect(m_ui->btnTraversalHost, &QPushButton::pressed, fnCheckValid);
  // when pressed revert back to the previous ui state so people can start a new session.
  connect(m_ui->btnStopSession, &QPushButton::pressed, fnOnStopSession);
  // used to copy the room code to the clipboard
  connect(m_ui->btnCopyHostCode, &QPushButton::pressed, [this] {
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(m_ui->lblHostCodeResult->text());
  });
}

void NetplayWidget::SetupConstraints()
{
  m_ui->lwChatWindow->setWordWrap(true);
  m_ui->sbLocalPort->setRange(0, 65535);
  m_ui->sbRemotePort->setRange(0, 65535);
  m_ui->sbInputDelay->setRange(0, 10);
  m_ui->leRemoteAddr->setMaxLength(15);
  m_ui->lePlayerName->setMaxLength(12);
  QString IpRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
  QRegularExpression IpRegex("^" + IpRange + "(\\." + IpRange + ")" + "(\\." + IpRange + ")" + "(\\." + IpRange + ")$");
  QRegularExpressionValidator* ipValidator = new QRegularExpressionValidator(IpRegex, this);
  m_ui->leRemoteAddr->setValidator(ipValidator);
}

bool NetplayWidget::CheckInfoValid(bool direct_ip)
{
  if (direct_ip && m_ui->cbLocalPlayer->currentIndex() == 3)
  {
    QMessageBox errBox;
    errBox.setFixedSize(500, 200);
    errBox.information(this, "Netplay Session", "Spectators are currently only supported in Traversal mode!");
    errBox.show();
    return false;
  }

  bool err = false;
  // check nickname, game selected and player selected.
  if (m_ui->lePlayerName->text().trimmed().isEmpty() || m_ui->cbSelectedGame->currentIndex() == 0 ||
      m_ui->cbLocalPlayer->currentIndex() == 0)
    err = true;
  // check if direct ip details have been filled in
  if (direct_ip && (m_ui->leRemoteAddr->text().trimmed().isEmpty() || m_ui->sbRemotePort->value() == 0 ||
                    m_ui->sbLocalPort->value() == 0))
    err = true;
  // check if host code has been filled in
  if (!direct_ip && m_ui->leHostCode->text().trimmed().isEmpty() &&
      m_ui->tabTraversal->currentWidget() == m_ui->tabJoin)
    err = true;
  // check if host code has been filled in
  if (!direct_ip && m_ui->leHostCode->text().trimmed().isEmpty() &&
      m_ui->tabTraversal->currentWidget() == m_ui->tabJoin)
    err = true;
  // if an err has been found throw
  if (err)
  {
    QMessageBox errBox;
    errBox.setFixedSize(500, 200);
    errBox.information(this, "Netplay Session", "Please fill in all the needed fields!");
    errBox.show();
    return !err;
  }
  // check if controllers are set
  err = !CheckControllersSet();
  // everything filled in. inverse cuz we would like to return true if the info is valid.
  return !err;
}

bool NetplayWidget::CheckControllersSet()
{
  bool err = false;
  // check whether its controllers are set right
  for (u32 i = 0; i < 2; i++)
  {
    const Controller::ControllerInfo* cinfo = Controller::GetControllerInfo(g_settings.controller_types[i]);
    if (!cinfo || cinfo->type != ControllerType::DigitalController)
      err = true;
  }
  // if an err has been found throw popup
  if (err)
  {
    QMessageBox errBox;
    errBox.information(this, "Netplay Session",
                       "Please make sure the controllers are both enabled and set as Digital Controllers");
    errBox.setFixedSize(500, 200);
    errBox.show();
  }
  // controllers are set right
  return !err;
}

bool NetplayWidget::StartSession(bool direct_ip)
{
  if (!g_emu_thread)
    return false;

  if (!direct_ip)
  {
    OpenTraversalSocket();
    return true;
  }

  int localHandle = m_ui->cbLocalPlayer->currentIndex();
  int inputDelay = m_ui->sbInputDelay->value();
  quint16 localPort = m_ui->sbLocalPort->value();
  const QString& remoteAddr = m_ui->leRemoteAddr->text();
  quint16 remotePort = m_ui->sbRemotePort->value();
  const QString& gamePath = QString::fromStdString(m_available_games[m_ui->cbSelectedGame->currentIndex() - 1]);

  g_emu_thread->startNetplaySession(localHandle, localPort, remoteAddr, remotePort, inputDelay, gamePath);
  return true;
}

void NetplayWidget::StartSessionTraversal()
{
  worker_thread.exit();
  // Log_InfoPrint("Setup Traversal Connections!");
  std::vector<std::string> addresses, nicknames;
  std::vector<quint16> ports, handles;
  // add your local information
  addresses.push_back("localhost");
  nicknames.push_back(m_ui->lePlayerName->text().trimmed().toStdString());
  ports.push_back(m_traversal_conf.local_port);
  handles.push_back(m_traversal_conf.local_handle);
  int inputDelay = m_ui->sbInputDelay->value();
  const QString& gamePath = QString::fromStdString(m_available_games[m_ui->cbSelectedGame->currentIndex() - 1]);
  // add remote information
  for (auto const& remote : m_traversal_conf.remote_user_info)
  {
    // addr[0] = ip, addr[1] = port
    // usr[0] = nickname, usr[1] = remote handle
    auto res = remote.split("&#");
    auto addr = res[0].split(":");
    auto usr = res[1].split("~$");
    Log_InfoPrintf("%s : %s", addr[0].toStdString().c_str(), addr[1].toStdString().c_str());
    Log_InfoPrintf("%s : %s", usr[0].toStdString().c_str(), usr[1].toStdString().c_str());
    addresses.push_back(addr[0].toStdString());
    nicknames.push_back(usr[0].toStdString());
    ports.push_back(addr[1].toInt());
    handles.push_back(usr[1].toInt());
  }
  g_emu_thread->startNetplaySessionTraversal(handles, addresses, ports, nicknames, inputDelay, gamePath);
}

void NetplayWidget::StopSession()
{
  if (!g_emu_thread)
    return;

  if (worker_thread.isRunning())
  {
    worker_thread.quit();
    worker_thread.wait();
  }

  g_emu_thread->stopNetplaySession();
}

void NetplayWidget::OnMsgReceived(const QString& msg)
{
  m_ui->lwChatWindow->addItem(msg);
}

void NetplayWidget::SetHostCode(const QString& code)
{
  m_ui->lblHostCodeResult->setText(code);
}

void NetplayWidget::OpenTraversalSocket()
{
  auto localPlayer = QString::number(m_ui->cbLocalPlayer->currentIndex());
  auto localName = m_ui->lePlayerName->text().trimmed();

  QString message = "";
  QString playerInfo = localName + "~$" + localPlayer;

  if (m_ui->tabTraversal->currentWidget() == m_ui->tabHost)
  {
    // there will always be atleast 2 players (for now) without counting spectators.
    int total = m_ui->cbSpectatorCount->currentIndex() + 2;
    auto numPlayers = QString::number(total);
    message = "&!CR&#" + numPlayers + "&#" + playerInfo;
  }
  else
  {
    auto code = m_ui->leHostCode->text().trimmed();
    message = "&!JR&#" + code + "&#" + playerInfo;
    OnMsgReceived("Joining room: " + code);
  }

  m_traversal_conf.opening_msg = message;
  m_traversal_conf.local_handle = localPlayer.toInt();

  TraversalThread* tThread = new TraversalThread();

  tThread->Init(&m_traversal_conf);
  tThread->moveToThread(&worker_thread);

  connect(tThread, &TraversalThread::resultReady, this, &NetplayWidget::StartSessionTraversal);
  connect(&worker_thread, &QThread::started, tThread, &TraversalThread::Activate);
  connect(&worker_thread, &QThread::finished, tThread, &QObject::deleteLater);
  connect(tThread, &TraversalThread::onMsg, this, &NetplayWidget::OnMsgReceived);
  connect(tThread, &TraversalThread::setHostCode, this, &NetplayWidget::SetHostCode);
  connect(tThread, &TraversalThread::resultFailed, [this] {
    OnMsgReceived("Failed to Exchange information!\nPlease try again!");
    worker_thread.quit();
    worker_thread.wait();
  });

  worker_thread.start();
}

TraversalThread::~TraversalThread()
{
  m_socket->deleteLater();
}

void TraversalThread::Init(TraversalConfig* config)
{
  m_traversal_conf = config;
}

void TraversalThread::OpenTraversalSocket()
{
  m_socket = new QUdpSocket(this);
  m_socket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
  m_traversal_conf->local_port = m_socket->localPort();
  m_socket->writeDatagram(m_traversal_conf->opening_msg.toUtf8(), QHostAddress::LocalHost, 4420);
}

void TraversalThread::HandleTraversalExchange()
{
  while (true)
  {
    QThread::msleep(200);
    if (m_socket->hasPendingDatagrams())
    {
      auto info = m_socket->receiveDatagram();
      auto data = info.data().toStdString();
      if (data.compare("&!RC") == 0)
      {
        emit onMsg("Room has been closed!");
        break;
      }
      else if (data.substr(0, 4).compare("&!RJ") == 0)
      {
        std::string num = data.substr(6, data.length() - 5);
        m_traversal_conf->room_size = std::stoi(num);
        emit onMsg("Room joined! size: " + QString::fromStdString(num));
      }
      else if (data.compare("&!NRF") == 0)
      {
        emit onMsg("No room found!");
        break;
      }
      else if (data.compare("&!RF") == 0)
      {
        emit onMsg("Room full!, couldn't join.");
        break;
      }
      else if (data.substr(0, 5).compare("&!RCR") == 0)
      {
        emit onMsg("Room code generated!");
        emit setHostCode(QString::fromUtf8(data.substr(7, data.length() - 6)));
      }
      else if (data.substr(0, 5).compare("&!RIE") == 0)
      {
        m_traversal_conf->remote_user_info.push_back(info.data().remove(0, 7));
        emit onMsg("Exchanging information!");
        // add one cuz you gotta include yourself.
        if (m_traversal_conf->room_size == m_traversal_conf->remote_user_info.size() + 1)
        {
          emit onMsg("Info exchange complete!");
          emit onMsg("Room has been closed!");
          emit resultReady();

          m_socket->close();
          return;
        }
      }
    }
  }
  // it exited the loop but didnt return early meaning it didnt complete info exchange.
  emit resultFailed();
  m_socket->close();
}
