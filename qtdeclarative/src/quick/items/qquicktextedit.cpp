/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquicktextedit_p.h"
#include "qquicktextedit_p_p.h"
#include "qquicktextcontrol_p.h"
#include "qquicktext_p_p.h"
#include "qquickevents_p_p.h"
#include "qquickwindow.h"
#include "qquicktextnode_p.h"
#include "qquicktextnodeengine_p.h"
#include "qquicktextutil_p.h"
#include <QtQuick/qsgsimplerectnode.h>

#include <QtQml/qqmlinfo.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qtextobject.h>
#include <QtGui/qtexttable.h>
#include <QtCore/qmath.h>
#include <QtCore/qalgorithms.h>

#include <private/qqmlglobal_p.h>
#include <private/qqmlproperty_p.h>
#include <private/qtextengine_p.h>
#include <private/qsgadaptationlayer_p.h>

#include "qquicktextdocument.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype TextEdit
    \instantiates QQuickTextEdit
    \inqmlmodule QtQuick 2
    \ingroup qtquick-visual
    \ingroup qtquick-input
    \inherits Item
    \brief Displays multiple lines of editable formatted text

    The TextEdit item displays a block of editable, formatted text.

    It can display both plain and rich text. For example:

    \qml
TextEdit {
    width: 240
    text: "<b>Hello</b> <i>World!</i>"
    font.family: "Helvetica"
    font.pointSize: 20
    color: "blue"
    focus: true
}
    \endqml

    \image declarative-textedit.gif

    Setting \l {Item::focus}{focus} to \c true enables the TextEdit item to receive keyboard focus.

    Note that the TextEdit does not implement scrolling, following the cursor, or other behaviors specific
    to a look-and-feel. For example, to add flickable scrolling that follows the cursor:

    \snippet qml/texteditor.qml 0

    A particular look-and-feel might use smooth scrolling (eg. using SmoothedAnimation), might have a visible
    scrollbar, or a scrollbar that fades in to show location, etc.

    Clipboard support is provided by the cut(), copy(), and paste() functions, and the selection can
    be handled in a traditional "mouse" mechanism by setting selectByMouse, or handled completely
    from QML by manipulating selectionStart and selectionEnd, or using selectAll() or selectWord().

    You can translate between cursor positions (characters from the start of the document) and pixel
    points using positionAt() and positionToRectangle().

    \sa Text, TextInput, {examples/quick/text/textselection}{Text Selection example}
*/

/*!
    \qmlsignal QtQuick2::TextEdit::onLinkActivated(string link)

    This handler is called when the user clicks on a link embedded in the text.
    The link must be in rich text or HTML format and the
    \a link string provides access to the particular link.
*/

// This is a pretty arbitrary figure. The idea is that we don't want to break down the document
// into text nodes corresponding to a text block each so that the glyph node grouping doesn't become pointless.
static const int nodeBreakingSize = 300;

namespace {
    class ProtectedLayoutAccessor: public QAbstractTextDocumentLayout
    {
    public:
        inline QTextCharFormat formatAccessor(int pos)
        {
            return format(pos);
        }
    };
}

QQuickTextEdit::QQuickTextEdit(QQuickItem *parent)
: QQuickImplicitSizeItem(*(new QQuickTextEditPrivate), parent)
{
    Q_D(QQuickTextEdit);
    d->init();
}

QString QQuickTextEdit::text() const
{
    Q_D(const QQuickTextEdit);
    if (!d->textCached) {
        QQuickTextEditPrivate *d = const_cast<QQuickTextEditPrivate *>(d_func());
#ifndef QT_NO_TEXTHTMLPARSER
        if (d->richText)
            d->text = d->control->toHtml();
        else
#endif
            d->text = d->control->toPlainText();
        d->textCached = true;
    }
    return d->text;
}

/*!
    \qmlproperty string QtQuick2::TextEdit::font.family

    Sets the family name of the font.

    The family name is case insensitive and may optionally include a foundry name, e.g. "Helvetica [Cronyx]".
    If the family is available from more than one foundry and the foundry isn't specified, an arbitrary foundry is chosen.
    If the family isn't available a family will be set using the font matching algorithm.
*/

/*!
    \qmlproperty bool QtQuick2::TextEdit::font.bold

    Sets whether the font weight is bold.
*/

/*!
    \qmlproperty enumeration QtQuick2::TextEdit::font.weight

    Sets the font's weight.

    The weight can be one of:
    \list
    \li Font.Light
    \li Font.Normal - the default
    \li Font.DemiBold
    \li Font.Bold
    \li Font.Black
    \endlist

    \qml
    TextEdit { text: "Hello"; font.weight: Font.DemiBold }
    \endqml
*/

/*!
    \qmlproperty bool QtQuick2::TextEdit::font.italic

    Sets whether the font has an italic style.
*/

/*!
    \qmlproperty bool QtQuick2::TextEdit::font.underline

    Sets whether the text is underlined.
*/

/*!
    \qmlproperty bool QtQuick2::TextEdit::font.strikeout

    Sets whether the font has a strikeout style.
*/

/*!
    \qmlproperty real QtQuick2::TextEdit::font.pointSize

    Sets the font size in points. The point size must be greater than zero.
*/

/*!
    \qmlproperty int QtQuick2::TextEdit::font.pixelSize

    Sets the font size in pixels.

    Using this function makes the font device dependent.  Use
    \l{TextEdit::font.pointSize} to set the size of the font in a
    device independent manner.
*/

/*!
    \qmlproperty real QtQuick2::TextEdit::font.letterSpacing

    Sets the letter spacing for the font.

    Letter spacing changes the default spacing between individual letters in the font.
    A positive value increases the letter spacing by the corresponding pixels; a negative value decreases the spacing.
*/

/*!
    \qmlproperty real QtQuick2::TextEdit::font.wordSpacing

    Sets the word spacing for the font.

    Word spacing changes the default spacing between individual words.
    A positive value increases the word spacing by a corresponding amount of pixels,
    while a negative value decreases the inter-word spacing accordingly.
*/

/*!
    \qmlproperty enumeration QtQuick2::TextEdit::font.capitalization

    Sets the capitalization for the text.

    \list
    \li Font.MixedCase - This is the normal text rendering option where no capitalization change is applied.
    \li Font.AllUppercase - This alters the text to be rendered in all uppercase type.
    \li Font.AllLowercase - This alters the text to be rendered in all lowercase type.
    \li Font.SmallCaps - This alters the text to be rendered in small-caps type.
    \li Font.Capitalize - This alters the text to be rendered with the first character of each word as an uppercase character.
    \endlist

    \qml
    TextEdit { text: "Hello"; font.capitalization: Font.AllLowercase }
    \endqml
*/

/*!
    \qmlproperty string QtQuick2::TextEdit::text

    The text to display.  If the text format is AutoText the text edit will
    automatically determine whether the text should be treated as
    rich text.  This determination is made using Qt::mightBeRichText().
*/
void QQuickTextEdit::setText(const QString &text)
{
    Q_D(QQuickTextEdit);
    if (QQuickTextEdit::text() == text)
        return;

    d->document->clearResources();
    d->richText = d->format == RichText || (d->format == AutoText && Qt::mightBeRichText(text));
    if (!isComponentComplete()) {
        d->text = text;
    } else if (d->richText) {
#ifndef QT_NO_TEXTHTMLPARSER
        d->control->setHtml(text);
#else
        d->control->setPlainText(text);
#endif
    } else {
        d->control->setPlainText(text);
    }
}

/*!
    \qmlproperty enumeration QtQuick2::TextEdit::textFormat

    The way the text property should be displayed.

    \list
    \li TextEdit.AutoText
    \li TextEdit.PlainText
    \li TextEdit.RichText
    \endlist

    The default is TextEdit.PlainText.  If the text format is TextEdit.AutoText the text edit
    will automatically determine whether the text should be treated as
    rich text.  This determination is made using Qt::mightBeRichText().

    \table
    \row
    \li
    \qml
Column {
    TextEdit {
        font.pointSize: 24
        text: "<b>Hello</b> <i>World!</i>"
    }
    TextEdit {
        font.pointSize: 24
        textFormat: TextEdit.RichText
        text: "<b>Hello</b> <i>World!</i>"
    }
    TextEdit {
        font.pointSize: 24
        textFormat: TextEdit.PlainText
        text: "<b>Hello</b> <i>World!</i>"
    }
}
    \endqml
    \li \image declarative-textformat.png
    \endtable
*/
QQuickTextEdit::TextFormat QQuickTextEdit::textFormat() const
{
    Q_D(const QQuickTextEdit);
    return d->format;
}

void QQuickTextEdit::setTextFormat(TextFormat format)
{
    Q_D(QQuickTextEdit);
    if (format == d->format)
        return;

    bool wasRich = d->richText;
    d->richText = format == RichText || (format == AutoText && (wasRich || Qt::mightBeRichText(text())));

#ifndef QT_NO_TEXTHTMLPARSER
    if (isComponentComplete()) {
        if (wasRich && !d->richText) {
            d->control->setPlainText(!d->textCached ? d->control->toHtml() : d->text);
            updateSize();
        } else if (!wasRich && d->richText) {
            d->control->setHtml(!d->textCached ? d->control->toPlainText() : d->text);
            updateSize();
        }
    }
#endif

    d->format = format;
    d->control->setAcceptRichText(d->format != PlainText);
    emit textFormatChanged(d->format);
}

/*!
    \qmlproperty enumeration QtQuick2::TextEdit::renderType

    Override the default rendering type for this component.

    Supported render types are:
    \list
    \li Text.QtRendering - the default
    \li Text.NativeRendering
    \endlist

    Select Text.NativeRendering if you prefer text to look native on the target platform and do
    not require advanced features such as transformation of the text. Using such features in
    combination with the NativeRendering render type will lend poor and sometimes pixelated
    results.

    On HighDpi "retina" displays and mobile and embedded platforms, this property is ignored
    and QtRendering is always used.
*/
QQuickTextEdit::RenderType QQuickTextEdit::renderType() const
{
    Q_D(const QQuickTextEdit);
    return d->renderType;
}

