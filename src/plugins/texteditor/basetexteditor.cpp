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

#include "texteditor_global.h"

#include "basetextdocument.h"
#include "basetexteditor_p.h"
#include "behaviorsettings.h"
#include "codecselector.h"
#include "completionsupport.h"
#include "tabsettings.h"
#include "texteditorconstants.h"
#include "texteditorplugin.h"

#include <aggregation/aggregate.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/manhattanstyle.h>
#include <extensionsystem/pluginmanager.h>
#include <find/basetextfind.h>
#include <utils/linecolumnlabel.h>
#include <utils/qtcassert.h>
#include <utils/stylehelper.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QTextCodec>
#include <QtCore/QFile>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QTimeLine>
#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QPainter>
#include <QtGui/QPrinter>
#include <QtGui/QPrintDialog>
#include <QtGui/QScrollBar>
#include <QtGui/QShortcut>
#include <QtGui/QStyle>
#include <QtGui/QSyntaxHighlighter>
#include <QtGui/QTextCursor>
#include <QtGui/QTextBlock>
#include <QtGui/QTextLayout>
#include <QtGui/QToolBar>
#include <QtGui/QToolTip>
#include <QtGui/QInputDialog>
#include <QtGui/QMenu>

using namespace TextEditor;
using namespace TextEditor::Internal;


namespace TextEditor {
namespace Internal {

class TextEditExtraArea : public QWidget {
    BaseTextEditor *textEdit;
public:
    TextEditExtraArea(BaseTextEditor *edit):QWidget(edit) {
        textEdit = edit;
        setAutoFillBackground(true);
    }
public:

    QSize sizeHint() const {
        return QSize(textEdit->extraAreaWidth(), 0);
    }
protected:
    void paintEvent(QPaintEvent *event){
        textEdit->extraAreaPaintEvent(event);
    }
    void mousePressEvent(QMouseEvent *event){
        textEdit->extraAreaMouseEvent(event);
    }
    void mouseMoveEvent(QMouseEvent *event){
        textEdit->extraAreaMouseEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event){
        textEdit->extraAreaMouseEvent(event);
    }
    void leaveEvent(QEvent *event){
        textEdit->extraAreaLeaveEvent(event);
    }

    void wheelEvent(QWheelEvent *event) {
        QCoreApplication::sendEvent(textEdit->viewport(), event);
    }
};

} // namespace Internal
} // namespace TextEditor


ITextEditor *BaseTextEditor::openEditorAt(const QString &fileName,
                                          int line,
                                          int column,
                                          const QString &editorKind)
{
    Core::EditorManager *editorManager = Core::EditorManager::instance();
    editorManager->addCurrentPositionToNavigationHistory();
    Core::IEditor *editor = editorManager->openEditor(fileName, editorKind, Core::EditorManager::IgnoreNavigationHistory);
    TextEditor::ITextEditor *texteditor = qobject_cast<TextEditor::ITextEditor *>(editor);
    if (texteditor) {
        texteditor->gotoLine(line, column);
        return texteditor;
    }

    return 0;
}

static void convertToPlainText(QString &txt)
{
    QChar *uc = txt.data();
    QChar *e = uc + txt.size();

    for (; uc != e; ++uc) {
        switch (uc->unicode()) {
        case 0xfdd0: // QTextBeginningOfFrame
        case 0xfdd1: // QTextEndOfFrame
        case QChar::ParagraphSeparator:
        case QChar::LineSeparator:
            *uc = QLatin1Char('\n');
            break;
        case QChar::Nbsp:
            *uc = QLatin1Char(' ');
            break;
        default:
            ;
        }
    }
}

BaseTextEditor::BaseTextEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    d = new BaseTextEditorPrivate();
    d->q = this;
    d->m_extraArea = new TextEditExtraArea(this);
    d->m_extraArea->setMouseTracking(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    d->m_overlay = new TextEditorOverlay(this);
    d->m_snippetOverlay = new TextEditorOverlay(this);
    d->m_searchResultOverlay = new TextEditorOverlay(this);

    d->setupDocumentSignals(d->m_document);
    d->setupDocumentSignals(d->m_document);

    d->m_lastScrollPos = -1;
    setCursorWidth(2);

    d->m_allowSkippingOfBlockEnd = false;

    // from RESEARCH

    setLayoutDirection(Qt::LeftToRight);
    viewport()->setMouseTracking(true);
    d->extraAreaSelectionAnchorBlockNumber
        = d->extraAreaToggleMarkBlockNumber
        = d->extraAreaHighlightCollapseBlockNumber
        = d->extraAreaHighlightCollapseColumn
        = -1;

    d->visibleCollapsedBlockNumber = d->suggestedVisibleCollapsedBlockNumber = -1;

    connect(this, SIGNAL(blockCountChanged(int)), this, SLOT(slotUpdateExtraAreaWidth()));
    connect(this, SIGNAL(modificationChanged(bool)), this, SLOT(slotModificationChanged(bool)));
    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(slotCursorPositionChanged()));
    connect(this, SIGNAL(updateRequest(QRect, int)), this, SLOT(slotUpdateRequest(QRect, int)));
    connect(this, SIGNAL(selectionChanged()), this, SLOT(slotSelectionChanged()));

//     (void) new QShortcut(tr("CTRL+L"), this, SLOT(centerCursor()), 0, Qt::WidgetShortcut);
//     (void) new QShortcut(tr("F9"), this, SLOT(slotToggleMark()), 0, Qt::WidgetShortcut);
//     (void) new QShortcut(tr("F11"), this, SLOT(slotToggleBlockVisible()));


    // parentheses matcher
    d->m_parenthesesMatchingEnabled = false;
    d->m_formatRange = true;
    d->m_matchFormat.setForeground(Qt::red);
    d->m_rangeFormat.setBackground(QColor(0xb4, 0xee, 0xb4));
    d->m_mismatchFormat.setBackground(Qt::magenta);
    d->m_parenthesesMatchingTimer = new QTimer(this);
    d->m_parenthesesMatchingTimer->setSingleShot(true);
    connect(d->m_parenthesesMatchingTimer, SIGNAL(timeout()), this, SLOT(_q_matchParentheses()));

    d->m_highlightBlocksTimer = new QTimer(this);
    d->m_highlightBlocksTimer->setSingleShot(true);
    connect(d->m_highlightBlocksTimer, SIGNAL(timeout()), this, SLOT(_q_highlightBlocks()));

    d->m_animator = 0;

    d->m_searchResultFormat.setBackground(QColor(0xffef0b));

    slotUpdateExtraAreaWidth();
    updateHighlights();
    setFrameStyle(QFrame::NoFrame);

    d->m_delayedUpdateTimer = new QTimer(this);
    d->m_delayedUpdateTimer->setSingleShot(true);
    connect(d->m_delayedUpdateTimer, SIGNAL(timeout()), viewport(), SLOT(update()));

    connect(Core::EditorManager::instance(), SIGNAL(currentEditorChanged(Core::IEditor*)),
            this, SLOT(currentEditorChanged(Core::IEditor*)));
}

BaseTextEditor::~BaseTextEditor()
{
    delete d;
    d = 0;
}

QString BaseTextEditor::mimeType() const
{
    return d->m_document->mimeType();
}

void BaseTextEditor::setMimeType(const QString &mt)
{
    d->m_document->setMimeType(mt);
}

void BaseTextEditor::print(QPrinter *printer)
{
    const bool oldFullPage =  printer->fullPage();
    printer->setFullPage(true);
    QPrintDialog *dlg = new QPrintDialog(printer, this);
    dlg->setWindowTitle(tr("Print Document"));
    if (dlg->exec() == QDialog::Accepted) {
        d->print(printer);
    }
    printer->setFullPage(oldFullPage);
    delete dlg;
}

static int collapseBoxWidth(const QFontMetrics &fm)
{
    const int lineSpacing = fm.lineSpacing();
    return lineSpacing + lineSpacing%2 + 1;
}

static void printPage(int index, QPainter *painter, const QTextDocument *doc,
                      const QRectF &body, const QRectF &titleBox,
                      const QString &title)
{
    painter->save();

    painter->translate(body.left(), body.top() - (index - 1) * body.height());
    QRectF view(0, (index - 1) * body.height(), body.width(), body.height());

    QAbstractTextDocumentLayout *layout = doc->documentLayout();
    QAbstractTextDocumentLayout::PaintContext ctx;

    painter->setFont(QFont(doc->defaultFont()));
    QRectF box = titleBox.translated(0, view.top());
    int dpix = painter->device()->logicalDpiX();
    int dpiy = painter->device()->logicalDpiY();
    int mx = 5 * dpix / 72.0;
    int my = 2 * dpiy / 72.0;
    painter->fillRect(box.adjusted(-mx, -my, mx, my), QColor(210, 210, 210));
    if (!title.isEmpty())
        painter->drawText(box, Qt::AlignCenter, title);
    const QString pageString = QString::number(index);
    painter->drawText(box, Qt::AlignRight, pageString);

    painter->setClipRect(view);
    ctx.clip = view;
    // don't use the system palette text as default text color, on HP/UX
    // for example that's white, and white text on white paper doesn't
    // look that nice
    ctx.palette.setColor(QPalette::Text, Qt::black);

    layout->draw(painter, ctx);

    painter->restore();
}

void BaseTextEditorPrivate::print(QPrinter *printer)
{

    QTextDocument *doc = q->document();

    QString title = q->displayName();
    if (title.isEmpty())
        printer->setDocName(title);


    QPainter p(printer);

    // Check that there is a valid device to print to.
    if (!p.isActive())
        return;

    doc = doc->clone(doc);

    QTextOption opt = doc->defaultTextOption();
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    doc->setDefaultTextOption(opt);

    (void)doc->documentLayout(); // make sure that there is a layout


    QColor background = q->palette().color(QPalette::Base);
    bool backgroundIsDark = background.value() < 128;

    for (QTextBlock srcBlock = q->document()->firstBlock(), dstBlock = doc->firstBlock();
         srcBlock.isValid() && dstBlock.isValid();
         srcBlock = srcBlock.next(), dstBlock = dstBlock.next()) {


        QList<QTextLayout::FormatRange> formatList = srcBlock.layout()->additionalFormats();
        if (backgroundIsDark) {
            // adjust syntax highlighting colors for better contrast
            for (int i = formatList.count() - 1; i >=0; --i) {
                QTextCharFormat &format = formatList[i].format;
                if (format.background().color() == background) {
                    QBrush brush = format.foreground();
                    QColor color = brush.color();
                    int h,s,v,a;
                    color.getHsv(&h, &s, &v, &a);
                    color.setHsv(h, s, qMin(128, v), a);
                    brush.setColor(color);
                    format.setForeground(brush);
                }
                format.setBackground(Qt::white);
            }
        }

        dstBlock.layout()->setAdditionalFormats(formatList);
    }

    QAbstractTextDocumentLayout *layout = doc->documentLayout();
    layout->setPaintDevice(p.device());

    int dpiy = p.device()->logicalDpiY();
    int margin = (int) ((2/2.54)*dpiy); // 2 cm margins

    QTextFrameFormat fmt = doc->rootFrame()->frameFormat();
    fmt.setMargin(margin);
    doc->rootFrame()->setFrameFormat(fmt);

    QRectF pageRect(printer->pageRect());
    QRectF body = QRectF(0, 0, pageRect.width(), pageRect.height());
    QFontMetrics fontMetrics(doc->defaultFont(), p.device());

    QRectF titleBox(margin,
                    body.top() + margin
                    - fontMetrics.height()
                    - 6 * dpiy / 72.0,
                    body.width() - 2*margin,
                    fontMetrics.height());
    doc->setPageSize(body.size());

    int docCopies;
    int pageCopies;
    if (printer->collateCopies() == true){
        docCopies = 1;
        pageCopies = printer->numCopies();
    } else {
        docCopies = printer->numCopies();
        pageCopies = 1;
    }

    int fromPage = printer->fromPage();
    int toPage = printer->toPage();
    bool ascending = true;

    if (fromPage == 0 && toPage == 0) {
        fromPage = 1;
        toPage = doc->pageCount();
    }
    // paranoia check
    fromPage = qMax(1, fromPage);
    toPage = qMin(doc->pageCount(), toPage);

    if (printer->pageOrder() == QPrinter::LastPageFirst) {
        int tmp = fromPage;
        fromPage = toPage;
        toPage = tmp;
        ascending = false;
    }

    for (int i = 0; i < docCopies; ++i) {

        int page = fromPage;
        while (true) {
            for (int j = 0; j < pageCopies; ++j) {
                if (printer->printerState() == QPrinter::Aborted
                    || printer->printerState() == QPrinter::Error)
                    goto UserCanceled;
                printPage(page, &p, doc, body, titleBox, title);
                if (j < pageCopies - 1)
                    printer->newPage();
            }

            if (page == toPage)
                break;

            if (ascending)
                ++page;
            else
                --page;

            printer->newPage();
        }

        if ( i < docCopies - 1)
            printer->newPage();
    }

UserCanceled:
    delete doc;
}


bool DocumentMarker::addMark(TextEditor::ITextMark *mark, int line)
{
    QTC_ASSERT(line >= 1, return false);
    int blockNumber = line - 1;
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(document->documentLayout());
    QTC_ASSERT(documentLayout, return false);
    QTextBlock block = document->findBlockByNumber(blockNumber);

    if (block.isValid()) {
        TextBlockUserData *userData = TextEditDocumentLayout::userData(block);
        userData->addMark(mark);
        mark->updateLineNumber(blockNumber + 1);
        mark->updateBlock(block);
        documentLayout->hasMarks = true;
        documentLayout->requestUpdate();
        return true;
    }
    return false;
}

int BaseTextEditorPrivate::visualIndent(const QTextBlock &block) const
{
    if (!block.isValid())
        return 0;
    const QTextDocument *document = block.document();
    int i = 0;
    while (i < block.length()) {
        if (!document->characterAt(block.position() + i).isSpace()) {
            QTextCursor cursor(block);
            cursor.setPosition(block.position() + i);
            return q->cursorRect(cursor).x();
        }
        ++i;
    }

    return 0;
}

TextEditor::TextMarks DocumentMarker::marksAt(int line) const
{
    QTC_ASSERT(line >= 1, return TextMarks());
    int blockNumber = line - 1;
    QTextBlock block = document->findBlockByNumber(blockNumber);

    if (block.isValid()) {
        if (TextBlockUserData *userData = TextEditDocumentLayout::testUserData(block))
            return userData->marks();
    }
    return TextMarks();
}

void DocumentMarker::removeMark(TextEditor::ITextMark *mark)
{
    bool needUpdate = false;
    QTextBlock block = document->begin();
    while (block.isValid()) {
        if (TextBlockUserData *data = static_cast<TextBlockUserData *>(block.userData())) {
            needUpdate |= data->removeMark(mark);
        }
        block = block.next();
    }
    if (needUpdate)
        updateMark(0);
}


bool DocumentMarker::hasMark(TextEditor::ITextMark *mark) const
{
    QTextBlock block = document->begin();
    while (block.isValid()) {
        if (TextBlockUserData *data = static_cast<TextBlockUserData *>(block.userData())) {
            if (data->hasMark(mark))
                return true;
        }
        block = block.next();
    }
    return false;
}

ITextMarkable *BaseTextEditor::markableInterface() const
{
    return baseTextDocument()->documentMarker();
}

ITextEditable *BaseTextEditor::editableInterface() const
{
    if (!d->m_editable) {
        d->m_editable = const_cast<BaseTextEditor*>(this)->createEditableInterface();
        connect(this, SIGNAL(textChanged()),
                d->m_editable, SIGNAL(contentsChanged()));
        connect(this, SIGNAL(changed()),
                d->m_editable, SIGNAL(changed()));
    }
    return d->m_editable;
}


void BaseTextEditor::currentEditorChanged(Core::IEditor *editor)
{
    if (editor == d->m_editable) {
        if (d->m_document->hasDecodingError()) {
            Core::EditorManager::instance()->showEditorInfoBar(QLatin1String(Constants::SELECT_ENCODING),
                tr("<b>Error:</b> Could not decode \"%1\" with \"%2\"-encoding. Editing not possible.")
                    .arg(displayName()).arg(QString::fromLatin1(d->m_document->codec()->name())),
                tr("Select Encoding"),
                this, SLOT(selectEncoding()));
        }
    }
}

void BaseTextEditor::selectEncoding()
{
    BaseTextDocument *doc = d->m_document;
    CodecSelector codecSelector(this, doc);

    switch (codecSelector.exec()) {
    case CodecSelector::Reload:
        doc->reload(codecSelector.selectedCodec());
        setReadOnly(d->m_document->hasDecodingError());
        if (doc->hasDecodingError())
            currentEditorChanged(Core::EditorManager::instance()->currentEditor());
        else
            Core::EditorManager::instance()->hideEditorInfoBar(QLatin1String(Constants::SELECT_ENCODING));
        break;
    case CodecSelector::Save:
        doc->setCodec(codecSelector.selectedCodec());
        Core::EditorManager::instance()->saveEditor(editableInterface());
        break;
    case CodecSelector::Cancel:
        break;
    }
}

void DocumentMarker::updateMark(ITextMark *mark)
{
    Q_UNUSED(mark)
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(document->documentLayout());
    QTC_ASSERT(documentLayout, return);
    documentLayout->requestUpdate();
}


void BaseTextEditor::triggerCompletions()
{
    emit requestAutoCompletion(editableInterface(), true);
}

void BaseTextEditor::triggerQuickFix()
{
    emit requestQuickFix(editableInterface());
}

bool BaseTextEditor::createNew(const QString &contents)
{
    setPlainText(contents);
    document()->setModified(false);
    return true;
}

bool BaseTextEditor::open(const QString &fileName)
{
    if (d->m_document->open(fileName)) {
        moveCursor(QTextCursor::Start);
        if (d->m_displaySettings.m_autoFoldFirstComment)
            d->collapseLicenseHeader();
        setReadOnly(d->m_document->hasDecodingError());
        return true;
    }
    return false;
}

/*
  Collapses the first comment in a file, if there is only whitespace above
  */
void BaseTextEditorPrivate::collapseLicenseHeader()
{
    QTextDocument *doc = q->document();
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(doc->documentLayout());
    QTC_ASSERT(documentLayout, return);
    QTextBlock block = doc->firstBlock();
    const TabSettings &ts = m_document->tabSettings();
    while (block.isValid() && block.isVisible()) {
        TextBlockUserData *data = TextBlockUserData::canCollapse(block);
        if (data && block.next().isVisible()) {
            QChar character;
            static_cast<TextBlockUserData*>(data)->collapseAtPos(&character);
            if (character != QLatin1Char('+'))
                break; // not a comment
            TextBlockUserData::doCollapse(block, false);
            moveCursorVisible();
            documentLayout->requestUpdate();
            documentLayout->emitDocumentSizeChanged();
            break;
        }
        QString text = block.text();
        if (ts.firstNonSpace(text) < text.size())
            break;
        block = block.next();
    }
}

const Utils::ChangeSet &BaseTextEditor::changeSet() const
{
    return d->m_changeSet;
}

void BaseTextEditor::setChangeSet(const Utils::ChangeSet &changeSet)
{
    using namespace Utils;

    d->m_changeSet = changeSet;

    foreach (const ChangeSet::EditOp &op, changeSet.operationList()) {
        // ### TODO: process the edit operation

        switch (op.type) {
        case ChangeSet::EditOp::Replace:
            break;

        case ChangeSet::EditOp::Move:
            break;

        case ChangeSet::EditOp::Insert:
            break;

        case ChangeSet::EditOp::Remove:
            break;

        case ChangeSet::EditOp::Flip:
            break;

        case ChangeSet::EditOp::Copy:
            break;

        default:
            break;
        } // switch
    }
}

Core::IFile *BaseTextEditor::file()
{
    return d->m_document;
}

void BaseTextEditor::editorContentsChange(int position, int charsRemoved, int charsAdded)
{
    if (d->m_animator)
        d->m_animator->finish();

    d->m_contentsChanged = true;

    // Keep the line numbers and the block information for the text marks updated
    if (charsRemoved != 0) {
        d->updateMarksLineNumber();
        d->updateMarksBlock(document()->findBlock(position));
    } else {
        const QTextBlock posBlock = document()->findBlock(position);
        const QTextBlock nextBlock = document()->findBlock(position + charsAdded);
        if (posBlock != nextBlock) {
            d->updateMarksLineNumber();
            d->updateMarksBlock(posBlock);
            d->updateMarksBlock(nextBlock);
        } else {
            d->updateMarksBlock(posBlock);
        }
    }
}


void BaseTextEditor::slotSelectionChanged()
{
    bool changed = (d->m_inBlockSelectionMode != d->m_lastEventWasBlockSelectionEvent);
    d->m_inBlockSelectionMode = d->m_lastEventWasBlockSelectionEvent;
    if (changed || d->m_inBlockSelectionMode)
        viewport()->update();
    if (!d->m_inBlockSelectionMode)
        d->m_blockSelectionExtraX = 0;
    if (!d->m_selectBlockAnchor.isNull() && !textCursor().hasSelection())
        d->m_selectBlockAnchor = QTextCursor();

    // Clear any link which might be showing when the selection changes
    clearLink();
}

void BaseTextEditor::gotoBlockStart()
{
    QTextCursor cursor = textCursor();
    if (TextBlockUserData::findPreviousOpenParenthesis(&cursor, false)) {
        setTextCursor(cursor);
        _q_matchParentheses();
    }
}

void BaseTextEditor::gotoBlockEnd()
{
    QTextCursor cursor = textCursor();
    if (TextBlockUserData::findNextClosingParenthesis(&cursor, false)) {
        setTextCursor(cursor);
        _q_matchParentheses();
    }
}

void BaseTextEditor::gotoBlockStartWithSelection()
{
    QTextCursor cursor = textCursor();
    if (TextBlockUserData::findPreviousOpenParenthesis(&cursor, true)) {
        setTextCursor(cursor);
        _q_matchParentheses();
    }
}

void BaseTextEditor::gotoBlockEndWithSelection()
{
    QTextCursor cursor = textCursor();
    if (TextBlockUserData::findNextClosingParenthesis(&cursor, true)) {
        setTextCursor(cursor);
        _q_matchParentheses();
    }
}

static QTextCursor flippedCursor(const QTextCursor &cursor) {
    QTextCursor flipped = cursor;
    flipped.clearSelection();
    flipped.setPosition(cursor.anchor(), QTextCursor::KeepAnchor);
    return flipped;
}

void BaseTextEditor::selectBlockUp()
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection())
        d->m_selectBlockAnchor = cursor;
    else
        cursor.setPosition(cursor.selectionStart());


    if (!TextBlockUserData::findPreviousOpenParenthesis(&cursor, false))
        return;
    if (!TextBlockUserData::findNextClosingParenthesis(&cursor, true))
        return;
    setTextCursor(flippedCursor(cursor));
    _q_matchParentheses();
}

