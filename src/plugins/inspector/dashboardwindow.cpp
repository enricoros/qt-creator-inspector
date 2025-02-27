/*************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009-2010 Enrico Ros
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

#include "dashboardwindow.h"
#include "ibackend.h"
#include "inspection.h"
#include "inspectorplugin.h"
#include "inspectorstyle.h"
#include "runcontrolwatcher.h"
#include "shareddebugger.h"
#include "tasksscroller.h"
#include <extensionsystem/pluginmanager.h>
#include <projectexplorer/applicationrunconfiguration.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/project.h>
#include <projectexplorer/session.h>
#include <utils/qtcassert.h>
#include <QtGui/QApplication>
#include <QtGui/QComboBox>
#include <QtGui/QGridLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QIcon>
#include <QtGui/QLabel>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPushButton>
#include <QtGui/QTableWidget>
#include <QtGui/QToolButton>

namespace Inspector {
namespace Internal {

const int ICON_SIZE(64);
const int ABOVE_HEADING_MARGIN(10);
const int ABOVE_CONTENTS_MARGIN(4);
const int BELOW_CONTENTS_MARGIN(16);
const int LEFT_MARGIN(16);

DashboardWindow::DashboardWindow(QWidget *parent)
  : QScrollArea(parent)
  , m_root(new QWidget(this))
{
    // We want a 900px wide widget with and the scrollbar at the side of the screen.
    m_root->setFixedWidth(900);

    m_layout = new QGridLayout(m_root);
    m_layout->setColumnMinimumWidth(0, ICON_SIZE + 4);
    m_layout->setSpacing(0);

    setWidget(m_root);
    setFrameStyle(QFrame::NoFrame);
    setWidgetResizable(true);
    m_root->setAutoFillBackground(false);
    viewport()->setAutoFillBackground(false);

    InspectorPlugin *plugin = InspectorPlugin::instance();

    // 1. create the Active Inspections widget
    {
        QWidget *widget = new QWidget;

        m_inspectionsLayout = new QVBoxLayout;
        m_inspectionsLayout->setContentsMargins(0, ABOVE_CONTENTS_MARGIN, 0, 0);
        m_inspectionsLayout->setSpacing(0);
        widget->setLayout(m_inspectionsLayout);

        m_noInspectionsLabel = new QLabel;
        m_noInspectionsLabel->setText(tr("No Inspection running. Start a new one on the tab below."));
        m_inspectionsLayout->addWidget(m_noInspectionsLabel);

        appendWrappedWidget(tr("Active Inspections"),
                            QIcon(":/inspector/images/icon-inspections.png"),
                            widget);
    }

    // 2. create the New Inspection widget
    {
        QWidget *panel = new QWidget;
        QGridLayout *grid = new QGridLayout(panel);
        grid->setMargin(0);
        grid->setSpacing(0);
        grid->setColumnMinimumWidth(0, LEFT_MARGIN);

        // 2.1 run a new Inspection (TODO: turn this into a parser of "Inspect %1 running %2 with %3")
        QWidget *runWidget = new QWidget;
        QHBoxLayout *runLayout = new QHBoxLayout(runWidget);
         runLayout->setMargin(0);
         runLayout->addWidget(new QLabel(tr("Inspect")));
        m_projectsCombo = new ProjectsComboBox;
         runLayout->addWidget(m_projectsCombo);
        m_devicesCombo = new DevicesComboBox;
         runLayout->addWidget(m_devicesCombo);
        m_runconfLabel = new QLabel(tr("running"));
         runLayout->addWidget(m_runconfLabel);
        m_runconfsCombo = new RunconfComboBox;
         runLayout->addWidget(m_runconfsCombo);
         runLayout->addWidget(new QLabel(tr("with")));
        m_backendsCombo = new BackendsComboBox;
         runLayout->addWidget(m_backendsCombo);
         runLayout->addWidget(new QLabel(tr("backend")));
         runLayout->addStretch();
        m_newRunButton = new QPushButton;
        m_newRunButton->setMaximumHeight(InspectorStyle::defaultComboHeight());
        m_newRunButton->setText(tr("Start"));
        m_newRunButton->setIcon(QIcon(":/projectexplorer/images/run_small.png"));
        connect(m_newRunButton, SIGNAL(clicked()),
                this, SLOT(slotStartNewTarget()));
         runLayout->addWidget(m_newRunButton);
        appendSubWidget(grid, runWidget, tr("Inspect a New Target")/*,
                        tr("Start a new Inspection on the selected project.")*/);

        connect(m_projectsCombo, SIGNAL(currentProjectChanged()),
                this, SLOT(slotProjectChanged()));
        connect(m_devicesCombo, SIGNAL(currentDeviceChanged()),
                this, SLOT(slotDeviceChanged()));
        connect(m_runconfsCombo, SIGNAL(currentRunconfChanged()),
                this, SLOT(slotRunconfChanged()));
        connect(m_backendsCombo, SIGNAL(currentBackendChanged()),
                this, SLOT(slotEvaluateNewTarget()));
        slotProjectChanged();
        slotDeviceChanged();
        slotRunconfChanged();

        // 2.2 attach to an existing Target
        QWidget *attWidget = new QWidget;
        QVBoxLayout *attLayout = new QVBoxLayout(attWidget);
         attLayout->setMargin(0);

        RunningTargetSelectorWidget *rcList = new RunningTargetSelectorWidget;
        connect(rcList, SIGNAL(inspectionTargetSelected(const InspectionTarget &)),
                this, SLOT(slotExistingTargetSelected(const InspectionTarget &)));
         attLayout->addWidget(rcList);

        QWidget *acPanel = new QWidget;
        QHBoxLayout *acLayout = new QHBoxLayout(acPanel);
         acLayout->setMargin(0);
         acLayout->addWidget(new QLabel(tr("With")));
        m_attBackends = new BackendsComboBox;
         acLayout->addWidget(m_attBackends);
         acLayout->addWidget(new QLabel(tr("backend")));
         acLayout->addStretch();
        m_attButton = new QPushButton;
        m_attButton->setMaximumHeight(InspectorStyle::defaultComboHeight());
        m_attButton->setText(tr("Attach"));
        m_attButton->setIcon(QIcon(":/projectexplorer/images/debugger_start_small.png"));
        m_attButton->setEnabled(false);
        connect(m_attButton, SIGNAL(clicked()),
               this, SLOT(slotStartExistingTarget()));
        connect(m_attButton, SIGNAL(clicked()),
               rcList, SLOT(deselect()));
         acLayout->addWidget(m_attButton);
         attLayout->addWidget(acPanel);

        appendSubWidget(grid, attWidget, tr("Inspect an Existing Target"));

        appendWrappedWidget(tr("Create a New Inspection"),
                            QIcon(":/inspector/images/icon-newinspection.png"),
                            panel);

        connect(m_attBackends, SIGNAL(currentBackendChanged()),
                this, SLOT(slotEvaluateExistingTarget()));

        // monitor shareddebugger state changes for changing the gui
        connect(plugin->sharedDebugger(), SIGNAL(acquirableChanged(bool)),
                this, SLOT(slotUpdateActionStatus()));
        slotUpdateActionStatus();
    }

    // create the Configure Backends widget
    {
        QWidget *widget = new QWidget;
        QGridLayout *grid = new QGridLayout(widget);
        grid->setMargin(0);
        grid->setSpacing(0);
        grid->setColumnMinimumWidth(0, LEFT_MARGIN);

        foreach (IBackendFactory *factory, BackendsComboBox::allFactories()) {
            QWidget *rowWidget = new QWidget;
            QHBoxLayout *rowLay = new QHBoxLayout(rowWidget);
            rowLay->setMargin(0);

            if (!factory->icon().isNull()) {
                QLabel *iconLabel = new QLabel;
                iconLabel->setFixedSize(32, 32);
                iconLabel->setPixmap(factory->icon().pixmap(32, 32));
                rowLay->addWidget(iconLabel);
            }

            // TODO
            QLabel *modulesLabel = new QLabel;
            modulesLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
            modulesLabel->setWordWrap(true);
            modulesLabel->setText(tr("modules: %1\nrequires: %2").arg("some, hardcoded, strings, fix, this").arg("exclusive-probe"));
            rowLay->addWidget(modulesLabel);

            QToolButton *btn = new QToolButton;
            btn->setIcon(QIcon(":/projectexplorer/images/rebuild_small.png"));
            if (factory->isConfigurable())
                connect(btn, SIGNAL(clicked()), factory, SLOT(configure()));
            else
                btn->setEnabled(false);
            rowLay->addWidget(btn);

            appendSubWidget(grid, rowWidget, factory->displayName());
        }

        appendWrappedWidget(tr("Configure Backends"),
                            QIcon(":/inspector/images/icon-configure.png"),
                            widget);
    }

    foreach (Inspection *inspection, plugin->inspections())
        slotInspectionAdded(inspection);
    connect(plugin, SIGNAL(inspectionAdded(Inspection*)),
            this, SLOT(slotInspectionAdded(Inspection*)));
    connect(plugin, SIGNAL(inspectionRemoved(Inspection*)),
            this, SLOT(slotInspectionRemoved(Inspection*)));
}

