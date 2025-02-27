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

#ifndef DEBUGGER_REMOTEGDBADAPTER_H
#define DEBUGGER_REMOTEGDBADAPTER_H

#include "abstractgdbadapter.h"

namespace Debugger {
namespace Internal {

///////////////////////////////////////////////////////////////////////
//
// RemoteGdbAdapter
//
///////////////////////////////////////////////////////////////////////

class RemoteGdbAdapter : public AbstractGdbAdapter
{
    Q_OBJECT

public:
    RemoteGdbAdapter(GdbEngine *engine, int toolChainType, QObject *parent = 0);

    virtual DumperHandling dumperHandling() const;

    void startAdapter();
    void startInferior();
    void startInferiorPhase2();
    void interruptInferior();
    void shutdown();

private:
    Q_SLOT void readUploadStandardOutput();
    Q_SLOT void readUploadStandardError();
    Q_SLOT void uploadProcError(QProcess::ProcessError error);

    void handleSetTargetAsync(const GdbResponse &response);
    void handleFileExecAndSymbols(const GdbResponse &response);
    void handleTargetRemote(const GdbResponse &response);

    const int m_toolChainType;

    QProcess m_uploadProc;
};

} // namespace Internal
} // namespace Debugger

#endif // DEBUGGER_PLAINGDBADAPTER_H