void BaseTextEditor::selectBlockDown()
{
    QTextCursor tc = textCursor();
    QTextCursor cursor = d->m_selectBlockAnchor;

    if (!tc.hasSelection() || cursor.isNull())
        return;
    tc.setPosition(tc.selectionStart());

    forever {
        QTextCursor ahead = cursor;
        if (!TextBlockUserData::findPreviousOpenParenthesis(&ahead, false))
            break;
        if (ahead.position() <= tc.position())
            break;
        cursor = ahead;
    }
    if ( cursor != d->m_selectBlockAnchor)
        TextBlockUserData::findNextClosingParenthesis(&cursor, true);

    setTextCursor(flippedCursor(cursor));
    _q_matchParentheses();
}

void BaseTextEditor::copyLineUp()
{
    copyLineUpDown(true);
}

void BaseTextEditor::copyLineDown()
{
    copyLineUpDown(false);
}

void BaseTextEditor::copyLineUpDown(bool up)
{
    QTextCursor cursor = textCursor();
    QTextCursor move = cursor;
    move.beginEditBlock();

    bool hasSelection = cursor.hasSelection();

    if (hasSelection) {
        move.setPosition(cursor.selectionStart());
        move.movePosition(QTextCursor::StartOfBlock);
        move.setPosition(cursor.selectionEnd(), QTextCursor::KeepAnchor);
        move.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    } else {
        move.movePosition(QTextCursor::StartOfBlock);
        move.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    }

    QString text = move.selectedText();

    if (up) {
        move.setPosition(cursor.selectionStart());
        move.movePosition(QTextCursor::StartOfBlock);
        move.insertBlock();
        move.movePosition(QTextCursor::Left);
    } else {
        move.movePosition(QTextCursor::EndOfBlock);
        if (move.atBlockStart()) {
            move.movePosition(QTextCursor::NextBlock);
            move.insertBlock();
            move.movePosition(QTextCursor::Left);
        } else {
            move.insertBlock();
        }
    }

    int start = move.position();
    move.clearSelection();
    move.insertText(text);
    int end = move.position();

    move.setPosition(start);
    move.setPosition(end, QTextCursor::KeepAnchor);

    indent(document(), move, QChar::Null);
    move.endEditBlock();

    setTextCursor(move);
}

void BaseTextEditor::joinLines()
{
    QTextCursor cursor = textCursor();
    QTextCursor start = cursor;
    QTextCursor end = cursor;

    start.setPosition(cursor.selectionStart());
    end.setPosition(cursor.selectionEnd() - 1);

    int lineCount = qMax(1, end.blockNumber() - start.blockNumber());

    cursor.beginEditBlock();
    cursor.setPosition(cursor.selectionStart());
    while (lineCount--) {
        cursor.movePosition(QTextCursor::NextBlock);
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString cutLine = cursor.selectedText();

        // Collapse leading whitespaces to one or insert whitespace
        cutLine.replace(QRegExp("^\\s*"), " ");
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();

        cursor.movePosition(QTextCursor::PreviousBlock);
        cursor.movePosition(QTextCursor::EndOfBlock);

        cursor.insertText(cutLine);
    }
    cursor.endEditBlock();

    setTextCursor(cursor);
}

void BaseTextEditor::moveLineUp()
{
    moveLineUpDown(true);
}

void BaseTextEditor::moveLineDown()
{
    moveLineUpDown(false);
}

void BaseTextEditor::moveLineUpDown(bool up)
{
    QTextCursor cursor = textCursor();
    QTextCursor move = cursor;

    move.setVisualNavigation(false); // this opens collapsed items instead of destroying them

    if (d->m_moveLineUndoHack)
        move.joinPreviousEditBlock();
    else
        move.beginEditBlock();

    bool hasSelection = cursor.hasSelection();

    if (cursor.hasSelection()) {
        move.setPosition(cursor.selectionStart());
        move.movePosition(QTextCursor::StartOfBlock);
        move.setPosition(cursor.selectionEnd(), QTextCursor::KeepAnchor);
        move.movePosition(move.atBlockStart() ? QTextCursor::Left: QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    } else {
        move.movePosition(QTextCursor::StartOfBlock);
        move.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    }
    QString text = move.selectedText();
    move.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
    move.removeSelectedText();

    if (up) {
        move.movePosition(QTextCursor::PreviousBlock);
        move.insertBlock();
        move.movePosition(QTextCursor::Left);
    } else {
        move.movePosition(QTextCursor::EndOfBlock);
        if (move.atBlockStart()) { // empty block
            move.movePosition(QTextCursor::NextBlock);
            move.insertBlock();
            move.movePosition(QTextCursor::Left);
        } else {
            move.insertBlock();
        }
    }

    int start = move.position();
    move.clearSelection();
    move.insertText(text);
    int end = move.position();

    if (hasSelection) {
        move.setPosition(start);
        move.setPosition(end, QTextCursor::KeepAnchor);
    }

    reindent(document(), move);
    move.endEditBlock();

    setTextCursor(move);
    d->m_moveLineUndoHack = true;
}

void BaseTextEditor::cleanWhitespace()
{
    d->m_document->cleanWhitespace(textCursor());
}

void BaseTextEditor::keyPressEvent(QKeyEvent *e)
{
    viewport()->setCursor(Qt::BlankCursor);
    QToolTip::hideText();

    d->m_moveLineUndoHack = false;
    d->clearVisibleCollapsedBlock();

    QKeyEvent *original_e = e;
    d->m_lastEventWasBlockSelectionEvent = false;

    if (e->key() == Qt::Key_Escape) {
        if (d->m_snippetOverlay->isVisible()) {
            e->accept();
            d->m_snippetOverlay->hide();
            d->m_snippetOverlay->clear();
            QTextCursor cursor = textCursor();
            cursor.clearSelection();
            setTextCursor(cursor);
            return;
        }
    }

    bool ro = isReadOnly();

    if (d->m_inBlockSelectionMode) {
        if (e == QKeySequence::Cut) {
            if (!ro) {
                cut();
                e->accept();
                return;
            }
        } else if (e == QKeySequence::Delete || e->key() == Qt::Key_Backspace) {
            if (!ro) {
                d->removeBlockSelection();
                e->accept();
                return;
            }
        } else if (e == QKeySequence::Paste) {
            if (!ro) {
                d->removeBlockSelection();
                // continue
            }
        }
    }


    if (!ro
        && (e == QKeySequence::InsertParagraphSeparator
            || (!d->m_lineSeparatorsAllowed && e == QKeySequence::InsertLineSeparator))
        ) {

        if (d->m_snippetOverlay->isVisible()) {
            e->accept();
            d->m_snippetOverlay->hide();
            d->m_snippetOverlay->clear();
            QTextCursor cursor = textCursor();
            cursor.movePosition(QTextCursor::EndOfBlock);
            setTextCursor(cursor);
            return;
        }


        QTextCursor cursor = textCursor();
        if (d->m_inBlockSelectionMode)
            cursor.clearSelection();
        const TabSettings &ts = d->m_document->tabSettings();
        cursor.beginEditBlock();

        int extraBlocks = paragraphSeparatorAboutToBeInserted(cursor); // virtual

        if (ts.m_autoIndent) {
            cursor.insertBlock();
            indent(document(), cursor, QChar::Null);
        } else {
            cursor.insertBlock();

            // After inserting the block, to avoid duplicating whitespace on the same line
            const QString previousBlockText = cursor.block().previous().text();
            cursor.insertText(ts.indentationString(previousBlockText));
        }
        cursor.endEditBlock();
        e->accept();

        if (extraBlocks > 0) {
            QTextCursor ensureVisible = cursor;
            while (extraBlocks > 0) {
                --extraBlocks;
                ensureVisible.movePosition(QTextCursor::NextBlock);
            }
            setTextCursor(ensureVisible);
        }

        setTextCursor(cursor);
        return;
    } else if (!ro
               && (e == QKeySequence::MoveToStartOfBlock
                   || e == QKeySequence::SelectStartOfBlock)){
        if ((e->modifiers() & (Qt::AltModifier | Qt::ShiftModifier)) == (Qt::AltModifier | Qt::ShiftModifier))
            d->m_lastEventWasBlockSelectionEvent = true;
        handleHomeKey(e == QKeySequence::SelectStartOfBlock);
        e->accept();
        return;
    } else if (!ro
               && (e == QKeySequence::MoveToStartOfLine
                   || e == QKeySequence::SelectStartOfLine)){
        if ((e->modifiers() & (Qt::AltModifier | Qt::ShiftModifier)) == (Qt::AltModifier | Qt::ShiftModifier))
            d->m_lastEventWasBlockSelectionEvent = true;
        QTextCursor cursor = textCursor();
        if (QTextLayout *layout = cursor.block().layout()) {
            if (layout->lineForTextPosition(cursor.position() - cursor.block().position()).lineNumber() == 0) {
                handleHomeKey(e == QKeySequence::SelectStartOfLine);
                e->accept();
                return;
            }
        }
    } else if (!ro
               && e == QKeySequence::DeleteStartOfWord
               && d->m_document->tabSettings().m_autoIndent
               && !textCursor().hasSelection()){
        e->accept();
        QTextCursor c = textCursor();
        int pos = c.position();
        c.movePosition(QTextCursor::PreviousWord);
        int targetpos = c.position();
        forever {
            handleBackspaceKey();
            int cpos = textCursor().position();
            if (cpos == pos || cpos <= targetpos)
                break;
            pos = cpos;
        }
        return;
    } else switch (e->key()) {


#if 0
    case Qt::Key_Dollar: {
            d->m_overlay->setVisible(!d->m_overlay->isVisible());
            d->m_overlay->setCursor(textCursor());
            e->accept();
        return;

    } break;
#endif
    case Qt::Key_Tab:
    case Qt::Key_Backtab: {
        if (ro) break;
        if (d->m_snippetOverlay->isVisible() && !d->m_snippetOverlay->isEmpty()) {
            d->snippetTabOrBacktab(e->key() == Qt::Key_Tab);
            e->accept();
            return;
        }
        QTextCursor cursor = textCursor();
        int newPosition;
        if (d->m_document->tabSettings().tabShouldIndent(document(), cursor, &newPosition)) {
            if (newPosition != cursor.position() && !cursor.hasSelection()) {
                cursor.setPosition(newPosition);
                setTextCursor(cursor);
            }
            indent(document(), cursor, QChar::Null);
        } else {
            indentOrUnindent(e->key() == Qt::Key_Tab);
        }
        e->accept();
        return;
    } break;
    case Qt::Key_Backspace:
        if (ro) break;
        if ((e->modifiers() & (Qt::ControlModifier
                               | Qt::ShiftModifier
                               | Qt::AltModifier
                               | Qt::MetaModifier)) == Qt::NoModifier
            && !textCursor().hasSelection()) {
            handleBackspaceKey();
            e->accept();
            return;
        }
        break;
    case Qt::Key_Up:
    case Qt::Key_Down:
        if (e->modifiers() & Qt::ControlModifier) {
            verticalScrollBar()->triggerAction(
                    e->key() == Qt::Key_Up ? QAbstractSlider::SliderSingleStepSub :
                                             QAbstractSlider::SliderSingleStepAdd);
            e->accept();
            return;
        }
        // fall through
    case Qt::Key_End:
    case Qt::Key_Right:
    case Qt::Key_Left:
#ifndef Q_WS_MAC
        if ((e->modifiers() & (Qt::AltModifier | Qt::ShiftModifier)) == (Qt::AltModifier | Qt::ShiftModifier)) {

            d->m_lastEventWasBlockSelectionEvent = true;

            if (d->m_inBlockSelectionMode) {
                if (e->key() == Qt::Key_Right && textCursor().atBlockEnd()) {
                    d->m_blockSelectionExtraX++;
                    viewport()->update();
                    e->accept();
                    return;
                } else if (e->key() == Qt::Key_Left && d->m_blockSelectionExtraX > 0) {
                    d->m_blockSelectionExtraX--;
                    e->accept();
                    viewport()->update();
                    return;
                }
            }

            e = new QKeyEvent(
                e->type(),
                e->key(),
                e->modifiers() & ~Qt::AltModifier,
                e->text(),
                e->isAutoRepeat(),
                e->count()
                );
        }
#endif
        break;
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
        if (e->modifiers() == Qt::ControlModifier) {
            verticalScrollBar()->triggerAction(
                    e->key() == Qt::Key_PageUp ? QAbstractSlider::SliderPageStepSub :
                                                 QAbstractSlider::SliderPageStepAdd);
            e->accept();
            return;
        }
        break;

    default:
        break;
    }

    if (d->m_inBlockSelectionMode) {
        QString text = e->text();
        if (!text.isEmpty() && (text.at(0).isPrint() || text.at(0) == QLatin1Char('\t'))) {
            d->removeBlockSelection(text);
            goto skip_event;
        }
    }

    if (e->key() == Qt::Key_H && e->modifiers() ==
#ifdef Q_OS_DARWIN
        Qt::MetaModifier
#else
        Qt::ControlModifier
#endif
        ) {
        universalHelper();
        e->accept();
        return;
    }

    if (d->m_snippetOverlay->isVisible()
        && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace)) {
        d->snippetCheckCursor(textCursor());
    }

    if (ro || e->text().isEmpty() || !e->text().at(0).isPrint()) {
        QPlainTextEdit::keyPressEvent(e);
    } else if ((e->modifiers() & (Qt::ControlModifier|Qt::AltModifier)) != Qt::ControlModifier){
        QTextCursor cursor = textCursor();
        QString text = e->text();
        QString autoText = autoComplete(cursor, text);

        QChar electricChar;
        if (d->m_document->tabSettings().m_autoIndent) {
            foreach (QChar c, text) {
                if (isElectricCharacter(c)) {
                    electricChar = c;
                    break;
                }
            }
        }

        if (d->m_snippetOverlay->isVisible())
            d->snippetCheckCursor(cursor);

        bool doEditBlock = !(electricChar.isNull() && autoText.isEmpty());
        if (doEditBlock)
            cursor.beginEditBlock();

        cursor.insertText(text);

        if (!autoText.isEmpty()) {
            int pos = cursor.position();
            cursor.insertText(autoText);
            cursor.setPosition(pos);
        }
        if (!electricChar.isNull())
            indent(document(), cursor, electricChar);

        if (doEditBlock)
            cursor.endEditBlock();

        setTextCursor(cursor);
    }

skip_event:
    if (!ro && e->key() == Qt::Key_Delete && d->m_parenthesesMatchingEnabled)
        d->m_parenthesesMatchingTimer->start(50);


    if (!ro && d->m_contentsChanged && !e->text().isEmpty() && e->text().at(0).isPrint())
        emit requestAutoCompletion(editableInterface(), false);

    if (e != original_e)
        delete e;
}

void BaseTextEditor::insertCodeSnippet(const QString &snippet)
{
    QList<QTextEdit::ExtraSelection> selections;

    QTextCursor cursor = textCursor();
    const int startCursorPosition = cursor.position();
    cursor.beginEditBlock();

    if ((snippet.count('$') % 2) != 0) {
        qWarning() << "invalid snippet";
        return;
    }

    int pos = 0;
    QMap<int, int> positions;

    while (pos < snippet.size()) {
        if (snippet.at(pos) != QChar::ObjectReplacementCharacter) {
            const int start = pos;
            do { ++pos; }
            while (pos < snippet.size() && snippet.at(pos) != QChar::ObjectReplacementCharacter);
            cursor.insertText(snippet.mid(start, pos - start));
        } else {
            // the start of a place holder.
            const int start = ++pos;
            for (; pos < snippet.size(); ++pos) {
                if (snippet.at(pos) == QChar::ObjectReplacementCharacter)
                    break;
            }

            Q_ASSERT(pos < snippet.size());
            Q_ASSERT(snippet.at(pos) == QChar::ObjectReplacementCharacter);

            const QString textToInsert = snippet.mid(start, pos - start);

            int cursorPosition = cursor.position();
            cursor.insertText(textToInsert);

            if (textToInsert.isEmpty()) {
                positions.insert(cursorPosition, 0);
            } else {
                positions.insert(cursorPosition-1, textToInsert.length()+1);
            }

            ++pos;
        }
    }

    QMapIterator<int,int> it(positions);
    while (it.hasNext()) {
        it.next();
        int length = it.value();
        int position = it.key();

        QTextCursor tc(document());
        tc.setPosition(position);
        tc.setPosition(position + length, QTextCursor::KeepAnchor);
        QTextEdit::ExtraSelection selection;
        selection.cursor = tc;
        selection.format.setBackground(length ? Qt::darkCyan : Qt::darkMagenta);
        selections.append(selection);
    }

    cursor.setPosition(startCursorPosition, QTextCursor::KeepAnchor);
    indent(cursor.document(), cursor, QChar());
    cursor.endEditBlock();

    setExtraSelections(BaseTextEditor::SnippetPlaceholderSelection, selections);

    if (! selections.isEmpty()) {
        const QTextEdit::ExtraSelection &selection = selections.first();

        cursor = textCursor();
        if (selection.cursor.hasSelection()) {
            cursor.setPosition(selection.cursor.selectionStart()+1);
            cursor.setPosition(selection.cursor.selectionEnd(), QTextCursor::KeepAnchor);
        } else {
            cursor.setPosition(selection.cursor.position());
        }
        setTextCursor(cursor);
    }
}

void BaseTextEditor::universalHelper()
{
    // Test function for development. Place your new fangled experiment here to
    // give it proper scrutiny before pushing it onto others.
}

void BaseTextEditor::setTextCursor(const QTextCursor &cursor)
{
    // workaround for QTextControl bug
    bool selectionChange = cursor.hasSelection() || textCursor().hasSelection();
    QTextCursor c = cursor;
    c.setVisualNavigation(true);
    QPlainTextEdit::setTextCursor(c);
    if (selectionChange)
        slotSelectionChanged();
}

void BaseTextEditor::gotoLine(int line, int column)
{
    d->m_lastCursorChangeWasInteresting = false; // avoid adding the previous position to history
    const int blockNumber = line - 1;
    const QTextBlock &block = document()->findBlockByNumber(blockNumber);
    if (block.isValid()) {
        QTextCursor cursor(block);
        if (column > 0) {
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
        } else {
            int pos = cursor.position();
            while (characterAt(pos).category() == QChar::Separator_Space) {
                ++pos;
            }
            cursor.setPosition(pos);
        }
        setTextCursor(cursor);
        centerCursor();
    }
    saveCurrentCursorPositionForNavigation();
}

int BaseTextEditor::position(ITextEditor::PositionOperation posOp, int at) const
{
    QTextCursor tc = textCursor();

    if (at != -1)
        tc.setPosition(at);

    if (posOp == ITextEditor::Current)
        return tc.position();

    switch (posOp) {
    case ITextEditor::EndOfLine:
        tc.movePosition(QTextCursor::EndOfLine);
        return tc.position();
    case ITextEditor::StartOfLine:
        tc.movePosition(QTextCursor::StartOfLine);
        return tc.position();
    case ITextEditor::Anchor:
        if (tc.hasSelection())
            return tc.anchor();
        break;
    case ITextEditor::EndOfDoc:
        tc.movePosition(QTextCursor::End);
        return tc.position();
    default:
        break;
    }

    return -1;
}

void BaseTextEditor::convertPosition(int pos, int *line, int *column) const
{
    QTextBlock block = document()->findBlock(pos);
    if (!block.isValid()) {
        (*line) = -1;
        (*column) = -1;
    } else {
        (*line) = block.blockNumber() + 1;
        (*column) = pos - block.position();
    }
}

QChar BaseTextEditor::characterAt(int pos) const
{
    return document()->characterAt(pos);
}

bool BaseTextEditor::event(QEvent *e)
{
    d->m_contentsChanged = false;
    switch (e->type()) {
    case QEvent::ShortcutOverride:
        if (static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape && d->m_snippetOverlay->isVisible()) {
            e->accept();
            return true;
        }
        e->ignore(); // we are a really nice citizen
        return true;
        break;
    default:
        break;
    }

    return QPlainTextEdit::event(e);
}

void BaseTextEditor::duplicateFrom(BaseTextEditor *editor)
{
    if (this == editor)
        return;
    setDisplayName(editor->displayName());
    d->m_revisionsVisible = editor->d->m_revisionsVisible;
    if (d->m_document == editor->d->m_document)
        return;
    d->setupDocumentSignals(editor->d->m_document);
    d->m_document = editor->d->m_document;
}

QString BaseTextEditor::displayName() const
{
    return d->m_displayName;
}

void BaseTextEditor::setDisplayName(const QString &title)
{
    d->m_displayName = title;
}

BaseTextDocument *BaseTextEditor::baseTextDocument() const
{
    return d->m_document;
}

void BaseTextEditor::setBaseTextDocument(BaseTextDocument *doc)
{
    if (doc) {
        d->setupDocumentSignals(doc);
        d->m_document = doc;
    }
}

// called before reload
void BaseTextEditor::memorizeCursorPosition()
{
    d->m_tempState = saveState();
}

// called after reload
void BaseTextEditor::restoreCursorPosition()
{
    restoreState(d->m_tempState);
    if (d->m_displaySettings.m_autoFoldFirstComment)
        d->collapseLicenseHeader();
}

QByteArray BaseTextEditor::saveState() const
{
    QByteArray state;
    QDataStream stream(&state, QIODevice::WriteOnly);
    stream << 0; // version number
    stream << verticalScrollBar()->value();
    stream << horizontalScrollBar()->value();
    int line, column;
    convertPosition(textCursor().position(), &line, &column);
    stream << line;
    stream << column;
    return state;
}

bool BaseTextEditor::restoreState(const QByteArray &state)
{
    int version;
    int vval;
    int hval;
    int lval;
    int cval;
    QDataStream stream(state);
    stream >> version;
    stream >> vval;
    stream >> hval;
    stream >> lval;
    stream >> cval;
    d->m_lastCursorChangeWasInteresting = false; // avoid adding last position to history
    gotoLine(lval, cval);
    verticalScrollBar()->setValue(vval);
    horizontalScrollBar()->setValue(hval);
    saveCurrentCursorPositionForNavigation();
    return true;
}

void BaseTextEditor::setDefaultPath(const QString &defaultPath)
{
    baseTextDocument()->setDefaultPath(defaultPath);
}

void BaseTextEditor::setSuggestedFileName(const QString &suggestedFileName)
{
    baseTextDocument()->setSuggestedFileName(suggestedFileName);
}

void BaseTextEditor::setParenthesesMatchingEnabled(bool b)
{
    d->m_parenthesesMatchingEnabled = b;
}

bool BaseTextEditor::isParenthesesMatchingEnabled() const
{
    return d->m_parenthesesMatchingEnabled;
}

void BaseTextEditor::setHighlightCurrentLine(bool b)
{
    d->m_highlightCurrentLine = b;
    updateCurrentLineHighlight();
}

bool BaseTextEditor::highlightCurrentLine() const
{
    return d->m_highlightCurrentLine;
}

void BaseTextEditor::setLineNumbersVisible(bool b)
{
    d->m_lineNumbersVisible = b;
    slotUpdateExtraAreaWidth();
}

bool BaseTextEditor::lineNumbersVisible() const
{
    return d->m_lineNumbersVisible;
}

