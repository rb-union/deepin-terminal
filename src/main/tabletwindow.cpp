/*
 *  Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd
 *
 * Author:  wangliang<wangliang@uniontech.com>
 *
 * Maintainer: wangliang<wangliang@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tabletwindow.h"
#include "tabbar.h"
#include "service.h"

#include <QtDBus>
#include <QDesktopWidget>

#include <DTitlebar>
#include <DIconButton>

DWIDGET_USE_NAMESPACE

// 平板模式窗口单例
TabletWindow *TabletWindow::m_window = nullptr;

/**
 平板模式终端窗口
*/
TabletWindow::TabletWindow(TermProperties properties, QWidget *parent): MainWindow(properties, parent)
{
    Q_ASSERT(m_isQuakeWindow == false);
    setObjectName("TabletWindow");
    initUI();
    initConnections();
    initShortcuts();
    createWindowComplete();
    setIsQuakeWindowTab(false);

    // 初始化虚拟键盘相关信号连接
    initVirtualKeyboardConnections();
}

TabletWindow::~TabletWindow()
{

}

/*******************************************************************************
 1. @函数:    instance
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-11
 4. @说明:    平板模式窗口单例
*******************************************************************************/
TabletWindow *TabletWindow::instance(TermProperties properties)
{
    if (nullptr == m_window) {
        m_window = new TabletWindow(properties);
    }

    return m_window;
}

/*******************************************************************************
 1. @函数:    initVirtualKeyboardConnections
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-13
 4. @说明:    初始化虚拟键盘相关信号连接
*******************************************************************************/
void TabletWindow::initVirtualKeyboardConnections()
{
    initVirtualKeyboardImActiveChangedSignal();
    initVirtualKeyboardGeometryChangedSignal();

    connect(this, &TabletWindow::onResizeWindowHeight, this, &TabletWindow::slotResizeWindowHeight);
}

/*******************************************************************************
 1. @函数:    initVirtualKeyboardImActiveChangedSignal
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-13
 4. @说明:    虚拟键盘是否激活(即是否显示)的DBus信号
*******************************************************************************/
void TabletWindow::initVirtualKeyboardImActiveChangedSignal()
{
    QDBusConnection dbusConn = QDBusConnection::sessionBus();

    if (dbusConn.connect(
            DUE_IM_DBUS_NAME, // service name
            DUE_IM_DBUS_PATH, // path name
            DUE_IM_DBUS_INTERFACE, // interface
            "imActiveChanged", // signal name
            this, // receiver
            SLOT(slotVirtualKeyboardImActiveChanged(bool)))) { // slot

        qDebug() << "dbus connect to imActiveChanged success";
    } else {
        qDebug() << "dbus connect to imActiveChanged failed";
    }
}

/*******************************************************************************
 1. @函数:    initVirtualKeyboardGeometryChangedSignal
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-13
 4. @说明:    虚拟键盘是否改变了布局(geometry)的DBus信号
*******************************************************************************/
void TabletWindow::initVirtualKeyboardGeometryChangedSignal()
{
    QDBusConnection dbusConn = QDBusConnection::sessionBus();

    if (dbusConn.connect(
            DUE_IM_DBUS_NAME, // service name
            DUE_IM_DBUS_PATH, // path name
            DUE_IM_DBUS_INTERFACE, // interface
            "geometryChanged", // signal name
            this, // receiver
            SLOT(slotVirtualKeyboardGeometryChanged(QRect)))) { // slot

        qDebug() << "dbus connect to geometryChanged success";
    } else {
        qDebug() << "dbus connect to geometryChanged failed";
    }
}

/*******************************************************************************
 1. @函数:    slotVirtualKeyboardImActiveChanged
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-13
 4. @说明:    判断虚拟键盘是否激活(即是否显示)的槽函数
*******************************************************************************/
void TabletWindow::slotVirtualKeyboardImActiveChanged(bool isShow)
{
    m_isVirtualKeyboardShow = isShow;

    Service::instance()->setVirtualKeyboardShow(isShow);
    handleVirtualKeyboardShowHide(isShow);
}

/*******************************************************************************
 1. @函数:    slotVirtualKeyboardGeometryChanged
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-13
 4. @说明:    判断虚拟键盘布局(geometry)是否改变的槽函数
*******************************************************************************/
void TabletWindow::slotVirtualKeyboardGeometryChanged(QRect rect)
{
    qDebug() << "slotOnVirtualKeyBoardLayoutChanged: " << rect;
}

