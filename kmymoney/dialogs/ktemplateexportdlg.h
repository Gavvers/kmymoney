/***************************************************************************
                     ktemplateexportlg.cpp
                     ---------------------
    copyright        : (C) 2016 by Ralf Habacker <ralf.habacker@freenet.de>
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

#ifndef KTEMPLATEEXPORTDLG_H
#define KTEMPLATEEXPORTDLG_H

#include <QDialog>

namespace Ui {
class KTemplateExportDlg;
}

class KTemplateExportDlg : public QDialog
{
    Q_OBJECT

public:
    explicit KTemplateExportDlg(QWidget *parent = nullptr);
    ~KTemplateExportDlg();

    QString title() const;
    QString shortDescription() const;
    QString longDescription() const;

private:
    Ui::KTemplateExportDlg *ui;
};

#endif // KTEMPLATEEXPORTDLG_H