void QQuickTextEdit::setRenderType(QQuickTextEdit::RenderType renderType)
{
    Q_D(QQuickTextEdit);
    if (d->renderType == renderType)
        return;

    d->renderType = renderType;
    emit renderTypeChanged();
    d->updateDefaultTextOption();

    if (isComponentComplete())
        updateSize();
}

QFont QQuickTextEdit::font() const
{
    Q_D(const QQuickTextEdit);
    return d->sourceFont;
}

void QQuickTextEdit::setFont(const QFont &font)
{
    Q_D(QQuickTextEdit);
    if (d->sourceFont == font)
        return;

    d->sourceFont = font;
    QFont oldFont = d->font;
    d->font = font;
    if (d->font.pointSizeF() != -1) {
        // 0.5pt resolution
        qreal size = qRound(d->font.pointSizeF()*2.0);
        d->font.setPointSizeF(size/2.0);
    }

    if (oldFont != d->font) {
        d->document->setDefaultFont(d->font);
        if (d->cursorItem) {
            d->cursorItem->setHeight(QFontMetrics(d->font).height());
            moveCursorDelegate();
        }
        updateSize();
        updateWholeDocument();
#ifndef QT_NO_IM
        updateInputMethod(Qt::ImCursorRectangle | Qt::ImFont);
#endif
    }
    emit fontChanged(d->sourceFont);
}

/*!
    \qmlproperty color QtQuick2::TextEdit::color

    The text color.

    \qml
    // green text using hexadecimal notation
    TextEdit { color: "#00FF00" }
    \endqml

    \qml
    // steelblue text using SVG color name
    TextEdit { color: "steelblue" }
    \endqml
*/
QColor QQuickTextEdit::color() const
{
    Q_D(const QQuickTextEdit);
    return d->color;
}

void QQuickTextEdit::setColor(const QColor &color)
{
    Q_D(QQuickTextEdit);
    if (d->color == color)
        return;

    d->color = color;
    updateWholeDocument();
    emit colorChanged(d->color);
}

/*!
    \qmlproperty color QtQuick2::TextEdit::selectionColor

    The text highlight color, used behind selections.
*/
QColor QQuickTextEdit::selectionColor() const
{
    Q_D(const QQuickTextEdit);
    return d->selectionColor;
}

void QQuickTextEdit::setSelectionColor(const QColor &color)
{
    Q_D(QQuickTextEdit);
    if (d->selectionColor == color)
        return;

    d->selectionColor = color;
    updateWholeDocument();
    emit selectionColorChanged(d->selectionColor);
}

/*!
    \qmlproperty color QtQuick2::TextEdit::selectedTextColor

    The selected text color, used in selections.
*/
QColor QQuickTextEdit::selectedTextColor() const
{
    Q_D(const QQuickTextEdit);
    return d->selectedTextColor;
}

void QQuickTextEdit::setSelectedTextColor(const QColor &color)
{
    Q_D(QQuickTextEdit);
    if (d->selectedTextColor == color)
        return;

    d->selectedTextColor = color;
    updateWholeDocument();
    emit selectedTextColorChanged(d->selectedTextColor);
}

/*!
    \qmlproperty enumeration QtQuick2::TextEdit::horizontalAlignment
    \qmlproperty enumeration QtQuick2::TextEdit::verticalAlignment
    \qmlproperty enumeration QtQuick2::TextEdit::effectiveHorizontalAlignment

    Sets the horizontal and vertical alignment of the text within the TextEdit item's
    width and height. By default, the text alignment follows the natural alignment
    of the text, for example text that is read from left to right will be aligned to
    the left.

    Valid values for \c horizontalAlignment are:
    \list
    \li TextEdit.AlignLeft (default)
    \li TextEdit.AlignRight
    \li TextEdit.AlignHCenter
    \li TextEdit.AlignJustify
    \endlist

    Valid values for \c verticalAlignment are:
    \list
    \li TextEdit.AlignTop (default)
    \li TextEdit.AlignBottom
    \li TextEdit.AlignVCenter
    \endlist

    When using the attached property LayoutMirroring::enabled to mirror application
    layouts, the horizontal alignment of text will also be mirrored. However, the property
    \c horizontalAlignment will remain unchanged. To query the effective horizontal alignment
    of TextEdit, use the read-only property \c effectiveHorizontalAlignment.
*/
QQuickTextEdit::HAlignment QQuickTextEdit::hAlign() const
{
    Q_D(const QQuickTextEdit);
    return d->hAlign;
}

void QQuickTextEdit::setHAlign(HAlignment align)
{
    Q_D(QQuickTextEdit);
    bool forceAlign = d->hAlignImplicit && d->effectiveLayoutMirror;
    d->hAlignImplicit = false;
    if (d->setHAlign(align, forceAlign) && isComponentComplete()) {
        d->updateDefaultTextOption();
        updateSize();
    }
}

void QQuickTextEdit::resetHAlign()
{
    Q_D(QQuickTextEdit);
    d->hAlignImplicit = true;
    if (d->determineHorizontalAlignment() && isComponentComplete()) {
        d->updateDefaultTextOption();
        updateSize();
    }
}

QQuickTextEdit::HAlignment QQuickTextEdit::effectiveHAlign() const
{
    Q_D(const QQuickTextEdit);
    QQuickTextEdit::HAlignment effectiveAlignment = d->hAlign;
    if (!d->hAlignImplicit && d->effectiveLayoutMirror) {
        switch (d->hAlign) {
        case QQuickTextEdit::AlignLeft:
            effectiveAlignment = QQuickTextEdit::AlignRight;
            break;
        case QQuickTextEdit::AlignRight:
            effectiveAlignment = QQuickTextEdit::AlignLeft;
            break;
        default:
            break;
        }
    }
    return effectiveAlignment;
}

bool QQuickTextEditPrivate::setHAlign(QQuickTextEdit::HAlignment alignment, bool forceAlign)
{
    Q_Q(QQuickTextEdit);
    if (hAlign != alignment || forceAlign) {
        QQuickTextEdit::HAlignment oldEffectiveHAlign = q->effectiveHAlign();
        hAlign = alignment;
        emit q->horizontalAlignmentChanged(alignment);
        if (oldEffectiveHAlign != q->effectiveHAlign())
            emit q->effectiveHorizontalAlignmentChanged();
        return true;
    }
    return false;
}


Qt::LayoutDirection QQuickTextEditPrivate::textDirection(const QString &text) const
{
    const QChar *character = text.constData();
    while (!character->isNull()) {
        switch (character->direction()) {
        case QChar::DirL:
            return Qt::LeftToRight;
        case QChar::DirR:
        case QChar::DirAL:
        case QChar::DirAN:
            return Qt::RightToLeft;
        default:
            break;
        }
        character++;
    }
    return Qt::LayoutDirectionAuto;
}

bool QQuickTextEditPrivate::determineHorizontalAlignment()
{
    Q_Q(QQuickTextEdit);
    if (hAlignImplicit && q->isComponentComplete()) {
        Qt::LayoutDirection direction = contentDirection;
#ifndef QT_NO_IM
        if (direction == Qt::LayoutDirectionAuto) {
            const QString preeditText = control->textCursor().block().layout()->preeditAreaText();
            direction = textDirection(preeditText);
        }
        if (direction == Qt::LayoutDirectionAuto)
            direction = qGuiApp->inputMethod()->inputDirection();
#endif

        return setHAlign(direction == Qt::RightToLeft ? QQuickTextEdit::AlignRight : QQuickTextEdit::AlignLeft);
    }
    return false;
}

void QQuickTextEditPrivate::mirrorChange()
{
    Q_Q(QQuickTextEdit);
    if (q->isComponentComplete()) {
        if (!hAlignImplicit && (hAlign == QQuickTextEdit::AlignRight || hAlign == QQuickTextEdit::AlignLeft)) {
            updateDefaultTextOption();
            q->updateSize();
            emit q->effectiveHorizontalAlignmentChanged();
        }
    }
}

#ifndef QT_NO_IM
Qt::InputMethodHints QQuickTextEditPrivate::effectiveInputMethodHints() const
{
    return inputMethodHints | Qt::ImhMultiLine;
}
#endif

QQuickTextEdit::VAlignment QQuickTextEdit::vAlign() const
{
    Q_D(const QQuickTextEdit);
    return d->vAlign;
}

void QQuickTextEdit::setVAlign(QQuickTextEdit::VAlignment alignment)
{
    Q_D(QQuickTextEdit);
    if (alignment == d->vAlign)
        return;
    d->vAlign = alignment;
    d->updateDefaultTextOption();
    updateSize();
    moveCursorDelegate();
    emit verticalAlignmentChanged(d->vAlign);
}
/*!
    \qmlproperty enumeration QtQuick2::TextEdit::wrapMode

    Set this property to wrap the text to the TextEdit item's width.
    The text will only wrap if an explicit width has been set.

    \list
    \li TextEdit.NoWrap - no wrapping will be performed. If the text contains insufficient newlines, then implicitWidth will exceed a set width.
    \li TextEdit.WordWrap - wrapping is done on word boundaries only. If a word is too long, implicitWidth will exceed a set width.
    \li TextEdit.WrapAnywhere - wrapping is done at any point on a line, even if it occurs in the middle of a word.
    \li TextEdit.Wrap - if possible, wrapping occurs at a word boundary; otherwise it will occur at the appropriate point on the line, even in the middle of a word.
    \endlist

    The default is TextEdit.NoWrap. If you set a width, consider using TextEdit.Wrap.
*/
QQuickTextEdit::WrapMode QQuickTextEdit::wrapMode() const
{
    Q_D(const QQuickTextEdit);
    return d->wrapMode;
}

void QQuickTextEdit::setWrapMode(WrapMode mode)
{
    Q_D(QQuickTextEdit);
    if (mode == d->wrapMode)
        return;
    d->wrapMode = mode;
    d->updateDefaultTextOption();
    updateSize();
    emit wrapModeChanged();
}

