/***************************************************************************
                             ofxpartner.h
                             ----------
    begin                : Fri Jan 23 2009
    copyright            : (C) 2009 by Thomas Baumgart
    email                : Thomas Baumgart <ipwizard@users.sourceforge.net>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OFXPARTNER_H
#define OFXPARTNER_H

// ----------------------------------------------------------------------------
// QT Includes


#include <QObject>
#include <q3http.h>
//Added by qt3to4:
#include <Q3ValueList>

// ----------------------------------------------------------------------------
// KDE Includes

#include <kurl.h>
namespace KIO
{
  class Job;
  class TransferJob;
}

// ----------------------------------------------------------------------------
// Project Includes

#include <libofx/libofx.h>

namespace OfxPartner
{
  /**
    * setup the directory where the files will be stored.
    * @a dir must end with a '/' and must exist. Call this
    * before any other of the functions of OfxPartner. The
    * default will be to store the files in the current
    * directory.
    */
  void setDirectory(const QString& dir);

  void ValidateIndexCache(void);
  OfxFiServiceInfo ServiceInfo(const QString& fipid);
  QStringList BankNames(void);
  QStringList FipidForBank(const QString& bank);

}

class OfxHttpRequest : public QObject
{
  Q_OBJECT
public:
  OfxHttpRequest(const QString& method, const KUrl &url, const QByteArray &postData, const QMap<QString, QString>& metaData, const KUrl& dst, bool showProgressInfo=true);
  virtual ~OfxHttpRequest() {}

  Q3Http::Error error(void) const { return m_error; }

protected slots:
  void slotOfxFinished(int, bool);

private:
  Q3Http*	      m_job;
  KUrl          m_dst;
  Q3Http::Error  m_error;

};

class OfxHttpsRequest : public QObject
{
Q_OBJECT
public:
  OfxHttpsRequest(const QString& method, const KUrl &url, const QByteArray &postData, const QMap<QString, QString>& metaData, const KUrl& dst, bool showProgressInfo=true);
  virtual ~OfxHttpsRequest();

  Q3Http::Error error(void) const { return m_error; }

protected slots:
  void slotOfxFinished(KIO::Job*);
  void slotOfxData(KIO::Job*,const QByteArray&);
  void slotOfxConnected(KIO::Job*);

private:
  class Private;
  Private*          d;
  KUrl              m_dst;
  QFile             m_file;
  Q3Http::Error      m_error;
  KIO::TransferJob* m_job;
};
#endif // OFXPARTNER_H