void DashboardWindow::newInspection(const InspectionTarget &target, IBackendFactory *factory)
{
    QTC_ASSERT(target.type != InspectionTarget::Undefined, return);
    QTC_ASSERT(factory, return);

    IBackend *backend = factory->createBackend(target);
    if (!backend) {
        qWarning("DashboardWindow::newInspection: factory refusal");
        return;
    }

    if (!backend->startInspection(target)) {
        qWarning("DashboardWindow::newInspection: can't start the target. inspection canceled");
        delete backend;
        return;
    }

    Inspection *inspection = new Inspection(backend);
    InspectorPlugin::instance()->addInspection(inspection);
}

void DashboardWindow::slotProjectChanged()
{
    ProjectExplorer::Project *project = m_projectsCombo->currentProject();
    m_devicesCombo->setProject(project);
    m_devicesCombo->setVisible(m_devicesCombo->count() > 1);
    m_projectsCombo->setEnabled(project);
    slotEvaluateNewTarget();
}

void DashboardWindow::slotDeviceChanged()
{
    ProjectExplorer::Target *device = m_devicesCombo->currentDevice();
    m_runconfsCombo->setDevice(device);
    m_runconfsCombo->setVisible(m_runconfsCombo->count() > 1);
    m_runconfLabel->setVisible(m_runconfsCombo->count() > 1);
    slotEvaluateNewTarget();
}