/*!
    \qmlproperty int QtQuick2::TextEdit::lineCount

    Returns the total number of lines in the textEdit item.
*/
int QQuickTextEdit::lineCount() const
{
    Q_D(const QQuickTextEdit);
    return d->lineCount;
}

/*!
    \qmlproperty int QtQuick2::TextEdit::length

    Returns the total number of plain text characters in the TextEdit item.

    As this number doesn't include any formatting markup it may not be the same as the
    length of the string returned by the \l text property.

    This property can be faster than querying the length the \l text property as it doesn't
    require any copying or conversion of the TextEdit's internal string data.
*/

int QQuickTextEdit::length() const
{
    Q_D(const QQuickTextEdit);
    // QTextDocument::characterCount() includes the terminating null character.
    return qMax(0, d->document->characterCount() - 1);
}

/*!
    \qmlproperty real QtQuick2::TextEdit::contentWidth

    Returns the width of the text, including the width past the width
    which is covered due to insufficient wrapping if \l wrapMode is set.
*/
qreal QQuickTextEdit::contentWidth() const
{
    Q_D(const QQuickTextEdit);
    return d->contentSize.width();
}

/*!
    \qmlproperty real QtQuick2::TextEdit::contentHeight

    Returns the height of the text, including the height past the height
    that is covered if the text does not fit within the set height.
*/
qreal QQuickTextEdit::contentHeight() const
{
    Q_D(const QQuickTextEdit);
    return d->contentSize.height();
}

/*!
    \qmlproperty url QtQuick2::TextEdit::baseUrl

    This property specifies a base URL which is used to resolve relative URLs
    within the text.

    The default value is the url of the QML file instantiating the TextEdit item.
*/

QUrl QQuickTextEdit::baseUrl() const
{
    Q_D(const QQuickTextEdit);
    if (d->baseUrl.isEmpty()) {
        if (QQmlContext *context = qmlContext(this))
            const_cast<QQuickTextEditPrivate *>(d)->baseUrl = context->baseUrl();
    }
    return d->baseUrl;
}

void QQuickTextEdit::setBaseUrl(const QUrl &url)
{
    Q_D(QQuickTextEdit);
    if (baseUrl() != url) {
        d->baseUrl = url;

        d->document->setBaseUrl(url, d->richText);
        emit baseUrlChanged();
    }
}

void QQuickTextEdit::resetBaseUrl()
{
    if (QQmlContext *context = qmlContext(this))
        setBaseUrl(context->baseUrl());
    else
        setBaseUrl(QUrl());
}

/*!
    \qmlmethod rectangle QtQuick2::TextEdit::positionToRectangle(position)

    Returns the rectangle at the given \a position in the text. The x, y,
    and height properties correspond to the cursor that would describe
    that position.
*/
QRectF QQuickTextEdit::positionToRectangle(int pos) const
{
    Q_D(const QQuickTextEdit);
    QTextCursor c(d->document);
    c.setPosition(pos);
    return d->control->cursorRect(c).translated(d->xoff, d->yoff);

}

/*!
    \qmlmethod int QtQuick2::TextEdit::positionAt(int x, int y)

    Returns the text position closest to pixel position (\a x, \a y).

    Position 0 is before the first character, position 1 is after the first character
    but before the second, and so on until position \l {text}.length, which is after all characters.
*/
int QQuickTextEdit::positionAt(qreal x, qreal y) const
{
    Q_D(const QQuickTextEdit);
    x -= d->xoff;
    y -= d->yoff;

    int r = d->document->documentLayout()->hitTest(QPointF(x, y), Qt::FuzzyHit);
#ifndef QT_NO_IM
    QTextCursor cursor = d->control->textCursor();
    if (r > cursor.position()) {
        // The cursor position includes positions within the preedit text, but only positions in the
        // same text block are offset so it is possible to get a position that is either part of the
        // preedit or the next text block.
        QTextLayout *layout = cursor.block().layout();
        const int preeditLength = layout
                ? layout->preeditAreaText().length()
                : 0;
        if (preeditLength > 0
                && d->document->documentLayout()->blockBoundingRect(cursor.block()).contains(x, y)) {
            r = r > cursor.position() + preeditLength
                    ? r - preeditLength
                    : cursor.position();
        }
    }
#endif
    return r;
}

/*!
    \qmlmethod QtQuick2::TextEdit::moveCursorSelection(int position, SelectionMode mode = TextEdit.SelectCharacters)

    Moves the cursor to \a position and updates the selection according to the optional \a mode
    parameter. (To only move the cursor, set the \l cursorPosition property.)

    When this method is called it additionally sets either the
    selectionStart or the selectionEnd (whichever was at the previous cursor position)
    to the specified position. This allows you to easily extend and contract the selected
    text range.

    The selection mode specifies whether the selection is updated on a per character or a per word
    basis.  If not specified the selection mode will default to TextEdit.SelectCharacters.

    \list
    \li TextEdit.SelectCharacters - Sets either the selectionStart or selectionEnd (whichever was at
    the previous cursor position) to the specified position.
    \li TextEdit.SelectWords - Sets the selectionStart and selectionEnd to include all
    words between the specified position and the previous cursor position.  Words partially in the
    range are included.
    \endlist

    For example, take this sequence of calls:

    \code
        cursorPosition = 5
        moveCursorSelection(9, TextEdit.SelectCharacters)
        moveCursorSelection(7, TextEdit.SelectCharacters)
    \endcode

    This moves the cursor to position 5, extend the selection end from 5 to 9
    and then retract the selection end from 9 to 7, leaving the text from position 5 to 7
    selected (the 6th and 7th characters).

    The same sequence with TextEdit.SelectWords will extend the selection start to a word boundary
    before or on position 5 and extend the selection end to a word boundary on or past position 9.
*/
void QQuickTextEdit::moveCursorSelection(int pos)
{
    //Note that this is the same as setCursorPosition but with the KeepAnchor flag set
    Q_D(QQuickTextEdit);
    QTextCursor cursor = d->control->textCursor();
    if (cursor.position() == pos)
        return;
    cursor.setPosition(pos, QTextCursor::KeepAnchor);
    d->control->setTextCursor(cursor);
}

void QQuickTextEdit::moveCursorSelection(int pos, SelectionMode mode)
{
    Q_D(QQuickTextEdit);
    QTextCursor cursor = d->control->textCursor();
    if (cursor.position() == pos)
        return;
    if (mode == SelectCharacters) {
        cursor.setPosition(pos, QTextCursor::KeepAnchor);
    } else if (cursor.anchor() < pos || (cursor.anchor() == pos && cursor.position() < pos)) {
        if (cursor.anchor() > cursor.position()) {
            cursor.setPosition(cursor.anchor(), QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);
            if (cursor.position() == cursor.anchor())
                cursor.movePosition(QTextCursor::PreviousWord, QTextCursor::MoveAnchor);
            else
                cursor.setPosition(cursor.position(), QTextCursor::MoveAnchor);
        } else {
            cursor.setPosition(cursor.anchor(), QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::MoveAnchor);
        }

        cursor.setPosition(pos, QTextCursor::KeepAnchor);
        cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);
        if (cursor.position() != pos)
            cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    } else if (cursor.anchor() > pos || (cursor.anchor() == pos && cursor.position() > pos)) {
        if (cursor.anchor() < cursor.position()) {
            cursor.setPosition(cursor.anchor(), QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::MoveAnchor);
        } else {
            cursor.setPosition(cursor.anchor(), QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
            cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
            if (cursor.position() != cursor.anchor()) {
                cursor.setPosition(cursor.anchor(), QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::MoveAnchor);
            }
        }

        cursor.setPosition(pos, QTextCursor::KeepAnchor);
        cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
        if (cursor.position() != pos) {
            cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
            cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);
        }
    }
    d->control->setTextCursor(cursor);
}

/*!
    \qmlproperty bool QtQuick2::TextEdit::cursorVisible
    If true the text edit shows a cursor.

    This property is set and unset when the text edit gets active focus, but it can also
    be set directly (useful, for example, if a KeyProxy might forward keys to it).
*/
bool QQuickTextEdit::isCursorVisible() const
{
    Q_D(const QQuickTextEdit);
    return d->cursorVisible;
}

void QQuickTextEdit::setCursorVisible(bool on)
{
    Q_D(QQuickTextEdit);
    if (d->cursorVisible == on)
        return;
    d->cursorVisible = on;
    if (on && isComponentComplete())
        QQuickTextUtil::createCursor(d);
    if (!on && !d->persistentSelection)
        d->control->setCursorIsFocusIndicator(true);
    d->control->setCursorVisible(on);
    emit cursorVisibleChanged(d->cursorVisible);
}

/*!
    \qmlproperty int QtQuick2::TextEdit::cursorPosition
    The position of the cursor in the TextEdit.
*/
int QQuickTextEdit::cursorPosition() const
{
    Q_D(const QQuickTextEdit);
    return d->control->textCursor().position();
}

void QQuickTextEdit::setCursorPosition(int pos)
{
    Q_D(QQuickTextEdit);
    if (pos < 0 || pos >= d->document->characterCount()) // characterCount includes the terminating null.
        return;
    QTextCursor cursor = d->control->textCursor();
    if (cursor.position() == pos && cursor.anchor() == pos)
        return;
    cursor.setPosition(pos);
    d->control->setTextCursor(cursor);
    d->control->updateCursorRectangle(true);
}

/*!
    \qmlproperty Component QtQuick2::TextEdit::cursorDelegate
    The delegate for the cursor in the TextEdit.

    If you set a cursorDelegate for a TextEdit, this delegate will be used for
    drawing the cursor instead of the standard cursor. An instance of the
    delegate will be created and managed by the text edit when a cursor is
    needed, and the x and y properties of delegate instance will be set so as
    to be one pixel before the top left of the current character.

    Note that the root item of the delegate component must be a QQuickItem or
    QQuickItem derived item.
*/
QQmlComponent* QQuickTextEdit::cursorDelegate() const
{
    Q_D(const QQuickTextEdit);
    return d->cursorComponent;
}

