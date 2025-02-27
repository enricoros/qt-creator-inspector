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

#include "componentnodeinstance.h"

#include <invalidnodeinstanceexception.h>
#include <QDeclarativeComponent>
#include <QDeclarativeContext>

namespace QmlDesigner {
namespace Internal {


ComponentNodeInstance::ComponentNodeInstance(QDeclarativeComponent *component)
   : ObjectNodeInstance(component)
{
}

QDeclarativeComponent *ComponentNodeInstance::component() const
{
    Q_ASSERT(qobject_cast<QDeclarativeComponent*>(object()));
    return static_cast<QDeclarativeComponent*>(object());
}

ComponentNodeInstance::Pointer ComponentNodeInstance::create(const NodeMetaInfo &/*metaInfo*/, QDeclarativeContext *context, QObject  *objectToBeWrapped)
{
    QDeclarativeComponent *component = 0;
    if (objectToBeWrapped)
        component = qobject_cast<QDeclarativeComponent *>(objectToBeWrapped);
    else
        component = new QDeclarativeComponent(context->engine());

    if (component == 0)
        throw InvalidNodeInstanceException(__LINE__, __FUNCTION__, __FILE__);


    Pointer instance(new ComponentNodeInstance(component));

    if (objectToBeWrapped)
        instance->setDeleteHeldInstance(false); // the object isn't owned

    instance->populateResetValueHash();

    return instance;
}

bool ComponentNodeInstance::hasContent() const
{
    return true;
}

void ComponentNodeInstance::setPropertyVariant(const QString &name, const QVariant &value)
{
    if (name == "__component_data") {
        QByteArray data(value.toByteArray());
        QByteArray imports;
        foreach(const Import &import, modelNode().model()->imports()) {
            imports.append(import.toString(true).toLatin1());
        }

        data.prepend(imports);

        component()->setData(data, nodeInstanceView()->model()->fileUrl());

    }
    if (component()->isError()) {
        qDebug() << value;
        foreach(const QDeclarativeError &error, component()->errors())
            qDebug() << error;
    }

}
} // Internal
} // QmlDesigner