/*******************************************************************************
 1. @函数:    slotResizeWindowHeight
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-14
 4. @说明:    根据虚拟键盘是否显示，调整主窗口高度
*******************************************************************************/
void TabletWindow::slotResizeWindowHeight(int windowHeight)
{
    // 关闭标签页时，键盘会频繁隐藏/显示，导致多次触发该槽函数
    // TODO: MainWindow设置setFixedHeight会在关闭标签页的时候卡一下，待优化
    this->setFixedHeight(windowHeight);
}

/*******************************************************************************
 1. @函数:    slotVirtualKeyboardShowHide
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-13
 4. @说明:    处理虚拟键盘显示/隐藏的槽函数
*******************************************************************************/
void TabletWindow::handleVirtualKeyboardShowHide(bool isShow)
{
    QDesktopWidget *desktopWidget = QApplication::desktop();
    QRect screenRect = desktopWidget->screenGeometry(); //获取设备屏幕大小
    QSize availableSize = desktopWidget->availableGeometry().size();

    int windowHeight = 0;

    if (isShow) {
        // 获取虚拟键盘高度
        if (-1 == m_virtualKeyboardHeight) {
            m_virtualKeyboardHeight = Service::instance()->getVirtualKeyboardHeight();
        }

        windowHeight = screenRect.height()-m_virtualKeyboardHeight;
    }
    else {
        windowHeight = availableSize.height();
    }

    // 根据虚拟键盘是否显示，调整主窗口高度
    emit onResizeWindowHeight(windowHeight);
}

/*******************************************************************************
 1. @函数:    initTitleBar
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-05
 4. @说明:    平板模式终端窗口初始化标题栏
*******************************************************************************/
void TabletWindow::initTitleBar()
{
    // titlebar在普通模式和雷神模式不一样的功能
    m_titleBar = new TitleBar(this, false);
    m_titleBar->setObjectName("TabletWindowTitleBar");//Add by ut001000 renfeixiang 2020-08-14
    m_titleBar->setTabBar(m_tabbar);

    titlebar()->setCustomWidget(m_titleBar);
    // titlebar()->setAutoHideOnFullscreen(true);
    titlebar()->setTitle("");

    //设置titlebar焦点策略为不抢占焦点策略，防止点击titlebar后终端失去输入焦点
    titlebar()->setFocusPolicy(Qt::NoFocus);
    initOptionButton();
    initOptionMenu();

    //增加主题菜单
    addThemeMenuItems();

    //处理titlebar、tabbar中的按钮焦点
    handleTitleAndTabButtonsFocus();
}

/*******************************************************************************
 1. @函数:    initWindowAttribute
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-05
 4. @说明:    平板模式的窗口设置
*******************************************************************************/
void TabletWindow::initWindowAttribute()
{
    show();
    setDefaultLocation();
    showMaximized();
}

/*******************************************************************************
 1. @函数:    saveWindowSize
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-05
 4. @说明:    普通窗口保存窗口大小
*******************************************************************************/
void TabletWindow::saveWindowSize()
{
    //TODO: 平板模式无需保存窗口大小
}

/*******************************************************************************
 1. @函数:    switchFullscreen
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-05
 4. @说明:    普通窗口全屏切换
*******************************************************************************/
void TabletWindow::switchFullscreen(bool forceFullscreen)
{
    Q_UNUSED(forceFullscreen)
    //TODO: 平板模式无需切换全屏
}

/*******************************************************************************
 1. @函数:    calculateShortcutsPreviewPoint
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-05
 4. @说明:    普通窗口计算快捷方式预览点
*******************************************************************************/
QPoint TabletWindow::calculateShortcutsPreviewPoint()
{
    QRect rect = window()->geometry();
    return QPoint(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2);
}

/*******************************************************************************
 1. @函数:    onAppFocusChangeForQuake
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-05
 4. @说明:    普通窗口处理雷神窗口丢失焦点自动隐藏功能，普通窗口不用该函数
*******************************************************************************/
void TabletWindow::onAppFocusChangeForQuake()
{
    return;
}

/*******************************************************************************
 1. @函数:    changeEvent
 2. @作者:    ut000438 王亮
 3. @日期:    2021-01-05
 4. @说明:    普通窗口变化事件
*******************************************************************************/
void TabletWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
}