void QQuickTextEdit::setCursorDelegate(QQmlComponent* c)
{
    Q_D(QQuickTextEdit);
    QQuickTextUtil::setCursorDelegate(d, c);
}

void QQuickTextEdit::createCursor()
{
    Q_D(QQuickTextEdit);
    d->cursorPending = true;
    QQuickTextUtil::createCursor(d);
}

/*!
    \qmlproperty int QtQuick2::TextEdit::selectionStart

    The cursor position before the first character in the current selection.

    This property is read-only. To change the selection, use select(start,end),
    selectAll(), or selectWord().

    \sa selectionEnd, cursorPosition, selectedText
*/
int QQuickTextEdit::selectionStart() const
{
    Q_D(const QQuickTextEdit);
    return d->control->textCursor().selectionStart();
}

/*!
    \qmlproperty int QtQuick2::TextEdit::selectionEnd

    The cursor position after the last character in the current selection.

    This property is read-only. To change the selection, use select(start,end),
    selectAll(), or selectWord().

    \sa selectionStart, cursorPosition, selectedText
*/
int QQuickTextEdit::selectionEnd() const
{
    Q_D(const QQuickTextEdit);
    return d->control->textCursor().selectionEnd();
}

/*!
    \qmlproperty string QtQuick2::TextEdit::selectedText

    This read-only property provides the text currently selected in the
    text edit.

    It is equivalent to the following snippet, but is faster and easier
    to use.
    \code
    //myTextEdit is the id of the TextEdit
    myTextEdit.text.toString().substring(myTextEdit.selectionStart,
            myTextEdit.selectionEnd);
    \endcode
*/
QString QQuickTextEdit::selectedText() const
{
    Q_D(const QQuickTextEdit);
#ifndef QT_NO_TEXTHTMLPARSER
    return d->richText
            ? d->control->textCursor().selectedText()
            : d->control->textCursor().selection().toPlainText();
#else
    return d->control->textCursor().selection().toPlainText();
#endif
}

/*!
    \qmlproperty bool QtQuick2::TextEdit::activeFocusOnPress

    Whether the TextEdit should gain active focus on a mouse press. By default this is
    set to true.
*/
bool QQuickTextEdit::focusOnPress() const
{
    Q_D(const QQuickTextEdit);
    return d->focusOnPress;
}

void QQuickTextEdit::setFocusOnPress(bool on)
{
    Q_D(QQuickTextEdit);
    if (d->focusOnPress == on)
        return;
    d->focusOnPress = on;
    emit activeFocusOnPressChanged(d->focusOnPress);
}

/*!
    \qmlproperty bool QtQuick2::TextEdit::persistentSelection

    Whether the TextEdit should keep the selection visible when it loses active focus to another
    item in the scene. By default this is set to false.
*/
bool QQuickTextEdit::persistentSelection() const
{
    Q_D(const QQuickTextEdit);
    return d->persistentSelection;
}

void QQuickTextEdit::setPersistentSelection(bool on)
{
    Q_D(QQuickTextEdit);
    if (d->persistentSelection == on)
        return;
    d->persistentSelection = on;
    emit persistentSelectionChanged(d->persistentSelection);
}

/*!
   \qmlproperty real QtQuick2::TextEdit::textMargin

   The margin, in pixels, around the text in the TextEdit.
*/
qreal QQuickTextEdit::textMargin() const
{
    Q_D(const QQuickTextEdit);
    return d->textMargin;
}

void QQuickTextEdit::setTextMargin(qreal margin)
{
    Q_D(QQuickTextEdit);
    if (d->textMargin == margin)
        return;
    d->textMargin = margin;
    d->document->setDocumentMargin(d->textMargin);
    emit textMarginChanged(d->textMargin);
}

/*!
    \qmlproperty enumeration QtQuick2::TextEdit::inputMethodHints

    Provides hints to the input method about the expected content of the text edit and how it
    should operate.

    The value is a bit-wise combination of flags or Qt.ImhNone if no hints are set.

    Flags that alter behaviour are:

    \list
    \li Qt.ImhHiddenText - Characters should be hidden, as is typically used when entering passwords.
    \li Qt.ImhSensitiveData - Typed text should not be stored by the active input method
            in any persistent storage like predictive user dictionary.
    \li Qt.ImhNoAutoUppercase - The input method should not try to automatically switch to upper case
            when a sentence ends.
    \li Qt.ImhPreferNumbers - Numbers are preferred (but not required).
    \li Qt.ImhPreferUppercase - Upper case letters are preferred (but not required).
    \li Qt.ImhPreferLowercase - Lower case letters are preferred (but not required).
    \li Qt.ImhNoPredictiveText - Do not use predictive text (i.e. dictionary lookup) while typing.

    \li Qt.ImhDate - The text editor functions as a date field.
    \li Qt.ImhTime - The text editor functions as a time field.
    \endlist

    Flags that restrict input (exclusive flags) are:

    \list
    \li Qt.ImhDigitsOnly - Only digits are allowed.
    \li Qt.ImhFormattedNumbersOnly - Only number input is allowed. This includes decimal point and minus sign.
    \li Qt.ImhUppercaseOnly - Only upper case letter input is allowed.
    \li Qt.ImhLowercaseOnly - Only lower case letter input is allowed.
    \li Qt.ImhDialableCharactersOnly - Only characters suitable for phone dialing are allowed.
    \li Qt.ImhEmailCharactersOnly - Only characters suitable for email addresses are allowed.
    \li Qt.ImhUrlCharactersOnly - Only characters suitable for URLs are allowed.
    \endlist

    Masks:

    \list
    \li Qt.ImhExclusiveInputMask - This mask yields nonzero if any of the exclusive flags are used.
    \endlist
*/

#ifndef QT_NO_IM
Qt::InputMethodHints QQuickTextEdit::inputMethodHints() const
{
    Q_D(const QQuickTextEdit);
    return d->inputMethodHints;
}

void QQuickTextEdit::setInputMethodHints(Qt::InputMethodHints hints)
{
    Q_D(QQuickTextEdit);

    if (hints == d->inputMethodHints)
        return;

    d->inputMethodHints = hints;
    updateInputMethod(Qt::ImHints);
    emit inputMethodHintsChanged();
}
#endif // QT_NO_IM

void QQuickTextEdit::geometryChanged(const QRectF &newGeometry,
                                  const QRectF &oldGeometry)
{
    Q_D(QQuickTextEdit);
    if (newGeometry.width() != oldGeometry.width() && widthValid() && !d->inLayout) {
        updateSize();
        updateWholeDocument();
        moveCursorDelegate();
    }
    QQuickImplicitSizeItem::geometryChanged(newGeometry, oldGeometry);

}

/*!
    Ensures any delayed caching or data loading the class
    needs to performed is complete.
*/
void QQuickTextEdit::componentComplete()
{
    Q_D(QQuickTextEdit);
    QQuickImplicitSizeItem::componentComplete();

    d->document->setBaseUrl(baseUrl(), d->richText);
#ifndef QT_NO_TEXTHTML_PARSER
    if (d->richText)
        d->control->setHtml(d->text);
    else
#endif
    if (!d->text.isEmpty())
        d->control->setPlainText(d->text);

    if (d->dirty) {
        d->determineHorizontalAlignment();
        d->updateDefaultTextOption();
        updateSize();
        d->dirty = false;
    }
    if (d->cursorComponent && isCursorVisible())
        QQuickTextUtil::createCursor(d);
}

/*!
    \qmlproperty bool QtQuick2::TextEdit::selectByKeyboard
    \since QtQuick 2.1

    Defaults to true when the editor is editable, and false
    when read-only.

    If true, the user can use the keyboard to select text
    even if the editor is read-only. If false, the user
    cannot use the keyboard to select text even if the
    editor is editable.

    \sa readOnly
*/
bool QQuickTextEdit::selectByKeyboard() const
{
    Q_D(const QQuickTextEdit);
    if (d->selectByKeyboardSet)
        return d->selectByKeyboard;
    return !isReadOnly();
}

void QQuickTextEdit::setSelectByKeyboard(bool on)
{
    Q_D(QQuickTextEdit);
    bool was = selectByKeyboard();
    if (!d->selectByKeyboardSet || was != on) {
        d->selectByKeyboardSet = true;
        d->selectByKeyboard = on;
        if (on)
            d->control->setTextInteractionFlags(d->control->textInteractionFlags() | Qt::TextSelectableByKeyboard);
        else
            d->control->setTextInteractionFlags(d->control->textInteractionFlags() & ~Qt::TextSelectableByKeyboard);
        emit selectByKeyboardChanged(on);
    }
}

/*!
    \qmlproperty bool QtQuick2::TextEdit::selectByMouse

    Defaults to false.

    If true, the user can use the mouse to select text in some
    platform-specific way. Note that for some platforms this may
    not be an appropriate interaction (eg. may conflict with how
    the text needs to behave inside a Flickable.
*/
bool QQuickTextEdit::selectByMouse() const
{
    Q_D(const QQuickTextEdit);
    return d->selectByMouse;
}

void QQuickTextEdit::setSelectByMouse(bool on)
{
    Q_D(QQuickTextEdit);
    if (d->selectByMouse != on) {
        d->selectByMouse = on;
        setKeepMouseGrab(on);
        if (on)
            d->control->setTextInteractionFlags(d->control->textInteractionFlags() | Qt::TextSelectableByMouse);
        else
            d->control->setTextInteractionFlags(d->control->textInteractionFlags() & ~Qt::TextSelectableByMouse);
        emit selectByMouseChanged(on);
    }
}