void BaseTextEditor::setMarksVisible(bool b)
{
    d->m_marksVisible = b;
    slotUpdateExtraAreaWidth();
}

bool BaseTextEditor::marksVisible() const
{
    return d->m_marksVisible;
}

void BaseTextEditor::setRequestMarkEnabled(bool b)
{
    d->m_requestMarkEnabled = b;
}

bool BaseTextEditor::requestMarkEnabled() const
{
    return d->m_requestMarkEnabled;
}

void BaseTextEditor::setLineSeparatorsAllowed(bool b)
{
    d->m_lineSeparatorsAllowed = b;
}

bool BaseTextEditor::lineSeparatorsAllowed() const
{
    return d->m_lineSeparatorsAllowed;
}

void BaseTextEditor::setCodeFoldingVisible(bool b)
{
    d->m_codeFoldingVisible = b && d->m_codeFoldingSupported;
    slotUpdateExtraAreaWidth();
}

bool BaseTextEditor::codeFoldingVisible() const
{
    return d->m_codeFoldingVisible;
}

/**
 * Sets whether code folding is supported by the syntax highlighter. When not
 * supported (the default), this makes sure the code folding is not shown.
 *
 * Needs to be called before calling setCodeFoldingVisible.
 */
void BaseTextEditor::setCodeFoldingSupported(bool b)
{
    d->m_codeFoldingSupported = b;
}

bool BaseTextEditor::codeFoldingSupported() const
{
    return d->m_codeFoldingSupported;
}

void BaseTextEditor::setMouseNavigationEnabled(bool b)
{
    d->m_behaviorSettings.m_mouseNavigation = b;
}

bool BaseTextEditor::mouseNavigationEnabled() const
{
    return d->m_behaviorSettings.m_mouseNavigation;
}

void BaseTextEditor::setScrollWheelZoomingEnabled(bool b)
{
    d->m_behaviorSettings.m_scrollWheelZooming = b;
}

bool BaseTextEditor::scrollWheelZoomingEnabled() const
{
    return d->m_behaviorSettings.m_scrollWheelZooming;
}

void BaseTextEditor::setRevisionsVisible(bool b)
{
    d->m_revisionsVisible = b;
    slotUpdateExtraAreaWidth();
}

bool BaseTextEditor::revisionsVisible() const
{
    return d->m_revisionsVisible;
}

void BaseTextEditor::setVisibleWrapColumn(int column)
{
    d->m_visibleWrapColumn = column;
    viewport()->update();
}

int BaseTextEditor::visibleWrapColumn() const
{
    return d->m_visibleWrapColumn;
}

//--------- BaseTextEditorPrivate -----------

BaseTextEditorPrivate::BaseTextEditorPrivate()
    :
    m_contentsChanged(false),
    m_lastCursorChangeWasInteresting(false),
    m_document(new BaseTextDocument()),
    m_parenthesesMatchingEnabled(false),
    m_extraArea(0),
    m_mouseOnCollapsedMarker(false),
    m_marksVisible(false),
    m_codeFoldingVisible(false),
    m_codeFoldingSupported(false),
    m_revisionsVisible(false),
    m_lineNumbersVisible(true),
    m_highlightCurrentLine(true),
    m_requestMarkEnabled(true),
    m_lineSeparatorsAllowed(false),
    m_visibleWrapColumn(0),
    m_linkPressed(false),
    m_editable(0),
    m_actionHack(0),
    m_inBlockSelectionMode(false),
    m_lastEventWasBlockSelectionEvent(false),
    m_blockSelectionExtraX(0),
    m_moveLineUndoHack(false),
    m_cursorBlockNumber(-1)
{
}

BaseTextEditorPrivate::~BaseTextEditorPrivate()
{
}

void BaseTextEditorPrivate::setupDocumentSignals(BaseTextDocument *document)
{
    BaseTextDocument *oldDocument = q->baseTextDocument();
    if (oldDocument) {
        q->disconnect(oldDocument->document(), 0, q, 0);
        q->disconnect(oldDocument, 0, q, 0);
    }

    QTextDocument *doc = document->document();
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(doc->documentLayout());
    if (!documentLayout) {
        QTextOption opt = doc->defaultTextOption();
        opt.setTextDirection(Qt::LeftToRight);
        opt.setFlags(opt.flags() | QTextOption::IncludeTrailingSpaces
                | QTextOption::AddSpaceForLineAndParagraphSeparators
                );
        doc->setDefaultTextOption(opt);
        documentLayout = new TextEditDocumentLayout(doc);
        doc->setDocumentLayout(documentLayout);
    }


    q->setDocument(doc);
    QObject::connect(documentLayout, SIGNAL(updateBlock(QTextBlock)), q, SLOT(slotUpdateBlockNotify(QTextBlock)));
    QObject::connect(q, SIGNAL(requestBlockUpdate(QTextBlock)), documentLayout, SIGNAL(updateBlock(QTextBlock)));
    QObject::connect(doc, SIGNAL(modificationChanged(bool)), q, SIGNAL(changed()));
    QObject::connect(doc, SIGNAL(contentsChange(int,int,int)), q,
        SLOT(editorContentsChange(int,int,int)), Qt::DirectConnection);
    QObject::connect(document, SIGNAL(changed()), q, SIGNAL(changed()));
    QObject::connect(document, SIGNAL(titleChanged(QString)), q, SLOT(setDisplayName(const QString &)));
    QObject::connect(document, SIGNAL(aboutToReload()), q, SLOT(memorizeCursorPosition()));
    QObject::connect(document, SIGNAL(reloaded()), q, SLOT(restoreCursorPosition()));
    q->slotUpdateExtraAreaWidth();
}


void BaseTextEditorPrivate::snippetCheckCursor(const QTextCursor &cursor)
{
    if (!m_snippetOverlay->isVisible() || m_snippetOverlay->isEmpty())
        return;

    QTextCursor start = cursor;
    start.setPosition(cursor.selectionStart());
    QTextCursor end = cursor;
    end.setPosition(cursor.selectionEnd());
    if (!m_snippetOverlay->hasCursorInSelection(start)
        || !m_snippetOverlay->hasCursorInSelection(end)) {
        m_snippetOverlay->setVisible(false);
        m_snippetOverlay->clear();
    }
}

void BaseTextEditorPrivate::snippetTabOrBacktab(bool forward)
{
    if (!m_snippetOverlay->isVisible() || m_snippetOverlay->isEmpty())
        return;
    QTextCursor cursor = q->textCursor();
    OverlaySelection final;
    if (forward) {
        for (int i = 0; i < m_snippetOverlay->m_selections.count(); ++i){
            const OverlaySelection &selection = m_snippetOverlay->m_selections.at(i);
            if (selection.m_cursor_begin.position() >= cursor.position()
                && selection.m_cursor_end.position() > cursor.position()) {
                final = selection;
                break;
            }
        }
    } else {
        for (int i = m_snippetOverlay->m_selections.count()-1; i >= 0; --i){
            const OverlaySelection &selection = m_snippetOverlay->m_selections.at(i);
            if (selection.m_cursor_end.position() < cursor.position()) {
                final = selection;
                break;
            }
        }

    }
    if (final.m_cursor_begin.isNull())
        final = forward ? m_snippetOverlay->m_selections.first() : m_snippetOverlay->m_selections.last();

    if (final.m_cursor_begin.position() == final.m_cursor_end.position()) { // empty tab stop
        cursor.setPosition(final.m_cursor_end.position());
    } else {
        cursor.setPosition(final.m_cursor_begin.position()+1);
        cursor.setPosition(final.m_cursor_end.position(), QTextCursor::KeepAnchor);
    }
    q->setTextCursor(cursor);
}

bool Parenthesis::hasClosingCollapse(const Parentheses &parentheses)
{
    return closeCollapseAtPos(parentheses) >= 0;
}


int Parenthesis::closeCollapseAtPos(const Parentheses &parentheses)
{
    int depth = 0;
    for (int i = 0; i < parentheses.size(); ++i) {
        const Parenthesis &p = parentheses.at(i);
        if (p.chr == QLatin1Char('{')
            || p.chr == QLatin1Char('+')
            || p.chr == QLatin1Char('[')) {
            ++depth;
        } else if (p.chr == QLatin1Char('}')
            || p.chr == QLatin1Char('-')
            || p.chr == QLatin1Char(']')) {
            if (--depth < 0)
                return p.pos;
        }
    }
    return -1;
}

int Parenthesis::collapseAtPos(const Parentheses &parentheses, QChar *character)
{
    int result = -1;
    QChar c;

    int depth = 0;
    for (int i = 0; i < parentheses.size(); ++i) {
        const Parenthesis &p = parentheses.at(i);
        if (p.chr == QLatin1Char('{')
            || p.chr == QLatin1Char('+')
            || p.chr == QLatin1Char('[')) {
            if (depth == 0) {
                result = p.pos;
                c = p.chr;
            }
            ++depth;
        } else if (p.chr == QLatin1Char('}')
            || p.chr == QLatin1Char('-')
            || p.chr == QLatin1Char(']')) {
            if (--depth < 0)
                depth = 0;
            result = -1;
        }
    }
    if (result >= 0 && character)
        *character = c;
    return result;
}


int TextBlockUserData::collapseAtPos(QChar *character) const
{
    return Parenthesis::collapseAtPos(m_parentheses, character);
}

int TextBlockUserData::braceDepthDelta() const
{
    int delta = 0;
    for (int i = 0; i < m_parentheses.size(); ++i) {
        switch (m_parentheses.at(i).chr.unicode()) {
        case '{': case '+': case '[': ++delta; break;
        case '}': case '-': case ']': --delta; break;
        default: break;
        }
    }
    return delta;
}

void TextEditDocumentLayout::setParentheses(const QTextBlock &block, const Parentheses &parentheses)
{
    if (parentheses.isEmpty()) {
        if (TextBlockUserData *userData = testUserData(block))
            userData->clearParentheses();
    } else {
        userData(block)->setParentheses(parentheses);
    }
}

Parentheses TextEditDocumentLayout::parentheses(const QTextBlock &block)
{
    if (TextBlockUserData *userData = testUserData(block))
        return userData->parentheses();
    return Parentheses();
}

bool TextEditDocumentLayout::hasParentheses(const QTextBlock &block)
{
    if (TextBlockUserData *userData = testUserData(block))
        return userData->hasParentheses();
    return false;
}

int TextEditDocumentLayout::braceDepthDelta(const QTextBlock &block)
{
    if (TextBlockUserData *userData = testUserData(block))
        return userData->braceDepthDelta();
    return 0;
}

int TextEditDocumentLayout::braceDepth(const QTextBlock &block)
{
    int state = block.userState();
    if (state == -1)
        return 0;
    return state >> 8;
}

void TextEditDocumentLayout::setBraceDepth(QTextBlock &block, int depth)
{
    int state = block.userState();
    if (state == -1)
        state = 0;
    state = state & 0xff;
    block.setUserState((depth << 8) | state);
}

void TextEditDocumentLayout::changeBraceDepth(QTextBlock &block, int delta)
{
    if (delta)
        setBraceDepth(block, braceDepth(block) + delta);
}

bool TextEditDocumentLayout::setIfdefedOut(const QTextBlock &block)
{
    return userData(block)->setIfdefedOut();
}

bool TextEditDocumentLayout::clearIfdefedOut(const QTextBlock &block)
{
    if (TextBlockUserData *userData = testUserData(block))
        return userData->clearIfdefedOut();
    return false;
}

bool TextEditDocumentLayout::ifdefedOut(const QTextBlock &block)
{
    if (TextBlockUserData *userData = testUserData(block))
        return userData->ifdefedOut();
    return false;
}


TextEditDocumentLayout::TextEditDocumentLayout(QTextDocument *doc)
    :QPlainTextDocumentLayout(doc) {
    lastSaveRevision = 0;
    hasMarks = 0;
}

TextEditDocumentLayout::~TextEditDocumentLayout()
{
}

QRectF TextEditDocumentLayout::blockBoundingRect(const QTextBlock &block) const
{
    QRectF r = QPlainTextDocumentLayout::blockBoundingRect(block);
    return r;
}


bool BaseTextEditor::viewportEvent(QEvent *event)
{
    d->m_contentsChanged = false;
    if (event->type() == QEvent::ContextMenu) {
        const QContextMenuEvent *ce = static_cast<QContextMenuEvent*>(event);
        if (ce->reason() == QContextMenuEvent::Mouse && !textCursor().hasSelection())
            setTextCursor(cursorForPosition(ce->pos()));
    } else if (event->type() == QEvent::ToolTip) {
        const QHelpEvent *he = static_cast<QHelpEvent*>(event);
        if (QApplication::keyboardModifiers() & Qt::ControlModifier)
            return true; // eat tooltip event when control is pressed
        const QPoint &pos = he->pos();

        // Allow plugins to show tooltips
        const QTextCursor &c = cursorForPosition(pos);
        QPoint cursorPos = mapToGlobal(cursorRect(c).bottomRight() + QPoint(1,1));
        cursorPos.setX(cursorPos.x() + d->m_extraArea->width());

        editableInterface(); // create if necessary

        emit d->m_editable->tooltipRequested(editableInterface(), cursorPos, c.position());
        return true;
    }
    return QPlainTextEdit::viewportEvent(event);
}


void BaseTextEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);
    QRect cr = rect();
    d->m_extraArea->setGeometry(
        QStyle::visualRect(layoutDirection(), cr,
                           QRect(cr.left(), cr.top(), extraAreaWidth(), cr.height())));
}

QRect BaseTextEditor::collapseBox()
{
    if (d->m_highlightBlocksInfo.isEmpty() || d->extraAreaHighlightCollapseBlockNumber < 0)
        return QRect();

    QTextBlock begin = document()->findBlockByNumber(d->m_highlightBlocksInfo.open.last());

    if (TextBlockUserData::hasCollapseAfter(begin.previous()))
        begin = begin.previous();

    QTextBlock end = document()->findBlockByNumber(d->m_highlightBlocksInfo.close.first());
    if (!begin.isValid() || !end.isValid())
        return QRect();
    QRectF br = blockBoundingGeometry(begin).translated(contentOffset());
    QRectF er = blockBoundingGeometry(end).translated(contentOffset());

    return QRect(d->m_extraArea->width() - collapseBoxWidth(fontMetrics()),
                 int(br.top()),
                 collapseBoxWidth(fontMetrics()),
                 er.bottom() - br.top());
}

QTextBlock BaseTextEditor::collapsedBlockAt(const QPoint &pos, QRect *box) const {
    QPointF offset(contentOffset());
    QTextBlock block = firstVisibleBlock();
    int top = (int)blockBoundingGeometry(block).translated(offset).top();
    int bottom = top + (int)blockBoundingRect(block).height();

    int viewportHeight = viewport()->height();

    while (block.isValid() && top <= viewportHeight) {
        QTextBlock nextBlock = block.next();
        if (block.isVisible() && bottom >= 0) {
            if (nextBlock.isValid() && !nextBlock.isVisible()) {
                QTextLayout *layout = block.layout();
                QTextLine line = layout->lineAt(layout->lineCount()-1);
                QRectF lineRect = line.naturalTextRect().translated(offset.x(), top);
                lineRect.adjust(0, 0, -1, -1);

                QRectF collapseRect(lineRect.right() + 12,
                                    lineRect.top(),
                                    fontMetrics().width(QLatin1String(" {...}; ")),
                                    lineRect.height());
                if (collapseRect.contains(pos)) {
                    QTextBlock result = block;
                    if (box)
                        *box = collapseRect.toAlignedRect();
                    return result;
                } else {
                    block = nextBlock;
                    while (nextBlock.isValid() && !nextBlock.isVisible()) {
                        block = nextBlock;
                        nextBlock = block.next();
                    }
                }
            }
        }

        block = nextBlock;
        top = bottom;
        bottom = top + (int)blockBoundingRect(block).height();
    }
    return QTextBlock();
}

void BaseTextEditorPrivate::highlightSearchResults(const QTextBlock &block,
                                                   TextEditorOverlay *overlay)
{
    if (m_searchExpr.isEmpty())
        return;

    int blockPosition = block.position();

    QTextCursor cursor = q->textCursor();
    QString text = block.text();
    text.replace(QChar::Nbsp, QLatin1Char(' '));
    int idx = -1;
    int l = 1;
    while (idx < text.length()) {
        idx = m_searchExpr.indexIn(text, idx + l);
        if (idx < 0)
            break;
        l = m_searchExpr.matchedLength();
        if ((m_findFlags & Find::IFindSupport::FindWholeWords)
            && ((idx && text.at(idx-1).isLetterOrNumber())
                || (idx + l < text.length() && text.at(idx + l).isLetterOrNumber())))
            continue;

        if (m_findScope.isNull()
            || (blockPosition + idx >= m_findScope.selectionStart()
                && blockPosition + idx + l <= m_findScope.selectionEnd())) {

            overlay->addOverlaySelection(blockPosition + idx,
                                         blockPosition + idx + l,
                                         m_searchResultFormat.background().color().darker(120),
                                         QColor(),
                                         (idx == cursor.selectionStart() - blockPosition
                                          && idx + l == cursor.selectionEnd() - blockPosition)?
                                         TextEditorOverlay::DropShadow : 0);

        }
    }
}


namespace TextEditor {
    namespace Internal {
        struct BlockSelectionData {
            int selectionIndex;
            int selectionStart;
            int selectionEnd;
            int firstColumn;
            int lastColumn;
        };
    }
}

void BaseTextEditorPrivate::clearBlockSelection()
{
    if (m_inBlockSelectionMode) {
        m_inBlockSelectionMode = false;
        QTextCursor cursor = q->textCursor();
        cursor.clearSelection();
        q->setTextCursor(cursor);
    }
}

QString BaseTextEditorPrivate::copyBlockSelection()
{
    QString text;

    QTextCursor cursor = q->textCursor();
    if (!cursor.hasSelection())
        return text;

    QTextDocument *doc = q->document();
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();
    QTextBlock startBlock = doc->findBlock(start);
    int columnA = start - startBlock.position();
    QTextBlock endBlock = doc->findBlock(end);
    int columnB = end - endBlock.position();
    int firstColumn = qMin(columnA, columnB);
    int lastColumn = qMax(columnA, columnB) + m_blockSelectionExtraX;

    QTextBlock block = startBlock;
    for (;;) {

        cursor.setPosition(block.position() + qMin(block.length()-1, firstColumn));
        cursor.setPosition(block.position() + qMin(block.length()-1, lastColumn), QTextCursor::KeepAnchor);
        text += cursor.selectedText();
        if (block == endBlock)
            break;
        text += QLatin1Char('\n');
        block = block.next();
    }

    return text;
}

