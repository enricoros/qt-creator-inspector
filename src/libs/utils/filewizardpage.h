/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#ifndef FILEWIZARDPAGE_H
#define FILEWIZARDPAGE_H

#include "utils_global.h"

#include <QtGui/QWizardPage>

namespace Utils {

struct FileWizardPagePrivate;

/**
 * Standard wizard page for a single file letting the user choose name
 * and path. Sets the "FileNames" QWizard field.
 *
 * The name and path labels can be changed. By default they are simply "Name:"
 * and "Path:".
 */
class QTCREATOR_UTILS_EXPORT FileWizardPage : public QWizardPage
{
    Q_OBJECT
    Q_DISABLE_COPY(FileWizardPage)
    Q_PROPERTY(QString path READ path WRITE setPath DESIGNABLE true)
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName DESIGNABLE true)
public:
    explicit FileWizardPage(QWidget *parent = 0);
    virtual ~FileWizardPage();

    QString fileName() const;
    QString path() const;

    virtual bool isComplete() const;

    void setFileNameLabel(const QString &label);
    void setPathLabel(const QString &label);

    // Validate a base name entry field (potentially containing extension)
    static bool validateBaseName(const QString &name, QString *errorMessage = 0);

signals:
    void activated();
    void pathChanged();

public slots:
    void setPath(const QString &path);
    void setFileName(const QString &name);

private slots:
    void slotValidChanged();
    void slotActivated();

protected:
    virtual void changeEvent(QEvent *e);

private:
    FileWizardPagePrivate *m_d;
};

} // namespace Utils

#endif // FILEWIZARDPAGE_H