/*!
    \qmlproperty enumeration QtQuick2::TextEdit::mouseSelectionMode

    Specifies how text should be selected using a mouse.

    \list
    \li TextEdit.SelectCharacters - The selection is updated with individual characters. (Default)
    \li TextEdit.SelectWords - The selection is updated with whole words.
    \endlist

    This property only applies when \l selectByMouse is true.
*/
QQuickTextEdit::SelectionMode QQuickTextEdit::mouseSelectionMode() const
{
    Q_D(const QQuickTextEdit);
    return d->mouseSelectionMode;
}

void QQuickTextEdit::setMouseSelectionMode(SelectionMode mode)
{
    Q_D(QQuickTextEdit);
    if (d->mouseSelectionMode != mode) {
        d->mouseSelectionMode = mode;
        d->control->setWordSelectionEnabled(mode == SelectWords);
        emit mouseSelectionModeChanged(mode);
    }
}

/*!
    \qmlproperty bool QtQuick2::TextEdit::readOnly

    Whether the user can interact with the TextEdit item. If this
    property is set to true the text cannot be edited by user interaction.

    By default this property is false.
*/
void QQuickTextEdit::setReadOnly(bool r)
{
    Q_D(QQuickTextEdit);
    if (r == isReadOnly())
        return;

#ifndef QT_NO_IM
    setFlag(QQuickItem::ItemAcceptsInputMethod, !r);
#endif
    Qt::TextInteractionFlags flags = Qt::LinksAccessibleByMouse;
    if (d->selectByMouse)
        flags = flags | Qt::TextSelectableByMouse;
    if (d->selectByKeyboardSet && d->selectByKeyboard)
        flags = flags | Qt::TextSelectableByKeyboard;
    else if (!d->selectByKeyboardSet && !r)
        flags = flags | Qt::TextSelectableByKeyboard;
    if (!r)
        flags = flags | Qt::TextEditable;
    d->control->setTextInteractionFlags(flags);
    if (!r)
        d->control->moveCursor(QTextCursor::End);

#ifndef QT_NO_IM
    updateInputMethod(Qt::ImEnabled);
#endif
    q_canPasteChanged();
    emit readOnlyChanged(r);
    if (!d->selectByKeyboardSet)
        emit selectByKeyboardChanged(!r);
}

bool QQuickTextEdit::isReadOnly() const
{
    Q_D(const QQuickTextEdit);
    return !(d->control->textInteractionFlags() & Qt::TextEditable);
}

/*!
    \qmlproperty rectangle QtQuick2::TextEdit::cursorRectangle

    The rectangle where the standard text cursor is rendered
    within the text edit. Read-only.

    The position and height of a custom cursorDelegate are updated to follow the cursorRectangle
    automatically when it changes.  The width of the delegate is unaffected by changes in the
    cursor rectangle.
*/
QRectF QQuickTextEdit::cursorRectangle() const
{
    Q_D(const QQuickTextEdit);
    return d->control->cursorRect().translated(d->xoff, d->yoff);
}

bool QQuickTextEdit::event(QEvent *event)
{
    Q_D(QQuickTextEdit);
    if (event->type() == QEvent::ShortcutOverride) {
        d->control->processEvent(event, QPointF(-d->xoff, -d->yoff));
        return event->isAccepted();
    }
    return QQuickImplicitSizeItem::event(event);
}

/*!
\overload
Handles the given key \a event.
*/
void QQuickTextEdit::keyPressEvent(QKeyEvent *event)
{
    Q_D(QQuickTextEdit);
    d->control->processEvent(event, QPointF(-d->xoff, -d->yoff));
    if (!event->isAccepted())
        QQuickImplicitSizeItem::keyPressEvent(event);
}

/*!
\overload
Handles the given key \a event.
*/
void QQuickTextEdit::keyReleaseEvent(QKeyEvent *event)
{
    Q_D(QQuickTextEdit);
    d->control->processEvent(event, QPointF(-d->xoff, -d->yoff));
    if (!event->isAccepted())
        QQuickImplicitSizeItem::keyReleaseEvent(event);
}

/*!
    \qmlmethod QtQuick2::TextEdit::deselect()

    Removes active text selection.
*/
void QQuickTextEdit::deselect()
{
    Q_D(QQuickTextEdit);
    QTextCursor c = d->control->textCursor();
    c.clearSelection();
    d->control->setTextCursor(c);
}

/*!
    \qmlmethod QtQuick2::TextEdit::selectAll()

    Causes all text to be selected.
*/
void QQuickTextEdit::selectAll()
{
    Q_D(QQuickTextEdit);
    d->control->selectAll();
}

/*!
    \qmlmethod QtQuick2::TextEdit::selectWord()

    Causes the word closest to the current cursor position to be selected.
*/
void QQuickTextEdit::selectWord()
{
    Q_D(QQuickTextEdit);
    QTextCursor c = d->control->textCursor();
    c.select(QTextCursor::WordUnderCursor);
    d->control->setTextCursor(c);
}

/*!
    \qmlmethod QtQuick2::TextEdit::select(int start, int end)

    Causes the text from \a start to \a end to be selected.

    If either start or end is out of range, the selection is not changed.

    After calling this, selectionStart will become the lesser
    and selectionEnd will become the greater (regardless of the order passed
    to this method).

    \sa selectionStart, selectionEnd
*/
void QQuickTextEdit::select(int start, int end)
{
    Q_D(QQuickTextEdit);
    if (start < 0 || end < 0 || start >= d->document->characterCount() || end >= d->document->characterCount())
        return;
    QTextCursor cursor = d->control->textCursor();
    cursor.beginEditBlock();
    cursor.setPosition(start, QTextCursor::MoveAnchor);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.endEditBlock();
    d->control->setTextCursor(cursor);

    // QTBUG-11100
    updateSelection();
}

/*!
    \qmlmethod QtQuick2::TextEdit::isRightToLeft(int start, int end)

    Returns true if the natural reading direction of the editor text
    found between positions \a start and \a end is right to left.
*/
bool QQuickTextEdit::isRightToLeft(int start, int end)
{
    if (start > end) {
        qmlInfo(this) << "isRightToLeft(start, end) called with the end property being smaller than the start.";
        return false;
    } else {
        return getText(start, end).isRightToLeft();
    }
}

#ifndef QT_NO_CLIPBOARD
/*!
    \qmlmethod QtQuick2::TextEdit::cut()

    Moves the currently selected text to the system clipboard.
*/
void QQuickTextEdit::cut()
{
    Q_D(QQuickTextEdit);
    d->control->cut();
}

/*!
    \qmlmethod QtQuick2::TextEdit::copy()

    Copies the currently selected text to the system clipboard.
*/
void QQuickTextEdit::copy()
{
    Q_D(QQuickTextEdit);
    d->control->copy();
}

/*!
    \qmlmethod QtQuick2::TextEdit::paste()

    Replaces the currently selected text by the contents of the system clipboard.
*/
void QQuickTextEdit::paste()
{
    Q_D(QQuickTextEdit);
    d->control->paste();
}
#endif // QT_NO_CLIPBOARD


/*!
    \qmlmethod QtQuick2::TextEdit::undo()

    Undoes the last operation if undo is \l {canUndo}{available}. Deselects any
    current selection, and updates the selection start to the current cursor
    position.
*/

void QQuickTextEdit::undo()
{
    Q_D(QQuickTextEdit);
    d->control->undo();
}

/*!
    \qmlmethod QtQuick2::TextEdit::redo()

    Redoes the last operation if redo is \l {canRedo}{available}.
*/

void QQuickTextEdit::redo()
{
    Q_D(QQuickTextEdit);
    d->control->redo();
}

/*!
\overload
Handles the given mouse \a event.
*/
void QQuickTextEdit::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickTextEdit);
    d->control->processEvent(event, QPointF(-d->xoff, -d->yoff));
    if (d->focusOnPress){
        bool hadActiveFocus = hasActiveFocus();
        forceActiveFocus(Qt::MouseFocusReason);
        // re-open input panel on press if already focused
#ifndef QT_NO_IM
        if (hasActiveFocus() && hadActiveFocus && !isReadOnly())
            qGuiApp->inputMethod()->show();
#endif
    }
    if (!event->isAccepted())
        QQuickImplicitSizeItem::mousePressEvent(event);
}

/*!
\overload
Handles the given mouse \a event.
*/
void QQuickTextEdit::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickTextEdit);
    d->control->processEvent(event, QPointF(-d->xoff, -d->yoff));

    if (!event->isAccepted())
        QQuickImplicitSizeItem::mouseReleaseEvent(event);
}

/*!
\overload
Handles the given mouse \a event.
*/
void QQuickTextEdit::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_D(QQuickTextEdit);
    d->control->processEvent(event, QPointF(-d->xoff, -d->yoff));
    if (!event->isAccepted())
        QQuickImplicitSizeItem::mouseDoubleClickEvent(event);
}

/*!
\overload
Handles the given mouse \a event.
*/
void QQuickTextEdit::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickTextEdit);
    d->control->processEvent(event, QPointF(-d->xoff, -d->yoff));
    if (!event->isAccepted())
        QQuickImplicitSizeItem::mouseMoveEvent(event);
}

#ifndef QT_NO_IM
/*!
\overload
Handles the given input method \a event.
*/
void QQuickTextEdit::inputMethodEvent(QInputMethodEvent *event)
{
    Q_D(QQuickTextEdit);
    const bool wasComposing = isInputMethodComposing();
    d->control->processEvent(event, QPointF(-d->xoff, -d->yoff));
    setCursorVisible(d->control->cursorVisible());
    if (wasComposing != isInputMethodComposing())
        emit inputMethodComposingChanged();
}

/*!
\overload
Returns the value of the given \a property.
*/
QVariant QQuickTextEdit::inputMethodQuery(Qt::InputMethodQuery property) const
{
    Q_D(const QQuickTextEdit);

    QVariant v;
    switch (property) {
    case Qt::ImEnabled:
        v = (bool)(flags() & ItemAcceptsInputMethod);
        break;
    case Qt::ImHints:
        v = (int)d->effectiveInputMethodHints();
        break;
    default:
        v = d->control->inputMethodQuery(property);
        break;
    }
    return v;

}
#endif // QT_NO_IM

