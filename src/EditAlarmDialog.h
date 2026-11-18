// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "AlarmEdit.h"

#include <QDialog>
#include <QTimer>

class QCalendarWidget;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QTimeEdit;

/**
 * Edit an alarm: one time expression (same syntax as the main input) with a
 * live preview of when it rings, plus label, notification style and state.
 * A date picker writes an absolute "yyyy-MM-dd HH:mm" into the expression,
 * so there is never a second, conflicting time field.
 */
class EditAlarmDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditAlarmDialog(const Alarm& alarm, QWidget* parent = nullptr);

    AlarmEdit edit() const;

private:
    void updatePreview();
    void normalizeExpression();
    void preparePicker();
    void applyPicker();

    Alarm m_alarm; // snapshot when the dialog opened
    QLineEdit* m_timeEdit = nullptr;
    QLabel* m_preview = nullptr;
    QLineEdit* m_labelEdit = nullptr;
    QComboBox* m_styleCombo = nullptr;
    QComboBox* m_stateCombo = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QTimer m_refresh;

    // Date picker (popup next to the time field)
    QCalendarWidget* m_pickDate = nullptr;
    QTimeEdit* m_pickTime = nullptr;
};
