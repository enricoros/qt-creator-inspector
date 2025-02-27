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

#include "designerconstants.h"
#include "editorwidget.h"
#include "formeditorw.h"
#include "formwindoweditor.h"
#include "formwindowfile.h"
#include "formwindowhost.h"
#include "faketoolbar.h"

#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectnodes.h>
#include <projectexplorer/nodesvisitor.h>
#include <projectexplorer/project.h>
#include <projectexplorer/session.h>

#include <utils/qtcassert.h>
#include <coreplugin/icore.h>
#include <coreplugin/editormanager/editormanager.h>

#include <QtDesigner/QDesignerFormWindowInterface>
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerFormWindowManagerInterface>
#include <QtDesigner/QDesignerPropertyEditorInterface>
#include <QtDesigner/QDesignerWidgetDataBaseInterface>
#include "qt_private/formwindowbase_p.h"
#include "qt_private/qtresourcemodel_p.h"
#include "qt_private/qdesigner_integration_p.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QByteArray>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryFile>
#include <QtCore/QDebug>
#include <QtGui/QToolBar>
#include <QtGui/QDockWidget>

using namespace Designer;
using namespace Designer::Internal;
using namespace Designer::Constants;
using namespace SharedTools;
using ProjectExplorer::NodesVisitor;
using ProjectExplorer::ProjectNode;
using ProjectExplorer::FolderNode;
using ProjectExplorer::FileNode;

class QrcFilesVisitor : public NodesVisitor
{
public:
    QStringList qrcFiles() const;

    void visitProjectNode(ProjectNode *node);
    void visitFolderNode(FolderNode *node);
private:
    QStringList m_qrcFiles;
};

QStringList QrcFilesVisitor::qrcFiles() const
{
    return m_qrcFiles;
}

void QrcFilesVisitor::visitProjectNode(ProjectNode *projectNode)
{
    visitFolderNode(projectNode);
}

void QrcFilesVisitor::visitFolderNode(FolderNode *folderNode)
{
    foreach (const FileNode *fileNode, folderNode->fileNodes()) {
        if (fileNode->fileType() == ProjectExplorer::ResourceType)
            m_qrcFiles.append(fileNode->path());
    }
}


FormWindowEditor::FormWindowEditor(QDesignerFormWindowInterface *form,
                                   QObject *parent)
  : Core::IEditor(parent),
    m_formWindow(form),
    m_file(0),
    m_host(new FormWindowHost(form)),
    m_editorWidget(new EditorWidget(m_host)),
    m_toolBar(0),
    m_sessionNode(0),
    m_sessionWatcher(0),
    m_fakeToolBar(new FakeToolBar(this, toolBar()))
{
    m_containerWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(m_containerWidget);
    m_containerWidget->setLayout(layout);

    layout->addWidget(m_fakeToolBar);
    layout->addWidget(m_editorWidget);
    layout->setStretch(0,0);
    layout->setStretch(1,1);
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->setContentsMargins(0,0,0,0);

    if (Designer::Constants::Internal::debug)
        qDebug() << Q_FUNC_INFO << form << parent;

    connect(m_host, SIGNAL(changed()), this, SIGNAL(changed()));

    connect(form, SIGNAL(toolChanged(int)), m_editorWidget, SLOT(toolChanged(int)));

    m_editorWidget->activate();
}

void FormWindowEditor::setFile(Core::IFile *file)
{
    if (m_file) {
        disconnect(m_file, SIGNAL(changed()), this, SIGNAL(changed()));
        disconnect(m_file, SIGNAL(changed()), this, SLOT(updateResources()));
    }

    m_file = file;
    m_formWindow->setFileName(file->fileName());

    if (m_file) {
        connect(m_file, SIGNAL(changed()), this, SIGNAL(changed()));
        connect(m_file, SIGNAL(changed()), this, SLOT(updateResources()));
    }
}

FormWindowEditor::~FormWindowEditor()
{
    // Close: Delete the Designer form window via embedding widget
    delete m_toolBar;
    delete m_fakeToolBar;
    delete m_host;
    delete m_editorWidget;
    if (Designer::Constants::Internal::debug)
        qDebug() << Q_FUNC_INFO << m_displayName;
    if (m_sessionNode && m_sessionWatcher) {
        m_sessionNode->unregisterWatcher(m_sessionWatcher);
        delete m_sessionWatcher;
    }
}

