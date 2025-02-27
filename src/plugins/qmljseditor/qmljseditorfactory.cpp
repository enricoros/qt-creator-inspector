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

#include "qmljseditorfactory.h"
#include "qmljseditor.h"
#include "qmljseditoractionhandler.h"
#include "qmljseditorconstants.h"
#include "qmljseditorplugin.h"

#include <coreplugin/editormanager/editormanager.h>

#include <QtCore/QFileInfo>
#include <QtCore/QDebug>

using namespace QmlJSEditor::Internal;
using namespace QmlJSEditor::Constants;

QmlJSEditorFactory::QmlJSEditorFactory(QObject *parent)
  : Core::IEditorFactory(parent)
{
    m_mimeTypes
            << QLatin1String(QmlJSEditor::Constants::QML_MIMETYPE)
            << QLatin1String(QmlJSEditor::Constants::JS_MIMETYPE)
            ;
}

QmlJSEditorFactory::~QmlJSEditorFactory()
{
}

QString QmlJSEditorFactory::id() const
{
    return QLatin1String(C_QMLJSEDITOR_ID);
}

QString QmlJSEditorFactory::displayName() const
{
    return tr(C_QMLJSEDITOR_DISPLAY_NAME);
}


Core::IFile *QmlJSEditorFactory::open(const QString &fileName)
{
    Core::IEditor *iface = Core::EditorManager::instance()->openEditor(fileName, id());
    if (!iface) {
        qWarning() << "QmlEditorFactory::open: openEditor failed for " << fileName;
        return 0;
    }
    return iface->file();
}

Core::IEditor *QmlJSEditorFactory::createEditor(QWidget *parent)
{
    QmlJSTextEditor *rc = new QmlJSTextEditor(parent);
    QmlJSEditorPlugin::instance()->initializeEditor(rc);
    return rc->editableInterface();
}

QStringList QmlJSEditorFactory::mimeTypes() const
{
    return m_mimeTypes;
}
