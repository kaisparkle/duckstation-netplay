#ifndef NETPLAYWIDGET_H
#define NETPLAYWIDGET_H

#include <QtNetwork/QUdpSocket>
#include <QtWidgets/QDialog>
#include <QtCore/QThread>
#include <frontend-common/game_list.h>

struct TraversalConfig
{
  QString opening_msg; // opening message. first message sent to the server.
  int local_handle;
  quint16 local_port;
  int room_size = 2; // by default there are always atleast 2 players.
  std::vector<QString> remote_user_info;
};

class TraversalThread : public QObject
{
  Q_OBJECT

public:
  ~TraversalThread();

  void Init(TraversalConfig* config);
  void Activate()
  {
    OpenTraversalSocket();
    HandleTraversalExchange();
  }

signals:
  void resultFailed();
  void resultReady();
  void onMsg(const QString& msg);
  void setHostCode(const QString& code);

private:
  void OpenTraversalSocket();
  void HandleTraversalExchange();

private:
  QUdpSocket* m_socket;
  TraversalConfig* m_traversal_conf;
};

namespace Ui {
class NetplayWidget;
}

class NetplayWidget : public QDialog
{
  Q_OBJECT
  QThread worker_thread;
public:
  explicit NetplayWidget(QWidget* parent = nullptr);
  ~NetplayWidget();

private:
  void FillGameList();
  void SetupConnections();
  void SetupConstraints();
  bool CheckInfoValid(bool direct_ip);
  bool CheckControllersSet();
  bool StartSession(bool direct_ip);
  void StopSession();
  void OnMsgReceived(const QString& msg);
  void SetHostCode(const QString& code);
  void StartSessionTraversal();
  void OpenTraversalSocket();

private:
  Ui::NetplayWidget* m_ui;
  std::vector<std::string> m_available_games;
  TraversalConfig m_traversal_conf;
};

#endif // NETPLAYWIDGET_H