void FormWindowEditor::setContext(QList<int> context)
{
    m_context = context;
}

bool FormWindowEditor::createNew(const QString &contents)
{
    if (Designer::Constants::Internal::debug)
        qDebug() << Q_FUNC_INFO << contents.size() << "chars";

    if (!m_formWindow)
        return false;

    m_formWindow->setContents(contents);
    if (!m_formWindow->mainContainer())
        return false;

    if (qdesigner_internal::FormWindowBase *fw = qobject_cast<qdesigner_internal::FormWindowBase *>(m_formWindow))
        fw->setDesignerGrid(qdesigner_internal::FormWindowBase::defaultDesignerGrid());

    initializeResources();

    return true;
}

bool FormWindowEditor::open(const QString &fileName /*= QString()*/)
{
    if (Designer::Constants::Internal::debug)
        qDebug() << Q_FUNC_INFO << fileName;

    if (fileName.isEmpty()) {
        setDisplayName(tr("untitled"));
    } else {
        const QFileInfo fi(fileName);
        const QString fileName = fi.absoluteFilePath();

        QFile file(fileName);
        if (!file.exists())
            return false;

        if (!fi.isReadable())
            return false;

        if (!file.open(QIODevice::ReadOnly|QIODevice::Text))
            return false;

        m_formWindow->setFileName(fileName);
        m_formWindow->setContents(&file);
        file.close();
        if (!m_formWindow->mainContainer())
            return false;
        m_formWindow->setDirty(false);

        initializeResources(fileName);

        setDisplayName(fi.fileName());

    }

    return true;
}
void FormWindowEditor::initializeResources(const QString &fileName /*= QString()*/)
{
    ProjectExplorer::ProjectExplorerPlugin *pe = ProjectExplorer::ProjectExplorerPlugin::instance();
    ProjectExplorer::SessionManager *session = pe->session();

    m_sessionNode = session->sessionNode();
    m_sessionWatcher = new ProjectExplorer::NodesWatcher();

    connect(m_sessionWatcher, SIGNAL(filesAdded()), this, SLOT(updateResources()));
    connect(m_sessionWatcher, SIGNAL(filesRemoved()), this, SLOT(updateResources()));
    connect(m_sessionWatcher, SIGNAL(foldersAdded()), this, SLOT(updateResources()));
    connect(m_sessionWatcher, SIGNAL(foldersRemoved()), this, SLOT(updateResources()));
    m_sessionNode->registerWatcher(m_sessionWatcher);

    if (qdesigner_internal::FormWindowBase *fw = qobject_cast<qdesigner_internal::FormWindowBase *>(m_formWindow)) {
        QtResourceSet *rs = fw->resourceSet();
        m_originalUiQrcPaths = rs->activeQrcPaths();
    }

    if (!fileName.isEmpty())
        emit opened(fileName);

    updateResources();

    QDesignerFormWindowManagerInterface *fwm = FormEditorW::instance()->designerEditor()->formWindowManager();
    fwm->setActiveFormWindow(m_formWindow);

    emit changed();
}

void FormWindowEditor::updateResources()
{
    if (qdesigner_internal::FormWindowBase *fw = qobject_cast<qdesigner_internal::FormWindowBase *>(m_formWindow)) {
        ProjectExplorer::ProjectExplorerPlugin *pe = ProjectExplorer::ProjectExplorerPlugin::instance();
        // filename could change in the meantime.
        ProjectExplorer::Project *project = pe->session()->projectForFile(m_file->fileName());

        qdesigner_internal::FormWindowBase::SaveResourcesBehaviour behaviour = qdesigner_internal::FormWindowBase::SaveAll;
        QtResourceSet *rs = fw->resourceSet();
        if (project) {
            ProjectNode *root = project->rootProjectNode();
            QrcFilesVisitor qrcVisitor;
            root->accept(&qrcVisitor);

            rs->activateQrcPaths(qrcVisitor.qrcFiles());
            behaviour = qdesigner_internal::FormWindowBase::SaveOnlyUsedQrcFiles;
        } else {
            rs->activateQrcPaths(m_originalUiQrcPaths);
        }
        fw->setSaveResourcesBehaviour(behaviour);
    }
}