void DashboardWindow::slotRunconfChanged()
{
    ProjectExplorer::RunConfiguration *runconf = m_runconfsCombo->currentRunConfiguration();
    m_backendsCombo->setEnabled(runconf);
    slotEvaluateNewTarget();
}

void DashboardWindow::slotEvaluateNewTarget()
{
    m_runTarget.type = InspectionTarget::StartLocalRunConfiguration;
    m_runTarget.runConfiguration = qobject_cast<ProjectExplorer::LocalApplicationRunConfiguration *>(
            m_runconfsCombo->currentRunConfiguration());
    if (m_runTarget.runConfiguration)
        m_runTarget.displayName = m_runTarget.runConfiguration->displayName();

    IBackendFactory *factory = m_backendsCombo->currentFactory();
    m_newRunButton->setEnabled(factory && factory->available(m_runTarget));
}

void DashboardWindow::slotStartNewTarget()
{
    newInspection(m_runTarget, m_backendsCombo->currentFactory());
}

void DashboardWindow::slotExistingTargetSelected(const InspectionTarget &target)
{
    m_attTarget = target;
    slotEvaluateExistingTarget();
}

void DashboardWindow::slotEvaluateExistingTarget()
{
    IBackendFactory *attFactory = m_attBackends->currentFactory();
    m_attButton->setEnabled(attFactory && attFactory->available(m_attTarget));
}

void DashboardWindow::slotStartExistingTarget()
{
    newInspection(m_attTarget, m_attBackends->currentFactory());
}

