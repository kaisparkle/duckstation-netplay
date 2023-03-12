#pragma once

#include "ui_netplaysessiondialog.h"
#include "ui_mainwindow.h"
#include <QtWidgets\QDialog>
#include <qthost.h>

class NetplaySessionDialog : public QDialog
{
	Q_OBJECT

public:
  NetplaySessionDialog(QWidget* parent, EmuThread* emu, Ui::MainWindow* mainWindow, const char* discFilter);
  ~NetplaySessionDialog();

private:
  void reject();
  void onStartSession();
  void onStopSession();
  void onSelectingGame();
  void onMessageReceived(const QString& message);
  void onSendChatMessage();

  void SetSelectedGame(const QString& filename);

private:
  Ui::NetplaySessionDialog m_ui;
  QString m_selected_game_file;
  QString m_player_name;
  
  Ui::MainWindow* p_main_window = nullptr;
  EmuThread* p_emu = nullptr;
  const char* p_disc_filter;
};
