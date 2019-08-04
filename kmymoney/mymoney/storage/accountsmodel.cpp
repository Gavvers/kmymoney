/*
 * Copyright 2019       Thomas Baumgart <tbaumgart@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// just make sure that the assertions always work in this model
#ifndef QT_FORCE_ASSERTS
#define QT_FORCE_ASSERTS
#endif

#include "accountsmodel.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QDebug>
#include <QString>
#include <QFont>
#include <QIcon>
#include <QDate>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KLocalizedString>

// ----------------------------------------------------------------------------
// Project Includes

#include "mymoneyfile.h"
#include "mymoneyenums.h"
#include "mymoneymoney.h"
#include "mymoneysecurity.h"
#include "securitiesmodel.h"
#include "mymoneyprice.h"
#include "kmymoneysettings.h"

#include "icons.h"

struct AccountsModel::Private
{
  Q_DECLARE_PUBLIC(AccountsModel)

  Private(AccountsModel* qq, QObject* parent)
    : q_ptr(qq)
    , parentObject(parent)
    , updateOnBalanceChange(true)
  {
    Q_Q(AccountsModel);
    q->m_file = qobject_cast<MyMoneyFile*>(parent);
  }

  int loadSubAccounts(const QModelIndex parent, const QMap<QString, MyMoneyAccount>& list)
  {
    Q_Q(AccountsModel);
    const auto parentAccount = static_cast<TreeItem<MyMoneyAccount>*>(parent.internalPointer())->constDataRef();

    // create entries for the sub accounts
    const int subAccounts = parentAccount.accountCount();
    int itemCount = subAccounts;
    if (subAccounts > 0) {
      q->insertRows(0, subAccounts, parent);
      for (int row = 0; row < subAccounts; ++row) {
        const auto subAccountId = parentAccount.accountList().at(row);
        const auto subAccount = list.value(subAccountId);
        if (subAccount.id() == subAccountId) {
          q->updateNextObjectId(subAccount.id());
          const auto idx = q->index(row, 0, parent);
          static_cast<TreeItem<MyMoneyAccount>*>(idx.internalPointer())->dataRef() = subAccount;
          if (subAccount.value("PreferredAccount") == QLatin1String("Yes")
            && !subAccount.isClosed() ) {
            q->addFavorite(subAccountId);
            ++itemCount;
          }
          itemCount += loadSubAccounts(idx, list);

        } else {
          qDebug() << "Account" << parentAccount.id() << ": subaccount with ID" << subAccountId << "not found in list";
        }
      }
    }
    return itemCount;
  }

  bool isFavoriteIndex(const QModelIndex& idx) const
  {
    if (idx.isValid()) {
      TreeItem<MyMoneyAccount> *item;
      item = static_cast<TreeItem<MyMoneyAccount>*>(idx.internalPointer());
      if (item->constDataRef().id() == MyMoneyAccount::stdAccName(eMyMoney::Account::Standard::Favorite)) {
        return true;
      }
    }
    return false;
  }

  MyMoneyMoney adjustedBalance(const MyMoneyMoney& amount, const MyMoneyAccount& account)
  {
    switch(account.accountGroup()) {
      case eMyMoney::Account::Type::Liability:
      case eMyMoney::Account::Type::Expense:
      case eMyMoney::Account::Type::Equity:
        return -amount;
      default:
        break;
    }
    return amount;
  }

  MyMoneyMoney calculateTotalValue(QModelIndex idx)
  {
    Q_Q(AccountsModel);
    MyMoneyMoney result = idx.data(eMyMoney::Model::AccountValueRole).value<MyMoneyMoney>();
    const auto rows = q->rowCount(idx);
    for (int row = 0; row < rows; ++row) {
      QModelIndex subIdx = q->index(row, 0, idx);
      result += calculateTotalValue(subIdx);
    }
    q->setData(idx, QVariant::fromValue(result), eMyMoney::Model::AccountTotalValueRole);
    return result;
  }

  MyMoneyMoney netWorth() const
  {
    Q_Q(const AccountsModel);
    return q->assetIndex().data(eMyMoney::Model::AccountTotalValueRole).value<MyMoneyMoney>()
         + q->liabilityIndex().data(eMyMoney::Model::AccountTotalValueRole).value<MyMoneyMoney>();
  }

  MyMoneyMoney profitLoss() const
  {
    Q_Q(const AccountsModel);
    return q->incomeIndex().data(eMyMoney::Model::AccountTotalValueRole).value<MyMoneyMoney>()
         + q->expenseIndex().data(eMyMoney::Model::AccountTotalValueRole).value<MyMoneyMoney>();
  }

  struct DefaultAccounts {
    eMyMoney::Account::Standard groupType;
    eMyMoney::Account::Type     accountType;
    const char*                 description;
  };
  const QVector<DefaultAccounts> defaults = {
    { eMyMoney::Account::Standard::Favorite,  eMyMoney::Account::Type::Asset,     I18N_NOOP("Favorite")},
    { eMyMoney::Account::Standard::Asset,     eMyMoney::Account::Type::Asset,     I18N_NOOP("Asset accounts") },
    { eMyMoney::Account::Standard::Liability, eMyMoney::Account::Type::Liability, I18N_NOOP("Liability accounts") },
    { eMyMoney::Account::Standard::Income,    eMyMoney::Account::Type::Income,    I18N_NOOP("Income categories") },
    { eMyMoney::Account::Standard::Expense,   eMyMoney::Account::Type::Expense,   I18N_NOOP("Expense categories") },
    { eMyMoney::Account::Standard::Equity,    eMyMoney::Account::Type::Equity,    I18N_NOOP("Equity accounts") },
  };

  AccountsModel*                q_ptr;
  QObject*                      parentObject;
  QHash<QString, MyMoneyMoney>  balance;
  QHash<QString, MyMoneyMoney>  value;
  QHash<QString, MyMoneyMoney>  totalValue;
  bool                          updateOnBalanceChange;
  QColor                        positiveScheme;
  QColor                        negativeScheme;
};

AccountsModel::AccountsModel(QObject* parent)
  : MyMoneyModel<MyMoneyAccount>(parent, QStringLiteral("A"), AccountsModel::ID_SIZE)
  , d(new Private(this, parent))
{
  // validate that our assumptions about the order of the
  // groups in the model are still correct
  Q_ASSERT(d->defaults[0].groupType == eMyMoney::Account::Standard::Favorite);
  Q_ASSERT(d->defaults[1].groupType == eMyMoney::Account::Standard::Asset);
  Q_ASSERT(d->defaults[2].groupType == eMyMoney::Account::Standard::Liability);
  Q_ASSERT(d->defaults[3].groupType == eMyMoney::Account::Standard::Income);
  Q_ASSERT(d->defaults[4].groupType == eMyMoney::Account::Standard::Expense);
  Q_ASSERT(d->defaults[5].groupType == eMyMoney::Account::Standard::Equity);

  setObjectName(QLatin1String("AccountsModel"));

  // force creation of empty account structure
  unload();
}

AccountsModel::~AccountsModel()
{
}

int AccountsModel::columnCount(const QModelIndex& parent) const
{
  Q_UNUSED(parent);
  return Column::MaxColumns;
}

QVariant AccountsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if(orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch(section) {
      case Column::AccountName:
        return i18n("Name");
      case Column::Type:
        return i18n("Type");
      case Column::Tax:
        return i18nc("Column heading for category in tax report", "Tax");
      case Column::Vat:
        return i18nc("Column heading for VAT category", "VAT");
      case Column::CostCenter:
        return i18nc("Column heading for Cost Center", "CC");
      case Column::TotalBalance:
        return i18n("Balance");
      case Column::PostedValue:
        return i18n("Posted Value");
      case Column::TotalPostedValue:
        return i18n("Total Value");
      case Column::Number:
        return i18n("Number");
      case Column::SortCode:
        return i18n("SortCode");
      default:
        return QVariant();
    }
  }
  return QAbstractItemModel::headerData(section, orientation, role);
}

QVariant AccountsModel::data(const QModelIndex& idx, int role) const
{
  if (!idx.isValid())
    return QVariant();
  if (idx.row() < 0 || idx.row() >= rowCount(idx.parent()))
    return QVariant();

  const MyMoneyAccount& account = static_cast<TreeItem<MyMoneyAccount>*>(idx.internalPointer())->constDataRef();

  if (d->isFavoriteIndex(idx.parent())) {
    const auto accountIdx = indexById(account.id());
    const auto subIdx = index(accountIdx.row(), idx.column(), accountIdx.parent());
    return data(subIdx, role);
  }

  MyMoneyAccount tradingCurrency;
  switch(role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
      switch(idx.column()) {
        case Column::AccountName:
          // make sure to never return any displayable text for the dummy entry
          if (!account.id().isEmpty()) {
            return account.name();
          }
          break;

        case Column::Type:
          return account.accountTypeToString(account.accountType());

        case Column::Tax:
          break;

        case Column::Vat:
          if (!account.value("VatAccount").isEmpty()) {
            const auto vatAccount = itemById(account.value("VatAccount"));
            return vatAccount.name();

            // VAT Rate
          } else if (!account.value("VatRate").isEmpty()) {
            const auto vatRate = MyMoneyMoney(account.value("VatRate")) * MyMoneyMoney(100, 1);
            return QString::fromLatin1("%1 %").arg(vatRate.formatMoney(QString(), 1));
          }
          break;

        case Column::CostCenter:
          break;

        case Column::TotalBalance:
          return QVariant();

        case Column::Balance:
          {
            auto security = MyMoneyFile::instance()->currency(account.currencyId());
            const auto prec = MyMoneyMoney::denomToPrec(account.fraction());
            return d->adjustedBalance(account.balance(), account).formatMoney(security.tradingSymbol(), prec);
          }

        case Column::PostedValue:
          {
            const auto baseCurrency = MyMoneyFile::instance()->baseCurrency();
            return d->adjustedBalance(account.postedValue(), account).formatMoney(baseCurrency.tradingSymbol(), MyMoneyMoney::denomToPrec(baseCurrency.smallestAccountFraction()));
          }

        case Column::TotalPostedValue:
        {
          const auto baseCurrency = MyMoneyFile::instance()->baseCurrency();
          return d->adjustedBalance(account.totalPostedValue(), account).formatMoney(baseCurrency.tradingSymbol(), MyMoneyMoney::denomToPrec(baseCurrency.smallestAccountFraction()));
        }

        case Column::Number:
          return account.number();

        case Column::SortCode:
          return account.value("iban");

        default:
          break;
      }
      break;

    case Qt::TextAlignmentRole:
      switch (idx.column()) {
        case AccountsModel::Column::Vat:
        case AccountsModel::Column::Balance:
        case AccountsModel::Column::PostedValue:
        case AccountsModel::Column::TotalBalance:
        case AccountsModel::Column::TotalPostedValue:
          return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        default:
          break;
      }
      return QVariant(Qt::AlignLeft | Qt::AlignVCenter);

    case Qt::ForegroundRole:
      switch(idx.column()) {
        case Column::Balance:
          return d->adjustedBalance(account.balance(), account).isNegative() ? d->negativeScheme : d->positiveScheme;

        case Column::PostedValue:
          return d->adjustedBalance(account.postedValue(), account).isNegative() ? d->negativeScheme : d->positiveScheme;

        case Column::TotalPostedValue:
          return d->adjustedBalance(account.totalPostedValue(), account).isNegative() ? d->negativeScheme : d->positiveScheme;
      }
      break;

    case Qt::FontRole:
      {
        QFont font;
        // display top level account groups in bold
        if (!idx.parent().isValid()) {
          font.setBold(true);
        }
        // display the names of closed accounts with strikeout font
        // all others without
        if (account.isClosed() != font.strikeOut()) {
          font.setStrikeOut(account.isClosed());
        }
        return font;
      }
      break;

    case Qt::DecorationRole:
      switch (idx.column()) {
        case AccountsModel::Column::AccountName:
          if (d->isFavoriteIndex(idx)) {
            return Icons::get(Icons::Icon::ViewBankAccount);
          } else {
            const bool isReconciledAccount = false;
            return QIcon(account.accountPixmap(isReconciledAccount));
          }
          break;

        case AccountsModel::Column::Tax:
          if (account.value("Tax").toLower() == "yes") {
            return Icons::get(Icons::Icon::DialogOK);
          }
          break;

        case AccountsModel::Column::CostCenter:
          if (account.isCostCenterRequired()) {
            return Icons::get(Icons::Icon::DialogOK);
          }
          break;

        default:
          break;
      }
      break;

    case eMyMoney::Model::IdRole:
      return account.id();

    case eMyMoney::Model::AccountTypeRole:
      return static_cast<int>(account.accountType());

    case eMyMoney::Model::AccountIsClosedRole:
      return account.isClosed();

    case eMyMoney::Model::AccountFullNameRole:
      return indexToHierarchicalName(idx, true);

    case eMyMoney::Model::AccountDisplayOrderRole:
      // is it a normal account, make it high
      if (idx.parent().isValid())
        return 99;
      // otherwise sort in the group order of the model
      return idx.row();

    case eMyMoney::Model::AccountFractionRole:
      return account.fraction();

    case eMyMoney::Model::AccountParentIdRole:
      return account.parentAccountId();

    case eMyMoney::Model::AccountTotalValueRole:
      return QVariant::fromValue(account.totalPostedValue());

    case eMyMoney::Model::AccountValueRole:
      return QVariant::fromValue(account.postedValue());

    case eMyMoney::Model::AccountBalanceRole:
      return QVariant::fromValue(account.balance());

    case eMyMoney::Model::AccountCurrencyIdRole:
      return account.currencyId();
  }
  return QVariant();
}

bool AccountsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if(!index.isValid()) {
    return false;
  }

  // check if something is performed on a favorite entry
  // and skip right away
  if (d->isFavoriteIndex(index.parent())) {
    return true;
  }

  MyMoneyAccount& account = static_cast<TreeItem<MyMoneyAccount>*>(index.internalPointer())->dataRef();
  switch(role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
#if 0
      switch(index.column()) {
        case Column::Name:
          account.setName(value.toString());
          rc = true;
          break;
        default:
          break;
      }
#endif
      break;
    case eMyMoney::Model::AccountBalanceRole:
      account.setBalance(value.value<MyMoneyMoney>());
      return true;

    case eMyMoney::Model::AccountValueRole:
      account.setPostedValue(value.value<MyMoneyMoney>());
      return true;

    case eMyMoney::Model::AccountTotalValueRole:
      account.setTotalPostedValue(value.value<MyMoneyMoney>());
      return true;

    default:
      if (role >= Qt::UserRole) {
        qDebug() << "setData(" << index.row() << index.column() << ")" << value << role;
      }
      break;
  }
  return QAbstractItemModel::setData(index, value, role);
}

void AccountsModel::setColorScheme(AccountsModel::ColorScheme scheme, const QColor& color)
{
  switch(scheme) {
    case Positive:
      d->positiveScheme = color;
      break;
    case Negative:
      d->negativeScheme = color;
      break;
  }
}

void AccountsModel::clearModelItems()
{
  MyMoneyModel<MyMoneyAccount>::clearModelItems();

  // create the account groups with favorite entry as the first thing
  int row = 0;
  insertRows(0, static_cast<int>(eMyMoney::Account::Standard::MaxGroups));
  foreach(auto baseAccount, d->defaults) {
    MyMoneyAccount account;
    account.setName(i18n(baseAccount.description));
    account.setAccountType(baseAccount.accountType);
    auto newAccount = MyMoneyAccount(MyMoneyAccount::stdAccName(baseAccount.groupType), account);
    static_cast<TreeItem<MyMoneyAccount>*>(index(row, 0).internalPointer())->dataRef() = newAccount;
    ++row;
  }
}

QModelIndex AccountsModel::favoriteIndex() const
{
  return index(0, 0);
}

QModelIndex AccountsModel::assetIndex() const
{
  return index(1, 0);
}

QModelIndex AccountsModel::liabilityIndex() const
{
  return index(2, 0);
}

QModelIndex AccountsModel::incomeIndex() const
{
  return index(3, 0);
}

QModelIndex AccountsModel::expenseIndex() const
{
  return index(4, 0);
}

QModelIndex AccountsModel::equityIndex() const
{
  return index(5, 0);
}

void AccountsModel::load(const QMap<QString, MyMoneyAccount>& list)
{
  beginResetModel();
  // first get rid of any existing entries
  clearModelItems();

  int itemCount = 0;
  foreach(auto baseAccount, d->defaults) {
    ++itemCount;
    // we have nothing to do for favorites
    if (baseAccount.groupType == eMyMoney::Account::Standard::Favorite)
      continue;
    const auto account = list.value(MyMoneyAccount::stdAccName(baseAccount.groupType));
    if (account.id() == MyMoneyAccount::stdAccName(baseAccount.groupType)) {
      const auto idx = indexById(account.id());
      static_cast<TreeItem<MyMoneyAccount>*>(idx.internalPointer())->dataRef() = account;
      itemCount += d->loadSubAccounts(idx, list);
    } else {
      qDebug() << "Baseaccount for" << MyMoneyAccount::stdAccName(baseAccount.groupType) << "not found in list";
    }
  }

  // and don't count loading as a modification
  setDirty(false);

  endResetModel();

  emit modelLoaded();

  qDebug() << "Model for \"A\" loaded with" << itemCount << "items";
}

QList<MyMoneyAccount> AccountsModel::itemList() const
{
  QList<MyMoneyAccount> list;
  // never search in the first row which is favorites
  QModelIndexList indexes = match(assetIndex(), eMyMoney::Model::IdRole, m_idLeadin, -1, Qt::MatchStartsWith | Qt::MatchRecursive);
  for (int row = 0; row < indexes.count(); ++row) {
    const MyMoneyAccount& account = static_cast<TreeItem<MyMoneyAccount>*>(indexes.value(row).internalPointer())->constDataRef();
    if (!account.id().startsWith("AStd"))
      list.append(account);
  }
  return list;
}

QModelIndex AccountsModel::indexById(const QString& id) const
{
  // never search in the first row which is favorites
  const QModelIndexList indexes = match(assetIndex(), eMyMoney::Model::IdRole, id, 1, Qt::MatchFixedString | Qt::MatchRecursive);
  if (indexes.isEmpty())
    return QModelIndex();
  return indexes.first();
}

QModelIndexList AccountsModel::indexListByName(const QString& name) const
{
  // never search in the first row which is favorites
  return match(index(1, 0), Qt::DisplayRole, name, 1, Qt::MatchFixedString | Qt::MatchCaseSensitive);
}

void AccountsModel::addFavorite(const QString& id)
{
  // no need to do anything if it is already listed
  // bypass our own indexById as it does not return favorites
  const auto favoriteIdx = MyMoneyModel<MyMoneyAccount>::indexById(MyMoneyAccount::stdAccName(d->defaults[0].groupType));

  // check if the favorite is already present, if not add it
  const QModelIndexList indexes = match(index(0, 0, favoriteIdx), eMyMoney::Model::IdRole, id, 1, Qt::MatchFixedString | Qt::MatchRecursive);
  if (indexes.isEmpty()) {
    const auto count = rowCount(favoriteIdx);
    // we append a single row at the end
    const bool dirty = isDirty();
    insertRows(count, 1, favoriteIdx);
    const auto idx = index(count, 0, favoriteIdx);
    MyMoneyAccount subAccount(id, MyMoneyAccount());
    static_cast<TreeItem<MyMoneyAccount>*>(idx.internalPointer())->dataRef() = subAccount;
    // don't modify the dirty flag here. This is done elsewhere.
    setDirty(dirty);
  }
}

void AccountsModel::removeFavorite(const QString& id)
{
  // no need to do anything if it is not listed
  // bypass our own indexById as it does not return favorites
  const auto favoriteIdx = MyMoneyModel<MyMoneyAccount>::indexById(MyMoneyAccount::stdAccName(d->defaults[0].groupType));

  // check if the favorite is already present, if not add it
  const QModelIndexList indexes = match(index(0, 0, favoriteIdx), eMyMoney::Model::IdRole, id, 1, Qt::MatchFixedString | Qt::MatchRecursive);
  if (!indexes.isEmpty()) {
    const QModelIndex& idx = indexes.first();
    // we remove a single row
    const bool dirty = isDirty();
    removeRows(idx.row(), 1, favoriteIdx);
    endRemoveRows();
    // don't modify the dirty flag here. This is done elsewhere.
    setDirty(dirty);
  }
}

int AccountsModel::processItems(Worker *worker)
{
  // make sure to work only on real entries and not on favorites
  return MyMoneyModel<MyMoneyAccount>::processItems(worker, match(assetIndex(), eMyMoney::Model::IdRole, m_idLeadin, -1, Qt::MatchStartsWith | Qt::MatchRecursive));
}

QString AccountsModel::indexToHierarchicalName(const QModelIndex& _idx, bool includeStandardAccounts) const
{
  QString rc;
  auto idx(_idx);

  if (idx.isValid()) {
    do {
      const MyMoneyAccount& acc = static_cast<TreeItem<MyMoneyAccount>*>(idx.internalPointer())->constDataRef();
      if (!rc.isEmpty())
        rc = MyMoneyAccount::accountSeparator() + rc;
      rc = acc.name() + rc;
      idx = idx.parent();
    } while (idx.isValid() && (includeStandardAccounts || idx.parent().isValid()));
  }
  return rc;
}

QString AccountsModel::accountIdToHierarchicalName(const QString& accountId, bool includeStandardAccounts) const
{
  return indexToHierarchicalName(indexById(accountId), includeStandardAccounts);
}

QString AccountsModel::accountNameToId(const QString& category, eMyMoney::Account::Type type) const
{
  QString id;

  // search the category in the expense accounts and if it is not found, try
  // to locate it in the income accounts in case the type is not provided
  if (type == eMyMoney::Account::Type::Unknown
   || type == eMyMoney::Account::Type::Expense) {
    id = itemByName(category, expenseIndex()).name();
  }

  if ((id.isEmpty() && type == eMyMoney::Account::Type::Unknown)
   || type == eMyMoney::Account::Type::Income) {
    id = itemByName(category, incomeIndex()).name();
   }

  return id;
}

void AccountsModel::setupAccountFractions()
{
  QModelIndexList indexes = match(assetIndex(), eMyMoney::Model::IdRole, m_idLeadin, -1, Qt::MatchStartsWith | Qt::MatchRecursive);
  MyMoneySecurity currency;
  for (int row = 0; row < indexes.count(); ++row) {
    MyMoneyAccount& account = static_cast<TreeItem<MyMoneyAccount>*>(indexes.value(row).internalPointer())->dataRef();
    if (account.currencyId() != currency.id())
      currency = MyMoneyFile::instance()->currency(account.currencyId());
    account.fraction(currency);
  }
}


void AccountsModel::updateAccountBalances(const QHash<QString, MyMoneyMoney>& balances)
{
  const MyMoneyFile* file = MyMoneyFile::instance();
  const MyMoneySecurity baseCurrency = file->baseCurrency();

  beginResetModel();

  // suppress single updates while processing whole batch
  d->updateOnBalanceChange = false;

  bool approximate = false;
  for(auto it = balances.constBegin(); it != balances.constEnd(); ++it) {
    auto accountIdx = indexById(it.key());
    MyMoneyAccount& account = static_cast<TreeItem<MyMoneyAccount>*>(accountIdx.internalPointer())->dataRef();
    account.setBalance(it.value());

    // now calculate the value, but only for balances differing from null
    bool needConvert = false;
    MyMoneyMoney accountValue(it.value());
    QList<MyMoneyPrice> prices;
    if (!it.value().isZero()) {
      MyMoneySecurity security(baseCurrency);
      if (account.isInvest()) {
        security = file->currency(account.currencyId());
        if (!security.id().isEmpty()) {
          prices += file->price(account.currencyId(), security.tradingCurrency());
          if (security.tradingCurrency() != baseCurrency.id()) {
            MyMoneySecurity sec = file->currency(security.tradingCurrency());
            if (!sec.id().isEmpty()) {
              prices += file->price(sec.id(), baseCurrency.id());
            } else {
              qDebug() << security.tradingCurrency() << "not found";
              approximate = true;
            }
          }
        }
        needConvert = true;
      } else if (account.currencyId() != baseCurrency.id()) {
        security = file->currency(account.currencyId());
        if (!security.id().isEmpty()) {
          prices += file->price(account.currencyId(), baseCurrency.id());
        } else {
          qDebug() << security.id() << "not found";
          approximate = true;
        }
        needConvert = true;
      }
    }

    if (needConvert) {
      QList<MyMoneyPrice>::const_iterator it_p;
      QString securityID = account.currencyId();
      for (it_p = prices.constBegin(); it_p != prices.constEnd(); ++it_p) {
        accountValue = (accountValue * (MyMoneyMoney::ONE / (*it_p).rate(securityID))).convertPrecision(m_file->security(securityID).pricePrecision());
        if ((*it_p).from() == securityID)
          securityID = (*it_p).to();
        else
          securityID = (*it_p).from();
      }
      accountValue = accountValue.convert(m_file->baseCurrency().smallestAccountFraction());
    }
    account.setPostedValue(accountValue);
  }

  // now that we have all values, we can calculate the total values in the parent accounts
  MyMoneyMoney netWorth = d->netWorth();
  MyMoneyMoney profit = d->profitLoss();

  for (int row = assetIndex().row(); row < rowCount(); ++row) {
    d->calculateTotalValue(index(row, 0));
  }
  // turn on update on balance change
  d->updateOnBalanceChange = true;

  MyMoneyMoney newNetWorth = d->netWorth();
  if (netWorth != newNetWorth)
    emit netWorthChanged(newNetWorth, approximate);

  MyMoneyMoney newProfit = d->profitLoss();
  if (profit != newProfit)
    emit profitLossChanged(newProfit, approximate);

  endResetModel();
}

void AccountsModel::addAccount(MyMoneyAccount& account)
{
  auto parentIdx = indexById(account.parentAccountId());
  if (parentIdx.isValid()) {
    auto rows = rowCount(parentIdx);
    insertRow(rows, parentIdx);
    auto idx = index(rows, 0, parentIdx);
    MyMoneyAccount item(nextId(), account);
    static_cast<TreeItem<MyMoneyAccount>*>(idx.internalPointer())->dataRef() = item;
    account = item;
    emit dataChanged(idx, idx);
    setDirty();
  }
}