void DashboardWindow::slotUpdateActionStatus()
{
    slotEvaluateNewTarget();
    slotEvaluateExistingTarget();
}

void DashboardWindow::slotInspectionAdded(Inspection *inspection)
{
    // create the RunningInspectionWidget
    m_inspections.append(inspection);
    RunningInspectionWidget *r = new RunningInspectionWidget(inspection);
    connect(r, SIGNAL(displayInspection(Inspection*)),
            this, SIGNAL(requestInspectionDisplay(Inspection*)));
    connect(r, SIGNAL(closeInspection(Inspection*)),
            this, SLOT(slotCloseInspection(Inspection*)));
    m_inspectionWidgets.append(r);
    m_inspectionsLayout->addWidget(r);

    // hide a label if have inspections
    if (!m_inspections.isEmpty())
        m_noInspectionsLabel->hide();
    slotUpdateActionStatus();
}

void DashboardWindow::slotInspectionRemoved(Inspection *removedInspection)
{
    // remove the RunningInspectionWidget
    int index = 0;
    foreach (Inspection *inspection, m_inspections) {
        if (inspection == removedInspection) {
            m_inspections.removeAt(index);
            QWidget *iWidget = m_inspectionWidgets.takeAt(index);
            m_inspectionsLayout->removeWidget(iWidget);
            delete iWidget;
            break;
        }
    }

    // show a label if no inspections
    if (m_inspections.isEmpty())
        m_noInspectionsLabel->show();
    slotUpdateActionStatus();
}

void DashboardWindow::slotCloseInspection(Inspection *inspection)
{
    InspectorPlugin::instance()->deleteInspection(inspection);
}

// keep this in sync with PanelsWidget::addPropertiesPanel in projectwindow.cpp
void DashboardWindow::appendWrappedWidget(const QString &title, const QIcon &icon,
                                          QWidget *widget)
{
    // icon:
    const int headerRow(m_layout->rowCount() - 1);
    m_layout->setRowStretch(headerRow, 0);

    if (!icon.isNull()) {
        QLabel *iconLabel = new QLabel(m_root);
        iconLabel->setPixmap(icon.pixmap(ICON_SIZE, ICON_SIZE));
        iconLabel->setContentsMargins(0, ABOVE_HEADING_MARGIN, 0, 0);
        m_layout->addWidget(iconLabel, headerRow, 0, 3, 1, Qt::AlignTop | Qt::AlignHCenter);
    }

    // name:
    QLabel *nameLabel = new QLabel(m_root);
    nameLabel->setText(title);
    nameLabel->setContentsMargins(0, ABOVE_HEADING_MARGIN, 0, 0);
    QFont f = nameLabel->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.6);
    nameLabel->setFont(f);
    m_layout->addWidget(nameLabel, headerRow, 1, 1, 1, Qt::AlignBottom | Qt::AlignLeft);

    // line:
    const int lineRow(headerRow + 1);
    m_layout->addWidget(InspectorStyle::createOnePixelBlackLine(m_root), lineRow, 1);

    // add the widget:
    const int widgetRow(lineRow + 1);
    widget->setContentsMargins(m_layout->columnMinimumWidth(0),
                               ABOVE_CONTENTS_MARGIN, 0,
                               BELOW_CONTENTS_MARGIN);
    widget->setParent(m_root);
    m_layout->addWidget(widget, widgetRow, 0, 1, 2);

    // stretch:
    const int stretchRow(widgetRow + 1);
    m_layout->setRowStretch(stretchRow, ABOVE_HEADING_MARGIN);
}

