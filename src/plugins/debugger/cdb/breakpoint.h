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

#ifndef CDBCOREBREAKPOINTS_H
#define CDBCOREBREAKPOINTS_H

#include "cdbcom.h"

#include <QtCore/QString>
#include <QtCore/QList>

QT_BEGIN_NAMESPACE
class QDebug;
QT_END_NAMESPACE

namespace CdbCore {

/* CDB Break point data structure with utilities to
 * apply to engine and to retrieve them from the engine and comparison.
 * Can stop on 'sourcefile:line', function or address.
 * When/How many times it triggers can be influenced by
 * condition/ignorecount and 'oneshot'-flag. */

struct BreakPoint
{
    BreakPoint();

    int compare(const BreakPoint& rhs) const;

    void clear();
    void clearExpressionData();

    QString expression() const;

    // Apply parameters
    bool apply(IDebugBreakpoint2 *ibp, QString *errorMessage) const;
    // Convenience to add to a IDebugControl4
    bool add(CIDebugControl* debugControl,
             QString *errorMessage,
             unsigned long *id = 0,
             quint64 *address = 0) const;

    // Retrieve/parse breakpoints from the interfaces
    bool retrieve(IDebugBreakpoint2 *ibp, QString *errorMessage);
    bool parseExpression(const QString &expr);
    // Retrieve all breakpoints from the engine
    static bool getBreakPointCount(CIDebugControl* debugControl, ULONG *count, QString *errorMessage = 0);
    static bool getBreakPoints(CIDebugControl* debugControl, QList<BreakPoint> *bps, QString *errorMessage);
    // Control helpers
    static IDebugBreakpoint2 *breakPointById(CIDebugControl *ctl, unsigned long id, QString *errorMessage);
    static bool removeBreakPointById(CIDebugControl *ctl, unsigned long id, QString *errorMessage);
    static bool setBreakPointEnabledById(CIDebugControl *ctl, unsigned long id, bool enabled, QString *errorMessage);

    // Return a 'canonical' file (using '/' and capitalized drive letter)
    static QString normalizeFileName(const QString &f);
    static void clearNormalizeFileNameCache();

    QString fileName;       // short name of source file
    int lineNumber;     // line in source file
    QString funcName;       // name of containing function
    quint64 address;

    QString condition;      // condition associated with breakpoint
    unsigned long ignoreCount;    // ignore count associated with breakpoint
    bool oneShot;
    bool enabled;
};

QDebug operator<<(QDebug, const BreakPoint &bp);

inline bool operator==(const BreakPoint& b1, const BreakPoint& b2)
    { return b1.compare(b2) == 0; }
inline bool operator!=(const BreakPoint& b1, const BreakPoint& b2)
    { return b1.compare(b2) != 0; }
}

#endif // CDBCOREBREAKPOINTS_H