void QQuickTextEdit::triggerPreprocess()
{
    Q_D(QQuickTextEdit);
    if (d->updateType == QQuickTextEditPrivate::UpdateNone)
        d->updateType = QQuickTextEditPrivate::UpdateOnlyPreprocess;
    update();
}

typedef QQuickTextEditPrivate::Node TextNode;
typedef QList<TextNode*>::iterator TextNodeIterator;


static bool comesBefore(TextNode* n1, TextNode* n2)
{
    return n1->startPos() < n2->startPos();
}

static inline void updateNodeTransform(QQuickTextNode* node, const QPointF &topLeft)
{
    QMatrix4x4 transformMatrix;
    transformMatrix.translate(topLeft.x(), topLeft.y());
    node->setMatrix(transformMatrix);
}

QSGNode *QQuickTextEdit::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData)
{
    Q_UNUSED(updatePaintNodeData);
    Q_D(QQuickTextEdit);

    if (d->updateType != QQuickTextEditPrivate::UpdatePaintNode && oldNode != 0) {
        // Update done in preprocess() in the nodes
        d->updateType = QQuickTextEditPrivate::UpdateNone;
        return oldNode;
    }

    d->updateType = QQuickTextEditPrivate::UpdateNone;

    QSGTransformNode *rootNode = static_cast<QSGTransformNode *>(oldNode);
    TextNodeIterator nodeIterator = d->textNodeMap.begin();
    while (nodeIterator != d->textNodeMap.end() && !(*nodeIterator)->dirty())
        ++nodeIterator;


    if (!oldNode || nodeIterator < d->textNodeMap.end()) {

        if (!oldNode)
            rootNode = new QSGTransformNode;

        int firstDirtyPos = 0;
        if (nodeIterator != d->textNodeMap.end()) {
            firstDirtyPos = (*nodeIterator)->startPos();
            do {
                rootNode->removeChildNode((*nodeIterator)->textNode());
                delete (*nodeIterator)->textNode();
                delete *nodeIterator;
                nodeIterator = d->textNodeMap.erase(nodeIterator);
            } while (nodeIterator != d->textNodeMap.end() && (*nodeIterator)->dirty());
        }

        // FIXME: the text decorations could probably be handled separately (only updated for affected textFrames)
        if (d->frameDecorationsNode) {
            rootNode->removeChildNode(d->frameDecorationsNode);
            delete d->frameDecorationsNode;
        }
        d->frameDecorationsNode = d->createTextNode();

        QQuickTextNode *node = 0;

        int currentNodeSize = 0;
        int nodeStart = firstDirtyPos;
        QPointF basePosition(d->xoff, d->yoff);
        QPointF nodeOffset;
        TextNode *firstCleanNode = (nodeIterator != d->textNodeMap.end()) ? *nodeIterator : 0;

        QList<QTextFrame *> frames;
        frames.append(d->document->rootFrame());

        while (!frames.isEmpty()) {
            QTextFrame *textFrame = frames.takeFirst();
            frames.append(textFrame->childFrames());
            d->frameDecorationsNode->m_engine->addFrameDecorations(d->document, textFrame);


            if (textFrame->lastPosition() < firstDirtyPos || (firstCleanNode && textFrame->firstPosition() >= firstCleanNode->startPos()))
                continue;
            node = d->createTextNode();

            if (textFrame->firstPosition() > textFrame->lastPosition()
                    && textFrame->frameFormat().position() != QTextFrameFormat::InFlow) {
                updateNodeTransform(node, d->document->documentLayout()->frameBoundingRect(textFrame).topLeft());
                const int pos = textFrame->firstPosition() - 1;
                ProtectedLayoutAccessor *a = static_cast<ProtectedLayoutAccessor *>(d->document->documentLayout());
                QTextCharFormat format = a->formatAccessor(pos);
                QTextBlock block = textFrame->firstCursorPosition().block();
                node->m_engine->setCurrentLine(block.layout()->lineForTextPosition(pos - block.position()));
                node->m_engine->addTextObject(QPointF(0, 0), format, QQuickTextNodeEngine::Unselected, d->document,
                                              pos, textFrame->frameFormat().position());
                nodeStart = pos;
            } else if (qobject_cast<QTextTable*>(textFrame)) { // To keep things simple, map text tables as one text node
                QTextFrame::iterator it = textFrame->begin();
                nodeOffset =  d->document->documentLayout()->frameBoundingRect(textFrame).topLeft();
                updateNodeTransform(node, nodeOffset);
                while (!it.atEnd())
                    node->m_engine->addTextBlock(d->document, (it++).currentBlock(), basePosition - nodeOffset, d->color, QColor(), selectionStart(), selectionEnd() - 1);
                nodeStart = textFrame->firstPosition();
            } else {
                // Having nodes spanning across frame boundaries will break the current bookkeeping mechanism. We need to prevent that.
                QList<int> frameBoundaries;
                frameBoundaries.reserve(frames.size());
                Q_FOREACH (QTextFrame *frame, frames)
                    frameBoundaries.append(frame->firstPosition());
                std::sort(frameBoundaries.begin(), frameBoundaries.end());

                QTextFrame::iterator it = textFrame->begin();
                while (!it.atEnd()) {
                    QTextBlock block = it.currentBlock();
                    ++it;
                    if (block.position() < firstDirtyPos)
                        continue;

                    if (!node->m_engine->hasContents()) {
                        nodeOffset = d->document->documentLayout()->blockBoundingRect(block).topLeft();
                        updateNodeTransform(node, nodeOffset);
                        nodeStart = block.position();
                    }

                    node->m_engine->addTextBlock(d->document, block, basePosition - nodeOffset, d->color, QColor(), selectionStart(), selectionEnd() - 1);
                    currentNodeSize += block.length();

                    if ((it.atEnd()) || (firstCleanNode && block.next().position() >= firstCleanNode->startPos())) // last node that needed replacing or last block of the frame
                        break;

                    QList<int>::const_iterator lowerBound = qLowerBound(frameBoundaries, block.next().position());
                    if (currentNodeSize > nodeBreakingSize || *lowerBound > nodeStart) {
                        currentNodeSize = 0;
                        d->addCurrentTextNodeToRoot(rootNode, node, nodeIterator, nodeStart);
                        node = d->createTextNode();
                        nodeStart = block.next().position();
                    }
                }
            }
            d->addCurrentTextNodeToRoot(rootNode, node, nodeIterator, nodeStart);
        }
        d->frameDecorationsNode->m_engine->addToSceneGraph(d->frameDecorationsNode, QQuickText::Normal, QColor());
        // Now prepend the frame decorations since we want them rendered first, with the text nodes and cursor in front.
        rootNode->prependChildNode(d->frameDecorationsNode);

        Q_ASSERT(nodeIterator == d->textNodeMap.end() || (*nodeIterator) == firstCleanNode);
        // Update the position of the subsequent text blocks.
        if (firstCleanNode) {
            QPointF oldOffset = firstCleanNode->textNode()->matrix().map(QPointF(0,0));
            QPointF currentOffset = d->document->documentLayout()->blockBoundingRect(d->document->findBlock(firstCleanNode->startPos())).topLeft();
            QPointF delta = currentOffset - oldOffset;
            while (nodeIterator != d->textNodeMap.end()) {
                QMatrix4x4 transformMatrix = (*nodeIterator)->textNode()->matrix();
                transformMatrix.translate(delta.x(), delta.y());
                (*nodeIterator)->textNode()->setMatrix(transformMatrix);
                ++nodeIterator;
            }

        }

        // Since we iterate over blocks from different text frames that are potentially not sorted
        // we need to ensure that our list of nodes is sorted again:
        std::sort(d->textNodeMap.begin(), d->textNodeMap.end(), &comesBefore);
    }

    if (d->cursorComponent == 0 && !isReadOnly()) {
        QColor color = (!d->cursorVisible || !d->control->cursorOn())
                ? QColor(0, 0, 0, 0)
                : d->color;
        if (d->cursorNode)
            rootNode->removeChildNode(d->cursorNode);
        delete d->cursorNode;
        d->cursorNode = new QSGSimpleRectNode(cursorRectangle(), color);
        rootNode->appendChildNode(d->cursorNode);
    }

    return rootNode;
}

/*!
    \qmlproperty bool QtQuick2::TextEdit::canPaste

    Returns true if the TextEdit is writable and the content of the clipboard is
    suitable for pasting into the TextEdit.
*/
bool QQuickTextEdit::canPaste() const
{
    Q_D(const QQuickTextEdit);
    if (!d->canPasteValid) {
        const_cast<QQuickTextEditPrivate *>(d)->canPaste = d->control->canPaste();
        const_cast<QQuickTextEditPrivate *>(d)->canPasteValid = true;
    }
    return d->canPaste;
}

/*!
    \qmlproperty bool QtQuick2::TextEdit::canUndo

    Returns true if the TextEdit is writable and there are previous operations
    that can be undone.
*/

bool QQuickTextEdit::canUndo() const
{
    Q_D(const QQuickTextEdit);
    return d->document->isUndoAvailable();
}

/*!
    \qmlproperty bool QtQuick2::TextEdit::canRedo

    Returns true if the TextEdit is writable and there are \l {undo}{undone}
    operations that can be redone.
*/

bool QQuickTextEdit::canRedo() const
{
    Q_D(const QQuickTextEdit);
    return d->document->isRedoAvailable();
}

#ifndef QT_NO_IM
/*!
    \qmlproperty bool QtQuick2::TextEdit::inputMethodComposing


    This property holds whether the TextEdit has partial text input from an
    input method.

    While it is composing an input method may rely on mouse or key events from
    the TextEdit to edit or commit the partial text.  This property can be used
    to determine when to disable events handlers that may interfere with the
    correct operation of an input method.
*/
bool QQuickTextEdit::isInputMethodComposing() const
{
    Q_D(const QQuickTextEdit);
    return d->control->hasImState();
}
#endif // QT_NO_IM

