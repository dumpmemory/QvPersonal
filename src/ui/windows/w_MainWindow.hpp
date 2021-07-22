#pragma once
#include "ConnectionModelHelper/ConnectionModelHelper.hpp"
#include "QvPlugin/Gui/QvGUIPluginInterface.hpp"
#include "LogHighlighter/LogHighlighter.hpp"
#include "MessageBus/MessageBus.hpp"
#include "SpeedWidget/SpeedWidget.hpp"
#include "ui/WidgetUIBase.hpp"
#include "ui/widgets/ConnectionInfoWidget.hpp"
#include "ui/widgets/ConnectionItemWidget.hpp"
#include "ui_w_MainWindow.h"

#include <QMainWindow>
#include <QMenu>
#include <QSystemTrayIcon>

namespace Qv2rayPlugin
{
    class PluginMainWindowWidget;
}

class MainWindow
    : public QMainWindow
    , Ui::MainWindow
{
    Q_OBJECT
  public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void ProcessCommand(QString command, QStringList commands, QMap<QString, QString> args);

  signals:
    void StartConnection() const;
    void StopConnection() const;
    void RestartConnection() const;

  private:
    QvMessageBusSlotDecl;
  private slots:
    void on_activatedTray(QSystemTrayIcon::ActivationReason reason);
    void on_preferencesBtn_clicked();
    void on_clearlogButton_clicked();
    void on_connectionTreeView_customContextMenuRequested(QPoint pos);
    void on_importConfigButton_clicked();
    void on_subsButton_clicked();
    //
    void on_connectionFilterTxt_textEdited(const QString &arg1);
    void on_locateBtn_clicked();
    //
    void on_chartVisibilityBtn_clicked();
    void on_logVisibilityBtn_clicked();
    void on_clearChartBtn_clicked();
    void on_masterLogBrowser_textChanged();
    //
    void on_pluginsBtn_clicked();
    void on_collapseGroupsBtn_clicked();
    void on_newConnectionBtn_clicked();
    void on_newComplexConnectionBtn_clicked();
    //
    void on_connectionTreeView_doubleClicked(const QModelIndex &index);
    void on_connectionTreeView_clicked(const QModelIndex &index);

    void on_aboutBtn_clicked();

  private:
    // Do not declare as slots, we connect them manually.
    void Action_Exit();
    void Action_Start();
    void Action_SetAutoConnection();
    void Action_Edit();
    void Action_EditJson();
    void Action_EditComplex();
    void Action_UpdateSubscription();
    void Action_TestLatency();
    //    void Action_TestRealLatency();
    void Action_RenameConnection();
    void Action_DeleteConnections();
    void Action_DuplicateConnection();
    void Action_ResetStats();
    void Action_CopyGraphAsImage();
    void Action_CopyRecentLogs();

  private:
    void MWToggleVisibility();
    void OnEditRequested(const ConnectionId &id);
    void OnEditJsonRequested(const ConnectionId &id);
    void OnConnected(const ProfileId &id);
    void OnDisconnected(const ProfileId &id);
    //
    void OnStatsAvailable(const ProfileId &id, const StatisticsObject &speed);
    void OnKernelLogAvailable(const ProfileId &id, const QString &log);
    //
    void SortConnectionList(ConnectionInfoRole byCol, bool asending);
    //
    void ReloadRecentConnectionList();
    //
    void OnLogScrollbarValueChanged(int value);
    //
    void updateActionTranslations();
    void OnPluginButtonClicked();

  protected:
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;
    void closeEvent(QCloseEvent *) override;
    void changeEvent(QEvent *e) override;

  public:
    void MWShowWindow();
    void MWHideWindow();

  private:
    // Charts
    SpeedWidget *speedChartWidget;
    LogHighlighter::LogHighlighter *vCoreLogHighlighter;
    ConnectionInfoWidget *infoWidget;
    //
    // Declare Actions
#define DECL_ACTION(parent, name) QAction *name = new QAction(parent)
    QMenu *tray_RootMenu = new QMenu(this);
    QMenu *tray_RecentConnectionsMenu = new QMenu(this);
    QMenu *sortMenu = new QMenu(this);
    QMenu *logRCM_Menu = new QMenu(this);
    QMenu *connectionListRCM_Menu = new QMenu(this);
    QMenu *graphWidgetMenu = new QMenu(this);
    // Do not set parent=tray_RecentConnectionsMenu
    // Calling clear() will cause this QAction being deleted.
    DECL_ACTION(this, tray_ClearRecentConnectionsAction);
    DECL_ACTION(tray_RootMenu, tray_action_ToggleVisibility);
    DECL_ACTION(tray_RootMenu, tray_action_Preferences);
    DECL_ACTION(tray_RootMenu, tray_action_Quit);
    DECL_ACTION(tray_RootMenu, tray_action_Start);
    DECL_ACTION(tray_RootMenu, tray_action_Restart);
    DECL_ACTION(tray_RootMenu, tray_action_Stop);
    DECL_ACTION(connectionListRCM_Menu, action_RCM_Start);
    DECL_ACTION(connectionListRCM_Menu, action_RCM_SetAutoConnection);
    DECL_ACTION(connectionListRCM_Menu, action_RCM_UpdateSubscription);
    DECL_ACTION(connectionListRCM_Menu, action_RCM_Edit);
    DECL_ACTION(connectionListRCM_Menu, action_RCM_EditJson);
    DECL_ACTION(connectionListRCM_Menu, action_RCM_EditComplex);
    DECL_ACTION(connectionListRCM_Menu, action_RCM_RenameConnection);
    DECL_ACTION(connectionListRCM_Menu, action_RCM_DuplicateConnection);
    DECL_ACTION(connectionListRCM_Menu, action_RCM_TestLatency);
    DECL_ACTION(connectionListRCM_Menu, action_RCM_ResetStats);
    DECL_ACTION(connectionListRCM_Menu, action_RCM_DeleteConnection);
    DECL_ACTION(sortMenu, sortAction_SortByName_Asc);
    DECL_ACTION(sortMenu, sortAction_SortByName_Dsc);
    DECL_ACTION(sortMenu, sortAction_SortByPing_Asc);
    DECL_ACTION(sortMenu, sortAction_SortByPing_Dsc);
    DECL_ACTION(sortMenu, sortAction_SortByData_Asc);
    DECL_ACTION(sortMenu, sortAction_SortByData_Dsc);
    DECL_ACTION(graphWidgetMenu, action_RCM_CopyGraph);
    DECL_ACTION(logRCM_Menu, action_RCM_CopySelected);
    DECL_ACTION(logRCM_Menu, action_RCM_CopyRecentLogs);
#undef DECL_ACTION

    void CheckSubscriptionsUpdate();
    bool StartAutoConnectionEntry();
    void updateColorScheme();

    bool qvLogAutoScoll = true;
    ProfileId lastConnected;

    QList<Qv2rayPlugin::Gui::PluginMainWindowWidget *> pluginWidgets;
    Qv2ray::ui::widgets::models::ConnectionListHelper *modelHelper;
};
