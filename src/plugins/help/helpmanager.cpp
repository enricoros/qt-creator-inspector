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
#include "helpmanager.h"

#include "helpplugin.h"

#include <QtCore/QUrl>
#include <QtCore/QString>

using namespace Help;
using namespace Help::Internal;

HelpManager::HelpManager(HelpPlugin* plugin)
    : m_plugin(plugin)
{
}

void HelpManager::openHelpPage(const QString& url)
{
    m_plugin->handleHelpRequest(url);
}

void HelpManager::openContextHelpPage(const QString& url)
{
    m_plugin->openContextHelpPage(url);
}

void HelpManager::registerDocumentation(const QStringList &fileNames)
{
    if (m_plugin) {
        m_plugin->setFilesToRegister(fileNames);
        emit helpPluginUpdateDocumentation();
    }
}
