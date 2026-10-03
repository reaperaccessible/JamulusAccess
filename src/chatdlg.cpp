/******************************************************************************\
 * Copyright (c) 2004-2026
 *
 * Author(s):
 *  Volker Fischer
 *
 * As of Jamulus 3.12.1dev (commit eb172d47): All new source code contributions must be licensed
 * under AGPL 3.0 or any later version.
 *
 * Existing code: Code contributed before 3.12.1dev (commit eb172d47) was licensed under GPL 2.0+.
 * This code will be licensed under GPL 3.0 (or any later version) from
 * 3.12.1dev (commit eb172d47).  When distributed as part of Jamulus, the AGPL 3.0 terms govern
 * the combined work, including network use provisions.
 *
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * ---------------------------------------------------------------------------
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
\******************************************************************************/

#include "chatdlg.h"
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QTextDocumentFragment>
#include "access/accessibility.h"

/* Implementation *************************************************************/
CChatDlg::CChatDlg ( QWidget* parent ) : CBaseDlg ( parent, Qt::Window ) // use Qt::Window to get min/max window buttons
{
    setupUi ( this );

    // Add help text to controls -----------------------------------------------
    // chat window
    txvChatWindow->setWhatsThis ( "<b>" + tr ( "Chat Window" ) + ":</b> " + tr ( "The chat window shows a history of all chat messages." ) );

    txvChatWindow->setAccessibleName ( tr ( "Chat history" ) );

    // the conversation for screen readers and braille: one row per message ("Paul: hello"), Up and
    // Down from one message to the next, Enter opens a link, Ctrl+C copies the message. It takes the
    // place of the history view, which only kept the colours.
    lstMessages = new QListWidget ( this );
    lstMessages->setAccessibleName ( tr ( "Messages" ) ); // short and fixed: the braille line keeps the row
    lstMessages->setSelectionMode ( QAbstractItemView::SingleSelection );
    lstMessages->setWordWrap ( true );
    lstMessages->installEventFilter ( this );
    Access::installListEdges ( lstMessages );
    if ( auto* box = qobject_cast<QBoxLayout*> ( layout() ) )
    {
        box->insertWidget ( box->indexOf ( txvChatWindow ), lstMessages );
    }
    txvChatWindow->hide();
    QWidget::setTabOrder ( lstMessages, edtLocalInputText );
    QWidget::setTabOrder ( edtLocalInputText, butSend );

    // input message text
    edtLocalInputText->setWhatsThis ( "<b>" + tr ( "Input Message Text" ) + ":</b> " +
                                      tr ( "Enter the chat message text in the edit box and press enter to send the "
                                           "message to the server which distributes the message to all connected "
                                           "clients. Your message will then show up in the chat window." ) );

    edtLocalInputText->setAccessibleName ( tr ( "Message" ) );

    // clear chat window and edit line
    txvChatWindow->clear();
    edtLocalInputText->clear();

    // we do not want to show a cursor in the chat history
    txvChatWindow->setCursorWidth ( 0 );

    // set a placeholder text to make sure where to type the message in (#384)
    edtLocalInputText->setPlaceholderText ( tr ( "Type a message here" ) );

    // Menu  -------------------------------------------------------------------
    pMenu            = new QMenuBar ( this );
    QMenu* pEditMenu = new QMenu ( tr ( "&Edit" ), this );

    pEditMenu->addAction ( tr ( "Cl&ear Chat History" ), this, SLOT ( OnClearChatHistory() ), QKeySequence ( Qt::CTRL + Qt::Key_E ) );

    pMenu->addMenu ( pEditMenu );
#if defined( Q_OS_IOS )
    QAction* closeAction = pMenu->addAction ( tr ( "&Close" ) );
#endif

#if defined( ANDROID ) || defined( Q_OS_ANDROID )
    pEditMenu->addAction ( tr ( "&Close" ), this, SLOT ( OnCloseClicked() ), QKeySequence ( Qt::CTRL + Qt::Key_W ) );
#endif

    // Now tell the layout about the menu
    layout()->setMenuBar ( pMenu );

    // Connections -------------------------------------------------------------
    QObject::connect ( edtLocalInputText, &QLineEdit::textChanged, this, &CChatDlg::OnLocalInputTextTextChanged );

    QObject::connect ( butSend, &QPushButton::clicked, this, &CChatDlg::OnSendText );

    QObject::connect ( txvChatWindow, &QTextBrowser::anchorClicked, this, &CChatDlg::OnAnchorClicked );

#if defined( Q_OS_IOS )
    QObject::connect ( closeAction, &QAction::triggered, this, &CChatDlg::OnCloseClicked );
#endif
}

