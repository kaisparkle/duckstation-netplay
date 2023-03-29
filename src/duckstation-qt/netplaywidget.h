#ifndef NETPLAYWIDGET_H
#define NETPLAYWIDGET_H

#include <QtNetwork/QUdpSocket>
#include <QtWidgets/QDialog>
#include <frontend-common/game_list.h>

struct TraversalConfig
{
  quint16 local_port;
  int room_size = 2; // by default there are always atleast 2 players.
  std::vector<std::string> user_info;
};

namespace Ui {
class NetplayWidget;
}

class NetplayWidget : public QDialog
{
  Q_OBJECT

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

  void OpenTraversalSocket();
  void HandleTraversalExchange();

private:
  bool m_socket_loop_close = true; // closed by default
  QUdpSocket* m_socket;
  Ui::NetplayWidget* m_ui;
  std::vector<std::string> m_available_games;
  TraversalConfig m_traversal_conf;
};

#endif // NETPLAYWIDGET_H