void BaseTextEditorPrivate::removeBlockSelection(const QString &text)
{
    QTextCursor cursor = q->textCursor();
    if (!cursor.hasSelection())
        return;

    QTextDocument *doc = q->document();
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();
    QTextBlock startBlock = doc->findBlock(start);
    int columnA = start - startBlock.position();
    QTextBlock endBlock = doc->findBlock(end);
    int columnB = end - endBlock.position();
    int firstColumn = qMin(columnA, columnB);
    int lastColumn = qMax(columnA, columnB) + m_blockSelectionExtraX;

    cursor.clearSelection();
    cursor.beginEditBlock();

    QTextBlock block = startBlock;
    for (;;) {

        cursor.setPosition(block.position() + qMin(block.length()-1, firstColumn));
        cursor.setPosition(block.position() + qMin(block.length()-1, lastColumn), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        if (block == endBlock)
            break;
        block = block.next();
    }

    cursor.setPosition(start);
    if (!text.isEmpty())
        cursor.insertText(text);
    cursor.endEditBlock();
    q->setTextCursor(cursor);
}

void BaseTextEditorPrivate::moveCursorVisible(bool ensureVisible)
{
    QTextCursor cursor = q->textCursor();
    if (!cursor.block().isVisible()) {
        cursor.setVisualNavigation(true);
        cursor.movePosition(QTextCursor::Up);
        q->setTextCursor(cursor);
    }
    if (ensureVisible)
        q->ensureCursorVisible();
}

static QColor blendColors(const QColor &a, const QColor &b, int alpha)
{
    return QColor((a.red()   * (256 - alpha) + b.red()   * alpha) / 256,
                  (a.green() * (256 - alpha) + b.green() * alpha) / 256,
                  (a.blue()  * (256 - alpha) + b.blue()  * alpha) / 256);
}

static QColor calcBlendColor(const QColor &baseColor, int level, int count)
{
    QColor color80;
    QColor color90;

    if (baseColor.value() > 128) {
        const int f90 = 15;
        const int f80 = 30;
        color80.setRgb(qMax(0, baseColor.red() - f80),
                       qMax(0, baseColor.green() - f80),
                       qMax(0, baseColor.blue() - f80));
        color90.setRgb(qMax(0, baseColor.red() - f90),
                       qMax(0, baseColor.green() - f90),
                       qMax(0, baseColor.blue() - f90));
    } else {
        const int f90 = 20;
        const int f80 = 40;
        color80.setRgb(qMin(255, baseColor.red() + f80),
                       qMin(255, baseColor.green() + f80),
                       qMin(255, baseColor.blue() + f80));
        color90.setRgb(qMin(255, baseColor.red() + f90),
                       qMin(255, baseColor.green() + f90),
                       qMin(255, baseColor.blue() + f90));
    }

    if (level == count)
        return baseColor;
    if (level == 0)
        return color80;
    if (level == count - 1)
        return color90;

    const int blendFactor = level * (256 / (count - 2));

    return blendColors(color80, color90, blendFactor);
}

void BaseTextEditor::paintEvent(QPaintEvent *e)
{
    /*
      Here comes an almost verbatim copy of
      QPlainTextEdit::paintEvent() so we can adjust the extra
      selections dynamically to indicate all search results.
    */
    //begin QPlainTextEdit::paintEvent()

    QPainter painter(viewport());
    QTextDocument *doc = document();
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(doc->documentLayout());
    QTC_ASSERT(documentLayout, return);

    QPointF offset(contentOffset());

    bool hasMainSelection = textCursor().hasSelection();

    QRect er = e->rect();
    QRect viewportRect = viewport()->rect();

    const QColor baseColor = palette().base().color();

    qreal lineX = 0;

    if (d->m_visibleWrapColumn > 0) {
        lineX = fontMetrics().averageCharWidth() * d->m_visibleWrapColumn + offset.x() + 4;

        if (lineX < viewportRect.width()) {
            const QBrush background = d->m_ifdefedOutFormat.background();
            painter.fillRect(QRectF(lineX, er.top(), viewportRect.width() - lineX, er.height()),
                             background);

            const QColor col = (palette().base().color().value() > 128) ? Qt::black : Qt::white;
            const QPen pen = painter.pen();
            painter.setPen(blendColors(background.color(), col, 32));
            painter.drawLine(QPointF(lineX, er.top()), QPointF(lineX, er.bottom()));
            painter.setPen(pen);
        }
    }

    // Set a brush origin so that the WaveUnderline knows where the wave started
    painter.setBrushOrigin(offset);

//    // keep right margin clean from full-width selection
//    int maxX = offset.x() + qMax((qreal)viewportRect.width(), documentLayout->documentSize().width())
//               - doc->documentMargin();
//    er.setRight(qMin(er.right(), maxX));
//    painter.setClipRect(er);

    bool editable = !isReadOnly();
    QTextBlock block = firstVisibleBlock();

    QAbstractTextDocumentLayout::PaintContext context = getPaintContext();

    if (!d->m_highlightBlocksInfo.isEmpty()) {
        // extra pass for the block highlight

        const int margin = 5;
        QTextBlock blockFP = block;
        QPointF offsetFP = offset;
        while (blockFP.isValid()) {
            QRectF r = blockBoundingRect(blockFP).translated(offsetFP);

            int n = blockFP.blockNumber();
            int depth = 0;
            foreach (int i, d->m_highlightBlocksInfo.open)
                if (n >= i)
                    ++depth;
            foreach (int i, d->m_highlightBlocksInfo.close)
                if (n > i)
                    --depth;

            int count = d->m_highlightBlocksInfo.count();
            if (count) {
                QRectF rr = r;
                rr.setWidth(viewport()->width());
                if (lineX > 0)
                    rr.setRight(qMin(lineX, rr.right()));
                for (int i = 0; i <= depth; ++i) {
                    int vi = i > 0 ? d->m_highlightBlocksInfo.visualIndent.at(i-1) : 0;
                    painter.fillRect(rr.adjusted(vi, 0, -8*i, 0), calcBlendColor(baseColor, i, count));
                }
            }
            offsetFP.ry() += r.height();

            if (offsetFP.y() > viewportRect.height() + margin)
                break;

            blockFP = blockFP.next();
            if (!blockFP.isVisible()) {
                // invisible blocks do have zero line count
                blockFP = doc->findBlockByLineNumber(blockFP.firstLineNumber());
            }
        }
    }



    if (!d->m_findScope.isNull()) {

        TextEditorOverlay *overlay = new TextEditorOverlay(this);
        overlay->addOverlaySelection(d->m_findScope, d->m_searchScopeFormat.background().color().darker(120),
                                     d->m_searchScopeFormat.background().color());
        overlay->setAlpha(false);
        overlay->paint(&painter, e->rect());
        delete overlay;
    }

    BlockSelectionData *blockSelection = 0;

    if (d->m_inBlockSelectionMode
        && context.selections.count() && context.selections.last().cursor == textCursor()) {
        blockSelection = new BlockSelectionData;
        blockSelection->selectionIndex = context.selections.size()-1;
        const QAbstractTextDocumentLayout::Selection &selection = context.selections[blockSelection->selectionIndex];
        int start = blockSelection->selectionStart = selection.cursor.selectionStart();
        int end = blockSelection->selectionEnd = selection.cursor.selectionEnd();
        QTextBlock block = doc->findBlock(start);
        int columnA = start - block.position();
        block = doc->findBlock(end);
        int columnB = end - block.position();
        blockSelection->firstColumn = qMin(columnA, columnB);
        blockSelection->lastColumn = qMax(columnA, columnB) + d->m_blockSelectionExtraX;
    }

    QTextBlock visibleCollapsedBlock;
    QPointF visibleCollapsedBlockOffset;

    QTextLayout *cursor_layout = 0;
    QPointF cursor_offset;
    int cursor_cpos = 0;
    QPen cursor_pen;

    d->m_searchResultOverlay->clear();
    if (!d->m_searchExpr.isEmpty()) { // first pass for the search result overlays

        const int margin = 5;
        QTextBlock blockFP = block;
        QPointF offsetFP = offset;
        while (blockFP.isValid()) {
            QRectF r = blockBoundingRect(blockFP).translated(offsetFP);

            if (r.bottom() >= er.top() - margin && r.top() <= er.bottom() + margin) {
                d->highlightSearchResults(blockFP,
                                          d->m_searchResultOverlay);
            }
            offsetFP.ry() += r.height();

            if (offsetFP.y() > viewportRect.height() + margin)
                break;

            blockFP = blockFP.next();
            if (!blockFP.isVisible()) {
                // invisible blocks do have zero line count
                blockFP = doc->findBlockByLineNumber(blockFP.firstLineNumber());
            }
        }

    } // end first pass


    d->m_searchResultOverlay->fill(&painter,
                                   d->m_searchResultFormat.background().color(),
                                   e->rect());


    while (block.isValid()) {

        QRectF r = blockBoundingRect(block).translated(offset);

        if (r.bottom() >= er.top() && r.top() <= er.bottom()) {

            if (TextEditDocumentLayout::ifdefedOut(block)) {
                QRectF rr = r;
                rr.setWidth(viewport()->width());
                if (lineX > 0)
                    rr.setRight(qMin(lineX, rr.right()));
                painter.fillRect(rr, d->m_ifdefedOutFormat.background());
            }

            QTextLayout *layout = block.layout();

#if 0
            QTextOption option = layout->textOption();
            if (TextEditDocumentLayout::ifdefedOut(block)) {
                option.setFlags(option.flags() /*| QTextOption::SuppressColors*/);
                painter.setPen(d->m_ifdefedOutFormat.foreground().color());
            } else {
                option.setFlags(option.flags() & ~QTextOption::SuppressColors);
                painter.setPen(context.palette.text().color());
            }
            layout->setTextOption(option);
#endif

            int blpos = block.position();
            int bllen = block.length();

            QVector<QTextLayout::FormatRange> selections;
            QVector<QTextLayout::FormatRange> prioritySelections;

            for (int i = 0; i < context.selections.size(); ++i) {
                const QAbstractTextDocumentLayout::Selection &range = context.selections.at(i);
                const int selStart = range.cursor.selectionStart() - blpos;
                const int selEnd = range.cursor.selectionEnd() - blpos;
                if (selStart < bllen && selEnd > 0
                    && selEnd > selStart) {
                    QTextLayout::FormatRange o;
                    o.start = selStart;
                    o.length = selEnd - selStart;
                    o.format = range.format;
                    if (blockSelection && blockSelection->selectionIndex == i) {
                        o.start = qMin(blockSelection->firstColumn, bllen-1);
                        o.length = qMin(blockSelection->lastColumn, bllen-1) - o.start;
                    }
                    if ((hasMainSelection && i == context.selections.size()-1)
                        || (o.format.foreground().style() == Qt::NoBrush
                        && o.format.underlineStyle() != QTextCharFormat::NoUnderline
                        && o.format.background() == Qt::NoBrush))
                        prioritySelections.append(o);
                    else
                        selections.append(o);
                } else if (!range.cursor.hasSelection() && range.format.hasProperty(QTextFormat::FullWidthSelection)
                    && block.contains(range.cursor.position())) {
                    // for full width selections we don't require an actual selection, just
                    // a position to specify the line. that's more convenience in usage.
                    QTextLayout::FormatRange o;
                    QTextLine l = layout->lineForTextPosition(range.cursor.position() - blpos);
                    o.start = l.textStart();
                    o.length = l.textLength();
                    if (o.start + o.length == bllen - 1)
                        ++o.length; // include newline
                    o.format = range.format;
                    selections.append(o);
                }
            }
            selections += prioritySelections;

            bool drawCursor = ((editable || true) // we want the cursor in read-only mode
                               && context.cursorPosition >= blpos
                               && context.cursorPosition < blpos + bllen);

            bool drawCursorAsBlock = drawCursor && overwriteMode() ;

            if (drawCursorAsBlock) {
                if (context.cursorPosition == blpos + bllen - 1) {
                    drawCursorAsBlock = false;
                } else {
                    QTextLayout::FormatRange o;
                    o.start = context.cursorPosition - blpos;
                    o.length = 1;
                    o.format.setForeground(palette().base());
                    o.format.setBackground(palette().text());
                    selections.append(o);
                }
            }

            layout->draw(&painter, offset, selections, er);

            if ((drawCursor && !drawCursorAsBlock)
                || (editable && context.cursorPosition < -1
                    && !layout->preeditAreaText().isEmpty())) {
                int cpos = context.cursorPosition;
                if (cpos < -1)
                    cpos = layout->preeditAreaPosition() - (cpos + 2);
                else
                    cpos -= blpos;
                cursor_layout = layout;
                cursor_offset = offset;
                cursor_cpos = cpos;
                cursor_pen = painter.pen();
            }
        }

        offset.ry() += r.height();

        if (offset.y() > viewportRect.height())
            break;

        block = block.next();

        if (!block.isVisible()) {
            if (block.blockNumber() == d->visibleCollapsedBlockNumber) {
                visibleCollapsedBlock = block;
                visibleCollapsedBlockOffset = offset + QPointF(0,1);
            }

            // invisible blocks do have zero line count
            block = doc->findBlockByLineNumber(block.firstLineNumber());
        }
    }
    painter.setPen(context.palette.text().color());

    if (backgroundVisible() && !block.isValid() && offset.y() <= er.bottom()
        && (centerOnScroll() || verticalScrollBar()->maximum() == verticalScrollBar()->minimum())) {
        painter.fillRect(QRect(QPoint((int)er.left(), (int)offset.y()), er.bottomRight()), palette().background());
    }

    //end QPlainTextEdit::paintEvent()

    delete blockSelection;

    offset = contentOffset();
    block = firstVisibleBlock();

    int top = (int)blockBoundingGeometry(block).translated(offset).top();
    int bottom = top + (int)blockBoundingRect(block).height();

    QTextCursor cursor = textCursor();
    bool hasSelection = cursor.hasSelection();
    int selectionStart = cursor.selectionStart();
    int selectionEnd = cursor.selectionEnd();


    while (block.isValid() && top <= e->rect().bottom()) {
        QTextBlock nextBlock = block.next();
        QTextBlock nextVisibleBlock = nextBlock;

        if (!nextVisibleBlock.isVisible()) {
            // invisible blocks do have zero line count
            nextVisibleBlock = doc->findBlockByLineNumber(nextVisibleBlock.firstLineNumber());
            // paranoia in case our code somewhere did not set the line count
            // of the invisible block to 0
            while (nextVisibleBlock.isValid() && !nextVisibleBlock.isVisible())
                nextVisibleBlock = nextVisibleBlock.next();
        }
        if (block.isVisible() && bottom >= e->rect().top()) {
            if (d->m_displaySettings.m_visualizeWhitespace) {
                QTextLayout *layout = block.layout();
                int lineCount = layout->lineCount();
                if (lineCount >= 2 || !nextBlock.isValid()) {
                    painter.save();
                    painter.setPen(Qt::lightGray);
                    for (int i = 0; i < lineCount-1; ++i) { // paint line wrap indicator
                        QTextLine line = layout->lineAt(i);
                        QRectF lineRect = line.naturalTextRect().translated(offset.x(), top);
                        QChar visualArrow((ushort)0x21b5);
                        painter.drawText(static_cast<int>(lineRect.right()),
                                         static_cast<int>(lineRect.top() + line.ascent()), visualArrow);
                    }
                    if (!nextBlock.isValid()) { // paint EOF symbol
                        QTextLine line = layout->lineAt(lineCount-1);
                        QRectF lineRect = line.naturalTextRect().translated(offset.x(), top);
                        int h = 4;
                        lineRect.adjust(0, 0, -1, -1);
                        QPainterPath path;
                        QPointF pos(lineRect.topRight() + QPointF(h+4, line.ascent()));
                        path.moveTo(pos);
                        path.lineTo(pos + QPointF(-h, -h));
                        path.lineTo(pos + QPointF(0, -2*h));
                        path.lineTo(pos + QPointF(h, -h));
                        path.closeSubpath();
                        painter.setBrush(painter.pen().color());
                        painter.drawPath(path);
                    }
                    painter.restore();
                }
            }

            if (nextBlock.isValid() && !nextBlock.isVisible()) {

                bool selectThis = (hasSelection
                                   && nextBlock.position() >= selectionStart
                                   && nextBlock.position() < selectionEnd);
                if (selectThis) {
                    painter.save();
                    painter.setBrush(palette().highlight());
                }

                QTextLayout *layout = block.layout();
                QTextLine line = layout->lineAt(layout->lineCount()-1);
                QRectF lineRect = line.naturalTextRect().translated(offset.x(), top);
                lineRect.adjust(0, 0, -1, -1);

                QRectF collapseRect(lineRect.right() + 12,
                                    lineRect.top(),
                                    fontMetrics().width(QLatin1String(" {...}; ")),
                                    lineRect.height());
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.translate(.5, .5);
                painter.drawRoundedRect(collapseRect.adjusted(0, 0, 0, -1), 3, 3);
                painter.setRenderHint(QPainter::Antialiasing, false);
                painter.translate(-.5, -.5);

                QString replacement = QLatin1String("...");

                QTextBlock info = block;
                if (block.userData()
                    && static_cast<TextBlockUserData*>(block.userData())->collapseMode() == TextBlockUserData::CollapseAfter)
                    ;
                else if (block.next().userData()
                         && static_cast<TextBlockUserData*>(block.next().userData())->collapseMode()
                         == TextBlockUserData::CollapseThis) {
                    replacement.prepend(nextBlock.text().trimmed().left(1));
                    info = nextBlock;
                }


                block = nextVisibleBlock.previous();
                if (!block.isValid())
                    block = doc->lastBlock();

                if (info.userData()
                    && static_cast<TextBlockUserData*>(info.userData())->collapseIncludesClosure()) {
                    QString right = block.text().trimmed();
                    if (right.endsWith(QLatin1Char(';'))) {
                        right.chop(1);
                        right = right.trimmed();
                        replacement.append(right.right(right.endsWith(QLatin1Char('/')) ? 2 : 1));
                        replacement.append(QLatin1Char(';'));
                    } else {
                        replacement.append(right.right(right.endsWith(QLatin1Char('/')) ? 2 : 1));
                    }
                }
                if (selectThis)
                    painter.setPen(palette().highlightedText().color());
                painter.drawText(collapseRect, Qt::AlignCenter, replacement);
                if (selectThis)
                    painter.restore();
            }
        }

        block = nextVisibleBlock;
        top = bottom;
        bottom = top + (int)blockBoundingRect(block).height();
    }

    if (visibleCollapsedBlock.isValid() ) {
        int margin = doc->documentMargin();
        qreal maxWidth = 0;
        qreal blockHeight = 0;
        QTextBlock b = visibleCollapsedBlock;

        while (!b.isVisible()) {
            b.setVisible(true); // make sure block bounding rect works
            QRectF r = blockBoundingRect(b).translated(visibleCollapsedBlockOffset);

            QTextLayout *layout = b.layout();
            for (int i = layout->lineCount()-1; i >= 0; --i)
                maxWidth = qMax(maxWidth, layout->lineAt(i).naturalTextWidth() + 2*margin);

            blockHeight += r.height();

            b.setVisible(false); // restore previous state
            b.setLineCount(0); // restore 0 line count for invisible block
            b = b.next();
        }

        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.translate(.5, .5);
        painter.setBrush(d->m_ifdefedOutFormat.background());
        painter.drawRoundedRect(QRectF(visibleCollapsedBlockOffset.x(),
                                       visibleCollapsedBlockOffset.y(),
                                       maxWidth, blockHeight).adjusted(0, 0, 0, 0), 3, 3);
        painter.restore();

        QTextBlock end = b;
        b = visibleCollapsedBlock;
        while (b != end) {
            b.setVisible(true); // make sure block bounding rect works
            QRectF r = blockBoundingRect(b).translated(visibleCollapsedBlockOffset);
            QTextLayout *layout = b.layout();
            QVector<QTextLayout::FormatRange> selections;
            layout->draw(&painter, visibleCollapsedBlockOffset, selections, er);

            b.setVisible(false); // restore previous state
            visibleCollapsedBlockOffset.ry() += r.height();
            b = b.next();
        }
    }

    if (d->m_animator && d->m_animator->isRunning()) {
        QTextCursor cursor = textCursor();
        cursor.setPosition(d->m_animator->position());
        d->m_animator->draw(&painter, cursorRect(cursor).topLeft());
    }


    if (lineX > 0) {
        const QColor bg = palette().base().color();
        QColor col = (bg.value() > 128) ? Qt::black : Qt::white;
        col.setAlpha(32);
        painter.setPen(QPen(col, 0));
        painter.drawLine(QPointF(lineX, 0), QPointF(lineX, viewport()->height()));
    }

    if (d->m_overlay && d->m_overlay->isVisible())
        d->m_overlay->paint(&painter, e->rect());

    if (d->m_snippetOverlay && d->m_snippetOverlay->isVisible())
        d->m_snippetOverlay->paint(&painter, e->rect());

    if (!d->m_searchResultOverlay->isEmpty()) {
        d->m_searchResultOverlay->paint(&painter, e->rect());
        d->m_searchResultOverlay->clear();
    }

    // draw the cursor last, on top of everything
    if (cursor_layout) {
        painter.setPen(cursor_pen);
        cursor_layout->drawCursor(&painter, cursor_offset, cursor_cpos, cursorWidth());
    }

}

QWidget *BaseTextEditor::extraArea() const
{
    return d->m_extraArea;
}

int BaseTextEditor::extraAreaWidth(int *markWidthPtr) const
{
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(document()->documentLayout());
    if (!documentLayout)
        return 0;

    if (!d->m_marksVisible && documentLayout->hasMarks)
        d->m_marksVisible = true;

    int space = 0;
    const QFontMetrics fm(d->m_extraArea->fontMetrics());

    if (d->m_lineNumbersVisible) {
        QFont fnt = d->m_extraArea->font();
        // this works under the assumption that bold or italic can only make a font wider
        fnt.setBold(d->m_currentLineNumberFormat.font().bold());
        fnt.setItalic(d->m_currentLineNumberFormat.font().italic());
        const QFontMetrics linefm(fnt);

        int digits = 2;
        int max = qMax(1, blockCount());
        while (max >= 100) {
            max /= 10;
            ++digits;
        }
        space += linefm.width(QLatin1Char('9')) * digits;
    }
    int markWidth = 0;

    if (d->m_marksVisible) {
        markWidth += fm.lineSpacing();
//     if (documentLayout->doubleMarkCount)
//         markWidth += fm.lineSpacing() / 3;
        space += markWidth;
    } else {
        space += 2;
    }

    if (markWidthPtr)
        *markWidthPtr = markWidth;

    space += 4;

    if (d->m_codeFoldingVisible)
        space += collapseBoxWidth(fm);
    return space;
}

void BaseTextEditor::slotUpdateExtraAreaWidth()
{
    if (isLeftToRight())
        setViewportMargins(extraAreaWidth(), 0, 0, 0);
    else
        setViewportMargins(0, 0, extraAreaWidth(), 0);
}

static void drawRectBox(QPainter *painter, const QRect &rect, bool start, bool end,
                        const QPalette &pal)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    QRgb b = pal.base().color().rgb();
    QRgb h = pal.highlight().color().rgb();
    QColor c = Utils::StyleHelper::mergedColors(b,h, 50);

    QLinearGradient grad(rect.topLeft(), rect.topRight());
    grad.setColorAt(0, c.lighter(110));
    grad.setColorAt(1, c.lighter(130));
    QColor outline = c;
    QRect r = rect;

    painter->fillRect(rect, grad);
    painter->setPen(outline);
    if (start)
        painter->drawLine(rect.topLeft() + QPoint(1, 0), rect.topRight() -  QPoint(1, 0));
    if (end)
        painter->drawLine(rect.bottomLeft() + QPoint(1, 0), rect.bottomRight() -  QPoint(1, 0));

    painter->drawLine(rect.topRight() + QPoint(0, start ? 1 : 0), rect.bottomRight() - QPoint(0, end ? 1 : 0));
    painter->drawLine(rect.topLeft() + QPoint(0, start ? 1 : 0), rect.bottomLeft() - QPoint(0, end ? 1 : 0));

    painter->restore();
}

