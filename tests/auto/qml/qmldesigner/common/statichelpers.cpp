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

static void testMessageOutput(QtMsgType type, const char *msg)
 {
     switch (type) {
     case QtDebugMsg:
#ifdef QDEBUG_IN_TESTS
         std::fprintf(stderr, "Debug: %s\n", msg);
#endif // QDEBUG_IN_TESTS
         break;
     case QtWarningMsg:
#ifdef WARNINGS_IN_TESTS
         std::fprintf(stderr, "Warning: %s\n", msg);
#endif // WARNINGS_IN_TESTS
         break;
     case QtCriticalMsg:
         std::fprintf(stderr, "Critical: %s\n", msg);
         break;
     case QtFatalMsg:
         std::fprintf(stderr, "Fatal: %s\n", msg);
         break;
     }
 }

static ModelNode addChildNode(const ModelNode &parentNode, const QString &typeName, int major, int minor, const QString &parentProperty)
{
    ModelNode newNode = parentNode.view()->createModelNode(typeName, major, minor);
    parentNode.nodeListProperty(parentProperty).reparentHere(newNode);
    return newNode;
}

static QString bareTemplate("import Qt 4.6\n"
                            "Item { id: parentItem;"
                            "  %1"
                            "}");
static QString contentsTemplate(bareTemplate.arg("Text { id: textChild; x:10; y: 10; text: \"%1\"; %2 }"));


void printErrors(const QList<QmlError> &errors, const QString &fileName)
{
    if (errors.isEmpty())
        return;

    qDebug() << "Error loading file \"" << fileName << "\":";

    foreach (const QmlError &error, errors) {
        qDebug() << error.line() << ":" << error.column() << ": " << error.description();
    }
}

// TODO: this need to e updated for states
static bool compareProperty(const AbstractProperty &property1, const AbstractProperty &property2)
{
    return (property1.name() == property2.name());
//            && (property1.value().type() == property2.value().type());
//            && (property1.value() == property2.value()));
}

// TODO: this need to e updated for states
static bool compareTree(const ModelNode &node1, const ModelNode &node2)
{
    if (!node1.isValid() || !node2.isValid()) {
        return false;
    }

    if (node1.type() != node2.type()) {
        return false;
    }

    // Compare properties
    {
        const QList<AbstractProperty> propList1 = node1.properties();
        const QList<AbstractProperty> propList2 = node2.properties();

        QList<AbstractProperty>::const_iterator iter1 = propList1.constBegin();
        QList<AbstractProperty>::const_iterator iter2 = propList2.constBegin();
        for (;
             iter1 != propList1.constEnd() && iter2 != propList2.constEnd();
             iter1++, iter2++) {
            if (!compareProperty(*iter1, *iter2))
                return false;
        }

        if (iter1 != propList1.constEnd() || iter2 != propList2.constEnd())
            return false;
    }

    // Compare list of childs
    {
        const QList<ModelNode> childList1 = node1.allDirectSubModelNodes();
        const QList<ModelNode> childList2 = node2.allDirectSubModelNodes();

        QList<ModelNode>::const_iterator iter1;
        QList<ModelNode>::const_iterator iter2;
        for (iter1 = childList1.constBegin(), iter2 = childList2.constBegin();
             iter1 != childList1.constEnd() && iter2 != childList2.constEnd();
             iter1++, iter2++) {
            if (!compareTree((*iter1), (*iter2)))
                return false;
        }

        if (iter1 != childList1.constEnd() || iter2 != childList2.constEnd())
            return false;
    }
    return true;
}

//void load(const QString &data, Model *&model, ByteArrayModifier *&modifier)
//{
//    model = 0;
//    QByteArray bytes = data.toLatin1();
//    QBuffer file(&bytes, 0);
//    QVERIFY(file.open(QIODevice::ReadOnly));
//    QList<QmlError> errors;
//    QString fileText(file.readAll());
//    modifier = ByteArrayModifier::create(QString(fileText));
//    model = Model::create(modifier, QUrl(), &errors);
//
//    if (!errors.isEmpty()) {
//        printErrors(errors, "<inline>");
//    }
//
//    file.close();
//}

//void reload(const QString &data, ByteArrayModifier *modifier)
//{
//    modifier->setText(data);
//}

//static Model* create(const QString& document)
//{
//    ByteArrayModifier* modifier = 0;
//    Model *model = 0;
//
//    load(document, model, modifier);
//
//    if (modifier && model)
//        modifier->setParent(model);
//
//    return model;
//}