void QQuickTextEditPrivate::init()
{
    Q_Q(QQuickTextEdit);

#ifndef QT_NO_CLIPBOARD
    if (QGuiApplication::clipboard()->supportsSelection())
        q->setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton);
    else
#endif
        q->setAcceptedMouseButtons(Qt::LeftButton);

#ifndef QT_NO_IM
    q->setFlag(QQuickItem::ItemAcceptsInputMethod);
#endif
    q->setFlag(QQuickItem::ItemHasContents);

    document = new QQuickTextDocumentWithImageResources(q);

    control = new QQuickTextControl(document, q);
    control->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByKeyboard | Qt::TextEditable);
    control->setAcceptRichText(false);
    control->setCursorIsFocusIndicator(true);

    qmlobject_connect(control, QQuickTextControl, SIGNAL(updateCursorRequest()), q, QQuickTextEdit, SLOT(updateCursor()));
    qmlobject_connect(control, QQuickTextControl, SIGNAL(selectionChanged()), q, QQuickTextEdit, SIGNAL(selectedTextChanged()));
    qmlobject_connect(control, QQuickTextControl, SIGNAL(selectionChanged()), q, QQuickTextEdit, SLOT(updateSelection()));
    qmlobject_connect(control, QQuickTextControl, SIGNAL(cursorPositionChanged()), q, QQuickTextEdit, SLOT(updateSelection()));
    qmlobject_connect(control, QQuickTextControl, SIGNAL(cursorPositionChanged()), q, QQuickTextEdit, SIGNAL(cursorPositionChanged()));
    qmlobject_connect(control, QQuickTextControl, SIGNAL(cursorRectangleChanged()), q, QQuickTextEdit, SLOT(moveCursorDelegate()));
    qmlobject_connect(control, QQuickTextControl, SIGNAL(linkActivated(QString)), q, QQuickTextEdit, SIGNAL(linkActivated(QString)));
    qmlobject_connect(control, QQuickTextControl, SIGNAL(textChanged()), q, QQuickTextEdit, SLOT(q_textChanged()));
#ifndef QT_NO_CLIPBOARD
    qmlobject_connect(QGuiApplication::clipboard(), QClipboard, SIGNAL(dataChanged()), q, QQuickTextEdit, SLOT(q_canPasteChanged()));
#endif
    qmlobject_connect(document, QQuickTextDocumentWithImageResources, SIGNAL(undoAvailable(bool)), q, QQuickTextEdit, SIGNAL(canUndoChanged()));
    qmlobject_connect(document, QQuickTextDocumentWithImageResources, SIGNAL(redoAvailable(bool)), q, QQuickTextEdit, SIGNAL(canRedoChanged()));
    qmlobject_connect(document, QQuickTextDocumentWithImageResources, SIGNAL(imagesLoaded()), q, QQuickTextEdit, SLOT(updateSize()));
    QObject::connect(document, &QQuickTextDocumentWithImageResources::contentsChange, q, &QQuickTextEdit::q_contentsChange);

    document->setDefaultFont(font);
    document->setDocumentMargin(textMargin);
    document->setUndoRedoEnabled(false); // flush undo buffer.
    document->setUndoRedoEnabled(true);
    updateDefaultTextOption();
    q->updateSize();
}

void QQuickTextEdit::q_textChanged()
{
    Q_D(QQuickTextEdit);
    d->textCached = false;
    for (QTextBlock it = d->document->begin(); it != d->document->end(); it = it.next()) {
        d->contentDirection = d->textDirection(it.text());
        if (d->contentDirection != Qt::LayoutDirectionAuto)
            break;
    }
    d->determineHorizontalAlignment();
    d->updateDefaultTextOption();
    updateSize();
    updateTotalLines();
    emit textChanged();
}

void QQuickTextEdit::markDirtyNodesForRange(int start, int end, int charDelta)
{
    Q_D(QQuickTextEdit);
    if (start == end)
        return;

    TextNode dummyNode(start, 0);
    TextNodeIterator it = qLowerBound(d->textNodeMap.begin(), d->textNodeMap.end(), &dummyNode, &comesBefore);
    // qLowerBound gives us the first node past the start of the affected portion, rewind to the first node
    // that starts at the last position before the edit position. (there might be several because of images)
    if (it != d->textNodeMap.begin()) {
        --it;
        TextNode otherDummy((*it)->startPos(), 0);
        it = qLowerBound(d->textNodeMap.begin(), d->textNodeMap.end(), &otherDummy, &comesBefore);
    }

    // mark the affected nodes as dirty
    while (it != d->textNodeMap.constEnd()) {
        if ((*it)->startPos() <= end)
            (*it)->setDirty();
        else if (charDelta)
            (*it)->moveStartPos(charDelta);
        else
            return;
        ++it;
    }
}

void QQuickTextEdit::q_contentsChange(int pos, int charsRemoved, int charsAdded)
{
    Q_D(QQuickTextEdit);

    const int editRange = pos + qMax(charsAdded, charsRemoved);
    const int delta = charsAdded - charsRemoved;

    markDirtyNodesForRange(pos, editRange, delta);

    if (isComponentComplete()) {
        d->updateType = QQuickTextEditPrivate::UpdatePaintNode;
        update();
    }
}

void QQuickTextEdit::moveCursorDelegate()
{
    Q_D(QQuickTextEdit);
#ifndef QT_NO_IM
    updateInputMethod();
#endif
    emit cursorRectangleChanged();
    if (!d->cursorItem)
        return;
    QRectF cursorRect = cursorRectangle();
    d->cursorItem->setX(cursorRect.x());
    d->cursorItem->setY(cursorRect.y());
}

void QQuickTextEdit::updateSelection()
{
    Q_D(QQuickTextEdit);

    // No need for node updates when we go from an empty selection to another empty selection
    if (d->control->textCursor().hasSelection() || d->hadSelection) {
        markDirtyNodesForRange(qMin(d->lastSelectionStart, d->control->textCursor().selectionStart()), qMax(d->control->textCursor().selectionEnd(), d->lastSelectionEnd), 0);
        if (isComponentComplete()) {
            d->updateType = QQuickTextEditPrivate::UpdatePaintNode;
            update();
        }
    }

    d->hadSelection = d->control->textCursor().hasSelection();

    if (d->lastSelectionStart != d->control->textCursor().selectionStart()) {
        d->lastSelectionStart = d->control->textCursor().selectionStart();
        emit selectionStartChanged();
    }
    if (d->lastSelectionEnd != d->control->textCursor().selectionEnd()) {
        d->lastSelectionEnd = d->control->textCursor().selectionEnd();
        emit selectionEndChanged();
    }
}

QRectF QQuickTextEdit::boundingRect() const
{
    Q_D(const QQuickTextEdit);
    QRectF r(
            QQuickTextUtil::alignedX(d->contentSize.width(), width(), effectiveHAlign()),
            d->yoff,
            d->contentSize.width(),
            d->contentSize.height());

    int cursorWidth = 1;
    if (d->cursorItem)
        cursorWidth = 0;
    else if (!d->document->isEmpty())
        cursorWidth += 3;// ### Need a better way of accounting for space between char and cursor

    // Could include font max left/right bearings to either side of rectangle.
    r.setRight(r.right() + cursorWidth);

    return r;
}

QRectF QQuickTextEdit::clipRect() const
{
    Q_D(const QQuickTextEdit);
    QRectF r = QQuickImplicitSizeItem::clipRect();
    int cursorWidth = 1;
    if (d->cursorItem)
        cursorWidth = d->cursorItem->width();
    if (!d->document->isEmpty())
        cursorWidth += 3;// ### Need a better way of accounting for space between char and cursor

    // Could include font max left/right bearings to either side of rectangle.

    r.setRight(r.right() + cursorWidth);
    return r;
}

qreal QQuickTextEditPrivate::getImplicitWidth() const
{
    Q_Q(const QQuickTextEdit);
    if (!requireImplicitWidth) {
        // We don't calculate implicitWidth unless it is required.
        // We need to force a size update now to ensure implicitWidth is calculated
        const_cast<QQuickTextEditPrivate*>(this)->requireImplicitWidth = true;
        const_cast<QQuickTextEdit*>(q)->updateSize();
    }
    return implicitWidth;
}

//### we should perhaps be a bit smarter here -- depending on what has changed, we shouldn't
//    need to do all the calculations each time
void QQuickTextEdit::updateSize()
{
    Q_D(QQuickTextEdit);
    if (!isComponentComplete()) {
        d->dirty = true;
        return;
    }

    qreal naturalWidth = d->implicitWidth;

    qreal newWidth = d->document->idealWidth();
    // ### assumes that if the width is set, the text will fill to edges
    // ### (unless wrap is false, then clipping will occur)
    if (widthValid()) {
        if (!d->requireImplicitWidth) {
            emit implicitWidthChanged();
            // if the implicitWidth is used, then updateSize() has already been called (recursively)
            if (d->requireImplicitWidth)
                return;
        }
        if (d->requireImplicitWidth) {
            d->document->setTextWidth(-1);
            naturalWidth = d->document->idealWidth();

            const bool wasInLayout = d->inLayout;
            d->inLayout = true;
            setImplicitWidth(naturalWidth);
            d->inLayout = wasInLayout;
            if (d->inLayout)    // probably the result of a binding loop, but by letting it
                return;         // get this far we'll get a warning to that effect.
        }
        if (d->document->textWidth() != width()) {
            d->document->setTextWidth(width());
            newWidth = d->document->idealWidth();
        }
        //### need to confirm cost of always setting these
    } else if (d->wrapMode == NoWrap && d->document->textWidth() != newWidth) {
        d->document->setTextWidth(newWidth); // ### Text does not align if width is not set or the idealWidth exceeds the textWidth (QTextDoc bug)
    } else {
        d->document->setTextWidth(-1);
    }

    QFontMetricsF fm(d->font);
    qreal newHeight = d->document->isEmpty() ? qCeil(fm.height()) : d->document->size().height();

    // ### Setting the implicitWidth triggers another updateSize(), and unless there are bindings nothing has changed.
    if (!widthValid() && !d->requireImplicitWidth)
        setImplicitSize(newWidth, newHeight);
    else
        setImplicitHeight(newHeight);

    d->xoff = qMax(qreal(0), QQuickTextUtil::alignedX(d->document->size().width(), width(), effectiveHAlign()));
    d->yoff = QQuickTextUtil::alignedY(d->document->size().height(), height(), d->vAlign);
    setBaselineOffset(fm.ascent() + d->yoff + d->textMargin);

    QSizeF size(newWidth, newHeight);
    if (d->contentSize != size) {
        d->contentSize = size;
        emit contentSizeChanged();
    }
}