void DashboardWindow::appendSubWidget(QGridLayout *layout, QWidget *widget,
                                      const QString &title, const QString &subTitle)
{
    int insertionRow = layout->rowCount();
    if (!title.isEmpty()) {
        QLabel *label = new QLabel;
        label->setText(title);
        QFont f = label->font();
        f.setBold(true);
        f.setPointSizeF(f.pointSizeF() * 1.2);
        label->setFont(f);
        label->setContentsMargins(0, ABOVE_HEADING_MARGIN, 0, 0);

        layout->addWidget(label, insertionRow, 0, 1, 2);
        layout->setRowStretch(insertionRow, 0);
        ++insertionRow;

        if (!subTitle.isEmpty()) {
            QLabel *subLabel = new QLabel;
            subLabel->setText(subTitle);
            subLabel->setWordWrap(true);
            QFont smallFont = subLabel->font();
            smallFont.setPointSizeF(smallFont.pointSizeF() / 1.2);
            subLabel->setFont(smallFont);

            layout->addWidget(subLabel, insertionRow, 0, 1, 2);
            layout->setRowStretch(insertionRow, 0);
            ++insertionRow;
        }

        if (widget) {
            widget->setContentsMargins(0, ABOVE_HEADING_MARGIN, 0, 0);
            layout->addWidget(widget, insertionRow, 1, 1, 1);
        }
    } else if (widget)
        layout->addWidget(widget, insertionRow, 0, 1, 2);
}

//
// ProjectsComboBox
//
ProjectsComboBox::ProjectsComboBox(QWidget *parent)
  : QComboBox(parent)
{
    setMaximumHeight(InspectorStyle::defaultComboHeight());
    connect(this, SIGNAL(currentIndexChanged(int)),
            this, SIGNAL(currentProjectChanged()));

    ProjectExplorer::SessionManager *session = ProjectExplorer::ProjectExplorerPlugin::instance()->session();
    foreach(ProjectExplorer::Project *project, session->projects())
        add(project);
    connect(session, SIGNAL(projectAdded(ProjectExplorer::Project*)),
            this, SLOT(add(ProjectExplorer::Project*)));
    connect(session, SIGNAL(aboutToRemoveProject(ProjectExplorer::Project*)),
            this, SLOT(remove(ProjectExplorer::Project*)));
    connect(session, SIGNAL(startupProjectChanged(ProjectExplorer::Project*)),
            this, SLOT(activeChanged(ProjectExplorer::Project*)));
}

bool ProjectsComboBox::isEmpty() const
{
    return !count();
}

ProjectExplorer::Project *ProjectsComboBox::currentProject() const
{
    int index = currentIndex();
    if (index < 0 || index >= m_comboIndexToProject.size())
        return 0;
    return m_comboIndexToProject.at(index);
}

void ProjectsComboBox::add(ProjectExplorer::Project *project)
{
    ProjectExplorer::SessionManager *session = ProjectExplorer::ProjectExplorerPlugin::instance()->session();
    int index = session->projects().indexOf(project);
    m_comboIndexToProject.insert(index, project);
    insertItem(index, project->displayName());
}

void ProjectsComboBox::remove(ProjectExplorer::Project *project)
{
    int index = m_comboIndexToProject.indexOf(project);
    m_comboIndexToProject.removeAt(index);
    removeItem(index);
}

void ProjectsComboBox::activeChanged(ProjectExplorer::Project *project)
{
    int index = m_comboIndexToProject.indexOf(project);
    setCurrentIndex(index);
}

//
// DevicesComboBox
//
DevicesComboBox::DevicesComboBox(QWidget *parent)
  : QComboBox(parent)
  , m_project(0)
{
    setMaximumHeight(InspectorStyle::defaultComboHeight());
    connect(this, SIGNAL(currentIndexChanged(int)),
            this, SIGNAL(currentDeviceChanged()));
}

void DevicesComboBox::setProject(ProjectExplorer::Project *project)
{
    if (m_project == project)
        return;

    // remove links to previous project
    if (m_project) {
        disconnect(m_project, 0, this, 0);
        while (count())
            remove(itemData(0).value<ProjectExplorer::Target *>());
    }

    m_project = project;

    // link this project
    if (m_project) {
        foreach(ProjectExplorer::Target *device, m_project->targets())
            add(device);
        connect(m_project, SIGNAL(addedTarget(ProjectExplorer::Target*)),
                this, SLOT(add(ProjectExplorer::Target*)));
        connect(m_project, SIGNAL(removedTarget(ProjectExplorer::Target*)),
                this, SLOT(remove(ProjectExplorer::Target*)));
        connect(m_project, SIGNAL(activeTargetChanged(ProjectExplorer::Target*)),
                this, SLOT(activeChanged(ProjectExplorer::Target*)));
    }
}

