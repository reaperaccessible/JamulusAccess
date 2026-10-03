/******************************************************************************\
 * Copyright (c) 2026
 *
 * Author(s):
 *  JamulusAccess
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
\******************************************************************************/

#pragma once

#include <QListWidget>
#include <QTimer>
#include "../util.h"

class CChannelFader;

// The mixer as a screen reader list (the family's "Parameters" list): one row per musician, in the
// order of the faders on screen.
// - Up/Down, Home/End: musicians; "First," / "Last," at the ends.
// - Left/Right: the musician's settings, in a loop (Volume, Pan, Mute, Solo, Group, Level, Profile).
// - Alt+Up/Down: change the value (5 %), Alt+Shift+Up/Down: small step; "Maximum," / "Minimum,".
// - Enter or Space: switch Mute or Solo, refresh Level; M and S: Mute and Solo from any setting.
// What is said is the row's text, so speech and braille always agree: the whole row on arrival
// ("Paul, Volume, 80 %"), the setting and its value with Left/Right ("Pan, Center"), the value alone
// when it changes ("85 %").
class CMixerAccessList : public QListWidget
{
    Q_OBJECT

public:
    CMixerAccessList ( CVector<CChannelFader*>& faders, QWidget* parent = nullptr );

    // The visible faders, in screen order (channel indices). Says who joined and who left, except
    // when the mixer fills on connecting or empties on disconnecting.
    void Sync ( const QList<int>& visibleChannels );

    // Something changed on a fader (value, mute, group, name, remote mute)
    void Refresh ( int iChannel );

    // Help rows for Ctrl+Shift+H
    static QStringList ShortcutRows();

protected:
    enum EParam
    {
        P_VOLUME,
        P_PAN,
        P_MUTE,
        P_SOLO,
        P_GROUP,
        P_LEVEL,
        P_PROFILE
    };

    // How much of the row the current row says
    enum EMode
    {
        M_FULL,  // musician, setting, value
        M_PARAM, // setting, value
        M_VALUE  // value
    };

    void keyPressEvent ( QKeyEvent* event ) override;
    bool event ( QEvent* event ) override;

    QListWidgetItem* ItemFor ( int iChannel ) const;
    CChannelFader*   FaderOf ( const QListWidgetItem* item ) const;
    QList<EParam>    Params ( CChannelFader* fader ) const;
    EParam           CurrentParam ( CChannelFader* fader ) const;
    QString          Identity ( CChannelFader* fader ) const;
    QString          ParamName ( EParam param ) const;
    QString          ValueText ( CChannelFader* fader, EParam param ) const;
    QString          RowText ( CChannelFader* fader, EMode mode ) const;
    void             UpdateItem ( QListWidgetItem* item );
    void             SpeakCurrent ( EMode mode, const QString& prefix = QString(), bool bSayIfSame = true );
    void             ChangeValue ( int iDirection, bool bSmallStep );
    void             Activate();
    void             ToggleBox ( EParam param );

    CVector<CChannelFader*>& vecpChanFader;
    EParam                   eParam;   // the setting chosen with Left/Right, the same for every row
    EMode                    eMode;    // what the current row says
    QString                  strPrefix; // "First, ", "Maximum, "... on the current row
    QString                  strBase;   // the current row's text without the prefix
    QTimer                   TimerLevels;
};