void BaseTextEditor::extraAreaPaintEvent(QPaintEvent *e)
{
    QTextDocument *doc = document();
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(doc->documentLayout());
    QTC_ASSERT(documentLayout, return);

    int selStart = textCursor().selectionStart();
    int selEnd = textCursor().selectionEnd();

    const QColor baseColor = palette().base().color();
    QPalette pal = d->m_extraArea->palette();
    pal.setCurrentColorGroup(QPalette::Active);
    QPainter painter(d->m_extraArea);
    const QFontMetrics fm(d->m_extraArea->font());
    int fmLineSpacing = fm.lineSpacing();

    int markWidth = 0;
    if (d->m_marksVisible)
        markWidth += fm.lineSpacing();

    const int collapseColumnWidth = d->m_codeFoldingVisible ? collapseBoxWidth(fm): 0;
    const int extraAreaWidth = d->m_extraArea->width() - collapseColumnWidth;

    painter.fillRect(e->rect(), pal.color(QPalette::Base));
    painter.fillRect(e->rect().intersected(QRect(0, 0, extraAreaWidth, INT_MAX)),
                     pal.color(QPalette::Background));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top;

    while (block.isValid() && top <= e->rect().bottom()) {

        top = bottom;
        bottom = top + (int)blockBoundingRect(block).height();
        QTextBlock nextBlock = block.next();

        QTextBlock nextVisibleBlock = nextBlock;
        int nextVisibleBlockNumber = blockNumber + 1;

        if (!nextVisibleBlock.isVisible()) {
            // invisible blocks do have zero line count
            nextVisibleBlock = doc->findBlockByLineNumber(nextVisibleBlock.firstLineNumber());
            nextVisibleBlockNumber = nextVisibleBlock.blockNumber();
        }

        if (bottom < e->rect().top()) {
            block = nextVisibleBlock;
            blockNumber = nextVisibleBlockNumber;
            continue;
        }

        painter.setPen(pal.color(QPalette::Dark));

        if (d->m_codeFoldingVisible || d->m_marksVisible) {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, false);

            int previousBraceDepth = block.previous().userState();
            if (previousBraceDepth >= 0)
                previousBraceDepth >>= 8;
            else
                previousBraceDepth = 0;

            int braceDepth = block.userState();
            if (!nextBlock.isVisible()) {
                QTextBlock lastInvisibleBlock = nextVisibleBlock.previous();
                if (!lastInvisibleBlock.isValid())
                    lastInvisibleBlock = doc->lastBlock();
                braceDepth = lastInvisibleBlock.userState();
            }
            if (braceDepth >= 0)
                braceDepth >>= 8;
            else
                braceDepth = 0;

            if (TextBlockUserData *userData = static_cast<TextBlockUserData*>(block.userData())) {
                if (d->m_marksVisible) {
                    int xoffset = 0;
                    foreach (ITextMark *mrk, userData->marks()) {
                        int x = 0;
                        int radius = fmLineSpacing - 1;
                        QRect r(x + xoffset, top, radius, radius);
                        mrk->icon().paint(&painter, r, Qt::AlignCenter);
                        xoffset += 2;
                    }
                }
            }

            if (d->m_codeFoldingVisible) {

                bool collapseThis = false;
                bool collapseAfter = false;
                bool hasClosingCollapse = false;

                if (TextBlockUserData *userData = static_cast<TextBlockUserData*>(block.userData())) {
                    if (!userData->ifdefedOut()) {
                        collapseAfter = (userData->collapseMode() == TextBlockUserData::CollapseAfter);
                        collapseThis = (userData->collapseMode() == TextBlockUserData::CollapseThis);
                        hasClosingCollapse = userData->hasClosingCollapse() && (previousBraceDepth > 0);
                    }
                }

                int extraAreaHighlightCollapseBlockNumber = -1;
                int extraAreaHighlightCollapseEndBlockNumber = -1;
                bool endIsVisible = false;
                if (!d->m_highlightBlocksInfo.isEmpty()) {
                    extraAreaHighlightCollapseBlockNumber =  d->m_highlightBlocksInfo.open.last();
                    extraAreaHighlightCollapseEndBlockNumber =  d->m_highlightBlocksInfo.close.first();
                    endIsVisible = doc->findBlockByNumber(extraAreaHighlightCollapseEndBlockNumber).isVisible();

                    QTextBlock before = doc->findBlockByNumber(extraAreaHighlightCollapseBlockNumber-1);
                    if (TextBlockUserData::hasCollapseAfter(before)) {
                        extraAreaHighlightCollapseBlockNumber--;
                    }
                }

                TextBlockUserData *nextBlockUserData = TextEditDocumentLayout::testUserData(nextBlock);

                bool collapseNext = nextBlockUserData
                                    && nextBlockUserData->collapseMode() == TextBlockUserData::CollapseThis
                                    && !nextBlockUserData->ifdefedOut();

                bool nextHasClosingCollapse = nextBlockUserData
                                              && nextBlockUserData->hasClosingCollapseInside()
                                              && nextBlockUserData->ifdefedOut();

                bool drawBox = ((collapseAfter || collapseNext) && !nextHasClosingCollapse);
                bool active = blockNumber == extraAreaHighlightCollapseBlockNumber;
                bool drawStart = drawBox && active;
                bool drawEnd = blockNumber == extraAreaHighlightCollapseEndBlockNumber || (drawStart && !endIsVisible);
                bool hovered = blockNumber >= extraAreaHighlightCollapseBlockNumber
                               && blockNumber <= extraAreaHighlightCollapseEndBlockNumber;

                int boxWidth = collapseBoxWidth(fm);
                if (hovered) {
                    QRect box = QRect(extraAreaWidth + 1, top, boxWidth - 2, bottom - top);
                    drawRectBox(&painter, box, drawStart, drawEnd, pal);
                }

                if (drawBox) {
                    bool expanded = nextBlock.isVisible();
                    int size = boxWidth/4;
                    QRect box(extraAreaWidth + size, top + size,
                              2 * (size) + 1, 2 * (size) + 1);
                    drawFoldingMarker(&painter, pal, box, expanded, active, hovered);
                }
            }

            painter.restore();
        }


        if (d->m_revisionsVisible && block.revision() != documentLayout->lastSaveRevision) {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, false);
            if (block.revision() < 0)
                painter.setPen(QPen(Qt::darkGreen, 2));
            else
                painter.setPen(QPen(Qt::red, 2));
            painter.drawLine(extraAreaWidth - 1, top, extraAreaWidth - 1, bottom - 1);
            painter.restore();
        }

        if (d->m_lineNumbersVisible) {
            const QString &number = QString::number(blockNumber + 1);
            bool selected = (
                    (selStart < block.position() + block.length()
                    && selEnd > block.position())
                    || (selStart == selEnd && selStart == block.position())
                    );
            if (selected) {
                painter.save();
                QFont f = painter.font();
                f.setBold(d->m_currentLineNumberFormat.font().bold());
                f.setItalic(d->m_currentLineNumberFormat.font().italic());
                painter.setFont(f);
                painter.setPen(d->m_currentLineNumberFormat.foreground().color());
            }
            painter.drawText(markWidth, top, extraAreaWidth - markWidth - 4, fm.height(), Qt::AlignRight, number);
            if (selected)
                painter.restore();
        }

        block = nextVisibleBlock;
        blockNumber = nextVisibleBlockNumber;
    }
}

void BaseTextEditor::drawFoldingMarker(QPainter *painter, const QPalette &pal,
                                       const QRect &rect,
                                       bool expanded,
                                       bool active,
                                       bool hovered) const
{
    Q_UNUSED(active)
    Q_UNUSED(hovered)
    QStyle *s = style();
    if (ManhattanStyle *ms = qobject_cast<ManhattanStyle*>(s))
        s = ms->baseStyle();

    if (!qstrcmp(s->metaObject()->className(), "OxygenStyle")) {
        painter->save();
        painter->setPen(Qt::NoPen);
        int size = rect.size().width();
        int sqsize = 2*(size/2);

        QColor textColor = pal.buttonText().color();
        QColor brushColor = textColor;

        textColor.setAlpha(100);
        brushColor.setAlpha(100);

        QPolygon a;
        if (expanded) {
            // down arrow
            a.setPoints(3, 0, sqsize/3,  sqsize/2, sqsize  - sqsize/3,  sqsize, sqsize/3);
        } else {
            // right arrow
            a.setPoints(3, sqsize - sqsize/3, sqsize/2,  sqsize/2 - sqsize/3, 0,  sqsize/2 - sqsize/3, sqsize);
            painter->setBrush(brushColor);
        }
        painter->translate(0.5, 0.5);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(rect.topLeft());
        painter->setPen(textColor);
        painter->setBrush(textColor);
        painter->drawPolygon(a);
        painter->restore();
    } else {
        QStyleOptionViewItemV2 opt;
        opt.rect = rect;
        opt.state = QStyle::State_Active | QStyle::State_Item | QStyle::State_Children;
        if (expanded)
            opt.state |= QStyle::State_Open;
        if (active)
            opt.state |= QStyle::State_MouseOver | QStyle::State_Enabled | QStyle::State_Selected;
        if (hovered)
            opt.palette.setBrush(QPalette::Window, pal.highlight());

         // QGtkStyle needs a small correction to draw the marker in the right place
        if (!qstrcmp(s->metaObject()->className(), "QGtkStyle"))
           opt.rect.translate(-2, 0);
        else if (!qstrcmp(s->metaObject()->className(), "QMacStyle"))
            opt.rect.translate(-1, 0);

        s->drawPrimitive(QStyle::PE_IndicatorBranch, &opt, painter, this);
    }
}

void BaseTextEditor::slotModificationChanged(bool m)
{
    if (m)
        return;

    QTextDocument *doc = document();
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(doc->documentLayout());
    QTC_ASSERT(documentLayout, return);
    int oldLastSaveRevision = documentLayout->lastSaveRevision;
    documentLayout->lastSaveRevision = doc->revision();

    if (oldLastSaveRevision != documentLayout->lastSaveRevision) {
        QTextBlock block = doc->begin();
        while (block.isValid()) {
            if (block.revision() < 0 || block.revision() != oldLastSaveRevision) {
                block.setRevision(-documentLayout->lastSaveRevision - 1);
            } else {
                block.setRevision(documentLayout->lastSaveRevision);
            }
            block = block.next();
        }
    }
    d->m_extraArea->update();
}

void BaseTextEditor::slotUpdateRequest(const QRect &r, int dy)
{
    if (dy)
        d->m_extraArea->scroll(0, dy);
    else if (r.width() > 4) { // wider than cursor width, not just cursor blinking
        d->m_extraArea->update(0, r.y(), d->m_extraArea->width(), r.height());
        if (!d->m_searchExpr.isEmpty()) {
            const int m = d->m_searchResultOverlay->dropShadowWidth();
            viewport()->update(r.adjusted(-m, -m, m, m));
        }
    }

    if (r.contains(viewport()->rect()))
        slotUpdateExtraAreaWidth();
}

void BaseTextEditor::saveCurrentCursorPositionForNavigation()
{
    d->m_lastCursorChangeWasInteresting = true;
    d->m_tempNavigationState = saveState();
}

void BaseTextEditor::updateCurrentLineHighlight()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (d->m_highlightCurrentLine) {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(d->m_currentLineFormat.background());
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        extraSelections.append(sel);
    }

    setExtraSelections(CurrentLineSelection, extraSelections);


    // the extra area shows information for the entire current block, not just the currentline.
    // This is why we must force a bigger update region.
    int cursorBlockNumber = textCursor().blockNumber();
    if (cursorBlockNumber != d->m_cursorBlockNumber) {
        QPointF offset = contentOffset();
        QTextBlock block = document()->findBlockByNumber(d->m_cursorBlockNumber);
        if (block.isValid())
            d->m_extraArea->update(blockBoundingGeometry(block).translated(offset).toAlignedRect());
        block = document()->findBlockByNumber(cursorBlockNumber);
        if (block.isValid())
            d->m_extraArea->update(blockBoundingGeometry(block).translated(offset).toAlignedRect());
        d->m_cursorBlockNumber = cursorBlockNumber;
    }

}

void BaseTextEditor::slotCursorPositionChanged()
{
#if 0
    qDebug() << "block" << textCursor().blockNumber()+1
            << "depth:" << TextEditDocumentLayout::braceDepth(textCursor().block())
            << '/' << TextEditDocumentLayout::braceDepth(document()->lastBlock());
#endif
    if (!d->m_contentsChanged && d->m_lastCursorChangeWasInteresting) {
        Core::EditorManager::instance()->addCurrentPositionToNavigationHistory(editableInterface(), d->m_tempNavigationState);
        d->m_lastCursorChangeWasInteresting = false;
    } else if (d->m_contentsChanged) {
        saveCurrentCursorPositionForNavigation();
    }
    updateHighlights();
}

void BaseTextEditor::updateHighlights()
{
    if (d->m_parenthesesMatchingEnabled && hasFocus()) {
        // Delay update when no matching is displayed yet, to avoid flicker
        if (extraSelections(ParenthesesMatchingSelection).isEmpty()
            && d->m_animator == 0) {
            d->m_parenthesesMatchingTimer->start(50);
        } else {
             // use 0-timer, not direct call, to give the syntax highlighter a chance
            // to update the parantheses information
            d->m_parenthesesMatchingTimer->start(0);
        }
    }

    updateCurrentLineHighlight();

    if (d->m_displaySettings.m_highlightBlocks) {
        QTextCursor cursor = textCursor();
        d->extraAreaHighlightCollapseBlockNumber = cursor.blockNumber();
        d->extraAreaHighlightCollapseColumn = cursor.position() - cursor.block().position();
        d->m_highlightBlocksTimer->start(100);
    }
}

void BaseTextEditor::slotUpdateBlockNotify(const QTextBlock &block)
{
    static bool blockRecursion = false;
    if (blockRecursion)
        return;
    blockRecursion = true;
    if (d->m_overlay->isVisible()) {
        /* an overlay might draw outside the block bounderies, force
           complete viewport update */
        viewport()->update();
    } else {
        if (block.previous().isValid() && block.userState() != block.previous().userState()) {
        /* The syntax highlighting state changes. This opens up for
           the possibility that the paragraph has braces that support
           code folding. In this case, do the save thing and also
           update the previous block, which might contain a collapse
           box which now is invalid.*/
            emit requestBlockUpdate(block.previous());
        }
        if (!d->m_findScope.isNull()) {
            if (block.position() < d->m_findScope.selectionEnd()
                && block.position()+block.length() >= d->m_findScope.selectionStart() ) {
                QTextBlock b = block.document()->findBlock(d->m_findScope.selectionStart());
                do {
                    emit requestBlockUpdate(b);
                    b = b.next();
                } while (b.isValid() && b.position() < d->m_findScope.selectionEnd());
            }
        }
    }
    blockRecursion = false;
}

void BaseTextEditor::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == d->autoScrollTimer.timerId()) {
        const QPoint globalPos = QCursor::pos();
        const QPoint pos = d->m_extraArea->mapFromGlobal(globalPos);
        QRect visible = d->m_extraArea->rect();
        verticalScrollBar()->triggerAction( pos.y() < visible.center().y() ?
                                            QAbstractSlider::SliderSingleStepSub
                                            : QAbstractSlider::SliderSingleStepAdd);
        QMouseEvent ev(QEvent::MouseMove, pos, globalPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        extraAreaMouseEvent(&ev);
        int delta = qMax(pos.y() - visible.top(), visible.bottom() - pos.y()) - visible.height();
        if (delta < 7)
            delta = 7;
        int timeout = 4900 / (delta * delta);
        d->autoScrollTimer.start(timeout, this);

    } else if (e->timerId() == d->collapsedBlockTimer.timerId()) {
        d->visibleCollapsedBlockNumber = d->suggestedVisibleCollapsedBlockNumber;
        d->suggestedVisibleCollapsedBlockNumber = -1;
        d->collapsedBlockTimer.stop();
        viewport()->update();
    }
    QPlainTextEdit::timerEvent(e);
}


void BaseTextEditorPrivate::clearVisibleCollapsedBlock()
{
    if (suggestedVisibleCollapsedBlockNumber) {
        suggestedVisibleCollapsedBlockNumber = -1;
        collapsedBlockTimer.stop();
    }
    if (visibleCollapsedBlockNumber >= 0) {
        visibleCollapsedBlockNumber = -1;
        q->viewport()->update();
    }
}

void BaseTextEditor::mouseMoveEvent(QMouseEvent *e)
{
    d->m_lastEventWasBlockSelectionEvent = (e->modifiers() & Qt::AltModifier);

    updateLink(e);

    if (e->buttons() == Qt::NoButton) {
        const QTextBlock collapsedBlock = collapsedBlockAt(e->pos());
        const int blockNumber = collapsedBlock.next().blockNumber();
        if (blockNumber < 0) {
            d->clearVisibleCollapsedBlock();
        } else if (blockNumber != d->visibleCollapsedBlockNumber) {
            d->suggestedVisibleCollapsedBlockNumber = blockNumber;
            d->collapsedBlockTimer.start(40, this);
        }

        // Update the mouse cursor
        if (collapsedBlock.isValid() && !d->m_mouseOnCollapsedMarker) {
            d->m_mouseOnCollapsedMarker = true;
            viewport()->setCursor(Qt::PointingHandCursor);
        } else if (!collapsedBlock.isValid() && d->m_mouseOnCollapsedMarker) {
            d->m_mouseOnCollapsedMarker = false;
            viewport()->setCursor(Qt::IBeamCursor);
        }
    } else {
        QPlainTextEdit::mouseMoveEvent(e);
    }
    if (d->m_lastEventWasBlockSelectionEvent && d->m_inBlockSelectionMode) {
        if (textCursor().atBlockEnd()) {
            d->m_blockSelectionExtraX = qMax(0, e->pos().x() - cursorRect().center().x()) / fontMetrics().averageCharWidth();
        } else {
            d->m_blockSelectionExtraX = 0;
        }
    }
    if (viewport()->cursor().shape() == Qt::BlankCursor)
        viewport()->setCursor(Qt::IBeamCursor);
}

void BaseTextEditor::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        d->clearBlockSelection(); // just in case, otherwise we might get strange drag and drop

        QTextBlock collapsedBlock = collapsedBlockAt(e->pos());
        if (collapsedBlock.isValid()) {
            toggleBlockVisible(collapsedBlock);
            viewport()->setCursor(Qt::IBeamCursor);
        }

        updateLink(e);

        if (d->m_currentLink.isValid())
            d->m_linkPressed = true;
    }
    QPlainTextEdit::mousePressEvent(e);
}

void BaseTextEditor::mouseReleaseEvent(QMouseEvent *e)
{
    if (mouseNavigationEnabled()
        && d->m_linkPressed
        && e->modifiers() & Qt::ControlModifier
        && !(e->modifiers() & Qt::ShiftModifier)
        && e->button() == Qt::LeftButton
        ) {
        const QTextCursor cursor = cursorForPosition(e->pos());
        if (openLink(findLinkAt(cursor))) {
            clearLink();
            return;
        }
    }

    QPlainTextEdit::mouseReleaseEvent(e);
}

void BaseTextEditor::leaveEvent(QEvent *e)
{
    // Clear link emulation when the mouse leaves the editor
    clearLink();
    QPlainTextEdit::leaveEvent(e);
}

void BaseTextEditor::keyReleaseEvent(QKeyEvent *e)
{
    // Clear link emulation when Ctrl is released
    if (e->key() == Qt::Key_Control)
        clearLink();

    QPlainTextEdit::keyReleaseEvent(e);
}

void BaseTextEditor::dragEnterEvent(QDragEnterEvent *e)
{
    // If the drag event contains URLs, we don't want to insert them as text
    if (e->mimeData()->hasUrls()) {
        e->ignore();
        return;
    }

    QPlainTextEdit::dragEnterEvent(e);
}

void BaseTextEditor::extraAreaLeaveEvent(QEvent *)
{
    // fake missing mouse move event from Qt
    QMouseEvent me(QEvent::MouseMove, QPoint(-1, -1), Qt::NoButton, 0, 0);
    extraAreaMouseEvent(&me);
}

void BaseTextEditor::extraAreaMouseEvent(QMouseEvent *e)
{
    QTextCursor cursor = cursorForPosition(QPoint(0, e->pos().y()));
    cursor.setPosition(cursor.block().position());

    int markWidth;
    extraAreaWidth(&markWidth);

    if (d->m_codeFoldingVisible
        && e->type() == QEvent::MouseMove && e->buttons() == 0) { // mouse tracking
        // Update which folder marker is highlighted
        const int highlightBlockNumber = d->extraAreaHighlightCollapseBlockNumber;
        const int highlightColumn = d->extraAreaHighlightCollapseColumn;
        d->extraAreaHighlightCollapseBlockNumber = -1;
        d->extraAreaHighlightCollapseColumn = -1;

        if (e->pos().x() > extraArea()->width() - collapseBoxWidth(fontMetrics())) {
            d->extraAreaHighlightCollapseBlockNumber = cursor.blockNumber();
            if (TextBlockUserData::canCollapse(cursor.block())
                || !TextBlockUserData::hasClosingCollapse(cursor.block()))
                d->extraAreaHighlightCollapseColumn = cursor.block().length()-1;
            if (TextBlockUserData::hasCollapseAfter(cursor.block())) {
                d->extraAreaHighlightCollapseBlockNumber++;
                d->extraAreaHighlightCollapseColumn = -1;
                if (TextBlockUserData::canCollapse(cursor.block().next())
                    || !TextBlockUserData::hasClosingCollapse(cursor.block().next()))
                    d->extraAreaHighlightCollapseColumn = cursor.block().next().length()-1;
            }
        } else if (d->m_displaySettings.m_highlightBlocks) {
            QTextCursor cursor = textCursor();
            d->extraAreaHighlightCollapseBlockNumber = cursor.blockNumber();
            d->extraAreaHighlightCollapseColumn = cursor.position() - cursor.block().position();
        }

        if (highlightBlockNumber != d->extraAreaHighlightCollapseBlockNumber
            || highlightColumn != d->extraAreaHighlightCollapseColumn) {
            d->m_highlightBlocksTimer->start(d->m_highlightBlocksInfo.isEmpty() ? 120 : 0);
        }
    }

    // Set whether the mouse cursor is a hand or normal arrow
    if (e->type() == QEvent::MouseMove) {
        bool hand = (e->pos().x() <= markWidth);
        if (hand != (d->m_extraArea->cursor().shape() == Qt::PointingHandCursor))
            d->m_extraArea->setCursor(hand ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }

    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonDblClick) {
        if (e->button() == Qt::LeftButton) {
            int boxWidth = collapseBoxWidth(fontMetrics());
            if (d->m_codeFoldingVisible && e->pos().x() > extraArea()->width() - boxWidth) {
                if (!cursor.block().next().isVisible()) {
                    toggleBlockVisible(cursor.block());
                    d->moveCursorVisible(false);
                } else if (collapseBox().contains(e->pos())) {
                    cursor.setPosition(
                            document()->findBlockByNumber(d->m_highlightBlocksInfo.open.last()).position()
                            );
                    QTextBlock c = cursor.block();
                    if (TextBlockUserData::hasCollapseAfter(c.previous()))
                        c = c.previous();
                    toggleBlockVisible(c);
                    d->moveCursorVisible(false);
                }
            } else if (d->m_marksVisible && e->pos().x() > markWidth) {
                QTextCursor selection = cursor;
                selection.setVisualNavigation(true);
                d->extraAreaSelectionAnchorBlockNumber = selection.blockNumber();
                selection.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                selection.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
                setTextCursor(selection);
            } else {
                d->extraAreaToggleMarkBlockNumber = cursor.blockNumber();
            }
        } else if (d->m_marksVisible && e->button() == Qt::RightButton) {
            QMenu * contextMenu = new QMenu(this);
            emit d->m_editable->markContextMenuRequested(editableInterface(), cursor.blockNumber() + 1, contextMenu);
            if (!contextMenu->isEmpty())
                contextMenu->exec(e->globalPos());
            delete contextMenu;
        }
    } else if (d->extraAreaSelectionAnchorBlockNumber >= 0) {
        QTextCursor selection = cursor;
        selection.setVisualNavigation(true);
        if (e->type() == QEvent::MouseMove) {
            QTextBlock anchorBlock = document()->findBlockByNumber(d->extraAreaSelectionAnchorBlockNumber);
            selection.setPosition(anchorBlock.position());
            if (cursor.blockNumber() < d->extraAreaSelectionAnchorBlockNumber) {
                selection.movePosition(QTextCursor::EndOfBlock);
                selection.movePosition(QTextCursor::Right);
            }
            selection.setPosition(cursor.block().position(), QTextCursor::KeepAnchor);
            if (cursor.blockNumber() >= d->extraAreaSelectionAnchorBlockNumber) {
                selection.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                selection.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            }

            if (e->pos().y() >= 0 && e->pos().y() <= d->m_extraArea->height())
                d->autoScrollTimer.stop();
            else if (!d->autoScrollTimer.isActive())
                d->autoScrollTimer.start(100, this);

        } else {
            d->autoScrollTimer.stop();
            d->extraAreaSelectionAnchorBlockNumber = -1;
            return;
        }
        setTextCursor(selection);
    } else if (d->extraAreaToggleMarkBlockNumber >= 0 && d->m_marksVisible && d->m_requestMarkEnabled) {
        if (e->type() == QEvent::MouseButtonRelease && e->button() == Qt::LeftButton) {
            int n = d->extraAreaToggleMarkBlockNumber;
            d->extraAreaToggleMarkBlockNumber = -1;
            if (cursor.blockNumber() == n) {
                int line = n + 1;
                emit d->m_editable->markRequested(editableInterface(), line);
            }
        }
    }
}

QTextBlock TextBlockUserData::testCollapse(const QTextBlock& block)
{
    QTextBlock info = block;
    if (block.userData() && static_cast<TextBlockUserData*>(block.userData())->collapseMode() == CollapseAfter)
        ;
    else if (block.next().userData()
             && static_cast<TextBlockUserData*>(block.next().userData())->collapseMode()
             == TextBlockUserData::CollapseThis)
        info = block.next();
    else
        return QTextBlock();
    int pos = static_cast<TextBlockUserData*>(info.userData())->collapseAtPos();
    if (pos < 0)
        return QTextBlock();
    QTextCursor cursor(info);
    cursor.setPosition(cursor.position() + pos);
    matchCursorForward(&cursor);
    return cursor.block();
}

