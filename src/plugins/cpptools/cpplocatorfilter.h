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

#ifndef CPPLOCATORFILTER_H
#define CPPLOCATORFILTER_H

#include "searchsymbols.h"

#include <locator/ilocatorfilter.h>

namespace Core {
class EditorManager;
}

namespace CppTools {
namespace Internal {

class CppModelManager;

class CppLocatorFilter : public Locator::ILocatorFilter
{
    Q_OBJECT
public:
    CppLocatorFilter(CppModelManager *manager, Core::EditorManager *editorManager);
    ~CppLocatorFilter();

    QString displayName() const { return tr("Classes and Methods"); }
    QString id() const { return QLatin1String("Classes and Methods"); }
    Priority priority() const { return Medium; }
    QList<Locator::FilterEntry> matchesFor(const QString &entry);
    void accept(Locator::FilterEntry selection) const;
    void refresh(QFutureInterface<void> &future);

protected:
    SearchSymbols search;

private slots:
    void onDocumentUpdated(CPlusPlus::Document::Ptr doc);
    void onAboutToRemoveFiles(const QStringList &files);

private:
    CppModelManager *m_manager;
    Core::EditorManager *m_editorManager;

    struct Info {
        Info(): dirty(true) {}
        Info(CPlusPlus::Document::Ptr doc): doc(doc), dirty(true) {}

        CPlusPlus::Document::Ptr doc;
        QList<ModelItemInfo> items;
        bool dirty;
    };

    QMap<QString, Info> m_searchList;
    QList<ModelItemInfo> m_previousResults;
    bool m_forceNewSearchList;
    QString m_previousEntry;
};

} // namespace Internal
} // namespace CppTools

#endif // CPPLOCATORFILTER_H
