#include "qmlprojectitem.h"
#include "filefilteritems.h"
#include <qdebug.h>

namespace QmlProjectManager {

class QmlProjectItemPrivate : public QObject {
    Q_OBJECT

public:
    QString sourceDirectory;
    QStringList libraryPaths;

    QList<QmlFileFilterItem*> qmlFileFilters() const;

    // content property
    QList<QmlProjectContentItem*> content;
};

QList<QmlFileFilterItem*> QmlProjectItemPrivate::qmlFileFilters() const
{
    QList<QmlFileFilterItem*> qmlFilters;
    for (int i = 0; i < content.size(); ++i) {
        QmlProjectContentItem *contentElement = content.at(i);
        QmlFileFilterItem *qmlFileFilter = qobject_cast<QmlFileFilterItem*>(contentElement);
        if (qmlFileFilter) {
            qmlFilters << qmlFileFilter;
        }
    }
    return qmlFilters;
}

QmlProjectItem::QmlProjectItem(QObject *parent) :
        QObject(parent),
        d_ptr(new QmlProjectItemPrivate)
{
//    Q_D(QmlProjectItem);
//
//    QmlFileFilter *defaultQmlFilter = new QmlFileFilter(this);
//    d->content.append(defaultQmlFilter);
}

QmlProjectItem::~QmlProjectItem()
{
    delete d_ptr;
}

QDeclarativeListProperty<QmlProjectContentItem> QmlProjectItem::content()
{
    Q_D(QmlProjectItem);
    return QDeclarativeListProperty<QmlProjectContentItem>(this, d->content);
}

QString QmlProjectItem::sourceDirectory() const
{
    const Q_D(QmlProjectItem);
    return d->sourceDirectory;
}

// kind of initialization
void QmlProjectItem::setSourceDirectory(const QString &directoryPath)
{
    Q_D(QmlProjectItem);

    if (d->sourceDirectory == directoryPath)
        return;

    d->sourceDirectory = directoryPath;

    for (int i = 0; i < d->content.size(); ++i) {
        QmlProjectContentItem *contentElement = d->content.at(i);
        FileFilterBaseItem *fileFilter = qobject_cast<FileFilterBaseItem*>(contentElement);
        if (fileFilter) {
            fileFilter->setDefaultDirectory(directoryPath);
            connect(fileFilter, SIGNAL(filesChanged()), this, SIGNAL(qmlFilesChanged()));
        }
    }

    emit sourceDirectoryChanged();
}

QStringList QmlProjectItem::libraryPaths() const
{
    const Q_D(QmlProjectItem);
    return d->libraryPaths;
}

void QmlProjectItem::setLibraryPaths(const QStringList &libraryPaths)
{
    Q_D(QmlProjectItem);

    if (d->libraryPaths == libraryPaths)
        return;

    d->libraryPaths = libraryPaths;
    emit libraryPathsChanged();
}

/* Returns list of absolute paths */
QStringList QmlProjectItem::files() const
{
    const Q_D(QmlProjectItem);
    QStringList files;

    for (int i = 0; i < d->content.size(); ++i) {
        QmlProjectContentItem *contentElement = d->content.at(i);
        FileFilterBaseItem *fileFilter = qobject_cast<FileFilterBaseItem*>(contentElement);
        if (fileFilter) {
            foreach (const QString &file, fileFilter->files()) {
                if (!files.contains(file))
                    files << file;
            }
        }
    }
    return files;
}

/**
  Check whether the project would include a file path
  - regardless whether the file already exists or not.

  @param filePath: absolute file path to check
  */
bool QmlProjectItem::matchesFile(const QString &filePath) const
{
    const Q_D(QmlProjectItem);
    for (int i = 0; i < d->content.size(); ++i) {
        QmlProjectContentItem *contentElement = d->content.at(i);
        FileFilterBaseItem *fileFilter = qobject_cast<FileFilterBaseItem*>(contentElement);
        if (fileFilter) {
            if (fileFilter->matchesFile(filePath))
                return true;
        }
    }
    return false;
}

} // namespace QmlProjectManager

#include "qmlprojectitem.moc"