void TextBlockUserData::doCollapse(const QTextBlock& block, bool visible)
{
    QTextBlock info = block;
    if (block.userData() && static_cast<TextBlockUserData*>(block.userData())->collapseMode() == CollapseAfter)
        ;
    else if (block.next().userData()
             && static_cast<TextBlockUserData*>(block.next().userData())->collapseMode()
             == TextBlockUserData::CollapseThis)
        info = block.next();
    else {
        if (visible && !block.next().isVisible()) {
            // no match, at least unfold!
            QTextBlock b = block.next();
            while (b.isValid() && !b.isVisible()) {
                b.setVisible(true);
                b.setLineCount(visible ? qMax(1, b.layout()->lineCount()) : 0);
                b = b.next();
            }
        }
        return;
    }
    int pos = static_cast<TextBlockUserData*>(info.userData())->collapseAtPos();
    if (pos < 0)
        return;
    QTextCursor cursor(info);
    cursor.setPosition(cursor.position() + pos);
    if (matchCursorForward(&cursor) != Match) {
        if (visible) {
            // no match, at least unfold!
            QTextBlock b = block.next();
            while (b.isValid() && !b.isVisible()) {
                b.setVisible(true);
                b.setLineCount(visible ? qMax(1, b.layout()->lineCount()) : 0);
                b = b.next();
            }
        }
        return;
    }

    QTextBlock b = block.next();
    while (b < cursor.block()) {
        b.setVisible(visible);
        b.setLineCount(visible ? qMax(1, b.layout()->lineCount()) : 0);
        if (visible) {
            TextBlockUserData *data = canCollapse(b);
            if (data && data->collapsed()) {
                QTextBlock end =  testCollapse(b);
                if (data->collapseIncludesClosure())
                    end = end.next();
                if (end.isValid()) {
                    b = end;
                    continue;
                }
            }
        }
        b = b.next();
    }

    bool collapseIncludesClosure = hasClosingCollapseAtEnd(b);
    if (collapseIncludesClosure) {
        b.setVisible(visible);
        b.setLineCount(visible ? qMax(1, b.layout()->lineCount()) : 0);
    }
    static_cast<TextBlockUserData*>(info.userData())->setCollapseIncludesClosure(collapseIncludesClosure);
    static_cast<TextBlockUserData*>(info.userData())->setCollapsed(!block.next().isVisible());

}


void BaseTextEditor::ensureCursorVisible()
{
    QTextBlock block = textCursor().block();
    if (!block.isVisible()) {
        while (!block.isVisible() && block.previous().isValid())
            block = block.previous();
        toggleBlockVisible(block);
    }
    QPlainTextEdit::ensureCursorVisible();
}

void BaseTextEditor::toggleBlockVisible(const QTextBlock &block)
{
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(document()->documentLayout());
    QTC_ASSERT(documentLayout, return);

    bool visible = block.next().isVisible();
    TextBlockUserData::doCollapse(block, !visible);
    documentLayout->requestUpdate();
    documentLayout->emitDocumentSizeChanged();
}


const TabSettings &BaseTextEditor::tabSettings() const
{
    return d->m_document->tabSettings();
}

const DisplaySettings &BaseTextEditor::displaySettings() const
{
    return d->m_displaySettings;
}


void BaseTextEditor::indentOrUnindent(bool doIndent)
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();

    int pos = cursor.position();
    const TextEditor::TabSettings &tabSettings = d->m_document->tabSettings();

    QTextDocument *doc = document();

    if (!cursor.hasSelection() && doIndent) {
        // Insert tab if there is no selection and indent is requested
        QTextBlock block = cursor.block();
        QString text = block.text();
        int indentPosition = (cursor.position() - block.position());;
        int spaces = tabSettings.spacesLeftFromPosition(text, indentPosition);
        int startColumn = tabSettings.columnAt(text, indentPosition - spaces);
        int targetColumn = tabSettings.indentedColumn(tabSettings.columnAt(text, indentPosition), doIndent);
        cursor.setPosition(block.position() + indentPosition);
        cursor.setPosition(block.position() + indentPosition - spaces, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(tabSettings.indentationString(startColumn, targetColumn, block));
    } else {
        // Indent or unindent the selected lines
        int anchor = cursor.anchor();
        int start = qMin(anchor, pos);
        int end = qMax(anchor, pos);

        QTextBlock startBlock = doc->findBlock(start);
        QTextBlock endBlock = doc->findBlock(end-1).next();

        for (QTextBlock block = startBlock; block != endBlock; block = block.next()) {
            QString text = block.text();
            int indentPosition = tabSettings.lineIndentPosition(text);
            if (!doIndent && !indentPosition)
                indentPosition = tabSettings.firstNonSpace(text);
            int targetColumn = tabSettings.indentedColumn(tabSettings.columnAt(text, indentPosition), doIndent);
            cursor.setPosition(block.position() + indentPosition);
            cursor.insertText(tabSettings.indentationString(0, targetColumn, block));
            cursor.setPosition(block.position());
            cursor.setPosition(block.position() + indentPosition, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
        }
    }

    cursor.endEditBlock();
}

void BaseTextEditor::handleHomeKey(bool anchor)
{
    QTextCursor cursor = textCursor();
    QTextCursor::MoveMode mode = QTextCursor::MoveAnchor;

    if (anchor)
        mode = QTextCursor::KeepAnchor;

    const int initpos = cursor.position();
    int pos = cursor.block().position();
    QChar character = characterAt(pos);
    const QLatin1Char tab = QLatin1Char('\t');

    while (character == tab || character.category() == QChar::Separator_Space) {
        ++pos;
        if (pos == initpos)
            break;
        character = characterAt(pos);
    }

    // Go to the start of the block when we're already at the start of the text
    if (pos == initpos)
        pos = cursor.block().position();

    cursor.setPosition(pos, mode);
    setTextCursor(cursor);
}

void BaseTextEditor::handleBackspaceKey()
{
    QTextCursor cursor = textCursor();
    int pos = cursor.position();
    QTC_ASSERT(!cursor.hasSelection(), return);

    if (d->m_snippetOverlay->isVisible()) {
        QTextCursor snippetCursor = cursor;
        snippetCursor.movePosition(QTextCursor::Left);
        d->snippetCheckCursor(snippetCursor);
    }

    const TextEditor::TabSettings &tabSettings = d->m_document->tabSettings();

    if (tabSettings.m_autoIndent && autoBackspace(cursor))
        return;

    if (!tabSettings.m_smartBackspace) {
        cursor.deletePreviousChar();
        return;
    }

    QTextBlock currentBlock = cursor.block();
    int positionInBlock = pos - currentBlock.position();
    const QString blockText = currentBlock.text();
    if (cursor.atBlockStart() || tabSettings.firstNonSpace(blockText) < positionInBlock) {
        cursor.deletePreviousChar();
        return;
    }

    int previousIndent = 0;
    const int indent = tabSettings.columnAt(blockText, positionInBlock);

    for (QTextBlock previousNonEmptyBlock = currentBlock.previous();
         previousNonEmptyBlock.isValid();
         previousNonEmptyBlock = previousNonEmptyBlock.previous()) {
        QString previousNonEmptyBlockText = previousNonEmptyBlock.text();
        if (previousNonEmptyBlockText.trimmed().isEmpty())
            continue;
        previousIndent = tabSettings.columnAt(previousNonEmptyBlockText,
                                              tabSettings.firstNonSpace(previousNonEmptyBlockText));
        if (previousIndent < indent) {
            cursor.beginEditBlock();
            cursor.setPosition(currentBlock.position(), QTextCursor::KeepAnchor);
            cursor.insertText(tabSettings.indentationString(previousNonEmptyBlockText));
            cursor.endEditBlock();
            return;
        }
    }
    cursor.deletePreviousChar();
}

void BaseTextEditor::wheelEvent(QWheelEvent *e)
{
    d->clearVisibleCollapsedBlock();
    if (scrollWheelZoomingEnabled() && e->modifiers() & Qt::ControlModifier) {
        const int delta = e->delta();
        if (delta < 0)
            zoomOut();
        else if (delta > 0)
            zoomIn();
        return;
    }
    QPlainTextEdit::wheelEvent(e);
}

void BaseTextEditor::zoomIn(int range)
{
    d->clearVisibleCollapsedBlock();
    emit requestFontZoom(range*10);
}

void BaseTextEditor::zoomOut(int range)
{
    zoomIn(-range);
}

void BaseTextEditor::zoomReset()
{
    emit requestZoomReset();
}

bool BaseTextEditor::isElectricCharacter(const QChar &) const
{
    return false;
}

void BaseTextEditor::indentInsertedText(const QTextCursor &tc)
{
    indent(tc.document(), tc, QChar::Null);
}

void BaseTextEditor::countBracket(QChar open, QChar close, QChar c, int *errors, int *stillopen)
{
    if (c == open)
        ++*stillopen;
    else if (c == close)
        --*stillopen;

    if (*stillopen < 0) {
        *errors += -1 * (*stillopen);
        *stillopen = 0;
    }
}

void BaseTextEditor::countBrackets(QTextCursor cursor, int from, int end, QChar open, QChar close, int *errors, int *stillopen)
{
    cursor.setPosition(from);
    QTextBlock block = cursor.block();
    while (block.isValid() && block.position() < end) {
        TextEditor::Parentheses parenList = TextEditor::TextEditDocumentLayout::parentheses(block);
        if (!parenList.isEmpty() && !TextEditor::TextEditDocumentLayout::ifdefedOut(block)) {
            for (int i = 0; i < parenList.count(); ++i) {
                TextEditor::Parenthesis paren = parenList.at(i);
                int position = block.position() + paren.pos;
                if (position < from || position >= end)
                    continue;
                countBracket(open, close, paren.chr, errors, stillopen);
            }
        }
        block = block.next();
    }
}

bool BaseTextEditor::contextAllowsAutoParentheses(const QTextCursor &cursor,
                                                  const QString &textToInsert) const
{
    Q_UNUSED(cursor);
    Q_UNUSED(textToInsert);
    return false;
}

bool BaseTextEditor::isInComment(const QTextCursor &cursor) const
{
    Q_UNUSED(cursor);
    return false;
}

QString BaseTextEditor::insertMatchingBrace(const QTextCursor &tc, const QString &text,
                                            const QChar &la, int *skippedChars) const
{
    Q_UNUSED(tc);
    Q_UNUSED(text);
    Q_UNUSED(la);
    Q_UNUSED(skippedChars);
    return QString();
}

QString BaseTextEditor::insertParagraphSeparator(const QTextCursor &tc) const
{
    Q_UNUSED(tc);
    return QString();
}

QString BaseTextEditor::autoComplete(QTextCursor &cursor, const QString &textToInsert) const
{
    const bool checkBlockEnd = d->m_allowSkippingOfBlockEnd;
    d->m_allowSkippingOfBlockEnd = false; // consume blockEnd.

    if (!contextAllowsAutoParentheses(cursor, textToInsert))
        return QString();

    const QString text = textToInsert;
    const QChar lookAhead = characterAt(cursor.selectionEnd());

    QChar character = textToInsert.at(0);
    const QString parentheses = QLatin1String("()");
    const QString brackets = QLatin1String("[]");
    if (parentheses.contains(character) || brackets.contains(character)) {
        QTextCursor tmp= cursor;
        bool foundBlockStart = TextEditor::TextBlockUserData::findPreviousBlockOpenParenthesis(&tmp);
        int blockStart = foundBlockStart ? tmp.position() : 0;
        tmp = cursor;
        bool foundBlockEnd = TextEditor::TextBlockUserData::findNextBlockClosingParenthesis(&tmp);
        int blockEnd = foundBlockEnd ? tmp.position() : (cursor.document()->characterCount() - 1);
        const QChar openChar = parentheses.contains(character) ? QLatin1Char('(') : QLatin1Char('[');
        const QChar closeChar = parentheses.contains(character) ? QLatin1Char(')') : QLatin1Char(']');

        int errors = 0;
        int stillopen = 0;
        countBrackets(cursor, blockStart, blockEnd, openChar, closeChar, &errors, &stillopen);
        int errorsBeforeInsertion = errors + stillopen;
        errors = 0;
        stillopen = 0;
        countBrackets(cursor, blockStart, cursor.position(), openChar, closeChar, &errors, &stillopen);
        countBracket(openChar, closeChar, character, &errors, &stillopen);
        countBrackets(cursor, cursor.position(), blockEnd, openChar, closeChar, &errors, &stillopen);
        int errorsAfterInsertion = errors + stillopen;
        if (errorsAfterInsertion < errorsBeforeInsertion)
            return QString(); // insertion fixes parentheses or bracket errors, do not auto complete
    }

    int skippedChars = 0;
    const QString autoText = insertMatchingBrace(cursor, text, lookAhead, &skippedChars);

    if (checkBlockEnd && textToInsert.at(0) == QLatin1Char('}')) {
        if (textToInsert.length() > 1)
            qWarning() << "*** handle event compression";

        int startPos = cursor.selectionEnd(), pos = startPos;
        while (characterAt(pos).isSpace())
            ++pos;

        if (characterAt(pos) == QLatin1Char('}'))
            skippedChars += (pos - startPos) + 1;
    }

    if (skippedChars) {
        const int pos = cursor.position();
        cursor.setPosition(pos + skippedChars);
        cursor.setPosition(pos, QTextCursor::KeepAnchor);
    }

    return autoText;
}

bool BaseTextEditor::autoBackspace(QTextCursor &cursor)
{
    d->m_allowSkippingOfBlockEnd = false;

    int pos = cursor.position();
    if (pos == 0)
        return false;
    QTextCursor c = cursor;
    c.setPosition(pos - 1);

    QChar lookAhead = characterAt(pos);
    QChar lookBehind = characterAt(pos-1);
    QChar lookFurtherBehind = characterAt(pos-2);

    QChar character = lookBehind;
    if (character == QLatin1Char('(') || character == QLatin1Char('[')) {
        QTextCursor tmp = cursor;
        TextEditor::TextBlockUserData::findPreviousBlockOpenParenthesis(&tmp);
        int blockStart = tmp.isNull() ? 0 : tmp.position();
        tmp = cursor;
        TextEditor::TextBlockUserData::findNextBlockClosingParenthesis(&tmp);
        int blockEnd = tmp.isNull() ? (cursor.document()->characterCount()-1) : tmp.position();
        QChar openChar = character;
        QChar closeChar = (character == QLatin1Char('(')) ? QLatin1Char(')') : QLatin1Char(']');

        int errors = 0;
        int stillopen = 0;
        countBrackets(cursor, blockStart, blockEnd, openChar, closeChar, &errors, &stillopen);
        int errorsBeforeDeletion = errors + stillopen;
        errors = 0;
        stillopen = 0;
        countBrackets(cursor, blockStart, pos - 1, openChar, closeChar, &errors, &stillopen);
        countBrackets(cursor, pos, blockEnd, openChar, closeChar, &errors, &stillopen);
        int errorsAfterDeletion = errors + stillopen;

        if (errorsAfterDeletion < errorsBeforeDeletion)
            return false; // insertion fixes parentheses or bracket errors, do not auto complete
    }

    // ### this code needs to be generalized
    if    ((lookBehind == QLatin1Char('(') && lookAhead == QLatin1Char(')'))
        || (lookBehind == QLatin1Char('[') && lookAhead == QLatin1Char(']'))
        || (lookBehind == QLatin1Char('"') && lookAhead == QLatin1Char('"')
            && lookFurtherBehind != QLatin1Char('\\'))
        || (lookBehind == QLatin1Char('\'') && lookAhead == QLatin1Char('\'')
            && lookFurtherBehind != QLatin1Char('\\'))) {
        if (! isInComment(c)) {
            cursor.beginEditBlock();
            cursor.deleteChar();
            cursor.deletePreviousChar();
            cursor.endEditBlock();
            return true;
        }
    }
    return false;
}

int BaseTextEditor::paragraphSeparatorAboutToBeInserted(QTextCursor &cursor)
{
    if (characterAt(cursor.position()-1) != QLatin1Char('{'))
        return 0;

    if (!contextAllowsAutoParentheses(cursor))
        return 0;

    // verify that we indeed do have an extra opening brace in the document
    int braceDepth = document()->lastBlock().userState();
    if (braceDepth >= 0)
        braceDepth >>= 8;
    else
        braceDepth= 0;

    if (braceDepth <= 0)
        return 0; // braces are all balanced or worse, no need to do anything

    // we have an extra brace , let's see if we should close it


    /* verify that the next block is not further intended compared to the current block.
       This covers the following case:

            if (condition) {|
                statement;
    */
    const TabSettings &ts = tabSettings();
    QTextBlock block = cursor.block();
    int indentation = ts.indentationColumn(block.text());
    if (block.next().isValid()
        && ts.indentationColumn(block.next().text()) > indentation)
        return 0;

    int pos = cursor.position();

    const QString textToInsert = insertParagraphSeparator(cursor);

    cursor.insertText(textToInsert);
    cursor.setPosition(pos);
    if (ts.m_autoIndent) {
        cursor.insertBlock();
        indent(document(), cursor, QChar::Null);
    } else {
        QString previousBlockText = cursor.block().text();
        cursor.insertBlock();
        cursor.insertText(ts.indentationString(previousBlockText));
    }
    cursor.setPosition(pos);
    d->m_allowSkippingOfBlockEnd = true;
    return 1;
}

void BaseTextEditor::indentBlock(QTextDocument *, QTextBlock, QChar)
{
}

void BaseTextEditor::indent(QTextDocument *doc, const QTextCursor &cursor, QChar typedChar)
{
    if (cursor.hasSelection()) {
        QTextBlock block = doc->findBlock(cursor.selectionStart());
        const QTextBlock end = doc->findBlock(cursor.selectionEnd()).next();
        do {
            indentBlock(doc, block, typedChar);
            block = block.next();
        } while (block.isValid() && block != end);
    } else {
        indentBlock(doc, cursor.block(), typedChar);
    }
}

void BaseTextEditor::reindent(QTextDocument *doc, const QTextCursor &cursor)
{
    if (cursor.hasSelection()) {
        QTextBlock block = doc->findBlock(cursor.selectionStart());
        const QTextBlock end = doc->findBlock(cursor.selectionEnd()).next();

        const TabSettings &ts = d->m_document->tabSettings();

        // skip empty blocks
        while (block.isValid() && block != end) {
            QString bt = block.text();
            if (ts.firstNonSpace(bt) < bt.size())
                break;
            indentBlock(doc, block, QChar::Null);
            block = block.next();
        }

        int previousIndentation = ts.indentationColumn(block.text());
        indentBlock(doc, block, QChar::Null);
        int currentIndentation = ts.indentationColumn(block.text());
        int delta = currentIndentation - previousIndentation;

        block = block.next();
        while (block.isValid() && block != end) {
            ts.reindentLine(block, delta);
            block = block.next();
        }
    } else {
        indentBlock(doc, cursor.block(), QChar::Null);
    }
}


BaseTextEditor::Link BaseTextEditor::findLinkAt(const QTextCursor &, bool)
{
    return Link();
}

bool BaseTextEditor::openLink(const Link &link)
{
    if (link.fileName.isEmpty())
        return false;

    if (baseTextDocument()->fileName() == link.fileName) {
        Core::EditorManager *editorManager = Core::EditorManager::instance();
        editorManager->addCurrentPositionToNavigationHistory();
        gotoLine(link.line, link.column);
        setFocus();
        return true;
    }

    return openEditorAt(link.fileName, link.line, link.column);
}

void BaseTextEditor::updateLink(QMouseEvent *e)
{
    bool linkFound = false;

    if (mouseNavigationEnabled() && e->modifiers() & Qt::ControlModifier) {
        // Link emulation behaviour for 'go to definition'
        const QTextCursor cursor = cursorForPosition(e->pos());

        // Check that the mouse was actually on the text somewhere
        bool onText = cursorRect(cursor).right() >= e->x();
        if (!onText) {
            QTextCursor nextPos = cursor;
            nextPos.movePosition(QTextCursor::Right);
            onText = cursorRect(nextPos).right() >= e->x();
        }

        const Link link = findLinkAt(cursor, false);

        if (onText && link.isValid()) {
            showLink(link);
            linkFound = true;
        }
    }

    if (!linkFound)
        clearLink();
}

void BaseTextEditor::showLink(const Link &link)
{
    if (d->m_currentLink == link)
        return;

    QTextEdit::ExtraSelection sel;
    sel.cursor = textCursor();
    sel.cursor.setPosition(link.begin);
    sel.cursor.setPosition(link.end, QTextCursor::KeepAnchor);
    sel.format = d->m_linkFormat;
    sel.format.setFontUnderline(true);
    setExtraSelections(OtherSelection, QList<QTextEdit::ExtraSelection>() << sel);
    viewport()->setCursor(Qt::PointingHandCursor);
    d->m_currentLink = link;
    d->m_linkPressed = false;
}

void BaseTextEditor::clearLink()
{
    if (!d->m_currentLink.isValid())
        return;

    setExtraSelections(OtherSelection, QList<QTextEdit::ExtraSelection>());
    viewport()->setCursor(Qt::IBeamCursor);
    d->m_currentLink = Link();
    d->m_linkPressed = false;
}

void BaseTextEditorPrivate::updateMarksBlock(const QTextBlock &block)
{
    if (const TextBlockUserData *userData = TextEditDocumentLayout::testUserData(block))
        foreach (ITextMark *mrk, userData->marks())
            mrk->updateBlock(block);
}

void BaseTextEditorPrivate::updateMarksLineNumber()
{
    QTextDocument *doc = q->document();
    QTextBlock block = doc->begin();
    int blockNumber = 0;
    while (block.isValid()) {
        if (const TextBlockUserData *userData = TextEditDocumentLayout::testUserData(block))
            foreach (ITextMark *mrk, userData->marks()) {
                mrk->updateLineNumber(blockNumber + 1);
            }
        block = block.next();
        ++blockNumber;
    }
}

void BaseTextEditor::markBlocksAsChanged(QList<int> blockNumbers)
{
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        if (block.revision() < 0)
            block.setRevision(-block.revision() - 1);
        block = block.next();
    }
    foreach (const int blockNumber, blockNumbers) {
        QTextBlock block = document()->findBlockByNumber(blockNumber);
        if (block.isValid())
            block.setRevision(-block.revision() - 1);
    }
}