void CChatDlg::OnLocalInputTextTextChanged ( const QString& strNewText )
{
    // check and correct length
    if ( strNewText.length() > MAX_LEN_CHAT_TEXT )
    {
        // text is too long, update control with shortened text
        edtLocalInputText->setText ( strNewText.left ( MAX_LEN_CHAT_TEXT ) );
    }
}

void CChatDlg::OnSendText()
{
    // send new text and clear line afterwards, do not send an empty message
    if ( !edtLocalInputText->text().isEmpty() )
    {
        strLastSent = edtLocalInputText->text();
        emit NewLocalInputText ( edtLocalInputText->text() );
        edtLocalInputText->clear();
    }
}

void CChatDlg::OnClearChatHistory()
{
    // clear chat window
    txvChatWindow->clear();
    lstMessages->clear();
    Access::announce ( this, tr ( "Chat history cleared" ) );
}

bool CChatDlg::eventFilter ( QObject* object, QEvent* event )
{
    if ( object == lstMessages && ( event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride ) )
    {
        auto*           key  = static_cast<QKeyEvent*> ( event );
        QListWidgetItem* item = lstMessages->currentItem();
        const bool      copy = key->matches ( QKeySequence::Copy );
        const bool      open = ( key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter ) && key->modifiers() == Qt::NoModifier;

        if ( event->type() == QEvent::ShortcutOverride )
        {
            // Ctrl+C copies the message (the main window has Ctrl+C for its connection setup)
            if ( copy )
            {
                event->accept();
                return true;
            }
            return false;
        }
        if ( copy && item )
        {
            QApplication::clipboard()->setText ( item->data ( Qt::UserRole + 1 ).toString() );
            Access::announce ( lstMessages, tr ( "Copied" ) );
            return true;
        }
        if ( open && item )
        {
            const QUrl url = item->data ( Qt::UserRole ).toUrl();
            if ( url.isValid() )
            {
                OpenLink ( url );
            }
            else
            {
                Access::announce ( lstMessages, tr ( "No link" ) );
            }
            return true;
        }
    }
    return CBaseDlg::eventFilter ( object, event );
}

