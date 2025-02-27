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

#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H


#include <QtCore/QObject>
#include <QtCore/QList>

QT_BEGIN_NAMESPACE
class QString;
class QAbstractItemModel;
class QDialog;
QT_END_NAMESPACE

namespace QmlDesigner {

class IPlugin;

class PluginManagerPrivate;

// PluginManager: Loads the plugin libraries on demand "as lazy as
// possible", that is, directories are scanned and
// instances are created only when  instances() is called.

class PluginManager
{
    Q_DISABLE_COPY(PluginManager)
public:
    typedef QList<IPlugin *> IPluginList;

    PluginManager();
    ~PluginManager();

    void setPluginPaths(const QStringList &paths);

    IPluginList instances();

    QDialog *createAboutPluginDialog(QWidget *parent);

private:
    // Convenience to create a model for an "About Plugins"
    // dialog. Forces plugin initialization.
    QAbstractItemModel *createModel(QObject *parent = 0);

    PluginManagerPrivate *m_d;
};

} // namespace QmlDesigner
#endif // PLUGINMANAGER_H
