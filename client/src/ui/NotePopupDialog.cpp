#include "NotePopupDialog.h"

#include "settings/AppSettings.h"
#include "ui/StyleHelper.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

NotePopupDialog::NotePopupDialog(const QString &peer, const QString &displayName, itl::AppSettings *settings,
                                 QWidget *parent)
    : QDialog(parent)
    , m_peer(peer)
    , m_settings(settings)
{
  setWindowTitle(tr("Заметка — %1").arg(displayName));
  setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
  resize(380, 240);

  auto *root = new QVBoxLayout(this);
  root->addWidget(new QLabel(tr("Заметка для %1").arg(displayName)));

  m_hintLabel = new QLabel;
  m_hintLabel->setWordWrap(true);
  m_hintLabel->setObjectName(QStringLiteral("noteHint"));
  m_hintLabel->setVisible(false);
  root->addWidget(m_hintLabel);

  m_editor = new QTextEdit;
  m_editor->setPlaceholderText(tr("Введите или дополните заметку..."));
  if (m_settings) {
    m_editor->setPlainText(m_settings->noteForPeer(peer));
  }
  root->addWidget(m_editor, 1);

  auto *buttons = new QHBoxLayout;
  buttons->addStretch();
  m_primaryBtn = new QPushButton(tr("Сохранить"));
  auto *closeBtn = new QPushButton(tr("Закрыть"));
  buttons->addWidget(m_primaryBtn);
  buttons->addWidget(closeBtn);
  root->addLayout(buttons);

  connect(m_primaryBtn, &QPushButton::clicked, this, &NotePopupDialog::onSave);
  connect(closeBtn, &QPushButton::clicked, this, &NotePopupDialog::onClose);

  itl::applyDialogStyle(this);
}

void NotePopupDialog::setShowCallAction(bool show)
{
  m_showCallAction = show;
  if (!m_primaryBtn) {
    return;
  }
  m_primaryBtn->setText(show ? tr("Позвонить") : tr("Сохранить"));
  disconnect(m_primaryBtn, nullptr, this, nullptr);
  connect(m_primaryBtn, &QPushButton::clicked, this, show ? &NotePopupDialog::onCall : &NotePopupDialog::onSave);
}

void NotePopupDialog::setDuringCall(bool duringCall)
{
  m_hintLabel->setVisible(duringCall);
  if (duringCall) {
    m_hintLabel->setText(tr("Разговор идёт — можно дополнить заметку. Изменения сохранятся для этого контакта."));
    m_editor->setFocus();
    m_editor->moveCursor(QTextCursor::End);
  }
}

void NotePopupDialog::saveNote()
{
  if (m_settings) {
    m_settings->setNoteForPeer(m_peer, m_editor->toPlainText());
  }
}

void NotePopupDialog::onSave()
{
  saveNote();
  accept();
}

void NotePopupDialog::onCall()
{
  saveNote();
  done(CallResult);
}

void NotePopupDialog::onClose()
{
  saveNote();
  reject();
}
