/*
    SPDX-FileCopyrightText: 2006 Darren Gould <darren_gould@gmx.de>
    SPDX-FileCopyrightText: 2006 Alvaro Soliverez <asoliverez@gmail.com>
    SPDX-FileCopyrightText: 2017 Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
    SPDX-FileCopyrightText: 2019 Thomas Baumgart <tbaumgart@kde.org>
    SPDX-FileCopyrightText: 2020 Robert Szczesiak <dev.rszczesiak@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kbudgetview_p.h"

// ----------------------------------------------------------------------------
// QT Includes

// ----------------------------------------------------------------------------
// KDE Includes

#include <KXMLGUIClient>
#include <KXMLGUIFactory>
#include <KActionCollection>

// ----------------------------------------------------------------------------
// Project Includes

#include "mymoneyexception.h"

using namespace Icons;

KBudgetView::KBudgetView(QWidget* parent)
    : KMyMoneyViewBase(*new KBudgetViewPrivate(this), parent)
{
    connect(MyMoneyFile::instance()->budgetsModel(), &QAbstractItemModel::dataChanged, this, [&](const QModelIndex& topLeft, const QModelIndex& bottomRight) {
        Q_UNUSED(bottomRight)
        Q_D(KBudgetView);
        // only process if full initialized
        if (d->m_budgetProxyModel) {
            if (d->ui->m_budgetList->currentIndex().row() == topLeft.row()) {
                const auto yearIdx = topLeft.model()->index(topLeft.row(), BudgetsModel::Columns::Year, topLeft.parent());
                if (d->ui->m_budgetValue->budgetStartDate().year() != yearIdx.data().toInt()) {
                    slotResetBudget();
                }
            }
        }
    });
}

KBudgetView::~KBudgetView()
{
}

void KBudgetView::slotSettingsChanged()
{
    Q_D(KBudgetView);
    if (d->m_budgetProxyModel) {
        d->m_budgetProxyModel->setColorScheme(AccountsModel::Positive, KMyMoneySettings::schemeColor(SchemeColor::Positive));
        d->m_budgetProxyModel->setColorScheme(AccountsModel::Negative, KMyMoneySettings::schemeColor(SchemeColor::Negative));
        d->ui->m_budgetValue->setBudgetStartDate(QDate(QDate::currentDate().year(), KMyMoneySettings::firstFiscalMonth(), KMyMoneySettings::firstFiscalDay()));
    }
}

void KBudgetView::showEvent(QShowEvent * event)
{
    Q_D(KBudgetView);
    if (!d->m_budgetProxyModel) {
        d->init();
        slotSettingsChanged();

        connect(d->ui->m_budgetList, &QTableView::customContextMenuRequested, this, [&](const QPoint&) {
            Q_D(KBudgetView);
            if (d->m_budgetContextMenu) {
                d->m_budgetContextMenu->exec(QCursor::pos());
            } else {
                qDebug() << "No context menu assigned in KBudgetView";
            }
        });

        slotSelectBudget();
    }

    // don't forget base class implementation
    QWidget::showEvent(event);
}

void KBudgetView::slotNewBudget()
{
    Q_D(KBudgetView);
    auto date = QDate::currentDate();
    date.setDate(date.year(), KMyMoneySettings::firstFiscalMonth(), KMyMoneySettings::firstFiscalDay());
    auto newname = i18nc("@item:intable Your budgets, %1 budget year", "Budget %1", QString::number(date.year()));

    MyMoneyBudget budget;

    // make sure we have a unique name
    try {
        // Exception thrown when the name is not found
        int i = 1;
        while (!MyMoneyFile::instance()->budgetByName(newname).id().isEmpty())
            newname = i18nc("@item:intable Your bugets, %1 budget year, %2 unique index", "Budget %1 (%2)", QString::number(date.year()), i++);
    } catch (const MyMoneyException &) {
        // all ok, the name is unique
    }

    MyMoneyFileTransaction ft;
    try {
        budget.setName(newname);
        budget.setBudgetStart(date);

        MyMoneyFile::instance()->addBudget(budget);
        ft.commit();
        // select the newly created budget
        d->ui->m_budgetList->setCurrentIndex(MyMoneyFile::instance()->budgetsModel()->indexById(budget.id()));
        d->ui->m_budgetValue->setBudgetStartDate(date);
    } catch (const MyMoneyException &e) {
        KMessageBox::detailedError(this, i18n("Unable to add budget"), QString::fromLatin1(e.what()));
    }
}

void KBudgetView::slotDeleteBudget()
{
    Q_D(KBudgetView);
    const QModelIndexList indexes = d->ui->m_budgetList->selectionModel()->selectedIndexes();
    if (indexes.isEmpty())
        return; // shouldn't happen

    auto file = MyMoneyFile::instance();

    // get confirmation from user
    QString prompt;
    if (indexes.count() == 2) {
        const auto budgetName = indexes.first().data(eMyMoney::Model::BudgetNameRole).toString();
        prompt = i18n("<p>Do you really want to remove the budget <b>%1</b>?</p>", budgetName);
    } else
        prompt = i18n("Do you really want to remove all selected budgets?");

    if (KMessageBox::questionTwoActions(this, prompt, i18nc("@title:window ", "Remove Budget"), KMMYesNo::yes(), KMMYesNo::no())
        == KMessageBox::SecondaryAction)
        return;

    try {
        MyMoneyFileTransaction ft;
        // now loop over all selected d->m_budgetList and remove them
        // we first collect all budget ids because the indexes will
        // become invalid during the removal
        QStringList budgetIds;
        for (const auto& idx: indexes) {
            budgetIds << idx.data(eMyMoney::Model::IdRole).toString();
        }
        for (const auto& id : qAsConst(budgetIds)) {
            auto budget = file->budget(id);
            file->removeBudget(budget);
        }
        ft.commit();
    } catch (const MyMoneyException &e) {
        KMessageBox::detailedError(this, i18n("Unable to remove budget."), QString::fromLatin1(e.what()));
    }
}

void KBudgetView::slotCopyBudget()
{
    Q_D(KBudgetView);
    const QModelIndexList indexes = d->ui->m_budgetList->selectionModel()->selectedIndexes();
    if (indexes.count() == 2) {
        const auto file = MyMoneyFile::instance();
        MyMoneyFileTransaction ft;
        try {

            MyMoneyBudget budget = file->budget(indexes.first().data(eMyMoney::Model::IdRole).toString());
            budget.clearId();
            budget.setName(i18n("Copy of %1", budget.name()));

            file->addBudget(budget);
            ft.commit();
        } catch (const MyMoneyException &e) {
            KMessageBox::detailedError(this, i18n("Unable to add budget"), QString::fromLatin1(e.what()));
        }
    }
}

void KBudgetView::slotBudgetForecast()
{
    Q_D(KBudgetView);
    if (d->ui->m_budgetList->selectionModel()->selectedIndexes().count() == 2) {
        auto file = MyMoneyFile::instance();
        MyMoneyFileTransaction ft;
        try {
            auto idx = d->ui->m_budgetList->selectionModel()->selectedIndexes().first();
            idx = MyMoneyFile::baseModel()->mapToBaseSource(idx);
            if (idx.isValid()) {
                auto budget = file->budgetsModel()->itemByIndex(idx);
                bool calcBudget = budget.getaccounts().count() == 0;
                if (!calcBudget) {
                    if (KMessageBox::warningContinueCancel(0, i18n("The current budget already contains data. Continuing will replace all current values of this budget."), i18nc("Warning message box", "Warning")) == KMessageBox::Continue)
                        calcBudget = true;
                }

                if (calcBudget) {
                    QDate historyStart;
                    QDate historyEnd;
                    QDate budgetStart;
                    QDate budgetEnd;

                    budgetStart = budget.budgetStart();
                    budgetEnd = budgetStart.addYears(1).addDays(-1);
                    historyStart = budgetStart.addYears(-1);
                    historyEnd = budgetEnd.addYears(-1);

                    MyMoneyForecast forecast = KMyMoneyUtils::forecast();
                    forecast.createBudget(budget, historyStart, historyEnd, budgetStart, budgetEnd, true);

                    MyMoneyFile::instance()->modifyBudget(budget);
                    ft.commit();
                    // load updated data
                    slotSelectBudget();
                }
            }
        } catch (const MyMoneyException &e) {
            KMessageBox::detailedError(this, i18n("Unable to modify budget."), QString::fromLatin1(e.what()));
        }
    }
}

void KBudgetView::slotResetBudget()
{
    Q_D(KBudgetView);
    try {
        d->ui->m_budgetValue->clear();
        d->m_budget = MyMoneyFile::instance()->budget(d->m_budget.id());
        d->ui->m_updateButton->setEnabled(false);
        slotSelectBudget();
        d->loadBudgetAccountsView();
    } catch (const MyMoneyException &e) {
        KMessageBox::detailedError(this, i18n("Unable to reset budget"), QString::fromLatin1(e.what()));
    }
}

void KBudgetView::slotUpdateBudget()
{
    Q_D(KBudgetView);
    MyMoneyFileTransaction ft;
    try {
        MyMoneyFile::instance()->modifyBudget(d->m_budget);
        ft.commit();
        d->ui->m_updateButton->setEnabled(false);
        // load updated data
        slotSelectBudget();
    } catch (const MyMoneyException &e) {
        KMessageBox::detailedError(this, i18n("Unable to modify budget"), QString::fromLatin1(e.what()));
    }
}

void KBudgetView::slotStartRename()
{
    Q_D(KBudgetView);
    // check that a single row is selected (all columns)
    if (d->ui->m_budgetList->selectionModel()->selectedIndexes().count() == d->ui->m_budgetList->model()->columnCount()) {
        auto row = d->ui->m_budgetList->selectionModel()->selectedIndexes().first().row();
        auto idx = d->ui->m_budgetList->model()->index(row, 0);
        d->ui->m_budgetList->setCurrentIndex(idx);
        d->ui->m_budgetList->edit(idx);
    }
}

void KBudgetView::slotSelectAccount(const MyMoneyObject& obj)
{
    Q_D(KBudgetView);
    d->ui->m_assignmentBox->setEnabled(false);
    if (typeid(obj) != typeid(MyMoneyAccount))
        return;

    const MyMoneyAccount& acc = dynamic_cast<const MyMoneyAccount&>(obj);
    d->ui->m_assignmentBox->setEnabled(true);

    if (d->m_budget.id().isEmpty())
        return;

    QString id = acc.id();
    d->ui->m_leAccounts->setText(MyMoneyFile::instance()->accountToCategory(id));
    d->ui->m_cbBudgetSubaccounts->setChecked(d->m_budget.account(id).budgetSubaccounts());
    d->ui->m_accountTotal->setValue(d->m_budget.account(id).totalBalance());
    MyMoneyBudget::AccountGroup budgetAccount = d->m_budget.account(id);
    if (id != budgetAccount.id()) {
        budgetAccount.setBudgetLevel(eMyMoney::Budget::Level::Monthly);
    }
    d->ui->m_budgetValue->setBudgetValues(d->m_budget, budgetAccount);
}

void KBudgetView::slotBudgetedAmountChanged()
{
    Q_D(KBudgetView);
    if (d->m_budget.id().isEmpty())
        return;

    const auto indexes = d->ui->m_accountTree->selectionModel()->selectedIndexes();
    if (indexes.isEmpty())
        return;
    const auto idx = indexes.front();
    const auto accountID = idx.data(eMyMoney::Model::Roles::IdRole).toString();

    MyMoneyBudget::AccountGroup accountGroup = d->m_budget.account(accountID);
    accountGroup.setId(accountID);
    d->ui->m_budgetValue->budgetValues(d->m_budget, accountGroup);
    if (!idx.data(eMyMoney::Model::AccountIsIncomeExpenseRole).toBool()) {
        accountGroup.setBudgetType(idx.data(eMyMoney::Model::AccountTypeInBudgetRole).value<eMyMoney::Account::Type>());
    }
    d->m_budget.setAccount(accountGroup, accountID);

    d->m_budgetProxyModel->setBudget(d->m_budget);
    d->ui->m_accountTotal->setValue(accountGroup.totalBalance());

    d->ui->m_updateButton->setEnabled(!(d->selectedBudget() == d->m_budget));
    d->ui->m_resetButton->setEnabled(!(d->selectedBudget() == d->m_budget));
}

void KBudgetView::cb_includesSubaccounts_clicked()
{
    Q_D(KBudgetView);
    if (d->m_budget.id().isEmpty())
        return;

    QModelIndexList indexes = d->ui->m_accountTree->selectionModel()->selectedIndexes();
    if (!indexes.empty()) {
        QString accountID = indexes.front().data(eMyMoney::Model::Roles::IdRole).toString();
        // now, we get a reference to the accountgroup, to modify its attribute,
        // and then put the resulting account group instead of the original
        MyMoneyBudget::AccountGroup auxAccount = d->m_budget.account(accountID);
        auxAccount.setBudgetSubaccounts(d->ui->m_cbBudgetSubaccounts->isChecked());

        // in case we turn the option on, we check that no subordinate account
        // has a budget. If we find some, we ask the user if he wants to move it
        // to the current account or leave things as they are
        if (d->ui->m_cbBudgetSubaccounts->isChecked()) {
            // TODO: asking the user needs to be added. So long, we assume yes
            if (1) {
                MyMoneyBudget::AccountGroup subAccount;
                if (d->collectSubBudgets(subAccount, indexes.front())) {
                    // we found a sub-budget somewhere
                    // so we add those figures found and
                    // clear the subaccounts
                    auxAccount += subAccount;
                    d->clearSubBudgets(indexes.front());
                }

                if (auxAccount.budgetLevel() == eMyMoney::Budget::Level::None) {
                    MyMoneyBudget::PeriodGroup period;
                    auxAccount.addPeriod(d->m_budget.budgetStart(), period);
                    auxAccount.setBudgetLevel(eMyMoney::Budget::Level::Monthly);
                }
            }
        }

        d->m_budget.setAccount(auxAccount, accountID);
        d->m_budgetProxyModel->setBudget(d->m_budget);
        d->ui->m_budgetValue->setBudgetValues(d->m_budget, auxAccount);

        d->loadBudgetAccountsView();
    }
}

void KBudgetView::slotBudgetBalanceChanged(const MyMoneyMoney &balance)
{
    Q_D(KBudgetView);
    const auto formattedValue = balance.isNegative() ? d->formatViewLabelValue(-balance, KMyMoneySettings::schemeColor(SchemeColor::Negative))
                                : d->formatViewLabelValue(balance, KMyMoneySettings::schemeColor(SchemeColor::Positive));
    d->updateViewLabel(d->ui->m_balanceLabel,
                       balance.isNegative() ? i18nc("Profit/Loss", "Loss: %1", formattedValue)
                       : i18nc("Profit/Loss", "Profit: %1", formattedValue));
}

void KBudgetView::slotAccountSelectionChanged(const SelectedObjects& selections)
{
    Q_UNUSED(selections)
    Q_D(KBudgetView);
    const auto idx = d->ui->m_accountTree->currentIndex();
    if (idx.isValid()) {
        const auto baseIdx = MyMoneyFile::baseModel()->mapToBaseSource(idx);
        const auto account = MyMoneyFile::instance()->accountsModel()->itemByIndex(baseIdx);
        slotSelectAccount(account);
    }
}

void KBudgetView::slotSelectBudget()
{
    Q_D(KBudgetView);
    d->askSave();

    d->ui->m_accountTree->setEnabled(false);
    d->ui->m_assignmentBox->setEnabled(false);

    d->m_budget = d->selectedBudget();
    d->ui->m_accountTree->setDisabled(d->m_budget.id().isEmpty());

    SelectedObjects selection;
    if (!d->m_budget.id().isEmpty()) {
        selection.addSelection(SelectedObjects::Budget, d->m_budget.id());
        d->loadBudgetAccountsView();
        const auto idx = d->ui->m_accountTree->currentIndex();
        if (idx.isValid()) {
            const auto baseIdx = MyMoneyFile::baseModel()->mapToBaseSource(idx);
            const auto account = MyMoneyFile::instance()->accountsModel()->itemByIndex(baseIdx);
            slotSelectAccount(account);
        } else {
            d->ui->m_budgetValue->clear();
        }
    }

    Q_EMIT requestSelectionChange(selection);
}

void KBudgetView::slotHideUnused(bool toggled)
{
    Q_D(KBudgetView);
    // make sure we show all items for an empty budget
    const auto prevState = !toggled;
    if (prevState != d->ui->m_hideUnusedButton->isChecked())
        d->m_budgetProxyModel->setHideUnusedIncomeExpenseAccounts(d->ui->m_hideUnusedButton->isChecked());
}

void KBudgetView::slotTreatAsExpense()
{
    Q_D(KBudgetView);
    d->treatAs(eMyMoney::Account::Type::Expense);
}

void KBudgetView::slotTreatAsIncome()
{
    Q_D(KBudgetView);
    d->treatAs(eMyMoney::Account::Type::Income);
}

void KBudgetView::createActions(KXMLGUIFactory* guiFactory, KXMLGUIClient* guiClient)
{
    typedef void(KBudgetView::*KBudgetViewFunc)();
    struct actionInfo {
        QString             name;
        KBudgetViewFunc     callback;
        QString             text;
        Icon                icon;
        eMenu::BudgetAction id;
    };

    const QVector<actionInfo> budgetActionInfos{
        {QStringLiteral("budget_new"), &KBudgetView::slotNewBudget, i18n("New budget"), Icon::DocumentNew, eMenu::BudgetAction::NewBudget},
        {QStringLiteral("budget_rename"), &KBudgetView::slotStartRename, i18n("Rename budget"), Icon::DocumentEdit, eMenu::BudgetAction::RenameBudget},
        {QStringLiteral("budget_delete"), &KBudgetView::slotDeleteBudget, i18n("Delete budget"), Icon::EditRemove, eMenu::BudgetAction::DeleteBudget},
        {QStringLiteral("budget_copy"), &KBudgetView::slotCopyBudget, i18n("Copy budget"), Icon::EditCopy, eMenu::BudgetAction::CopyBudget},
        {QStringLiteral("budget_base_on_forecast"),
         &KBudgetView::slotBudgetForecast,
         i18n("Budget based on forecast"),
         Icon::OfficeChartLineForecast,
         eMenu::BudgetAction::BudgetForecast},
    };

    const QVector<actionInfo> accountActionInfos{
        {QStringLiteral("budget_treat_as_income"),
         &KBudgetView::slotTreatAsIncome,
         i18nc("@item:inmenu To make it income", "Change to income"),
         Icon::Income,
         eMenu::BudgetAction::TreatAsIncome},
        {QStringLiteral("budget_treat_as_expense"),
         &KBudgetView::slotTreatAsExpense,
         i18nc("@item:inmenu To make it expense", "Change to expense"),
         Icon::Expense,
         eMenu::BudgetAction::TreatAsExpense},
    };

    Q_D(KBudgetView);
    d->m_actionCollection = guiClient->actionCollection();
    for (const auto& actionInfo : budgetActionInfos) {
        QAction* action = d->m_actionCollection->addAction(actionInfo.name, this, actionInfo.callback);
        action->setText(actionInfo.text);
        action->setIcon(Icons::get(actionInfo.icon));
        d->m_actions.insert(actionInfo.id, action);
    }

    for (const auto& actionInfo : accountActionInfos) {
        QAction *action = d->m_actionCollection->addAction(actionInfo.name, this, actionInfo.callback);
        action->setText(actionInfo.text);
        action->setIcon(Icons::get(actionInfo.icon));
        d->m_actions.insert(actionInfo.id, action);
    }

    // create context menu
    d->m_budgetContextMenu = qobject_cast<QMenu*>(guiFactory->container(QStringLiteral("budget_context_menu"), guiClient));

    // For some unknown reason, the context menu does not get created this way from the .rc file.
    // I must be doing something wrong / don't understand something. This kxmlgui thingy
    // remains a mystery to me. Apparently, I am also too stupid to get the window tile showing up
    if (!d->m_budgetContextMenu) {
        d->m_budgetContextMenu = new QMenu(this);
        d->m_budgetContextMenu->addSection(i18nc("@title:menu Budget context menu", "Budget options"));
        for (const auto& actionInfo : budgetActionInfos) {
            d->m_budgetContextMenu->insertAction(nullptr, d->m_actions[actionInfo.id]);
        }
    }

    // create context menu
    d->m_accountContextMenu = qobject_cast<QMenu*>(guiFactory->container(QStringLiteral("budget_account_context_menu"), guiClient));

    if (!d->m_accountContextMenu) {
        d->m_accountContextMenu = new QMenu(this);
        d->m_accountContextMenu->addSection(i18nc("@title:menu Budget account context menu", "Account options"));
        for (const auto& actionInfo : accountActionInfos) {
            d->m_accountContextMenu->insertAction(nullptr, d->m_actions[actionInfo.id]);
        }
    }
}

void KBudgetView::removeActions()
{
    Q_D(KBudgetView);
    // remove and delete the actions for this plugin
    for (const auto& action : d->m_actions) {
        d->m_actionCollection->removeAction(action);
    }
    // the context menu should be our child, but you never know
    d->m_budgetContextMenu->deleteLater();
    d->m_accountContextMenu->deleteLater();
}

void KBudgetView::updateActions(const SelectedObjects& selections)
{
    Q_D(KBudgetView);
    d->m_actions[eMenu::BudgetAction::NewBudget]->setEnabled(true);

    auto b = !selections.isEmpty(SelectedObjects::Budget);
    d->m_actions[eMenu::BudgetAction::DeleteBudget]->setEnabled(b);

    b = selections.count(SelectedObjects::Budget) == 1;
    d->m_actions[eMenu::BudgetAction::CopyBudget]->setEnabled(b);
    d->m_actions[eMenu::BudgetAction::RenameBudget]->setEnabled(b);
    d->m_actions[eMenu::BudgetAction::BudgetForecast]->setEnabled(b);
}

void KBudgetView::slotOpenAccountContextMenu(eMenu::Menu type, const QPoint& p)
{
    Q_D(KBudgetView);
    const auto currentIdx = d->ui->m_accountTree->selectionModel()->currentIndex();
    if (currentIdx.isValid() && !d->ui->m_accountTree->selectionModel()->selectedIndexes().isEmpty() && (type == eMenu::Menu::Account)) {
        const auto accountId = currentIdx.data(eMyMoney::Model::IdRole).toString();
        auto accountGroup = d->m_budget.account(accountId);
        QString tip;
        if (accountGroup.id() == accountId) {
            const bool isIncome = accountGroup.budgetType() == eMyMoney::Account::Type::Income;
            tip = i18nc("@info:tooltip Budget assignment", "Select how the budget of this account shall be treated.");
            d->m_actions[eMenu::BudgetAction::TreatAsIncome]->setEnabled(!isIncome);
            d->m_actions[eMenu::BudgetAction::TreatAsExpense]->setEnabled(isIncome);
        } else {
            tip = i18nc("@info:tooltip Budget assignment", "This option is only available when a budget value is assigned.");
            d->m_actions[eMenu::BudgetAction::TreatAsIncome]->setEnabled(false);
            d->m_actions[eMenu::BudgetAction::TreatAsExpense]->setEnabled(false);
        }
        d->m_actions[eMenu::BudgetAction::TreatAsIncome]->setToolTip(tip);
        d->m_actions[eMenu::BudgetAction::TreatAsExpense]->setToolTip(tip);
        d->m_accountContextMenu->popup(p);
    }
}