void QQuickTextEdit::updateWholeDocument()
{
    Q_D(QQuickTextEdit);
    if (!d->textNodeMap.isEmpty()) {
        Q_FOREACH (TextNode* node, d->textNodeMap)
            node->setDirty();
    }

    if (isComponentComplete()) {
        d->updateType = QQuickTextEditPrivate::UpdatePaintNode;
        update();
    }
}

void QQuickTextEdit::updateCursor()
{
    Q_D(QQuickTextEdit);
    if (isComponentComplete()) {
        d->updateType = QQuickTextEditPrivate::UpdatePaintNode;
        update();
    }
}

void QQuickTextEdit::q_updateAlignment()
{
    Q_D(QQuickTextEdit);
    if (d->determineHorizontalAlignment()) {
        d->updateDefaultTextOption();
        d->xoff = qMax(qreal(0), QQuickTextUtil::alignedX(d->document->size().width(), width(), effectiveHAlign()));
        moveCursorDelegate();
    }
}

void QQuickTextEdit::updateTotalLines()
{
    Q_D(QQuickTextEdit);

    int subLines = 0;

    for (QTextBlock it = d->document->begin(); it != d->document->end(); it = it.next()) {
        QTextLayout *layout = it.layout();
        if (!layout)
            continue;
        subLines += layout->lineCount()-1;
    }

    int newTotalLines = d->document->lineCount() + subLines;
    if (d->lineCount != newTotalLines) {
        d->lineCount = newTotalLines;
        emit lineCountChanged();
    }
}

void QQuickTextEditPrivate::updateDefaultTextOption()
{
    Q_Q(QQuickTextEdit);
    QTextOption opt = document->defaultTextOption();
    int oldAlignment = opt.alignment();
    Qt::LayoutDirection oldTextDirection = opt.textDirection();

    QQuickTextEdit::HAlignment horizontalAlignment = q->effectiveHAlign();
    if (contentDirection == Qt::RightToLeft) {
        if (horizontalAlignment == QQuickTextEdit::AlignLeft)
            horizontalAlignment = QQuickTextEdit::AlignRight;
        else if (horizontalAlignment == QQuickTextEdit::AlignRight)
            horizontalAlignment = QQuickTextEdit::AlignLeft;
    }
    if (!hAlignImplicit)
        opt.setAlignment((Qt::Alignment)(int)(horizontalAlignment | vAlign));
    else
        opt.setAlignment(Qt::Alignment(vAlign));

#ifndef QT_NO_IM
    if (contentDirection == Qt::LayoutDirectionAuto) {
        opt.setTextDirection(qGuiApp->inputMethod()->inputDirection());
    } else
#endif
    {
        opt.setTextDirection(contentDirection);
    }

    QTextOption::WrapMode oldWrapMode = opt.wrapMode();
    opt.setWrapMode(QTextOption::WrapMode(wrapMode));

    bool oldUseDesignMetrics = opt.useDesignMetrics();
    opt.setUseDesignMetrics(renderType != QQuickTextEdit::NativeRendering);

    if (oldWrapMode != opt.wrapMode() || oldAlignment != opt.alignment()
        || oldTextDirection != opt.textDirection()
        || oldUseDesignMetrics != opt.useDesignMetrics()) {
        document->setDefaultTextOption(opt);
    }
}

void QQuickTextEdit::focusInEvent(QFocusEvent *event)
{
    Q_D(QQuickTextEdit);
    d->handleFocusEvent(event);
    QQuickImplicitSizeItem::focusInEvent(event);
}

void QQuickTextEdit::focusOutEvent(QFocusEvent *event)
{
    Q_D(QQuickTextEdit);
    d->handleFocusEvent(event);
    QQuickImplicitSizeItem::focusOutEvent(event);
}

void QQuickTextEditPrivate::handleFocusEvent(QFocusEvent *event)
{
    Q_Q(QQuickTextEdit);
    bool focus = event->type() == QEvent::FocusIn;
    q->setCursorVisible(focus);
    control->processEvent(event, QPointF(-xoff, -yoff));
    if (focus) {
        q->q_updateAlignment();
#ifndef QT_NO_IM
        if (focusOnPress && !q->isReadOnly())
            qGuiApp->inputMethod()->show();
        q->connect(qApp->inputMethod(), SIGNAL(inputDirectionChanged(Qt::LayoutDirection)),
                q, SLOT(q_updateAlignment()));
    } else {
        q->disconnect(qApp->inputMethod(), SIGNAL(inputDirectionChanged(Qt::LayoutDirection)),
                   q, SLOT(q_updateAlignment()));
#endif
    }
}

void QQuickTextEditPrivate::addCurrentTextNodeToRoot(QSGTransformNode *root, QQuickTextNode *node, TextNodeIterator &it, int startPos)
{
    node->m_engine->addToSceneGraph(node, QQuickText::Normal, QColor());
    it = textNodeMap.insert(it, new TextNode(startPos, node));
    ++it;
    root->appendChildNode(node);
}

QQuickTextNode *QQuickTextEditPrivate::createTextNode()
{
    Q_Q(QQuickTextEdit);
    QQuickTextNode* node = new QQuickTextNode(QQuickItemPrivate::get(q)->sceneGraphContext(), q);
    node->setUseNativeRenderer(renderType == QQuickTextEdit::NativeRendering && window->devicePixelRatio() <= 1);
    node->initEngine(color, selectedTextColor, selectionColor);
    return node;
}

void QQuickTextEdit::q_canPasteChanged()
{
    Q_D(QQuickTextEdit);
    bool old = d->canPaste;
    d->canPaste = d->control->canPaste();
    bool changed = old!=d->canPaste || !d->canPasteValid;
    d->canPasteValid = true;
    if (changed)
        emit canPasteChanged();
}

/*!
    \qmlmethod string QtQuick2::TextEdit::getText(int start, int end)

    Returns the section of text that is between the \a start and \a end positions.

    The returned text does not include any rich text formatting.
*/

QString QQuickTextEdit::getText(int start, int end) const
{
    Q_D(const QQuickTextEdit);
    start = qBound(0, start, d->document->characterCount() - 1);
    end = qBound(0, end, d->document->characterCount() - 1);
    QTextCursor cursor(d->document);
    cursor.setPosition(start, QTextCursor::MoveAnchor);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
#ifndef QT_NO_TEXTHTMLPARSER
    return d->richText
            ? cursor.selectedText()
            : cursor.selection().toPlainText();
#else
    return cursor.selection().toPlainText();
#endif
}

/*!
    \qmlmethod string QtQuick2::TextEdit::getFormattedText(int start, int end)

    Returns the section of text that is between the \a start and \a end positions.

    The returned text will be formatted according the \l textFormat property.
*/

QString QQuickTextEdit::getFormattedText(int start, int end) const
{
    Q_D(const QQuickTextEdit);

    start = qBound(0, start, d->document->characterCount() - 1);
    end = qBound(0, end, d->document->characterCount() - 1);

    QTextCursor cursor(d->document);
    cursor.setPosition(start, QTextCursor::MoveAnchor);
    cursor.setPosition(end, QTextCursor::KeepAnchor);

    if (d->richText) {
#ifndef QT_NO_TEXTHTMLPARSER
        return cursor.selection().toHtml();
#else
        return cursor.selection().toPlainText();
#endif
    } else {
        return cursor.selection().toPlainText();
    }
}

/*!
    \qmlmethod QtQuick2::TextEdit::insert(int position, string text)

    Inserts \a text into the TextEdit at position.
*/
void QQuickTextEdit::insert(int position, const QString &text)
{
    Q_D(QQuickTextEdit);
    if (position < 0 || position >= d->document->characterCount())
        return;
    QTextCursor cursor(d->document);
    cursor.setPosition(position);
    d->richText = d->richText || (d->format == AutoText && Qt::mightBeRichText(text));
    if (d->richText) {
#ifndef QT_NO_TEXTHTMLPARSER
        cursor.insertHtml(text);
#else
        cursor.insertText(text);
#endif
    } else {
        cursor.insertText(text);
    }
    d->control->updateCursorRectangle(false);
}

/*!
    \qmlmethod string QtQuick2::TextEdit::remove(int start, int end)

    Removes the section of text that is between the \a start and \a end positions from the TextEdit.
*/

void QQuickTextEdit::remove(int start, int end)
{
    Q_D(QQuickTextEdit);
    start = qBound(0, start, d->document->characterCount() - 1);
    end = qBound(0, end, d->document->characterCount() - 1);
    QTextCursor cursor(d->document);
    cursor.setPosition(start, QTextCursor::MoveAnchor);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    d->control->updateCursorRectangle(false);
}

/*!
    \qmlproperty TextDocument QtQuick2::TextEdit::textDocument
    \since QtQuick 2.1

    Returns the QQuickTextDocument of this TextEdit.
    It can be used to implement syntax highlighting using
    \l QSyntaxHighlighter.

    \sa QQuickTextDocument
*/

QQuickTextDocument *QQuickTextEdit::textDocument()
{
    Q_D(QQuickTextEdit);
    if (!d->quickDocument)
        d->quickDocument = new QQuickTextDocument(this);
    return d->quickDocument;
}

QT_END_NAMESPACE