TextBlockUserData::MatchType TextBlockUserData::checkOpenParenthesis(QTextCursor *cursor, QChar c)
{
    QTextBlock block = cursor->block();
    if (!TextEditDocumentLayout::hasParentheses(block) || TextEditDocumentLayout::ifdefedOut(block))
        return NoMatch;

    Parentheses parenList = TextEditDocumentLayout::parentheses(block);
    Parenthesis openParen, closedParen;
    QTextBlock closedParenParag = block;

    const int cursorPos = cursor->position() - closedParenParag.position();
    int i = 0;
    int ignore = 0;
    bool foundOpen = false;
    for (;;) {
        if (!foundOpen) {
            if (i >= parenList.count())
                return NoMatch;
            openParen = parenList.at(i);
            if (openParen.pos != cursorPos) {
                ++i;
                continue;
            } else {
                foundOpen = true;
                ++i;
            }
        }

        if (i >= parenList.count()) {
            for (;;) {
                closedParenParag = closedParenParag.next();
                if (!closedParenParag.isValid())
                    return NoMatch;
                if (TextEditDocumentLayout::hasParentheses(closedParenParag)
                    && !TextEditDocumentLayout::ifdefedOut(closedParenParag)) {
                    parenList = TextEditDocumentLayout::parentheses(closedParenParag);
                    break;
                }
            }
            i = 0;
        }

        closedParen = parenList.at(i);
        if (closedParen.type == Parenthesis::Opened) {
            ignore++;
            ++i;
            continue;
        } else {
            if (ignore > 0) {
                ignore--;
                ++i;
                continue;
            }

            cursor->clearSelection();
            cursor->setPosition(closedParenParag.position() + closedParen.pos + 1, QTextCursor::KeepAnchor);

            if ((c == QLatin1Char('{') && closedParen.chr != QLatin1Char('}'))
                || (c == QLatin1Char('(') && closedParen.chr != QLatin1Char(')'))
                || (c == QLatin1Char('[') && closedParen.chr != QLatin1Char(']'))
                || (c == QLatin1Char('+') && closedParen.chr != QLatin1Char('-'))
               )
                return Mismatch;

            return Match;
        }
    }
}

TextBlockUserData::MatchType TextBlockUserData::checkClosedParenthesis(QTextCursor *cursor, QChar c)
{
    QTextBlock block = cursor->block();
    if (!TextEditDocumentLayout::hasParentheses(block) || TextEditDocumentLayout::ifdefedOut(block))
        return NoMatch;

    Parentheses parenList = TextEditDocumentLayout::parentheses(block);
    Parenthesis openParen, closedParen;
    QTextBlock openParenParag = block;

    const int cursorPos = cursor->position() - openParenParag.position();
    int i = parenList.count() - 1;
    int ignore = 0;
    bool foundClosed = false;
    for (;;) {
        if (!foundClosed) {
            if (i < 0)
                return NoMatch;
            closedParen = parenList.at(i);
            if (closedParen.pos != cursorPos - 1) {
                --i;
                continue;
            } else {
                foundClosed = true;
                --i;
            }
        }

        if (i < 0) {
            for (;;) {
                openParenParag = openParenParag.previous();
                if (!openParenParag.isValid())
                    return NoMatch;

                if (TextEditDocumentLayout::hasParentheses(openParenParag)
                    && !TextEditDocumentLayout::ifdefedOut(openParenParag)) {
                    parenList = TextEditDocumentLayout::parentheses(openParenParag);
                    break;
                }
            }
            i = parenList.count() - 1;
        }

        openParen = parenList.at(i);
        if (openParen.type == Parenthesis::Closed) {
            ignore++;
            --i;
            continue;
        } else {
            if (ignore > 0) {
                ignore--;
                --i;
                continue;
            }

            cursor->clearSelection();
            cursor->setPosition(openParenParag.position() + openParen.pos, QTextCursor::KeepAnchor);

            if ((c == '}' && openParen.chr != '{')    ||
                 (c == ')' && openParen.chr != '(')   ||
                 (c == ']' && openParen.chr != '[')   ||
                 (c == '-' && openParen.chr != '+'))
                return Mismatch;

            return Match;
        }
    }
}


bool TextBlockUserData::findPreviousOpenParenthesis(QTextCursor *cursor, bool select)
{
    QTextBlock block = cursor->block();
    int position = cursor->position();
    int ignore = 0;
    while (block.isValid()) {
        Parentheses parenList = TextEditDocumentLayout::parentheses(block);
        if (!parenList.isEmpty() && !TextEditDocumentLayout::ifdefedOut(block)) {
            for (int i = parenList.count()-1; i >= 0; --i) {
                Parenthesis paren = parenList.at(i);
                if (block == cursor->block() &&
                    (position - block.position() <= paren.pos + (paren.type == Parenthesis::Closed ? 1 : 0)))
                        continue;
                if (paren.type == Parenthesis::Closed) {
                    ++ignore;
                } else if (ignore > 0) {
                    --ignore;
                } else {
                    cursor->setPosition(block.position() + paren.pos, select ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
                    return true;
                }
            }
        }
        block = block.previous();
    }
    return false;
}

bool TextBlockUserData::findPreviousBlockOpenParenthesis(QTextCursor *cursor, bool checkStartPosition)
{
    QTextBlock block = cursor->block();
    int position = cursor->position();
    int ignore = 0;
    while (block.isValid()) {
        Parentheses parenList = TextEditDocumentLayout::parentheses(block);
        if (!parenList.isEmpty() && !TextEditDocumentLayout::ifdefedOut(block)) {
            for (int i = parenList.count()-1; i >= 0; --i) {
                Parenthesis paren = parenList.at(i);
                if (paren.chr != QLatin1Char('{') && paren.chr != QLatin1Char('}')
                    && paren.chr != QLatin1Char('+') && paren.chr != QLatin1Char('-')
                    && paren.chr != QLatin1Char('[') && paren.chr != QLatin1Char(']'))
                    continue;
                if (block == cursor->block()) {
                    if (position - block.position() <= paren.pos + (paren.type == Parenthesis::Closed ? 1 : 0))
                        continue;
                    if (checkStartPosition && paren.type == Parenthesis::Opened && paren.pos== cursor->position()) {
                        return true;
                    }
                }
                if (paren.type == Parenthesis::Closed) {
                    ++ignore;
                } else if (ignore > 0) {
                    --ignore;
                } else {
                    cursor->setPosition(block.position() + paren.pos);
                    return true;
                }
            }
        }
        block = block.previous();
    }
    return false;
}

bool TextBlockUserData::findNextClosingParenthesis(QTextCursor *cursor, bool select)
{
    QTextBlock block = cursor->block();
    int position = cursor->position();
    int ignore = 0;
    while (block.isValid()) {
        Parentheses parenList = TextEditDocumentLayout::parentheses(block);
        if (!parenList.isEmpty() && !TextEditDocumentLayout::ifdefedOut(block)) {
            for (int i = 0; i < parenList.count(); ++i) {
                Parenthesis paren = parenList.at(i);
                if (block == cursor->block() &&
                    (position - block.position() > paren.pos - (paren.type == Parenthesis::Opened ? 1 : 0)))
                    continue;
                if (paren.type == Parenthesis::Opened) {
                    ++ignore;
                } else if (ignore > 0) {
                    --ignore;
                } else {
                    cursor->setPosition(block.position() + paren.pos+1, select ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
                    return true;
                }
            }
        }
        block = block.next();
    }
    return false;
}

bool TextBlockUserData::findNextBlockClosingParenthesis(QTextCursor *cursor)
{
    QTextBlock block = cursor->block();
    int position = cursor->position();
    int ignore = 0;
    while (block.isValid()) {
        Parentheses parenList = TextEditDocumentLayout::parentheses(block);
        if (!parenList.isEmpty() && !TextEditDocumentLayout::ifdefedOut(block)) {
            for (int i = 0; i < parenList.count(); ++i) {
                Parenthesis paren = parenList.at(i);
                if (paren.chr != QLatin1Char('{') && paren.chr != QLatin1Char('}')
                    && paren.chr != QLatin1Char('+') && paren.chr != QLatin1Char('-')
                    && paren.chr != QLatin1Char('[') && paren.chr != QLatin1Char(']'))
                    continue;
                if (block == cursor->block() &&
                    (position - block.position() > paren.pos - (paren.type == Parenthesis::Opened ? 1 : 0)))
                    continue;
                if (paren.type == Parenthesis::Opened) {
                    ++ignore;
                } else if (ignore > 0) {
                    --ignore;
                } else {
                    cursor->setPosition(block.position() + paren.pos+1);
                    return true;
                }
            }
        }
        block = block.next();
    }
    return false;
}

TextBlockUserData::MatchType TextBlockUserData::matchCursorBackward(QTextCursor *cursor)
{
    cursor->clearSelection();
    const QTextBlock block = cursor->block();

    if (!TextEditDocumentLayout::hasParentheses(block) || TextEditDocumentLayout::ifdefedOut(block))
        return NoMatch;

    const int relPos = cursor->position() - block.position();

    Parentheses parentheses = TextEditDocumentLayout::parentheses(block);
    const Parentheses::const_iterator cend = parentheses.constEnd();
    for (Parentheses::const_iterator it = parentheses.constBegin();it != cend; ++it) {
        const Parenthesis &paren = *it;
        if (paren.pos == relPos - 1
            && paren.type == Parenthesis::Closed) {
            return checkClosedParenthesis(cursor, paren.chr);
        }
    }
    return NoMatch;
}

TextBlockUserData::MatchType TextBlockUserData::matchCursorForward(QTextCursor *cursor)
{
    cursor->clearSelection();
    const QTextBlock block = cursor->block();

    if (!TextEditDocumentLayout::hasParentheses(block) || TextEditDocumentLayout::ifdefedOut(block))
        return NoMatch;

    const int relPos = cursor->position() - block.position();

    Parentheses parentheses = TextEditDocumentLayout::parentheses(block);
    const Parentheses::const_iterator cend = parentheses.constEnd();
    for (Parentheses::const_iterator it = parentheses.constBegin();it != cend; ++it) {
        const Parenthesis &paren = *it;
        if (paren.pos == relPos
            && paren.type == Parenthesis::Opened) {
            return checkOpenParenthesis(cursor, paren.chr);
        }
    }
    return NoMatch;
}


void BaseTextEditor::highlightSearchResults(const QString &txt, Find::IFindSupport::FindFlags findFlags)
{
    QString pattern = txt;
    if (pattern.size() < 2)
        pattern.clear(); // highlighting single characters is a bit pointless

    if (d->m_searchExpr.pattern() == pattern)
        return;
    d->m_searchExpr.setPattern(pattern);
    d->m_searchExpr.setPatternSyntax((findFlags & Find::IFindSupport::FindRegularExpression) ?
                                     QRegExp::RegExp : QRegExp::FixedString);
    d->m_searchExpr.setCaseSensitivity((findFlags & Find::IFindSupport::FindCaseSensitively) ?
                                       Qt::CaseSensitive : Qt::CaseInsensitive);
    d->m_findFlags = findFlags;

    d->m_delayedUpdateTimer->start(10);
}

void BaseTextEditor::setFindScope(const QTextCursor &scope)
{
    if (scope.isNull() != d->m_findScope.isNull()) {
        d->m_findScope = scope;
        viewport()->update();
    }
}

void BaseTextEditor::_q_animateUpdate(int position, QPointF lastPos, QRectF rect)
{
    QTextCursor cursor(textCursor());
    cursor.setPosition(position);
    viewport()->update(QRectF(cursorRect(cursor).topLeft() + rect.topLeft(), rect.size()).toAlignedRect());
    if (!lastPos.isNull())
        viewport()->update(QRectF(lastPos + rect.topLeft(), rect.size()).toAlignedRect());
}


BaseTextEditorAnimator::BaseTextEditorAnimator(QObject *parent)
        :QObject(parent)
{
    m_value = 0;
    m_timeline = new QTimeLine(256, this);
    m_timeline->setCurveShape(QTimeLine::SineCurve);
    connect(m_timeline, SIGNAL(valueChanged(qreal)), this, SLOT(step(qreal)));
    connect(m_timeline, SIGNAL(finished()), this, SLOT(deleteLater()));
    m_timeline->start();
}


void BaseTextEditorAnimator::setData(QFont f, QPalette pal, const QString &text)
{
    m_font = f;
    m_palette = pal;
    m_text = text;
    QFontMetrics fm(m_font);
    m_size = QSizeF(fm.width(m_text), fm.height());
}

void BaseTextEditorAnimator::draw(QPainter *p, const QPointF &pos)
{
    m_lastDrawPos = pos;
    p->setPen(m_palette.text().color());
    QFont f = m_font;
    f.setPointSizeF(f.pointSizeF() * (1.0 + m_value/2));
    QFontMetrics fm(f);
    int width = fm.width(m_text);
    QRectF r((m_size.width()-width)/2, (m_size.height() - fm.height())/2, width, fm.height());
    r.translate(pos);
    p->fillRect(r, m_palette.base());
    p->setFont(f);
    p->drawText(r, m_text);
}

bool BaseTextEditorAnimator::isRunning() const
{
    return m_timeline->state() == QTimeLine::Running;
}

QRectF BaseTextEditorAnimator::rect() const
{
    QFont f = m_font;
    f.setPointSizeF(f.pointSizeF() * (1.0 + m_value/2));
    QFontMetrics fm(f);
    int width = fm.width(m_text);
    return QRectF((m_size.width()-width)/2, (m_size.height() - fm.height())/2, width, fm.height());
}

void BaseTextEditorAnimator::step(qreal v)
{
    QRectF before = rect();
    m_value = v;
    QRectF after = rect();
    emit updateRequest(m_position, m_lastDrawPos, before.united(after));
}

void BaseTextEditorAnimator::finish()
{
    m_timeline->stop();
    step(0);
    deleteLater();
}

void BaseTextEditor::_q_matchParentheses()
{
    if (isReadOnly())
        return;

    QTextCursor backwardMatch = textCursor();
    QTextCursor forwardMatch = textCursor();
    const TextBlockUserData::MatchType backwardMatchType = TextBlockUserData::matchCursorBackward(&backwardMatch);
    const TextBlockUserData::MatchType forwardMatchType = TextBlockUserData::matchCursorForward(&forwardMatch);

    QList<QTextEdit::ExtraSelection> extraSelections;

    if (backwardMatchType == TextBlockUserData::NoMatch && forwardMatchType == TextBlockUserData::NoMatch) {
        setExtraSelections(ParenthesesMatchingSelection, extraSelections); // clear
        return;
    }

    int animatePosition = -1;
    if (backwardMatch.hasSelection()) {
        QTextEdit::ExtraSelection sel;
        if (backwardMatchType == TextBlockUserData::Mismatch) {
            sel.cursor = backwardMatch;
            sel.format = d->m_mismatchFormat;
        } else {

            if (d->m_displaySettings.m_animateMatchingParentheses) {
                animatePosition = backwardMatch.selectionStart();
            } else if (d->m_formatRange) {
                sel.cursor = backwardMatch;
                sel.format = d->m_rangeFormat;
                extraSelections.append(sel);
            }

            sel.cursor = backwardMatch;
            sel.format = d->m_matchFormat;

            sel.cursor.setPosition(backwardMatch.selectionStart());
            sel.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            extraSelections.append(sel);

            sel.cursor.setPosition(backwardMatch.selectionEnd());
            sel.cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
        }
        extraSelections.append(sel);
    }

    if (forwardMatch.hasSelection()) {
        QTextEdit::ExtraSelection sel;
        if (forwardMatchType == TextBlockUserData::Mismatch) {
            sel.cursor = forwardMatch;
            sel.format = d->m_mismatchFormat;
        } else {

            if (d->m_displaySettings.m_animateMatchingParentheses) {
                animatePosition = forwardMatch.selectionEnd()-1;
            } else if (d->m_formatRange) {
                sel.cursor = forwardMatch;
                sel.format = d->m_rangeFormat;
                extraSelections.append(sel);
            }

            sel.cursor = forwardMatch;
            sel.format = d->m_matchFormat;

            sel.cursor.setPosition(forwardMatch.selectionStart());
            sel.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            extraSelections.append(sel);

            sel.cursor.setPosition(forwardMatch.selectionEnd());
            sel.cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
        }
        extraSelections.append(sel);
    }


    if (animatePosition >= 0) {
        foreach (const QTextEdit::ExtraSelection &sel, BaseTextEditor::extraSelections(ParenthesesMatchingSelection)) {
            if (sel.cursor.selectionStart() == animatePosition
                || sel.cursor.selectionEnd() - 1 == animatePosition) {
                animatePosition = -1;
                break;
            }
        }
    }

    if (animatePosition >= 0) {
        if (d->m_animator)
            d->m_animator->finish();  // one animation is enough
        d->m_animator = new BaseTextEditorAnimator(this);
        d->m_animator->setPosition(animatePosition);
        QPalette pal;
        pal.setBrush(QPalette::Text, d->m_matchFormat.foreground());
        pal.setBrush(QPalette::Base, d->m_rangeFormat.background());
        d->m_animator->setData(font(), pal, characterAt(d->m_animator->position()));
        connect(d->m_animator, SIGNAL(updateRequest(int,QPointF,QRectF)),
                this, SLOT(_q_animateUpdate(int,QPointF,QRectF)));
    }

    setExtraSelections(ParenthesesMatchingSelection, extraSelections);
}

void BaseTextEditor::_q_highlightBlocks()
{
    BaseTextEditorPrivateHighlightBlocks highlightBlocksInfo;

    if (d->extraAreaHighlightCollapseBlockNumber >= 0) {
        QTextBlock block = document()->findBlockByNumber(d->extraAreaHighlightCollapseBlockNumber);
        if (block.isValid()) {
            QTextCursor cursor(block);
            if (d->extraAreaHighlightCollapseColumn >= 0)
                cursor.setPosition(cursor.position() + qMin(d->extraAreaHighlightCollapseColumn,
                                                            block.length()-1));
            QTextCursor closeCursor;
            bool firstRun = true;
            while (TextBlockUserData::findPreviousBlockOpenParenthesis(&cursor, firstRun)) {
                firstRun = false;
                highlightBlocksInfo.open.prepend(cursor.blockNumber());
                int visualIndent = d->visualIndent(cursor.block());
                if (closeCursor.isNull())
                    closeCursor = cursor;
                if (TextBlockUserData::findNextBlockClosingParenthesis(&closeCursor)) {
                    highlightBlocksInfo.close.append(closeCursor.blockNumber());
                    visualIndent = qMin(visualIndent, d->visualIndent(closeCursor.block()));
                }
                highlightBlocksInfo.visualIndent.prepend(visualIndent);
            }
        }
    }

    if (d->m_highlightBlocksInfo != highlightBlocksInfo) {
        d->m_highlightBlocksInfo = highlightBlocksInfo;
        viewport()->update();
        d->m_extraArea->update();
    }
}

void BaseTextEditor::setActionHack(QObject *hack)
{
    d->m_actionHack = hack;
}

QObject *BaseTextEditor::actionHack() const
{
    return d->m_actionHack;
}

void BaseTextEditor::changeEvent(QEvent *e)
{
    QPlainTextEdit::changeEvent(e);
    if (e->type() == QEvent::ApplicationFontChange
        || e->type() == QEvent::FontChange) {
        if (d->m_extraArea) {
            QFont f = d->m_extraArea->font();
            f.setPointSize(font().pointSize());
            d->m_extraArea->setFont(f);
            slotUpdateExtraAreaWidth();
            d->m_extraArea->update();
        }
    }
}

void BaseTextEditor::focusInEvent(QFocusEvent *e)
{
    QPlainTextEdit::focusInEvent(e);
    updateHighlights();
}

void BaseTextEditor::focusOutEvent(QFocusEvent *e)
{
    QPlainTextEdit::focusOutEvent(e);
    if (viewport()->cursor().shape() == Qt::BlankCursor)
        viewport()->setCursor(Qt::IBeamCursor);
}


void BaseTextEditor::maybeSelectLine()
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        const QTextBlock &block = cursor.block();
        if (block.next().isValid()) {
            cursor.setPosition(block.position());
            cursor.setPosition(block.next().position(), QTextCursor::KeepAnchor);
        } else {
            cursor.movePosition(QTextCursor::EndOfBlock);
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
            cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
        }
        setTextCursor(cursor);
    }
}

// shift+del
void BaseTextEditor::cutLine()
{
    maybeSelectLine();
    cut();
}

void BaseTextEditor::deleteLine()
{
    maybeSelectLine();
    textCursor().removeSelectedText();
}

void BaseTextEditor::setExtraSelections(ExtraSelectionKind kind, const QList<QTextEdit::ExtraSelection> &selections)
{
    if (selections.isEmpty() && d->m_extraSelections[kind].isEmpty())
        return;
    d->m_extraSelections[kind] = selections;

    if (kind == CodeSemanticsSelection) {
        d->m_overlay->clear();
        foreach (const QTextEdit::ExtraSelection &selection, d->m_extraSelections[kind]) {
            d->m_overlay->addOverlaySelection(selection.cursor,
                                              selection.format.background().color(),
                                              selection.format.background().color(),
                                              TextEditorOverlay::LockSize);
        }
        d->m_overlay->setVisible(!d->m_overlay->isEmpty());
    } else if (kind == SnippetPlaceholderSelection) {
        d->m_snippetOverlay->clear();
        foreach (const QTextEdit::ExtraSelection &selection, d->m_extraSelections[kind]) {
            d->m_snippetOverlay->addOverlaySelection(selection.cursor,
                                              selection.format.background().color(),
                                              selection.format.background().color(),
                                              TextEditorOverlay::ExpandBegin);
        }
        d->m_snippetOverlay->setVisible(!d->m_snippetOverlay->isEmpty());
    } else {
        QList<QTextEdit::ExtraSelection> all;
        for (int i = 0; i < NExtraSelectionKinds; ++i) {
            if (i == CodeSemanticsSelection || i == SnippetPlaceholderSelection)
                continue;
            all += d->m_extraSelections[i];
        }
        QPlainTextEdit::setExtraSelections(all);
    }
}

QList<QTextEdit::ExtraSelection> BaseTextEditor::extraSelections(ExtraSelectionKind kind) const
{
    return d->m_extraSelections[kind];
}

QString BaseTextEditor::extraSelectionTooltip(int pos) const
{
    QList<QTextEdit::ExtraSelection> all;
    for (int i = 0; i < NExtraSelectionKinds; ++i) {
        const QList<QTextEdit::ExtraSelection> &sel = d->m_extraSelections[i];
        for (int j = 0; j < sel.size(); ++j) {
            const QTextEdit::ExtraSelection &s = sel.at(j);
            if (s.cursor.selectionStart() <= pos
                && s.cursor.selectionEnd() >= pos
                && !s.format.toolTip().isEmpty())
                return s.format.toolTip();
        }
    }
    return QString();
}

// the blocks list must be sorted
void BaseTextEditor::setIfdefedOutBlocks(const QList<BaseTextEditor::BlockRange> &blocks)
{
    QTextDocument *doc = document();
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(doc->documentLayout());
    QTC_ASSERT(documentLayout, return);

    bool needUpdate = false;

    QTextBlock block = doc->firstBlock();

    int rangeNumber = 0;
    int braceDepthDelta = 0;
    while (block.isValid()) {
        bool cleared = false;
        bool set = false;
        if (rangeNumber < blocks.size()) {
            const BlockRange &range = blocks.at(rangeNumber);
            if (block.position() >= range.first && ((block.position() + block.length() - 1) <= range.last || !range.last)) {
                set = TextEditDocumentLayout::setIfdefedOut(block);
            } else {
                cleared = TextEditDocumentLayout::clearIfdefedOut(block);
            }
            if (block.contains(range.last))
                ++rangeNumber;
        } else {
            cleared = TextEditDocumentLayout::clearIfdefedOut(block);
        }

        if (cleared || set) {
            needUpdate = true;
            int delta = TextEditDocumentLayout::braceDepthDelta(block);
            if (cleared)
                braceDepthDelta += delta;
            else if (set)
                braceDepthDelta -= delta;
        }

        if (braceDepthDelta)
            TextEditDocumentLayout::changeBraceDepth(block,braceDepthDelta);

        block = block.next();
    }

    if (needUpdate)
        documentLayout->requestUpdate();
}

