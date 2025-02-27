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

#ifndef IPROJECTPROPERTIES_H
#define IPROJECTPROPERTIES_H

#include "projectexplorer_export.h"

#include <QtGui/QIcon>

namespace ProjectExplorer {
class Project;
class Target;

namespace Constants {
    const int PANEL_LEFT_MARGIN = 70;
}

class PROJECTEXPLORER_EXPORT IPropertiesPanel
{
public:
    enum PanelFlag {
        NoFlag = 0x00,
        NoLeftMargin = 0x01,
        NoAutomaticStyle = 0x02
    };
    Q_DECLARE_FLAGS(PanelFlags, PanelFlag)

    IPropertiesPanel()
    { }
    virtual ~IPropertiesPanel()
    { }

    virtual QString displayName() const = 0;
    virtual QIcon icon() const = 0;
    virtual QWidget *widget() const = 0;
    virtual PanelFlags flags() const { return NoFlag; }
};

class PROJECTEXPLORER_EXPORT IPanelFactory : public QObject
{
    Q_OBJECT
public:
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual bool supports(Project *project) = 0;
    virtual bool supports(Target *target) = 0;
    virtual IPropertiesPanel *createPanel(Project *project) = 0;
    virtual IPropertiesPanel *createPanel(Target *target) = 0;
};

} // namespace ProjectExplorer

#endif // IPROJECTPROPERTIES_H
