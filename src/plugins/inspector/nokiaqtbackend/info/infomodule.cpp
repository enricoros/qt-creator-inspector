/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009-2010 Enrico Ros
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

#include "infomodule.h"
#include "infopanel.h"
#include "../nokiaqtbackend.h"

using namespace Inspector::Internal;

InfoModule::InfoModule(NokiaQtBackend *backend, QObject *parent)
  : IBackendModule(backend, parent)
{
}

QString InfoModule::name() const
{
    return tr("Info (0.6)");
}

ModuleMenuEntries InfoModule::menuEntries() const
{
    ModuleMenuEntries entries;
    entries.append(ModuleMenuEntry(QStringList() << tr("Status"), UID_MODULE_INFO, 0, QIcon(":/inspector/info/menu-status.png")));
    return entries;
}

AbstractPanel *InfoModule::createPanel(int panelId)
{
    if (panelId != 0) {
        qWarning("InfoModule::createPanel: unhandled panel %d", panelId);
        return 0;
    }
    return new InfoPanel(this);
}
