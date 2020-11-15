/***************************************************************************
                          kscheduledview.cpp  -  description
                             -------------------
    begin                : Sun Jan 27 2002
    copyright            : (C) 2000-2002 by Michael Edwardes
    email                : mte@users.sourceforge.net
                           Javier Campos Morales <javi_c@users.sourceforge.net>
                           Felix Rodriguez <frodriguez@users.sourceforge.net>
                           John C <thetacoturtle@users.sourceforge.net>
                           Thomas Baumgart <ipwizard@users.sourceforge.net>
                           Kevin Tambascio <ktambascio@users.sourceforge.net>
                           (C) 2017 by Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kscheduledview_p.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QList>
#include <QTimer>
#include <QPushButton>
#include <QMenu>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KLocalizedString>
#include <KConfig>
#include <KMessageBox>
#include <KSharedConfig>
#include <KTreeWidgetSearchLine>
#include <KTreeWidgetSearchLineWidget>

// ----------------------------------------------------------------------------
// Project Includes

#include "keditloanwizard.h"
#include "kmymoneyutils.h"
#include "kmymoneysettings.h"
#include "mymoneyexception.h"
#include "keditscheduledlg.h"

KScheduledView::KScheduledView(QWidget *parent) :
  KMyMoneyViewBase(*new KScheduledViewPrivate(this), parent)
{
  typedef void(KScheduledView::*KScheduledViewFunc)();
  const QHash<eMenu::Action, KScheduledViewFunc> actionConnections {
    {eMenu::Action::NewSchedule,        &KScheduledView::slotNewSchedule},
    {eMenu::Action::EditSchedule,       &KScheduledView::slotEditSchedule},
    {eMenu::Action::DeleteSchedule,     &KScheduledView::slotDeleteSchedule},
    {eMenu::Action::DuplicateSchedule,  &KScheduledView::slotDuplicateSchedule},
    {eMenu::Action::EnterSchedule,      &KScheduledView::slotEnterSchedule},
    {eMenu::Action::SkipSchedule,       &KScheduledView::slotSkipSchedule},
  };

  for (auto a = actionConnections.cbegin(); a != actionConnections.cend(); ++a)
    connect(pActions[a.key()], &QAction::triggered, this, a.value());

  Q_D(KScheduledView);
  d->m_balanceWarning.reset(new KBalanceWarning(this));
}

KScheduledView::~KScheduledView()
{
}

void KScheduledView::slotSettingsChanged()
{
  Q_D(KScheduledView);
  d->settingsChanged();
}

void KScheduledView::executeCustomAction(eView::Action action)
{
  switch(action) {
    case eView::Action::SetDefaultFocus:
      {
        Q_D(KScheduledView);
        QMetaObject::invokeMethod(d->ui->m_searchWidget, "setFocus", Qt::QueuedConnection);
      }
      break;

    case eView::Action::EditSchedule:
      slotEditSchedule();
      break;

    default:
      break;
  }
}

void KScheduledView::showEvent(QShowEvent* event)
{
  Q_D(KScheduledView);
  if (d->m_needLoad) {
    d->init();
    connect(d->ui->m_scheduleTree, &QWidget::customContextMenuRequested, this, &KScheduledView::customContextMenuRequested);
    connect(d->ui->m_scheduleTree->header(), &QHeaderView::sortIndicatorChanged, this, [&](int logicalIndex, Qt::SortOrder order) {
      Q_D(KScheduledView);
      d->m_filterModel->sort(logicalIndex, order);
    });
  }
  emit customActionRequested(View::Schedules, eView::Action::AboutToShow);

  QWidget::showEvent(event);
}

void KScheduledView::updateActions(const MyMoneyObject& obj)
{
  Q_D(KScheduledView);
  if (typeid(obj) != typeid(MyMoneySchedule) &&
      (obj.id().isEmpty() && d->m_currentSchedule.id().isEmpty())) // do not disable actions that were already disabled))
    return;

  const auto& sch = static_cast<const MyMoneySchedule&>(obj);

  const QVector<eMenu::Action> actionsToBeDisabled {
        eMenu::Action::EditSchedule, eMenu::Action::DuplicateSchedule, eMenu::Action::DeleteSchedule,
        eMenu::Action::EnterSchedule, eMenu::Action::SkipSchedule,
  };

  for (const auto& a : actionsToBeDisabled)
    pActions[a]->setEnabled(false);

  pActions[eMenu::Action::NewSchedule]->setEnabled(true);

  if (!sch.id().isEmpty()) {
    pActions[eMenu::Action::EditSchedule]->setEnabled(true);
    pActions[eMenu::Action::DuplicateSchedule]->setEnabled(true);
    pActions[eMenu::Action::DeleteSchedule]->setEnabled(!MyMoneyFile::instance()->isReferenced(sch));
    if (!sch.isFinished()) {
      pActions[eMenu::Action::EnterSchedule]->setEnabled(true);
      // a schedule with a single occurrence cannot be skipped
      if (sch.occurrence() != eMyMoney::Schedule::Occurrence::Once) {
        pActions[eMenu::Action::SkipSchedule]->setEnabled(true);
      }
    }
  }
  d->m_currentSchedule = sch;
}

eDialogs::ScheduleResultCode KScheduledView::enterSchedule(MyMoneySchedule& schedule, bool autoEnter, bool extendedKeys)
{
  Q_D(KScheduledView);
  return d->enterSchedule(schedule, autoEnter, extendedKeys);
}

void KScheduledView::slotEnterOverdueSchedules(const MyMoneyAccount& acc)
{
  Q_D(KScheduledView);
  const auto file = MyMoneyFile::instance();
  auto schedules = file->scheduleList(acc.id(), eMyMoney::Schedule::Type::Any, eMyMoney::Schedule::Occurrence::Any, eMyMoney::Schedule::PaymentType::Any, QDate(), QDate(), true);
  if (!schedules.isEmpty()) {
    if (KMessageBox::questionYesNo(this,
                                   i18n("KMyMoney has detected some overdue scheduled transactions for this account. Do you want to enter those scheduled transactions now?"),
                                   i18n("Scheduled transactions found")) == KMessageBox::Yes) {

      QMap<QString, bool> skipMap;
      bool processedOne;
      auto rc = eDialogs::ScheduleResultCode::Enter;
      do {
        processedOne = false;
        QList<MyMoneySchedule>::const_iterator it_sch;
        for (it_sch = schedules.constBegin(); (rc != eDialogs::ScheduleResultCode::Cancel) && (it_sch != schedules.constEnd()); ++it_sch) {
          MyMoneySchedule sch(*(it_sch));

          // and enter it if it is not on the skip list
          if (skipMap.find((*it_sch).id()) == skipMap.end()) {
            rc = d->enterSchedule(sch, false, true);
            if (rc == eDialogs::ScheduleResultCode::Ignore) {
              skipMap[(*it_sch).id()] = true;
            }
          }
        }

        // reload list (maybe this schedule needs to be added again)
        schedules = file->scheduleList(acc.id(), eMyMoney::Schedule::Type::Any, eMyMoney::Schedule::Occurrence::Any, eMyMoney::Schedule::PaymentType::Any, QDate(), QDate(), true);
      } while (processedOne);
    }
  }
  emit selectByObject(MyMoneySchedule(), eView::Intent::FinishEnteringOverdueScheduledTransactions);
}

void KScheduledView::slotSelectByObject(const MyMoneyObject& obj, eView::Intent intent)
{
  switch(intent) {
    case eView::Intent::UpdateActions:
      updateActions(obj);
      break;

    case eView::Intent::StartEnteringOverdueScheduledTransactions:
      slotEnterOverdueSchedules(static_cast<const MyMoneyAccount&>(obj));
      break;

    case eView::Intent::OpenContextMenu:
      slotShowScheduleMenu(static_cast<const MyMoneySchedule&>(obj));
      break;

    default:
      break;
  }
}

void KScheduledView::customContextMenuRequested(const QPoint)
{
  Q_D(KScheduledView);
  emit selectByObject(d->m_currentSchedule, eView::Intent::None);
  emit selectByObject(d->m_currentSchedule, eView::Intent::OpenContextMenu);
}

void KScheduledView::slotListViewExpanded(const QModelIndex& idx)
{
  Q_D(KScheduledView);
  // we only need to check the top level items
  if (!idx.parent().isValid()) {
    const auto groupType = static_cast<eMyMoney::Schedule::Type>(idx.data(eMyMoney::Model::ScheduleTypeRole).toInt());
    d->m_expandedGroups[groupType] = true;
  }
}

void KScheduledView::slotListViewCollapsed(const QModelIndex& idx)
{
  Q_D(KScheduledView);
  // we only need to check the top level items
  if (!idx.parent().isValid()) {
    const auto groupType = static_cast<eMyMoney::Schedule::Type>(idx.data(eMyMoney::Model::ScheduleTypeRole).toInt());
    d->m_expandedGroups[groupType] = false;
  }
}

void KScheduledView::slotShowScheduleMenu(const MyMoneySchedule& sch)
{
  Q_UNUSED(sch)
  pMenus[eMenu::Menu::Schedule]->exec(QCursor::pos());
}

void KScheduledView::slotSetSelectedItem(const QItemSelection& selected, const QItemSelection& deselected )
{
  Q_UNUSED(deselected)
  MyMoneySchedule sch;

  if (!selected.indexes().isEmpty()) {
    const auto basemodel = MyMoneyFile::instance()->schedulesModel();
    const auto idx = selected.indexes().at(0);
    sch = basemodel->itemByIndex(basemodel->mapToBaseSource(idx));
  }
  emit selectByObject(sch, eView::Intent::None);
}

void KScheduledView::slotNewSchedule()
{
  KEditScheduleDlg::newSchedule(MyMoneyTransaction(), eMyMoney::Schedule::Occurrence::Monthly);
}

void KScheduledView::slotEditSchedule()
{
  Q_D(KScheduledView);
  KEditScheduleDlg::editSchedule(d->m_currentSchedule);
}

void KScheduledView::slotDeleteSchedule()
{
  Q_D(KScheduledView);
  if (!d->m_currentSchedule.id().isEmpty()) {
    MyMoneyFileTransaction ft;
    try {
      MyMoneySchedule sched = MyMoneyFile::instance()->schedule(d->m_currentSchedule.id());
      QString msg = i18n("<p>Are you sure you want to delete the scheduled transaction <b>%1</b>?</p>", d->m_currentSchedule.name());
      if (sched.type() == eMyMoney::Schedule::Type::LoanPayment) {
        msg += QString(" ");
        msg += i18n("In case of loan payments it is currently not possible to recreate the scheduled transaction.");
      }
      if (KMessageBox::questionYesNo(this, msg) == KMessageBox::No)
        return;

      MyMoneyFile::instance()->removeSchedule(sched);
      ft.commit();

    } catch (const MyMoneyException &e) {
      KMessageBox::detailedSorry(this, i18n("Unable to remove scheduled transaction '%1'", d->m_currentSchedule.name()), QString::fromLatin1(e.what()));
    }
  }
}

void KScheduledView::slotDuplicateSchedule()
{
  Q_D(KScheduledView);
  // since we may jump here via code, we have to make sure to react only
  // if the action is enabled
  if (pActions[eMenu::Action::DuplicateSchedule]->isEnabled()) {
    MyMoneySchedule sch = d->m_currentSchedule;
    sch.clearId();
    sch.setLastPayment(QDate());
    sch.setName(i18nc("Copy of scheduled transaction name", "Copy of %1", sch.name()));
    // make sure that we set a valid next due date if the original next due date is invalid
    if (!sch.nextDueDate().isValid())
      sch.setNextDueDate(QDate::currentDate());

    MyMoneyFileTransaction ft;
    try {
      MyMoneyFile::instance()->addSchedule(sch);
      ft.commit();

      // select the new schedule in the view
      if (!d->m_currentSchedule.id().isEmpty())
        emit selectByObject(sch, eView::Intent::None);

    } catch (const MyMoneyException &e) {
      KMessageBox::detailedSorry(this, i18n("Unable to duplicate scheduled transaction: '%1'", d->m_currentSchedule.name()), QString::fromLatin1(e.what()));
    }
  }
}

void KScheduledView::slotEnterSchedule()
{
  Q_D(KScheduledView);
  if (!d->m_currentSchedule.id().isEmpty()) {
    try {
      auto schedule = MyMoneyFile::instance()->schedule(d->m_currentSchedule.id());
      d->enterSchedule(schedule);
    } catch (const MyMoneyException &e) {
      KMessageBox::detailedSorry(this, i18n("Unknown scheduled transaction '%1'", d->m_currentSchedule.name()), QString::fromLatin1(e.what()));
    }
  }
}

void KScheduledView::slotSkipSchedule()
{
  Q_D(KScheduledView);
  if (!d->m_currentSchedule.id().isEmpty()) {
    try {
      auto schedule = MyMoneyFile::instance()->schedule(d->m_currentSchedule.id());
      d->skipSchedule(schedule);
    } catch (const MyMoneyException &e) {
      KMessageBox::detailedSorry(this, i18n("Unknown scheduled transaction '%1'", d->m_currentSchedule.name()), QString::fromLatin1(e.what()));
    }
  }
}