ProjectExplorer::Target *DevicesComboBox::currentDevice() const
{
    if (currentIndex() < 0)
        return 0;
    return itemData(currentIndex()).value<ProjectExplorer::Target *>();
}

void DevicesComboBox::add(ProjectExplorer::Target *device)
{
    connect(device, SIGNAL(displayNameChanged()),
            this, SLOT(updateDisplayName()));
    addItem(device->displayName(), QVariant::fromValue(device));
    if (m_project->activeTarget() == device)
        setCurrentIndex(count() - 1);
}

void DevicesComboBox::remove(ProjectExplorer::Target *device)
{
    disconnect(device, 0, this, 0);
    removeItem(findData(QVariant::fromValue(device)));
}

void DevicesComboBox::activeChanged(ProjectExplorer::Target *device)
{
    setCurrentIndex(findData(QVariant::fromValue(device)));
}

void DevicesComboBox::updateDisplayName()
{
    ProjectExplorer::Target *device = static_cast<ProjectExplorer::Target *>(sender());
    setItemText(findData(QVariant::fromValue(device)), device->displayName());
}

//
// RunconfComboBox
//
RunconfComboBox::RunconfComboBox(QWidget *parent)
  : QComboBox(parent)
  , m_device(0)
{
    setMaximumHeight(InspectorStyle::defaultComboHeight());
    connect(this, SIGNAL(currentIndexChanged(int)),
            this, SIGNAL(currentRunconfChanged()));
}

void RunconfComboBox::setDevice(ProjectExplorer::Target *device)
{
    if (m_device == device)
        return;

    // remove previous links
    if (m_device) {
        disconnect(m_device, 0, this, 0);
        while (count())
            remove(itemData(0).value<ProjectExplorer::RunConfiguration *>());
    }

    m_device = device;

    // link this
    if (m_device) {
        foreach(ProjectExplorer::RunConfiguration *rc, m_device->runConfigurations())
            add(rc);
        connect(m_device, SIGNAL(addedRunConfiguration(ProjectExplorer::RunConfiguration*)),
                this, SLOT(add(ProjectExplorer::RunConfiguration*)));
        connect(m_device, SIGNAL(removedRunConfiguration(ProjectExplorer::RunConfiguration*)),
                this, SLOT(remove(ProjectExplorer::RunConfiguration*)));
        connect(m_device, SIGNAL(activeRunConfigurationChanged(ProjectExplorer::RunConfiguration*)),
                this, SLOT(activeChanged(ProjectExplorer::RunConfiguration*)));
    }
}

ProjectExplorer::RunConfiguration *RunconfComboBox::currentRunConfiguration() const
{
    if (currentIndex() < 0)
        return 0;
    return itemData(currentIndex()).value<ProjectExplorer::RunConfiguration*>();
}

void RunconfComboBox::add(ProjectExplorer::RunConfiguration *rc)
{
    connect(rc, SIGNAL(displayNameChanged()),
            this, SLOT(updateDisplayName()));
    addItem(rc->displayName(), QVariant::fromValue(rc));
    if (m_device->activeRunConfiguration() == rc)
        setCurrentIndex(count() - 1);
}

void RunconfComboBox::remove(ProjectExplorer::RunConfiguration *rc)
{
    disconnect(rc, 0, this, 0);
    removeItem(findData(QVariant::fromValue(rc)));
}

void RunconfComboBox::activeChanged(ProjectExplorer::RunConfiguration *rc)
{
    setCurrentIndex(findData(QVariant::fromValue(rc)));
}

void RunconfComboBox::updateDisplayName()
{
    ProjectExplorer::RunConfiguration *rc = static_cast<ProjectExplorer::RunConfiguration*>(sender());
    setItemText(findData(QVariant::fromValue(rc)), rc->displayName());
}

