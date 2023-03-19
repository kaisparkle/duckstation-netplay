#include "netplaywidget.h"
#include "ui_netplaywidget.h"
#include <QtWidgets/qmessagebox.h>
#include <common/log.h>
#include <core/controller.h>

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
  delete m_ui;
}

void NetplayWidget::FillGameList()
{
  // Get all games and fill the list later to know which game to boot.
  s32 numGames = GameList::GetEntryCount();
  for (s32 i = 0; i < numGames; i++)
  {
    const auto& entry = GameList::GetEntryByIndex(i);
    m_ui->cbSelectedGame->addItem(QString::fromStdString("[" + entry->serial + "] " + entry->title));
    m_available_games.push_back(entry);
  }
}

void NetplayWidget::SetupConnections()
{
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

  // check session when start button pressed if there is the needed info depending on the connection mode
  auto fnCheckValid = [this]() {
    const bool action = (m_ui->cbConnMode->currentIndex() == 0 ? true : false);
    if (CheckInfoValid(action))
    {
      m_ui->btnSendMsg->setEnabled(true);
      m_ui->tbNetplayChat->setEnabled(true);
      m_ui->btnStopSession->setEnabled(true);
      m_ui->btnStartSession->setEnabled(false);
      m_ui->btnTraversalHost->setEnabled(false);
      m_ui->btnTraversalJoin->setEnabled(false);
      StartSession(action);
    }
  };
  connect(m_ui->btnStartSession, &QPushButton::pressed, fnCheckValid);
  connect(m_ui->btnTraversalJoin, &QPushButton::pressed, fnCheckValid);
  connect(m_ui->btnTraversalHost, &QPushButton::pressed, fnCheckValid);

  // when pressed revert back to the previous ui state so people can start a new session.
  connect(m_ui->btnStopSession, &QPushButton::pressed, [this]() {
    m_ui->btnSendMsg->setEnabled(false);
    m_ui->tbNetplayChat->setEnabled(false);
    m_ui->btnStopSession->setEnabled(false);
    m_ui->btnStartSession->setEnabled(true);
    m_ui->btnTraversalHost->setEnabled(true);
    m_ui->btnTraversalJoin->setEnabled(true);
    m_ui->lblHostCodeResult->setText("XXXXXXXXX-");
    StopSession();
  });
}

void NetplayWidget::SetupConstraints()
{
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

void NetplayWidget::StartSession(bool direct_ip)
{
  Log_InfoPrint("Start Session!");
}

void NetplayWidget::StopSession()
{
  Log_InfoPrint("Stop Session!");
}
