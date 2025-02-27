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

#include "gccparser.h"
#include "projectexplorerconstants.h"
#include "taskwindow.h"

using namespace ProjectExplorer;

GccParser::GccParser()
{
    m_regExp.setPattern("^([^\\(\\)]+[^\\d]):(\\d+):(\\d+:)*(\\s(warning|error):)?\\s(.+)$");
    m_regExp.setMinimal(true);

    m_regExpIncluded.setPattern("^.*from\\s([^:]+):(\\d+)(,|:)$");
    m_regExpIncluded.setMinimal(true);

    m_regExpLinker.setPattern("^(\\S*)\\(\\S+\\):\\s(.+)$");
    m_regExpLinker.setMinimal(true);
}

void GccParser::stdError(const QString &line)
{
    QString lne = line.trimmed();
    if (m_regExpLinker.indexIn(lne) > -1) {
        QString description = m_regExpLinker.cap(2);
        emit addTask(TaskWindow::Task(TaskWindow::Error,
                                      description,
                                      m_regExpLinker.cap(1) /* filename */,
                                      -1 /* linenumber */,
                                      Constants::TASK_CATEGORY_COMPILE));
        return;
    } else if (m_regExp.indexIn(lne) > -1) {
        TaskWindow::Task task(TaskWindow::Unknown,
                              m_regExp.cap(6) /* description */,
                              m_regExp.cap(1) /* filename */,
                              m_regExp.cap(2).toInt() /* line number */,
                              Constants::TASK_CATEGORY_COMPILE);
        if (m_regExp.cap(5) == "warning")
            task.type = TaskWindow::Warning;
        else if (m_regExp.cap(5) == "error")
            task.type = TaskWindow::Error;

        emit addTask(task);
        return;
    } else if (m_regExpIncluded.indexIn(lne) > -1) {
        emit addTask(TaskWindow::Task(TaskWindow::Unknown,
                                      lne /* description */,
                                      m_regExpIncluded.cap(1) /* filename */,
                                      m_regExpIncluded.cap(2).toInt() /* linenumber */,
                                      Constants::TASK_CATEGORY_COMPILE));
        return;
    } else if (lne.startsWith(QLatin1String("collect2:")) ||
               lne.startsWith(QLatin1String("ERROR:")) ||
               lne == QLatin1String("* cpp failed")) {
        emit addTask(TaskWindow::Task(TaskWindow::Error,
                                      lne /* description */,
                                      QString() /* filename */,
                                      -1 /* linenumber */,
                                      Constants::TASK_CATEGORY_COMPILE));
        return;
    }
    IOutputParser::stdError(line);
}