//
// BackendsComboBox
//
BackendsComboBox::BackendsComboBox(QWidget *parent)
  : QComboBox(parent)
{
    setMaximumHeight(InspectorStyle::defaultComboHeight());
    foreach (IBackendFactory *factory, allFactories())
        addItem(factory->displayName());
    connect(this, SIGNAL(currentIndexChanged(int)),
            this, SIGNAL(currentBackendChanged()));
}

IBackendFactory *BackendsComboBox::currentFactory() const
{
    int index = currentIndex();
    QList<IBackendFactory *> factories = allFactories();
    if (index < 0 || index >= factories.size())
        return 0;
    return factories.at(index);
}

QList<IBackendFactory *> BackendsComboBox::allFactories()
{
    return ExtensionSystem::PluginManager::instance()->getObjects<IBackendFactory>();
}

//
// RunningInspectionWidget
//
RunningInspectionWidget::RunningInspectionWidget(Inspection *inspection, QWidget *parent)
  : QWidget(parent)
  , m_inspection(inspection)
{
    QHBoxLayout *lay = new QHBoxLayout(this);
    lay->setMargin(3);

    QLabel *label1 = new QLabel;
    label1->setMinimumWidth(30);
    label1->setText(tr("# %1.").arg(inspection->inspectionModel()->monotonicId()));
    lay->addWidget(label1);

    m_connLabel = new QLabel;
    m_connLabel->setFixedSize(16, 16);
    lay->addWidget(m_connLabel);

    QLabel *label2 = new QLabel;
    label2->setMinimumWidth(160);
    label2->setText(tr("%1").arg(inspection->inspectionModel()->displayName()));
    connect(label2, SIGNAL(linkActivated(QString)),
            this, SLOT(slotDisplayClicked()));
    lay->addWidget(label2);

    m_tasksWidget = new TasksScroller(this);
    m_tasksWidget->setTasksModel(inspection->tasksModel());
    m_tasksWidget->setTransparentBackground(true);
    lay->addWidget(m_tasksWidget);

    lay->addStretch(10);

    QPushButton *b = new QPushButton;
    b->setMaximumHeight(InspectorStyle::defaultComboHeight());
    b->setText(tr("Close"));
    b->setIcon(QIcon(":/debugger/images/debugger_stop_small.png"));
    connect(b, SIGNAL(clicked()),
            this, SLOT(slotCloseClicked()));
    lay->addWidget(b);

    connect(inspection->backend(), SIGNAL(targetConnected(bool)),
            this, SLOT(slotBackendConnected(bool)));
    slotBackendConnected(inspection->backend()->isTargetConnected());
}

void RunningInspectionWidget::enterEvent(QEvent *)
{
    QPalette pal;
     pal.setBrush(QPalette::WindowText, QApplication::palette().color(QPalette::HighlightedText));
     setPalette(pal);
    update();
}

void RunningInspectionWidget::leaveEvent(QEvent *)
{
    QPalette pal;
     pal.setBrush(QPalette::WindowText, QApplication::palette().color(QPalette::WindowText));
     setPalette(pal);
    update();
}

void RunningInspectionWidget::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    emit displayInspection(m_inspection);
}

void RunningInspectionWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (underMouse()) {
        QColor color = palette().color(QPalette::Highlight);
        color.setAlpha(200);
        InspectorStyle::drawCoolGradient(&p, rect(), color);
    } else {
        InspectorStyle::drawCoolGradient(&p, rect(), QColor(192, 192, 192, 64));
    }
}

void RunningInspectionWidget::slotBackendConnected(bool connected)
{
    if (connected)
        m_connLabel->setPixmap(QPixmap(":/projectexplorer/images/ConnectionOn.png"));
    else
        m_connLabel->setPixmap(QPixmap(":/projectexplorer/images/ConnectionOff.png"));
    m_tasksWidget->setVisible(connected);
}

void RunningInspectionWidget::slotDisplayClicked()
{
    emit displayInspection(m_inspection);
}

void RunningInspectionWidget::slotCloseClicked()
{
    emit closeInspection(m_inspection);
}

} // namespace Internal
} // namespace Inspector
