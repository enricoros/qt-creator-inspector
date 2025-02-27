/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "designercontext.h"
#include "designerconstants.h"
#include <coreplugin/uniqueidmanager.h>
#include <coreplugin/coreconstants.h>

#include <QWidget>

namespace Designer {
namespace Internal {

DesignerContext::DesignerContext(QWidget *widget) : Core::IContext(widget),
    m_widget(widget)
{
    Core::UniqueIDManager *idMan = Core::UniqueIDManager::instance();
    m_context << idMan->uniqueIdentifier(Designer::Constants::C_FORMEDITOR)
              << idMan->uniqueIdentifier(Core::Constants::C_EDITORMANAGER)
              << idMan->uniqueIdentifier(Core::Constants::C_DESIGN_MODE);

}

DesignerContext::~DesignerContext()
{

}

QList<int> DesignerContext::context() const
{
    return m_context;
}

QWidget *DesignerContext::widget()
{
    return m_widget;
}

void DesignerContext::setWidget(QWidget *widget)
{
    m_widget = widget;
}

}
}

