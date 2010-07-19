/***************************************************************************
                          kscheduledview.h  -  description
                             -------------------
    begin                : Sun Jan 27 2002
    copyright            : (C) 2000-2002 by Michael Edwardes
    email                : mte@users.sourceforge.net
                           Javier Campos Morales <javi_c@users.sourceforge.net>
                           Felix Rodriguez <frodriguez@users.sourceforge.net>
                           John C <thetacoturtle@users.sourceforge.net>
                           Thomas Baumgart <ipwizard@users.sourceforge.net>
                           Kevin Tambascio <ktambascio@users.sourceforge.net>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef KSCHEDULEDVIEW_H
#define KSCHEDULEDVIEW_H

// ----------------------------------------------------------------------------
// QT Includes

#include <QWidget>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KTreeWidgetSearchLineWidget>

// ----------------------------------------------------------------------------
// Project Includes

#include "ui_kscheduledviewdecl.h"
#include <mymoneyfile.h>
#include <mymoneyaccount.h>
#include "kmymoneyscheduledcalendar.h"

/**
  * Contains all the scheduled transactions be they bills, deposits or transfers.
  * Encapsulates all the operations including adding, editing and deleting.
  * Used by the KMyMoneyView class to show the view.
  *
  * @author Michael Edwardes 2000-2002
  * $Id: kscheduledview.h,v 1.33 2009/03/01 19:13:08 ipwizard Exp $
  *
  * @short A class to encapsulate recurring transaction operations.
  */
class KScheduledView : public QWidget, public Ui::KScheduledViewDecl
{
  Q_OBJECT

public:
  /**
    * Standard constructor for QWidgets.
    */
  KScheduledView(QWidget *parent = 0);

  /**
    * Standard destructor.
    */
  ~KScheduledView();

  /**
    * Called by KMyMoneyView.
    */
  void showEvent(QShowEvent* event);

public slots:
  void slotSelectSchedule(const QString& schedule);
  void slotReloadView(void);

signals:
  void scheduleSelected(const MyMoneySchedule& schedule);
  void openContextMenu(void);
  void skipSchedule(void);
  void enterSchedule(void);
  void editSchedule(void);

protected slots:
  /**
    * Shows the context menu when the user right clicks or presses
    * a 'windows' key when an item is selected.
    *
    * @param view a pointer to the view
    * @param item a pointer to the current selected listview item
    * @param pos The position to popup
    * @return none
  **/
  void slotListViewContextMenu(const QPoint& pos);

  void slotListItemExecuted(QTreeWidgetItem*, int);

  void slotAccountActivated(int);

  void slotListViewCollapsed(QTreeWidgetItem* item);
  void slotListViewExpanded(QTreeWidgetItem* item);

  void slotBriefSkipClicked(const MyMoneySchedule& schedule, const QDate&);
  void slotBriefEnterClicked(const MyMoneySchedule& schedule, const QDate&);

  void slotTimerDone(void);

  void slotSetSelectedItem();

  void slotRearrange(void);

protected:

  QTreeWidgetItem* addScheduleItem(QTreeWidgetItem* parent, MyMoneySchedule& schedule);

private:

  /// The selected schedule id in the list view.
  QString m_selectedSchedule;

  /// Read config file
  void readConfig(void);

  /// Write config file
  void writeConfig(void);

  /**
    * Refresh the view.
    */
  void refresh(bool full = true, const QString& schedId = QString());

  /**
    * Loads the accounts into the combo box.
    */
//  void loadAccounts(void);

  KMenu *m_kaccPopup;
  QStringList m_filterAccounts;
  bool m_openBills;
  bool m_openDeposits;
  bool m_openTransfers;
  bool m_openLoans;
  bool m_needReload;
  int  m_sortColumn;

  /**
   * Search widget for the list
   */
  KTreeWidgetSearchLineWidget*  m_searchWidget;
};

#endif
