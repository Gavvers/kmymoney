/*
    SPDX-FileCopyrightText: 2002-2003 Michael Edwardes <mte@users.sourceforge.net>
    SPDX-FileCopyrightText: 2002-2016 Thomas Baumgart <tbaumgart@kde.org>
    SPDX-FileCopyrightText: 2002 Kevin Tambascio <ktambascio@users.sourceforge.net>
    SPDX-FileCopyrightText: 2017 Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mymoneyutils.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QDate>
#include <QProcess>
#include <QRegularExpression>
#include <QTimeZone>

// ----------------------------------------------------------------------------
// KDE Headers

#include <KLocalizedString>

// ----------------------------------------------------------------------------
// Project Includes

#include "journalmodel.h"
#include "mymoneyaccount.h"
#include "mymoneyexception.h"
#include "mymoneyfile.h"
#include "mymoneymoney.h"
#include "mymoneyschedule.h"
#include "mymoneysecurity.h"
#include "mymoneysplit.h"
#include "mymoneytransaction.h"

QString MyMoneyUtils::getFileExtension(QString strFileName)
{
    QString strTemp;
    if (!strFileName.isEmpty()) {
        //find last . deliminator
        int nLoc = strFileName.lastIndexOf('.');
        if (nLoc != -1) {
            strTemp = strFileName.right(strFileName.length() - (nLoc + 1));
            return strTemp.toUpper();
        }
    }
    return strTemp;
}

QString MyMoneyUtils::formatMoney(const MyMoneyMoney& val,
                                  const MyMoneyAccount& acc,
                                  const MyMoneySecurity& sec,
                                  bool showThousandSeparator)
{
    return val.formatMoney(sec.tradingSymbol(),
                           val.denomToPrec(acc.fraction()),
                           showThousandSeparator);
}

QString MyMoneyUtils::formatMoney(const MyMoneyMoney& val,
                                  const MyMoneySecurity& sec,
                                  bool showThousandSeparator)
{
    return val.formatMoney(sec.tradingSymbol(),
                           val.denomToPrec(sec.smallestAccountFraction()),
                           showThousandSeparator);
}

QString MyMoneyUtils::dateToString(const QDate& date)
{
    if (!date.isNull() && date.isValid())
        return date.toString(Qt::ISODate);

    return QString();
}

QDate MyMoneyUtils::stringToDate(const QString& str)
{
    if (!str.isEmpty()) {
        QDate date = QDate::fromString(str, Qt::ISODate);
        if (!date.isNull() && date.isValid())
            return date;
    }
    return {};
}

QString MyMoneyUtils::dateTimeToString(const QDateTime& dateTime)
{
    return QDateTime(dateTime.date(), dateTime.time(), QTimeZone(dateTime.offsetFromUtc())).toString(Qt::ISODate);
}

QDateTime MyMoneyUtils::stringToDateTime(const QString& str)
{
    if (!str.isEmpty()) {
        QDateTime dateTime = QDateTime::fromString(str, Qt::ISODate);
        if (!dateTime.isNull() && dateTime.isValid())
            return dateTime;
    }
    return {};
}

QString MyMoneyUtils::QStringEmpty(const QString& val)
{
    if (!val.isEmpty())
        return QString(val);

    return QString();
}

unsigned long MyMoneyUtils::extractId(const QString& txt)
{
    int pos;
    unsigned long rc = 0;

    pos = txt.indexOf(QRegularExpression("\\d+"));
    if (pos != -1) {
        rc = txt.mid(pos).toInt();
    }
    return rc;
}

void MyMoneyUtils::dissectTransaction(const MyMoneyTransaction& transaction, const MyMoneySplit& split, MyMoneySplit& assetAccountSplit, QList<MyMoneySplit>& feeSplits, QList<MyMoneySplit>& interestSplits, MyMoneySecurity& security, MyMoneySecurity& currency, eMyMoney::Split::InvestmentTransactionType& transactionType)
{
    // collect the splits. split references the stock account and should already
    // be set up. assetAccountSplit references the corresponding asset account (maybe
    // empty), feeSplits is the list of all expenses and interestSplits
    // the list of all incomes
    assetAccountSplit = MyMoneySplit(); // set to none to check later if it was assigned
    auto file = MyMoneyFile::instance();
    Q_FOREACH (const auto tsplit, transaction.splits()) {
        auto acc = file->account(tsplit.accountId());
        if (tsplit.id() == split.id()) {
            security = file->security(acc.currencyId());
        } else if (acc.accountGroup() == eMyMoney::Account::Type::Expense) {
            feeSplits.append(tsplit);
            // feeAmount += tsplit.value();
        } else if (acc.accountGroup() == eMyMoney::Account::Type::Income) {
            interestSplits.append(tsplit);
            // interestAmount += tsplit.value();
        } else {
            if (assetAccountSplit == MyMoneySplit()) // first asset Account should be our requested brokerage account
                assetAccountSplit = tsplit;
            else if (tsplit.value().isNegative())  // the rest (if present) is handled as fee or interest
                feeSplits.append(tsplit);              // and shouldn't be allowed to override assetAccountSplit
            else if (tsplit.value().isPositive())
                interestSplits.append(tsplit);
        }
    }

    // determine transaction type
    transactionType = split.investmentTransactionType();
    if (transactionType == eMyMoney::Split::InvestmentTransactionType::UnknownTransactionType) {
        transactionType = eMyMoney::Split::InvestmentTransactionType::BuyShares;
    }

    currency.setTradingSymbol("???");
    try {
        currency = file->security(transaction.commodity());
    } catch (const MyMoneyException &) {
    }
}

static QString dateTimeFormat;
static QString timeFormat;
static QString dateFormat;

QString MyMoneyUtils::formatDateTime(const QDateTime& dt)
{
    if (dateTimeFormat.isEmpty()) {
        dateTimeFormat = QLocale().dateTimeFormat(QLocale::ShortFormat);
        if (!dateTimeFormat.contains(QLatin1String("yyyy")) && dateTimeFormat.contains(QLatin1String("yy"))) {
            dateTimeFormat.replace(QLatin1String("yy"), QLatin1String("yyyy"));
        }
    }
    return dt.toString(dateTimeFormat);
}

QString MyMoneyUtils::formatTime(const QTime& time)
{
    if (timeFormat.isEmpty()) {
        timeFormat = QLocale().timeFormat(QLocale::LongFormat);
    }
    return time.toString(timeFormat);
}

QString MyMoneyUtils::formatDate(const QDate& date, QLocale::FormatType formatType)
{
    if (dateFormat.isEmpty()) {
        dateFormat = QLocale().dateFormat(formatType);
        if (!dateFormat.contains(QLatin1String("yyyy")) && dateFormat.contains(QLatin1String("yy"))) {
            dateFormat.replace(QLatin1String("yy"), QLatin1String("yyyy"));
        }
    }
    return date.toString(dateFormat);
}

void MyMoneyUtils::clearFormatCaches()
{
    dateTimeFormat.clear();
    timeFormat.clear();
    dateFormat.clear();
}

QString MyMoneyUtils::paymentMethodToString(eMyMoney::Schedule::PaymentType paymentType)
{
    return i18n(MyMoneySchedule::paymentMethodToString(paymentType));
}

modifyTransactionWarnLevel_t MyMoneyUtils::transactionWarnLevel(const QStringList& journalEntryIds)
{
    modifyTransactionWarnLevel_t level = NoWarning;

    const auto file = MyMoneyFile::instance();
    const auto journalModel = file->journalModel();
    const auto rows = journalModel->rowCount();

    QString lastTransactionId;

    for (auto row = 0; row < rows; ++row) {
        const auto idx = journalModel->index(row, 0);
        if (idx.data(eMyMoney::Model::JournalTransactionIdRole).toString() != lastTransactionId) {
            if (journalEntryIds.contains(idx.data(eMyMoney::Model::IdRole).toString())) {
                modifyTransactionWarnLevel_t rc = NoWarning;
                try {
                    const auto journalEntry = journalModel->itemByIndex(idx);
                    for (const auto& split : journalEntry.transaction().splits()) {
                        auto acc = file->account(split.accountId());
                        if (acc.isClosed())
                            rc = OneAccountClosed;
                        else if (split.reconcileFlag() == eMyMoney::Split::State::Frozen)
                            rc = OneSplitFrozen;
                        else if (split.reconcileFlag() == eMyMoney::Split::State::Reconciled && rc < OneSplitReconciled)
                            rc = OneSplitReconciled;
                    }
                } catch (const MyMoneyException& e) {
                    qDebug() << "Exception in MyMoneyUtils::transactionWarnLevel():" << e.what();
                }
                lastTransactionId = idx.data(eMyMoney::Model::JournalTransactionIdRole).toString();
                if (rc > level) {
                    level = rc;
                }
            }
        }
    }
    return level;
}

modifyTransactionWarnLevel_t MyMoneyUtils::transactionWarnLevel(const QString& journalEntryId)
{
    return transactionWarnLevel(QStringList(journalEntryId));
}

QString MyMoneyUtils::convertWildcardToRegularExpression(const QString& pattern)
{
    QString rc;
    bool insideBrackets = false;
    int pos = 0;
    int len = pattern.length();

    // insert an escape character if c == d
    auto escapeChar = [&](const QChar& d, const QChar& c) {
        if (c == d) {
            rc.append(QLatin1Char('\\'));
        }
    };

    while (pos < len) {
        bool skipInResult(false);
        const auto c = pattern[pos];
        if (insideBrackets) {
            if (c == QLatin1Char(']')) {
                insideBrackets = false;
            } else {
                escapeChar(QLatin1Char('.'), c);
                escapeChar(QLatin1Char('?'), c);
                escapeChar(QLatin1Char('*'), c);
            }
        } else {
            if (c == QLatin1Char('[')) {
                insideBrackets = true;
            } else if (c == QLatin1Char('?')) {
                rc.append(QLatin1Char('.'));
                skipInResult = true;
            } else if (c == QLatin1Char('*')) {
                rc.append(QLatin1Char('.'));
            } else {
                escapeChar(QLatin1Char('.'), c);
            }
        }
        if (!skipInResult) {
            rc.append(c);
        }
        ++pos;
    }
    return rc;
}

QString MyMoneyUtils::convertRegularExpressionToWildcard(const QString& pattern)
{
    QString rc;
    int pos = 0;
    int len = pattern.length();

    while (pos < len) {
        auto c = pattern[pos];
        if (c == QLatin1Char('\\')) {
            if ((pos + 1) < len) {
                c = pattern[++pos];
            }
        } else if (c == QLatin1Char('.')) {
            c = QLatin1Char('?');
            if ((pos + 1) < len) {
                if (pattern[pos + 1] == QLatin1Char('*')) {
                    c = pattern[++pos];
                }
            }
        }
        rc.append(c);
        ++pos;
    }
    return rc;
}