void CChatDlg::AddChatText ( QString strChatText )
{
    // The message as a row of the list: "Paul: hello". The server sends
    // <font color=...>(10:21:33 AM) <b>Paul</b></font> hello
    static const QRegularExpression message ( "^\\s*<font[^>]*>\\s*\\(([^)]*)\\)\\s*<b>(.*)</b>\\s*</font>\\s*(.*)$",
                                              QRegularExpression::DotMatchesEverythingOption );
    const QRegularExpressionMatch   match = message.match ( strChatText );
    QString                         strSender;
    QString                         strText;
    QString                         strTime;
    if ( match.hasMatch() )
    {
        strTime   = match.captured ( 1 );
        strSender = QTextDocumentFragment::fromHtml ( match.captured ( 2 ) ).toPlainText().simplified();
        strText   = QTextDocumentFragment::fromHtml ( match.captured ( 3 ) ).toPlainText().simplified();
    }
    else
    {
        strText = QTextDocumentFragment::fromHtml ( strChatText ).toPlainText().simplified(); // welcome message
    }
    const QString strRow = strSender.isEmpty() ? strText : strSender + ": " + strText;

    auto* item = new QListWidgetItem ( strRow );
    item->setData ( Qt::UserRole + 1, strText );
    if ( !strTime.isEmpty() )
    {
        item->setToolTip ( strTime );
    }
    static const QRegularExpression link ( "https?://[^\\s<>\"]+" );
    if ( const QRegularExpressionMatch linkMatch = link.match ( strText ); linkMatch.hasMatch() )
    {
        item->setData ( Qt::UserRole, QUrl ( linkMatch.captured ( 0 ) ) );
    }
    // added at the end, the current row stays where the reader is (nothing is said by the list)
    lstMessages->addItem ( item );
    if ( !lstMessages->currentItem() )
    {
        lstMessages->setCurrentItem ( item, QItemSelectionModel::NoUpdate );
    }

    // said once, queued after what the screen reader is saying; not our own message coming back
    if ( !strLastSent.isEmpty() && strText == strLastSent.simplified() )
    {
        strLastSent.clear();
    }
    else
    {
        Access::announce ( this, strRow );
    }

    // analyze strChatText to check if hyperlink (limit ourselves to http(s)://) but do not
    // replace the hyperlinks if any HTML code for a hyperlink was found (the user has done the HTML
    // coding hisself and we should not mess with that)
    if ( !strChatText.contains ( QRegularExpression ( "href\\s*=|src\\s*=" ) ) )
    {
        // searches for all occurrences of http(s) and cuts until a space (\S matches any non-white-space
        // character and the + means that matches the previous element one or more times.)
        // This regex now contains three parts:
        // - https?://\\S+ matches as much non-whitespace as possible after the http:// or https://,
        //   subject to the next two parts, which exclude terminating punctuation
        // - (?<![!\"'()+,.:;<=>?\\[\\]{}]) is a negative look-behind assertion that disallows the match
        //   from ending with one of the characters !"'()+,.:;<=>?[]{}
        // - (?<!\\?[!\"'()+,.:;<=>?\\[\\]{}]) is a negative look-behind assertion that disallows the match
        //   from ending with a ? followed by one of the characters !"'()+,.:;<=>?[]{}
        // These last two parts must be separate, as a look-behind assertion must be fixed length.
#define PUNCT_NOEND_URL "[!\"'()+,.:;<=>?\\[\\]{}]"
        strChatText.replace ( QRegularExpression ( "(https?://\\S+(?<!" PUNCT_NOEND_URL ")(?<!\\?" PUNCT_NOEND_URL "))" ),
                              "<a href=\"\\1\">\\1</a>" );
    }

    // add new text in chat window
    txvChatWindow->append ( strChatText );
}

void CChatDlg::OnAnchorClicked ( const QUrl& Url ) { OpenLink ( Url ); }

void CChatDlg::OpenLink ( const QUrl& Url )
{
    // only allow http(s) URLs to be opened in an external browser
    if ( Url.scheme() == QLatin1String ( "https" ) || Url.scheme() == QLatin1String ( "http" ) )
    {
        if ( QMessageBox::question ( this,
                                     APP_NAME,
                                     tr ( "Do you want to open the link '%1' in your browser?" ).arg ( "<b>" + Url.toString() + "</b>" ),
                                     QMessageBox::Yes | QMessageBox::No ) == QMessageBox::Yes )
        {
            QDesktopServices::openUrl ( Url );
        }
    }
}

#if defined( Q_OS_IOS ) || defined( ANDROID ) || defined( Q_OS_ANDROID )
void CChatDlg::OnCloseClicked()
{
    // on mobile add a close button or menu entry
#    if defined( Q_OS_IOS )
    // On Qt6, iOS crashes if we call close() due to unknown reasons, therefore we just hide() the dialog. A Qt bug is suspected.
    // Checkout https://github.com/jamulussoftware/jamulus/pull/3413
    hide();
#    endif
#    if defined( ANDROID ) || defined( Q_OS_ANDROID )
    close();
#    endif
}
#endif