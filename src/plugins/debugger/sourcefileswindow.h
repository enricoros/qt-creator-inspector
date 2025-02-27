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

#ifndef DEBUGGER_SOURCEFILEWINDOW_H
#define DEBUGGER_SOURCEFILEWINDOW_H

#include <QtGui/QTreeWidget>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE
class QComboBox;
class QModelIndex;
class QStandardItemModel;
QT_END_NAMESPACE

namespace Debugger {
namespace Internal {

class SourceFilesModel;

class SourceFilesWindow : public QTreeView
{
    Q_OBJECT

public:
    SourceFilesWindow(QWidget *parent = 0);

    void setSourceFiles(const QMap<QString, QString> &sourceFiles);
    void removeAll();

signals:
    void reloadSourceFilesRequested();
    void fileOpenRequested(QString file);

private slots:
    void sourceFileActivated(const QModelIndex &index);
    void setAlternatingRowColorsHelper(bool on) { setAlternatingRowColors(on); }

private:
    void contextMenuEvent(QContextMenuEvent *ev);
    SourceFilesModel *m_model;
};

} // namespace Internal
} // namespace Debugger

#endif // DEBUGGER_SOURCEFILEWINDOW_H

