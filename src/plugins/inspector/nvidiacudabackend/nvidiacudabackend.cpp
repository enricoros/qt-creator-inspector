/**************************************************************************
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

#include "nvidiacudabackend.h"
#include "iinspectionmodel.h"

using namespace Inspector::Internal;

//
// NvidiaCudaBackend
//
NvidiaCudaBackend::NvidiaCudaBackend(IInspectionModel *inspectionModel, QObject *parent)
  : IBackend(inspectionModel, parent)
{
    //addModule(0);
}

bool NvidiaCudaBackend::startInspection(const InspectionTarget &target)
{
    Q_UNUSED(target);
    qWarning("NvidiaCudaBackend::startRunConfiguration: TODO");
    return true;
}

bool NvidiaCudaBackend::isTargetConnected() const
{
    return false;
}

int NvidiaCudaBackend::defaultModuleUid() const
{
    return 0;
}

//
// NvidiaCudaBackendFactory
//
QString NvidiaCudaBackendFactory::displayName() const
{
    return tr("NVIDIA CUDA");
}

QIcon NvidiaCudaBackendFactory::icon() const
{
    return QIcon(":/inspector/nvidiacudabackend.png");
}

IBackend *NvidiaCudaBackendFactory::createBackend(const InspectionTarget &target)
{
    IInspectionModel *model = new IInspectionModel;
    model->setTargetName(target.displayName);
    model->setBackendName(displayName());

    return new NvidiaCudaBackend(model);
}