void FormWindowEditor::slotOpen(const QString &fileName)
{
    open(fileName);
}

void FormWindowEditor::slotSetDisplayName(const QString &title)
{
    if (Designer::Constants::Internal::debug)
        qDebug() <<  Q_FUNC_INFO << title;
    setDisplayName(title);
}

bool FormWindowEditor::duplicateSupported() const
{
    return false;
}

Core::IEditor *FormWindowEditor::duplicate(QWidget *)
{
    return 0;
}

Core::IFile *FormWindowEditor::file()
{
    return m_file;
}

QString FormWindowEditor::id() const
{
    return QLatin1String(FORMEDITOR_ID);
}

QString FormWindowEditor::displayName() const
{
    return m_displayName;
}

void FormWindowEditor::setDisplayName(const QString &title)
{
    m_displayName = title;
}

QWidget *FormWindowEditor::toolBar()
{
    if (!m_toolBar)
        m_toolBar = FormEditorW::instance()->createEditorToolBar();
    return m_toolBar;
}

QByteArray FormWindowEditor::saveState() const
{
    return QByteArray();
}

bool FormWindowEditor::restoreState(const QByteArray &/*state*/)
{
    return true;
}

QList<int> FormWindowEditor::context() const
{
    return m_context;
}

QWidget *FormWindowEditor::widget()
{
    return m_containerWidget;
}


QDesignerFormWindowInterface *FormWindowEditor::formWindow() const
{
    return m_formWindow;
}

QWidget *FormWindowEditor::integrationContainer()
{
    return m_host->integrationContainer();
}

void FormWindowEditor::updateFormWindowSelectionHandles(bool state)
{
    m_host->updateFormWindowSelectionHandles(state);
}

void FormWindowEditor::activate()
{
    m_editorWidget->activate();
}

void FormWindowEditor::resetToDefaultLayout()
{
    m_editorWidget->resetToDefaultLayout();
}

QString FormWindowEditor::contextHelpId() const
{
    const QDesignerFormEditorInterface *core = FormEditorW::instance()->designerEditor();
#if QT_VERSION > 0x040500
    // Present from Qt 4.5.1 onwards. This will show the class documentation
    // scrolled to the current property.
    const qdesigner_internal::QDesignerIntegration *integration =
            qobject_cast<const qdesigner_internal::QDesignerIntegration*>(core->integration());
    if (integration)
        return integration->contextHelpId();
    return QString();
#else
    // Pre 4.5.1. This will show the class documentation.
    QObject *o = core->propertyEditor()->object();
    if (!o)
        return QString();
    const QDesignerWidgetDataBaseInterface *db = core->widgetDataBase();
    const int dbIndex = db->indexOfObject(o, true);
    if (dbIndex == -1)
        return QString();
    QString className = db->item(dbIndex)->name();
    if (className == QLatin1String("Line"))
        className = QLatin1String("QFrame");
    else if (className == QLatin1String("Spacer"))
        className = QLatin1String("QSpacerItem");
    else if (className == QLatin1String("QLayoutWidget"))
        className = QLatin1String("QLayout");

    return className;
#endif
}

QString FormWindowEditor::contents() const
{
    if (!m_formWindow)
        return QString();
//  Activate once all Qt branches around have integrated 4.5.2
//  (Kinetic)
/*
#if QT_VERSION > 0x040501
    // Quiet save as of Qt 4.5.2
    qdesigner_internal::FormWindowBase *fwb = qobject_cast<qdesigner_internal::FormWindowBase *>(m_formWindow);
    QTC_ASSERT(fwb, return QString::null);
    return fwb->fileContents();
#else
    return m_formWindow->contents();
#endif
*/
    return m_formWindow->contents();
}

QDockWidget* const* FormWindowEditor::dockWidgets() const
{
    return m_editorWidget->dockWidgets();
}

bool FormWindowEditor::isLocked() const
{
    return m_editorWidget->isLocked();
}

void FormWindowEditor::setLocked(bool locked)
{
    m_editorWidget->setLocked(locked);
}