void BaseTextEditor::format()
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    indent(document(), cursor, QChar::Null);
    cursor.endEditBlock();
}

void BaseTextEditor::rewrapParagraph()
{
    const int paragraphWidth = displaySettings().m_wrapColumn;
    const QRegExp anyLettersOrNumbers = QRegExp("\\w");
    const int tabSize = tabSettings().m_tabSize;

    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();

    // Find start of paragraph.

    while (cursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::MoveAnchor)) {
        QTextBlock block = cursor.block();
        QString text = block.text();

        // If this block is empty, move marker back to previous and terminate.
        if (!text.contains(anyLettersOrNumbers)) {
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
            break;
        }
    }

    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);

    // Find indent level of current block.

    int indentLevel = 0;
    QString text = cursor.block().text();

    for (int i = 0; i < text.length(); i++) {
        const QChar ch = text.at(i);

        if (ch == QLatin1Char(' '))
            indentLevel++;
        else if (ch == QLatin1Char('\t'))
            indentLevel += tabSize - (indentLevel % tabSize);
        else
            break;
    }

    // If there is a common prefix, it should be kept and expanded to all lines.
    // this allows nice reflowing of doxygen style comments.
    QTextCursor nextBlock = cursor;
    QString commonPrefix;

    if (nextBlock.movePosition(QTextCursor::NextBlock))
    {
         QString nText = nextBlock.block().text();
         int maxLength = qMin(text.length(), nText.length());

         for (int i = 0; i < maxLength; ++i) {
             const QChar ch = text.at(i);

             if (ch != nText[i] || ch.isLetterOrNumber())
                 break;
             commonPrefix.append(ch);
         }
    }


    // Find end of paragraph.
    while (cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor)) {
        QString text = cursor.block().text();

        if (!text.contains(anyLettersOrNumbers))
            break;
    }


    QString selectedText = cursor.selectedText();

    // Preserve initial indent level.or common prefix.
    QString spacing;

    if (commonPrefix.isEmpty()) {
        spacing = tabSettings().indentationString(0, indentLevel, textCursor().block());
    } else {
        spacing = commonPrefix;
        indentLevel = commonPrefix.length();
    }

    int currentLength = indentLevel;
    QString result;
    result.append(spacing);

    // Remove existing instances of any common prefix from paragraph to
    // reflow.
    selectedText.remove(0, commonPrefix.length());
    commonPrefix.prepend(QChar::ParagraphSeparator);
    selectedText.replace(commonPrefix, QLatin1String("\n"));

    // remove any repeated spaces, trim lines to PARAGRAPH_WIDTH width and
    // keep the same indentation level as first line in paragraph.
    QString currentWord;

    for (int i = 0; i < selectedText.length(); ++i) {
        QChar ch = selectedText.at(i);
        if (ch.isSpace()) {
            if (!currentWord.isEmpty()) {
                currentLength += currentWord.length() + 1;

                if (currentLength > paragraphWidth) {
                    currentLength = currentWord.length() + 1 + indentLevel;
                    result.chop(1); // remove trailing space
                    result.append(QChar::ParagraphSeparator);
                    result.append(spacing);
                }

                result.append(currentWord);
                result.append(QLatin1Char(' '));
                currentWord.clear();
            }

            continue;
        }

        currentWord.append(ch);
    }
    result.chop(1);
    result.append(QChar::ParagraphSeparator);

    cursor.insertText(result);
    cursor.endEditBlock();
}

void BaseTextEditor::unCommentSelection()
{
}

void BaseTextEditor::showEvent(QShowEvent* e)
{
    if (!d->m_fontSettings.isEmpty()) {
        setFontSettings(d->m_fontSettings);
        d->m_fontSettings.clear();
    }
    QPlainTextEdit::showEvent(e);
}


void BaseTextEditor::setFontSettingsIfVisible(const TextEditor::FontSettings &fs)
{
    if (!isVisible()) {
        d->m_fontSettings = fs;
        return;
    }
    setFontSettings(fs);
}

void BaseTextEditor::setFontSettings(const TextEditor::FontSettings &fs)
{
    const QTextCharFormat textFormat = fs.toTextCharFormat(QLatin1String(Constants::C_TEXT));
    const QTextCharFormat selectionFormat = fs.toTextCharFormat(QLatin1String(Constants::C_SELECTION));
    const QTextCharFormat lineNumberFormat = fs.toTextCharFormat(QLatin1String(Constants::C_LINE_NUMBER));
    const QTextCharFormat searchResultFormat = fs.toTextCharFormat(QLatin1String(Constants::C_SEARCH_RESULT));
    d->m_searchScopeFormat = fs.toTextCharFormat(QLatin1String(Constants::C_SEARCH_SCOPE));
    const QTextCharFormat parenthesesFormat = fs.toTextCharFormat(QLatin1String(Constants::C_PARENTHESES));
    d->m_currentLineFormat = fs.toTextCharFormat(QLatin1String(Constants::C_CURRENT_LINE));
    d->m_currentLineNumberFormat = fs.toTextCharFormat(QLatin1String(Constants::C_CURRENT_LINE_NUMBER));
    d->m_linkFormat = fs.toTextCharFormat(QLatin1String(TextEditor::Constants::C_LINK));
    d->m_ifdefedOutFormat = fs.toTextCharFormat(QLatin1String(Constants::C_DISABLED_CODE));
    QFont font(textFormat.font());

    const QColor foreground = textFormat.foreground().color();
    const QColor background = textFormat.background().color();
    QPalette p = palette();
    p.setColor(QPalette::Text, foreground);
    p.setColor(QPalette::Foreground, foreground);
    p.setColor(QPalette::Base, background);
    p.setColor(QPalette::Highlight, (selectionFormat.background().style() != Qt::NoBrush) ?
               selectionFormat.background().color() :
               QApplication::palette().color(QPalette::Highlight));
    p.setColor(QPalette::HighlightedText, selectionFormat.foreground().color());
    p.setBrush(QPalette::Inactive, QPalette::Highlight, p.highlight());
    p.setBrush(QPalette::Inactive, QPalette::HighlightedText, p.highlightedText());
    setPalette(p);
    setFont(font);
    setTabSettings(d->m_document->tabSettings()); // update tabs, they depend on the font

    // Line numbers
    QPalette ep = d->m_extraArea->palette();
    ep.setColor(QPalette::Dark, lineNumberFormat.foreground().color());
    ep.setColor(QPalette::Background, lineNumberFormat.background().style() != Qt::NoBrush ?
                lineNumberFormat.background().color() : background);
    d->m_extraArea->setPalette(ep);

    // Search results
    d->m_searchResultFormat.setBackground(searchResultFormat.background());

    // Matching braces
    d->m_matchFormat.setForeground(parenthesesFormat.foreground());
    d->m_rangeFormat.setBackground(parenthesesFormat.background());

    slotUpdateExtraAreaWidth();   // Adjust to new font width
    updateCurrentLineHighlight(); // Make sure it takes the new color
}

void BaseTextEditor::setTabSettings(const TabSettings &ts)
{
    d->m_document->setTabSettings(ts);
    int charWidth = QFontMetrics(font()).width(QChar(' '));
    setTabStopWidth(charWidth * ts.m_tabSize);
}

void BaseTextEditor::setDisplaySettings(const DisplaySettings &ds)
{
    setLineWrapMode(ds.m_textWrapping ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    setLineNumbersVisible(ds.m_displayLineNumbers);
    setVisibleWrapColumn(ds.m_showWrapColumn ? ds.m_wrapColumn : 0);
    setCodeFoldingVisible(ds.m_displayFoldingMarkers);
    setHighlightCurrentLine(ds.m_highlightCurrentLine);
    setRevisionsVisible(ds.m_markTextChanges);

    if (d->m_displaySettings.m_visualizeWhitespace != ds.m_visualizeWhitespace) {
        if (QSyntaxHighlighter *highlighter = baseTextDocument()->syntaxHighlighter())
            highlighter->rehighlight();
        QTextOption option =  document()->defaultTextOption();
        if (ds.m_visualizeWhitespace)
            option.setFlags(option.flags() | QTextOption::ShowTabsAndSpaces);
        else
            option.setFlags(option.flags() & ~QTextOption::ShowTabsAndSpaces);
        option.setFlags(option.flags() | QTextOption::AddSpaceForLineAndParagraphSeparators);
        document()->setDefaultTextOption(option);
    }

    d->m_displaySettings = ds;
    if (!ds.m_highlightBlocks) {
        d->extraAreaHighlightCollapseBlockNumber = d->extraAreaHighlightCollapseColumn = -1;
        d->m_highlightBlocksInfo = BaseTextEditorPrivateHighlightBlocks();
    }

    updateHighlights();
    viewport()->update();
    extraArea()->update();
}

void BaseTextEditor::setBehaviorSettings(const TextEditor::BehaviorSettings &bs)
{
    setMouseNavigationEnabled(bs.m_mouseNavigation);
    setScrollWheelZoomingEnabled(bs.m_scrollWheelZooming);
}

void BaseTextEditor::setStorageSettings(const StorageSettings &storageSettings)
{
    d->m_document->setStorageSettings(storageSettings);
}

void BaseTextEditor::collapse()
{
    QTextDocument *doc = document();
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(doc->documentLayout());
    QTC_ASSERT(documentLayout, return);
    QTextBlock block = textCursor().block();
    QTextBlock curBlock = block;
    while (block.isValid()) {
        if (TextBlockUserData::canCollapse(block) && block.next().isVisible()) {
            if (block == curBlock || block.next() == curBlock)
                break;
            if ((block.next().userState()) >> 8 <= (curBlock.previous().userState() >> 8))
                break;
        }
        block = block.previous();
    }
    if (block.isValid()) {
        TextBlockUserData::doCollapse(block, false);
        d->moveCursorVisible();
        documentLayout->requestUpdate();
        documentLayout->emitDocumentSizeChanged();
    }
}

void BaseTextEditor::expand()
{
    QTextDocument *doc = document();
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(doc->documentLayout());
    QTC_ASSERT(documentLayout, return);
    QTextBlock block = textCursor().block();
    while (block.isValid() && !block.isVisible())
        block = block.previous();
    TextBlockUserData::doCollapse(block, true);
    d->moveCursorVisible();
    documentLayout->requestUpdate();
    documentLayout->emitDocumentSizeChanged();
}

void BaseTextEditor::unCollapseAll()
{
    QTextDocument *doc = document();
    TextEditDocumentLayout *documentLayout = qobject_cast<TextEditDocumentLayout*>(doc->documentLayout());
    QTC_ASSERT(documentLayout, return);

    QTextBlock block = doc->firstBlock();
    bool makeVisible = true;
    while (block.isValid()) {
        if (block.isVisible() && TextBlockUserData::canCollapse(block) && block.next().isVisible()) {
            makeVisible = false;
            break;
        }
        block = block.next();
    }

    block = doc->firstBlock();

    while (block.isValid()) {
        if (TextBlockUserData::canCollapse(block))
            TextBlockUserData::doCollapse(block, makeVisible);
        block = block.next();
    }

    d->moveCursorVisible();
    documentLayout->requestUpdate();
    documentLayout->emitDocumentSizeChanged();
    centerCursor();
}

void BaseTextEditor::setTextCodec(QTextCodec *codec)
{
    baseTextDocument()->setCodec(codec);
}

QTextCodec *BaseTextEditor::textCodec() const
{
    return baseTextDocument()->codec();
}

void BaseTextEditor::setReadOnly(bool b)
{
    QPlainTextEdit::setReadOnly(b);
    if (b)
        setTextInteractionFlags(textInteractionFlags() | Qt::TextSelectableByKeyboard);
}

void BaseTextEditor::cut()
{
    if (d->m_inBlockSelectionMode) {
        copy();
        d->removeBlockSelection();
        return;
    }
    QPlainTextEdit::cut();
}

void BaseTextEditor::paste()
{
    if (d->m_inBlockSelectionMode) {
        d->removeBlockSelection();
    }
    QPlainTextEdit::paste();
}

QMimeData *BaseTextEditor::createMimeDataFromSelection() const
{
    if (d->m_inBlockSelectionMode) {
        QMimeData *mimeData = new QMimeData;
        QString text = d->copyBlockSelection();
        mimeData->setData(QLatin1String("application/vnd.nokia.qtcreator.vblocktext"), text.toUtf8());
        mimeData->setText(text); // for exchangeability
        return mimeData;
    } else if (textCursor().hasSelection()){
        QTextCursor cursor = textCursor();
        QMimeData *mimeData = new QMimeData;
        QString text = cursor.selectedText();
        convertToPlainText(text);
        mimeData->setText(text);

        /*
          Try to figure out whether we are copying an entire block, and store the complete block
          including indentation in the qtcreator.blocktext mimetype.
        */
        QTextCursor selstart = cursor;
        selstart.setPosition(cursor.selectionStart());
        QTextCursor selend = cursor;
        selend.setPosition(cursor.selectionEnd());
        const TabSettings &ts = d->m_document->tabSettings();

        bool startOk = ts.cursorIsAtBeginningOfLine(selstart);
        bool multipleBlocks = (selend.block() != selstart.block());

        if (startOk && multipleBlocks) {
            selstart.movePosition(QTextCursor::StartOfBlock);
            if (ts.cursorIsAtBeginningOfLine(selend))
                selend.movePosition(QTextCursor::StartOfBlock);
            cursor.setPosition(selstart.position());
            cursor.setPosition(selend.position(), QTextCursor::KeepAnchor);
            text = cursor.selectedText();
            mimeData->setData(QLatin1String("application/vnd.nokia.qtcreator.blocktext"), text.toUtf8());
        }
        return mimeData;
    }
    return 0;
}

bool BaseTextEditor::canInsertFromMimeData(const QMimeData *source) const
{
    return QPlainTextEdit::canInsertFromMimeData(source);
}

void BaseTextEditor::insertFromMimeData(const QMimeData *source)
{
    if (isReadOnly())
        return;

    if (source->hasFormat(QLatin1String("application/vnd.nokia.qtcreator.vblocktext"))) {
        QString text = QString::fromUtf8(source->data(QLatin1String("application/vnd.nokia.qtcreator.vblocktext")));
        if (text.isEmpty())
            return;
        QStringList lines = text.split(QLatin1Char('\n'));
        QTextCursor cursor = textCursor();
        cursor.beginEditBlock();
        int initialCursorPosition = cursor.position();
        int column = cursor.position() - cursor.block().position();
        cursor.insertText(lines.first());
        for (int i = 1; i < lines.count(); ++i) {
            QTextBlock next = cursor.block().next();
            if (next.isValid()) {
                cursor.setPosition(next.position() + qMin(column, next.length()-1));
            } else {
                cursor.movePosition(QTextCursor::EndOfBlock);
                cursor.insertBlock();
            }

            int actualColumn = cursor.position() - cursor.block().position();
            if (actualColumn < column)
                cursor.insertText(QString(column - actualColumn, QLatin1Char(' ')));
            cursor.insertText(lines.at(i));
        }
        cursor.setPosition(initialCursorPosition);
        cursor.endEditBlock();
        setTextCursor(cursor);
        ensureCursorVisible();
        return;
    }



    QString text = source->text();
    if (text.isEmpty())
        return;

    const TabSettings &ts = d->m_document->tabSettings();
    QTextCursor cursor = textCursor();
    if (!ts.m_autoIndent) {
        cursor.beginEditBlock();
        cursor.insertText(text);
        cursor.endEditBlock();
        setTextCursor(cursor);
        return;
    }

    cursor.beginEditBlock();
    cursor.removeSelectedText();

    bool insertAtBeginningOfLine = ts.cursorIsAtBeginningOfLine(cursor);

    if (insertAtBeginningOfLine
        && source->hasFormat(QLatin1String("application/vnd.nokia.qtcreator.blocktext"))) {
        text = QString::fromUtf8(source->data(QLatin1String("application/vnd.nokia.qtcreator.blocktext")));
        if (text.isEmpty())
            return;
    }

    int reindentBlockStart = cursor.blockNumber() + (insertAtBeginningOfLine?0:1);

    bool hasFinalNewline = (text.endsWith(QLatin1Char('\n'))
                            || text.endsWith(QChar::ParagraphSeparator)
                            || text.endsWith(QLatin1Char('\r')));

    if (insertAtBeginningOfLine
        && hasFinalNewline) // since we'll add a final newline, preserve current line's indentation
        cursor.setPosition(cursor.block().position());

    int cursorPosition = cursor.position();
    cursor.insertText(text);

    int reindentBlockEnd = cursor.blockNumber() - (hasFinalNewline?1:0);

    if (reindentBlockStart < reindentBlockEnd
        || (reindentBlockStart == reindentBlockEnd
            && (!insertAtBeginningOfLine || hasFinalNewline))) {
        if (insertAtBeginningOfLine && !hasFinalNewline) {
            QTextCursor unnecessaryWhitespace = cursor;
            unnecessaryWhitespace.setPosition(cursorPosition);
            unnecessaryWhitespace.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
            unnecessaryWhitespace.removeSelectedText();
        }
        QTextCursor c = cursor;
        c.setPosition(cursor.document()->findBlockByNumber(reindentBlockStart).position());
        c.setPosition(cursor.document()->findBlockByNumber(reindentBlockEnd).position(),
                      QTextCursor::KeepAnchor);
        reindent(document(), c);
    }

    cursor.endEditBlock();
    setTextCursor(cursor);
}

BaseTextEditorEditable::BaseTextEditorEditable(BaseTextEditor *editor)
  : e(editor)
{
    using namespace Find;
    Aggregation::Aggregate *aggregate = new Aggregation::Aggregate;
    BaseTextFind *baseTextFind = new BaseTextFind(editor);
    connect(baseTextFind, SIGNAL(highlightAll(QString, Find::IFindSupport::FindFlags)),
            editor, SLOT(highlightSearchResults(QString, Find::IFindSupport::FindFlags)));
    connect(baseTextFind, SIGNAL(findScopeChanged(QTextCursor)), editor, SLOT(setFindScope(QTextCursor)));
    aggregate->add(baseTextFind);
    aggregate->add(editor);

    m_cursorPositionLabel = new Utils::LineColumnLabel;

    QHBoxLayout *l = new QHBoxLayout;
    QWidget *w = new QWidget;
    l->setMargin(0);
    l->setContentsMargins(5, 0, 5, 0);
    l->addStretch(0);
    l->addWidget(m_cursorPositionLabel);
    w->setLayout(l);

    m_toolBar = new QToolBar;
    m_toolBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_toolBar->addWidget(w);

    connect(editor, SIGNAL(cursorPositionChanged()), this, SLOT(updateCursorPosition()));
}

void BaseTextEditor::appendStandardContextMenuActions(QMenu *menu)
{
    menu->addSeparator();
    Core::ActionManager *am = Core::ICore::instance()->actionManager();

    QAction *a = am->command(Core::Constants::CUT)->action();
    if (a && a->isEnabled())
        menu->addAction(a);
    a = am->command(Core::Constants::COPY)->action();
    if (a && a->isEnabled())
        menu->addAction(a);
    a = am->command(Core::Constants::PASTE)->action();
    if (a && a->isEnabled())
        menu->addAction(a);
}


BaseTextEditorEditable::~BaseTextEditorEditable()
{
    delete m_toolBar;
    delete e;
}

QWidget *BaseTextEditorEditable::toolBar()
{
    return m_toolBar;
}

int BaseTextEditorEditable::find(const QString &) const
{
    return 0;
}

int BaseTextEditorEditable::currentLine() const
{
    return e->textCursor().blockNumber() + 1;
}

int BaseTextEditorEditable::currentColumn() const
{
    QTextCursor cursor = e->textCursor();
    return cursor.position() - cursor.block().position() + 1;
}

QRect BaseTextEditorEditable::cursorRect(int pos) const
{
    QTextCursor tc = e->textCursor();
    if (pos >= 0)
        tc.setPosition(pos);
    QRect result = e->cursorRect(tc);
    result.moveTo(e->viewport()->mapToGlobal(result.topLeft()));
    return result;
}

QString BaseTextEditorEditable::contents() const
{
    return e->toPlainText();
}

QString BaseTextEditorEditable::selectedText() const
{
    if (e->textCursor().hasSelection())
        return e->textCursor().selectedText();
    return QString();
}

QString BaseTextEditorEditable::textAt(int pos, int length) const
{
    QTextCursor c = e->textCursor();

    if (pos < 0)
        pos = 0;
    c.movePosition(QTextCursor::End);
    if (pos + length > c.position())
        length = c.position() - pos;

    c.setPosition(pos);
    c.setPosition(pos + length, QTextCursor::KeepAnchor);

    return c.selectedText();
}

void BaseTextEditorEditable::remove(int length)
{
    QTextCursor tc = e->textCursor();
    tc.setPosition(tc.position() + length, QTextCursor::KeepAnchor);
    tc.removeSelectedText();
}

void BaseTextEditorEditable::insert(const QString &string)
{
    QTextCursor tc = e->textCursor();
    tc.insertText(string);
}

void BaseTextEditorEditable::replace(int length, const QString &string)
{
    QTextCursor tc = e->textCursor();
    tc.setPosition(tc.position() + length, QTextCursor::KeepAnchor);
    tc.insertText(string);
}

void BaseTextEditorEditable::setCurPos(int pos)
{
    QTextCursor tc = e->textCursor();
    tc.setPosition(pos);
    e->setTextCursor(tc);
}

void BaseTextEditorEditable::select(int toPos)
{
    QTextCursor tc = e->textCursor();
    tc.setPosition(toPos, QTextCursor::KeepAnchor);
    e->setTextCursor(tc);
}

void BaseTextEditorEditable::updateCursorPosition()
{
    const QTextCursor cursor = e->textCursor();
    const QTextBlock block = cursor.block();
    const int line = block.blockNumber() + 1;
    const int column = cursor.position() - block.position();
    m_cursorPositionLabel->setText(tr("Line: %1, Col: %2").arg(line).arg(e->tabSettings().columnAt(block.text(), column)+1),
                                   tr("Line: %1, Col: 999").arg(e->blockCount()));
    m_contextHelpId.clear();

    if (!block.isVisible())
        e->ensureCursorVisible();

}

QString BaseTextEditorEditable::contextHelpId() const
{
    if (m_contextHelpId.isEmpty())
        emit const_cast<BaseTextEditorEditable*>(this)->contextHelpIdRequested(e->editableInterface(),
                                                                               e->textCursor().position());
    return m_contextHelpId;
}


TextBlockUserData::~TextBlockUserData()
{
    TextMarks marks = m_marks;
    m_marks.clear();
    foreach (ITextMark *mrk, marks) {
        mrk->removedFromEditor();
    }
}

