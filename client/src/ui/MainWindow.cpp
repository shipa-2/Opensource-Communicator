#include "MainWindow.h"

#include "AddContactDialog.h"
#include "ConferenceDialog.h"
#include "DialKeypadWidget.h"
#include "TransferDialog.h"
#include "ContactRowWidget.h"
#include "HistoryRowWidget.h"
#include "HelpDialog.h"
#include "LoginDialog.h"
#include "NotePopupDialog.h"
#include "ProfileAvatarWidget.h"
#include "SettingsDialog.h"
#include "CallWindow.h"
#include "ChatDialog.h"
#include "PresenceSelector.h"
#include "calls/CallManager.h"
#include "chat/ChatManager.h"
#include "audio/MessageNotifyPlayer.h"
#include "audio/AudioDeviceUtils.h"
#include "demo/DemoData.h"
#include "protocol/AddressBookManager.h"
#include "protocol/CommunicatorClient.h"
#include "protocol/CallHistoryParser.h"
#include "protocol/ProtocolTypes.h"
#include "settings/AppSettings.h"
#include "settings/UserDataStore.h"
#include "ui/NativeScrollBars.h"
#include "ui/StyleHelper.h"
#include "ui/ThemeHelper.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QStyleOptionButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QButtonGroup>
#include <QAbstractButton>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QStyle>
#include <QTabBar>
#include <QTextStream>
#include <QTimer>
#include <QRandomGenerator>
#include <QVBoxLayout>
#include <QDateTime>
#include <QTime>
#include <QFont>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMimeData>
#include <QShowEvent>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTabBar>
#include <QUrl>
#include <QEvent>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QAbstractAnimation>

#include <algorithm>

Q_LOGGING_CATEGORY(lcHistory, "itl.history")

namespace {

int countDigits(const QString &value)
{
  int digits = 0;
  for (const QChar ch : value) {
    if (ch.isDigit()) {
      ++digits;
    }
  }
  return digits;
}

bool isDialableNumber(const QString &value)
{
  if (value.isEmpty()) {
    return false;
  }
  for (const QChar ch : value) {
    if (!ch.isDigit() && ch != QLatin1Char('+') && ch != QLatin1Char('*') && ch != QLatin1Char('#')) {
      return false;
    }
  }
  return true;
}

QString colorToRgba(const QColor &color, int alpha)
{
  return itl::colorToRgba(color, alpha);
}

int wallpaperAlphaFromOpacity(int opacityPercent)
{
  return qBound(40, qRound(255.0 * qBound(0, opacityPercent, 100) / 100.0), 255);
}

constexpr const char *kDimOverlayName = "itlDimOverlay";

void syncDimOverlayGeometry(QWidget *host)
{
  if (!host) {
    return;
  }
  if (auto *overlay =
          host->findChild<QWidget *>(QString::fromUtf8(kDimOverlayName), Qt::FindDirectChildrenOnly)) {
    overlay->setGeometry(host->rect());
    overlay->lower();
  }
}

void clearPanelStyle(QWidget *widget)
{
  if (!widget) {
    return;
  }
  widget->setStyleSheet({});
  widget->setAttribute(Qt::WA_StyledBackground, false);
  widget->setAttribute(Qt::WA_TranslucentBackground, false);
  widget->setAttribute(Qt::WA_NoSystemBackground, false);
  // Do NOT force WA_OpaquePaintEvent=true — complex widgets (menu bar, tab bar) that do not
  // paint every pixel will ghost/trail if that flag is set.
  widget->setAttribute(Qt::WA_OpaquePaintEvent, false);
  widget->setAutoFillBackground(false);
  widget->setPalette(QApplication::palette());
  if (auto *overlay =
          widget->findChild<QWidget *>(QString::fromUtf8(kDimOverlayName), Qt::FindDirectChildrenOnly)) {
    overlay->hide();
    overlay->setStyleSheet({});
  }
  widget->update();
}

void applyDimPanel(QWidget *widget, bool enabled, int alpha, const QColor &fillColor = QColor())
{
  if (!widget) {
    return;
  }
  // Dim via a child overlay (QSS only on that child) so native controls keep Breeze,
  // while rgba actually composites over the wallpaper.
  if (!enabled) {
    clearPanelStyle(widget);
    return;
  }

  widget->setStyleSheet({});
  widget->setAttribute(Qt::WA_StyledBackground, false);
  widget->setAttribute(Qt::WA_TranslucentBackground, false);
  widget->setAttribute(Qt::WA_NoSystemBackground, false);
  widget->setAutoFillBackground(false);
  widget->setAttribute(Qt::WA_OpaquePaintEvent, false);
  widget->setPalette(QApplication::palette());

  QWidget *overlay =
      widget->findChild<QWidget *>(QString::fromUtf8(kDimOverlayName), Qt::FindDirectChildrenOnly);
  if (!overlay) {
    overlay = new QWidget(widget);
    overlay->setObjectName(QString::fromUtf8(kDimOverlayName));
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay->setAttribute(Qt::WA_StyledBackground, true);
    overlay->setAttribute(Qt::WA_OpaquePaintEvent, false);
  }
  const QColor bg =
      fillColor.isValid() ? fillColor : QApplication::palette().color(QPalette::Window);
  overlay->setStyleSheet(
      QStringLiteral("#itlDimOverlay { background-color: %1; }").arg(colorToRgba(bg, alpha)));
  QRect geo = widget->rect();
  if (geo.isEmpty()) {
    // Not laid out yet — fill once the host gets a real size.
    QPointer<QWidget> hostGuard(widget);
    QPointer<QWidget> overlayGuard(overlay);
    QTimer::singleShot(0, widget, [hostGuard, overlayGuard]() {
      if (!hostGuard || !overlayGuard) {
        return;
      }
      overlayGuard->setGeometry(hostGuard->rect());
      overlayGuard->lower();
      overlayGuard->show();
      hostGuard->update();
    });
  } else {
    overlay->setGeometry(geo);
  }
  overlay->lower();
  overlay->show();
  overlay->update();
  widget->update();
}

constexpr const char *kListHoverRimName = "itlListHoverRim";
constexpr qreal kListCornerRadius = 4.0;

/** Soft Highlight rim around contact/history lists — fades like Breeze button hover (~150ms). */
class ListHoverChromeController final : public QObject {
public:
  explicit ListHoverChromeController(QListWidget *list)
      : QObject(list)
      , m_list(list)
  {
    m_rim = new QWidget(list);
    m_rim->setObjectName(QString::fromUtf8(kListHoverRimName));
    m_rim->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_rim->setAttribute(Qt::WA_NoSystemBackground);
    m_rim->setAutoFillBackground(false);
    m_rim->hide();

    m_anim.setDuration(150);
    m_anim.setEasingCurve(QEasingCurve::InOutQuad);
    QObject::connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
      m_strength = value.toReal();
      if (m_strength <= 0.001) {
        m_rim->hide();
      } else {
        syncRimGeometry();
        m_rim->show();
        m_rim->raise();
        m_rim->update();
      }
    });

    m_list->setAttribute(Qt::WA_Hover, true);
    m_list->setMouseTracking(true);
    m_list->installEventFilter(this);
    if (QWidget *viewport = m_list->viewport()) {
      viewport->setMouseTracking(true);
      viewport->installEventFilter(this);
    }
    m_rim->installEventFilter(this);
    syncRimGeometry();
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (watched == m_rim && event->type() == QEvent::Paint) {
      paintRim();
      return true;
    }

    switch (event->type()) {
    case QEvent::Enter:
    case QEvent::Leave:
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::FocusIn:
    case QEvent::FocusOut:
      QTimer::singleShot(0, this, [this]() { refreshTarget(); });
      break;
    case QEvent::Resize:
    case QEvent::Show:
      syncRimGeometry();
      break;
    default:
      break;
    }
    return QObject::eventFilter(watched, event);
  }

private:
  void syncRimGeometry()
  {
    if (!m_list || !m_rim) {
      return;
    }
    m_rim->setGeometry(m_list->rect());
    // Clip the list panel to the same corners as the hover rim (QSS radius alone
    // does not clip the viewport / item widgets).
    const QRect r = m_list->rect();
    if (r.isEmpty()) {
      m_list->clearMask();
      return;
    }
    QPainterPath path;
    path.addRoundedRect(QRectF(r), kListCornerRadius, kListCornerRadius);
    m_list->setMask(QRegion(path.toFillPolygon().toPolygon()));
  }

  void refreshTarget()
  {
    if (!m_list) {
      return;
    }
    const qreal target = (m_list->underMouse() || m_list->hasFocus()) ? 1.0 : 0.0;
    if (qFuzzyCompare(m_anim.endValue().toReal(), target) && m_anim.state() == QAbstractAnimation::Running) {
      return;
    }
    m_anim.stop();
    m_anim.setStartValue(m_strength);
    m_anim.setEndValue(target);
    m_anim.start();
  }

  void paintRim()
  {
    if (!m_rim || m_strength <= 0.0) {
      return;
    }
    QPainter painter(m_rim);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QColor accent = QApplication::palette().color(QPalette::Highlight);
    accent.setAlpha(qBound(0, qRound(255.0 * m_strength), 255));
    QPen pen(accent, 1.0);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(m_rim->rect()).adjusted(0.5, 0.5, -0.5, -0.5), kListCornerRadius,
                            kListCornerRadius);
  }

  QPointer<QListWidget> m_list;
  QWidget *m_rim = nullptr;
  QVariantAnimation m_anim;
  qreal m_strength = 0.0;
};

void ensureListHoverChrome(QListWidget *list)
{
  if (!list || list->property("itlListHoverChrome").toBool()) {
    return;
  }
  list->setProperty("itlListHoverChrome", true);
  new ListHoverChromeController(list);
}

void applyNativeListChrome(QListWidget *list, bool fillBackground)
{
  if (!list) {
    return;
  }

  if (list->objectName().isEmpty()) {
    list->setObjectName(QStringLiteral("dimList_%1").arg(reinterpret_cast<quintptr>(list), 0, 16));
  }
  const QString name = list->objectName();
  const QString viewportName = name + QStringLiteral("Viewport");

  if (QStyle *appStyle = QApplication::style()) {
    list->setStyle(appStyle);
  }

  // Use the real system palette: Base is usually darker than Window (recessed panel).
  QPalette listPalette = QApplication::palette();
  listPalette.setColor(QPalette::Text, listPalette.color(QPalette::WindowText));
  const QColor base = listPalette.color(QPalette::Base);

  // Base fill with the same corner radius as the hover rim.
  list->setStyleSheet(QStringLiteral("#%1 {"
                                     "  background-color: %2;"
                                     "  border: none;"
                                     "  border-radius: %3px;"
                                     "  outline: 0;"
                                     "}"
                                     "#%1::item {"
                                     "  background-color: transparent;"
                                     "  border: none;"
                                     "}")
                          .arg(name, base.name(QColor::HexRgb),
                               QString::number(int(kListCornerRadius))));

  // Do NOT set WA_OpaquePaintEvent — transparent rows would ghost previous stack pages.
  list->setAttribute(Qt::WA_StyledBackground, true);
  list->setAttribute(Qt::WA_TranslucentBackground, false);
  list->setAttribute(Qt::WA_NoSystemBackground, false);
  list->setAttribute(Qt::WA_OpaquePaintEvent, false);
  list->setAutoFillBackground(false);
  list->setPalette(listPalette);
  list->setFrameShape(QFrame::NoFrame);
  list->setMouseTracking(true);
  list->style()->unpolish(list);
  list->style()->polish(list);

  if (QWidget *viewport = list->viewport()) {
    viewport->setObjectName(viewportName);
    viewport->setStyleSheet(
        QStringLiteral("#%1 { background-color: transparent; border: none; }").arg(viewportName));
    viewport->setAttribute(Qt::WA_StyledBackground, true);
    viewport->setAttribute(Qt::WA_TranslucentBackground, false);
    viewport->setAttribute(Qt::WA_NoSystemBackground, false);
    viewport->setAttribute(Qt::WA_OpaquePaintEvent, false);
    viewport->setAutoFillBackground(false);
    viewport->setPalette(listPalette);
    viewport->setMouseTracking(true);
    viewport->style()->unpolish(viewport);
    viewport->style()->polish(viewport);
    viewport->update();
  }
  Q_UNUSED(fillBackground);
  ensureListHoverChrome(list);
  itl::applyNativeScrollBarStyle(list->verticalScrollBar());
  itl::applyNativeScrollBarStyle(list->horizontalScrollBar());
  list->update();
}

void applyDimList(QListWidget *list, bool enabled, int alpha)
{
  if (!list) {
    return;
  }
  if (!enabled) {
    applyNativeListChrome(list, true);
    QTimer::singleShot(0, list, [list]() {
      applyNativeListChrome(list, true);
      if (QWidget *viewport = list->viewport()) {
        viewport->repaint();
      }
      list->repaint();
    });
    return;
  }

  // Own Window fill at list opacity — same tint as page dim panels (not Base = black pit).
  // Viewport rules must be #id-scoped so child QMenus stay opaque.
  if (list->objectName().isEmpty()) {
    list->setObjectName(QStringLiteral("dimList_%1").arg(reinterpret_cast<quintptr>(list), 0, 16));
  }
  const QString name = list->objectName();
  const QString viewportName = name + QStringLiteral("Viewport");
  QPalette listPalette = QApplication::palette();
  listPalette.setColor(QPalette::Text, listPalette.color(QPalette::WindowText));
  const QColor fillColor = listPalette.color(QPalette::Window);
  const int clampedAlpha = qBound(40, alpha, 255);
  const QString fill = colorToRgba(fillColor, clampedAlpha);

  list->setStyleSheet(QStringLiteral("#%1 {"
                                     "  background-color: %2;"
                                     "  border: none;"
                                     "  border-radius: %3px;"
                                     "  outline: 0;"
                                     "}"
                                     "#%1::item {"
                                     "  background-color: transparent;"
                                     "  border: none;"
                                     "}")
                          .arg(name, fill, QString::number(int(kListCornerRadius))));
  list->setAutoFillBackground(false);
  list->setAttribute(Qt::WA_OpaquePaintEvent, false);
  list->setAttribute(Qt::WA_StyledBackground, true);
  list->setAttribute(Qt::WA_TranslucentBackground, false);
  list->setAttribute(Qt::WA_NoSystemBackground, false);
  list->setPalette(listPalette);
  list->setFrameShape(QFrame::NoFrame);
  list->setMouseTracking(true);
  if (QWidget *viewport = list->viewport()) {
    viewport->setObjectName(viewportName);
    viewport->setAutoFillBackground(false);
    viewport->setAttribute(Qt::WA_OpaquePaintEvent, false);
    viewport->setAttribute(Qt::WA_StyledBackground, true);
    viewport->setAttribute(Qt::WA_TranslucentBackground, false);
    viewport->setAttribute(Qt::WA_NoSystemBackground, false);
    viewport->setMouseTracking(true);
    viewport->setPalette(listPalette);
    viewport->setStyleSheet(
        QStringLiteral("#%1 { background-color: transparent; border: none; }").arg(viewportName));
  }
  ensureListHoverChrome(list);
  itl::applyNativeScrollBarStyle(list->verticalScrollBar());
  itl::applyNativeScrollBarStyle(list->horizontalScrollBar());
  QTimer::singleShot(0, list, [list]() {
    itl::applyNativeScrollBarStyle(list->verticalScrollBar());
    itl::applyNativeScrollBarStyle(list->horizontalScrollBar());
  });
}

void applySearchEditTransparency(QLineEdit *edit, bool enabled, int alpha)
{
  Q_UNUSED(enabled);
  Q_UNUSED(alpha);
  if (!edit) {
    return;
  }
  // Keep the system QLineEdit (placeholder, hover, focus ring) at every opacity.
  edit->setStyleSheet({});
  edit->setAttribute(Qt::WA_StyledBackground, false);
  edit->setPalette(QApplication::palette());
  if (QStyle *appStyle = QApplication::style()) {
    edit->setStyle(appStyle);
  }
  edit->style()->unpolish(edit);
  edit->style()->polish(edit);
  edit->update();
}

void applyNativePushButton(QPushButton *button)
{
  if (!button) {
    return;
  }
  // Same chrome as footer «Конференция»: system Breeze/Fusion, no QSS shaping.
  button->setStyleSheet({});
  button->setAttribute(Qt::WA_StyledBackground, false);
  button->setAttribute(Qt::WA_Hover, true);
  button->setAutoFillBackground(false);
  button->setFlat(false);
  button->setPalette(QApplication::palette());
  if (QStyle *appStyle = QApplication::style()) {
    button->setStyle(appStyle);
  }
  button->style()->unpolish(button);
  button->style()->polish(button);
  button->update();
}

/** Paints checked filter buttons with Highlight fill via the system style (no QSS). */
class CheckedAccentButtonFilter final : public QObject {
public:
  using QObject::QObject;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (event->type() != QEvent::Paint) {
      return QObject::eventFilter(watched, event);
    }
    auto *button = qobject_cast<QPushButton *>(watched);
    if (!button || !button->isChecked()) {
      return false;
    }

    QStyleOptionButton option;
    option.initFrom(button);
    option.rect = button->rect();
    option.text = button->text();
    option.icon = button->icon();
    option.iconSize = button->iconSize();
    option.features = QStyleOptionButton::None;
    if (button->isFlat()) {
      option.features |= QStyleOptionButton::Flat;
    }
    option.state = QStyle::State_None;
    if (button->isEnabled()) {
      option.state |= QStyle::State_Enabled;
    }
    if (button->hasFocus()) {
      option.state |= QStyle::State_HasFocus;
    }
    if (button->underMouse()) {
      option.state |= QStyle::State_MouseOver;
    }
    // Raised+On with Button=Highlight → solid accent chrome, system corner radius.
    option.state |= QStyle::State_Raised | QStyle::State_On;
    if (button->isDown()) {
      option.state |= QStyle::State_Sunken;
    }

    const QPalette appPal = QApplication::palette(button);
    const QColor accent = appPal.color(QPalette::Highlight);
    const QColor accentText = appPal.color(QPalette::HighlightedText);
    option.palette = appPal;
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive}) {
      option.palette.setColor(group, QPalette::Button, accent);
      option.palette.setColor(group, QPalette::Light, accent.lighter(112));
      option.palette.setColor(group, QPalette::Midlight, accent);
      option.palette.setColor(group, QPalette::Dark, accent.darker(118));
      option.palette.setColor(group, QPalette::Mid, accent.darker(110));
      option.palette.setColor(group, QPalette::ButtonText, accentText);
      option.palette.setColor(group, QPalette::WindowText, accentText);
      option.palette.setColor(group, QPalette::Text, accentText);
    }

    QPainter painter(button);
    button->style()->drawControl(QStyle::CE_PushButton, &option, &painter, button);
    return true;
  }
};

CheckedAccentButtonFilter *checkedAccentButtonFilter()
{
  static CheckedAccentButtonFilter filter;
  return &filter;
}

void applyCheckableFilterButtonStyle(QPushButton *button, bool /*checked*/)
{
  if (!button) {
    return;
  }
  applyNativePushButton(button);
  if (!button->property("itlAccentCheckedFilter").toBool()) {
    button->setProperty("itlAccentCheckedFilter", true);
    button->installEventFilter(checkedAccentButtonFilter());
  }
  button->update();
}

void applyFooterButtonTransparency(QWidget *footer, bool enabled, int alpha)
{
  Q_UNUSED(enabled);
  Q_UNUSED(alpha);
  if (!footer) {
    return;
  }
  for (QPushButton *btn : footer->findChildren<QPushButton *>(QString(), Qt::FindDirectChildrenOnly)) {
    if (!btn || btn->objectName() != QLatin1String("footerBtn")) {
      continue;
    }
    applyNativePushButton(btn);
  }
}

void applyDimmedMenu(QMenu *menu, bool /*enabled*/)
{
  itl::applyPopupMenuStyle(menu);
}

void applyDimTabs(QWidget *tabStrip, QTabBar *tabBar, QStackedWidget *stack, QWidget *tabBaseLine,
                  bool enabled, int alpha)
{
  // Separate QTabBar + QStackedWidget — no QTabWidget pane (that pane was opaque and left a gap).
  if (stack) {
    if (enabled) {
      stack->setStyleSheet({});
      stack->setAutoFillBackground(false);
      stack->setAttribute(Qt::WA_OpaquePaintEvent, false);
      stack->setAttribute(Qt::WA_StyledBackground, false);
      stack->setAttribute(Qt::WA_TranslucentBackground, false);
      stack->setAttribute(Qt::WA_NoSystemBackground, false);
      stack->setPalette(QApplication::palette());
    } else {
      clearPanelStyle(stack);
      // Opaque mode: stack must erase before the current page paints, or tab switches
      // leave dial/history pixels under transparent contact rows.
      stack->setAutoFillBackground(true);
      stack->setPalette(QApplication::palette());
    }
  }

  // Strip chrome matches header/footer (Window). Only the tab buttons use denser Base.
  applyDimPanel(tabStrip, enabled, alpha);

  if (tabBaseLine) {
    tabBaseLine->hide();
  }

  const QPalette appPal = QApplication::palette();
  if (tabBar) {
    const QColor base = appPal.color(QPalette::Base);
    const QColor accent = appPal.color(QPalette::Highlight);
    const QColor text = appPal.color(QPalette::WindowText);
    const QString tabFill =
        enabled ? colorToRgba(base, qBound(40, alpha + 24, 255)) : base.name(QColor::HexRgb);

    // Tab chips with accent top "cap" only — no horizontal rule under the row.
    tabBar->setStyleSheet(QStringLiteral("QTabBar#mainTabBar {"
                                         "  background: transparent;"
                                         "  border: none;"
                                         "}"
                                         "QTabBar#mainTabBar::tab {"
                                         "  background-color: %1;"
                                         "  color: %2;"
                                         "  padding: 6px 12px;"
                                         "  margin-right: 1px;"
                                         "  border: none;"
                                         "  border-top: 2px solid transparent;"
                                         "  border-top-left-radius: 4px;"
                                         "  border-top-right-radius: 4px;"
                                         "}"
                                         "QTabBar#mainTabBar::tab:selected {"
                                         "  background-color: %1;"
                                         "  border-top: 2px solid %3;"
                                         "}"
                                         "QTabBar#mainTabBar::tab:hover:!selected {"
                                         "  border-top: 2px solid %3;"
                                         "}")
                              .arg(tabFill, text.name(QColor::HexRgb), accent.name(QColor::HexRgb)));
    tabBar->setAttribute(Qt::WA_StyledBackground, true);
    tabBar->setAttribute(Qt::WA_TranslucentBackground, false);
    tabBar->setAttribute(Qt::WA_NoSystemBackground, false);
    tabBar->setAutoFillBackground(false);
    tabBar->setAttribute(Qt::WA_OpaquePaintEvent, false);
    tabBar->setPalette(appPal);
    tabBar->setDrawBase(false);
    tabBar->setExpanding(false);
    tabBar->style()->unpolish(tabBar);
    tabBar->style()->polish(tabBar);
    tabBar->update();
  }
}

void applyHistoryListPalette(QListWidget *historyList, const QPalette &sourcePalette)
{
  Q_UNUSED(sourcePalette);
  applyNativeListChrome(historyList, true);
}
} // namespace

MainWindow::MainWindow(itl::CommunicatorClient *client, itl::CallManager *calls, QWidget *parent)
    : QMainWindow(parent)
    , m_client(client)
    , m_calls(calls)
    , m_messageNotify(new itl::MessageNotifyPlayer(this))
    , m_callWindow(new CallWindow(this))
    , m_chatDialog(new ChatDialog(client, this))
{
  setWindowTitle(tr("OpenSource Communicator"));
  setProperty("itlNoMaximize", true);
  setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
  setFixedSize(390, 646);
  itl::preventFullscreen(this);

  auto *menuBar = new QMenuBar(this);
  m_menuBar = menuBar;
  menuBar->setObjectName(QStringLiteral("windowMenuBar"));
  menuBar->setNativeMenuBar(false);
  auto *headerLinks = new QWidget(menuBar);
  auto *linksRow = new QHBoxLayout(headerLinks);
  linksRow->setContentsMargins(0, 0, 4, 0);
  linksRow->setSpacing(8);
  auto *settingsBtn = new QPushButton(tr("Настройки"), headerLinks);
  settingsBtn->setObjectName(QStringLiteral("linkButton"));
  settingsBtn->setFlat(true);
  settingsBtn->setCursor(Qt::PointingHandCursor);
  auto *helpBtn = new QPushButton(tr("Помощь"), headerLinks);
  helpBtn->setObjectName(QStringLiteral("linkButton"));
  helpBtn->setFlat(true);
  helpBtn->setCursor(Qt::PointingHandCursor);
  linksRow->addWidget(settingsBtn);
  linksRow->addWidget(helpBtn);
  menuBar->setCornerWidget(headerLinks, Qt::TopLeftCorner);
  setMenuBar(menuBar);

  m_wallpaperBg = new QLabel(this);
  m_wallpaperBg->setObjectName(QStringLiteral("appWallpaperBg"));
  m_wallpaperBg->setScaledContents(true);
  m_wallpaperBg->setAlignment(Qt::AlignCenter);
  m_wallpaperBg->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_wallpaperBg->hide();

  auto *central = new QWidget;
  central->setObjectName(QStringLiteral("mainCentral"));
  setCentralWidget(central);

  auto *root = new QVBoxLayout(central);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto *header = new QWidget;
  header->setObjectName(QStringLiteral("mainHeader"));
  m_mainHeader = header;
  auto *headerOuter = new QVBoxLayout(header);
  headerOuter->setContentsMargins(10, 8, 10, 8);

  auto *profileRow = new QHBoxLayout;
  m_headerAvatar = new ProfileAvatarWidget(&m_client->appSettings());
  m_headerAvatar->setMenuEnabled(false);
  profileRow->addWidget(m_headerAvatar);

  auto *profileText = new QVBoxLayout;
  profileText->setSpacing(2);
  m_headerName = new QLabel(tr("Не авторизован"));
  m_headerName->setObjectName(QStringLiteral("headerName"));
  QFont headerNameFont = m_headerName->font();
  headerNameFont.setPixelSize(18);
  headerNameFont.setBold(true);
  m_headerName->setFont(headerNameFont);

  auto *nameRow = new QWidget;
  auto *nameRowLayout = new QHBoxLayout(nameRow);
  nameRowLayout->setContentsMargins(16, 0, 0, 0);
  nameRowLayout->setSpacing(0);
  nameRowLayout->addWidget(m_headerName);
  profileText->addWidget(nameRow);

  m_presenceSelector = new PresenceSelector;
  m_presenceSelector->setObjectName(QStringLiteral("presenceSelector"));
  profileText->addWidget(m_presenceSelector);
  profileRow->addLayout(profileText, 1);
  headerOuter->addLayout(profileRow);
  root->addWidget(header);

  m_tabStrip = new QWidget;
  m_tabStrip->setObjectName(QStringLiteral("mainTabStrip"));
  auto *tabStripLayout = new QVBoxLayout(m_tabStrip);
  tabStripLayout->setContentsMargins(0, 0, 0, 0);
  tabStripLayout->setSpacing(0);
  m_tabBar = new QTabBar(m_tabStrip);
  m_tabBar->setObjectName(QStringLiteral("mainTabBar"));
  m_tabBar->setExpanding(false);
  m_tabBar->setDrawBase(false);
  tabStripLayout->addWidget(m_tabBar);
  // Optional leftover; accent rule + notch are drawn inside QTabBar QSS.
  m_tabBaseLine = new QWidget(m_tabStrip);
  m_tabBaseLine->setObjectName(QStringLiteral("mainTabBaseLine"));
  m_tabBaseLine->setFixedHeight(2);
  m_tabBaseLine->hide();
  root->addWidget(m_tabStrip);

  m_tabStack = new QStackedWidget;
  m_tabStack->setObjectName(QStringLiteral("mainTabStack"));
  m_tabStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  root->addWidget(m_tabStack, 1);
  connect(m_tabBar, &QTabBar::currentChanged, m_tabStack, &QStackedWidget::setCurrentIndex);
  connect(m_tabBar, &QTabBar::currentChanged, this, [this](int) {
    for (QWidget *host : {static_cast<QWidget *>(m_contactsPage), static_cast<QWidget *>(m_historyPage),
                          static_cast<QWidget *>(m_dialPage)}) {
      syncDimOverlayGeometry(host);
    }
    syncDimOverlayGeometry(m_tabStrip);
  });

  auto *contactsPage = new QWidget;
  contactsPage->setObjectName(QStringLiteral("contactsPage"));
  m_contactsPage = contactsPage;
  auto *contactsLayout = new QVBoxLayout(contactsPage);
  contactsLayout->setContentsMargins(8, 8, 8, 8);
  contactsLayout->setSpacing(8);

  m_contactsToolbar = new QWidget(contactsPage);
  m_contactsToolbar->setObjectName(QStringLiteral("contactsToolbar"));
  auto *toolbarLayout = new QVBoxLayout(m_contactsToolbar);
  toolbarLayout->setContentsMargins(6, 6, 6, 6);
  toolbarLayout->setSpacing(8);

  auto *filterRow = new QHBoxLayout;
  m_filterGroup = new QButtonGroup(this);
  m_filterGroup->setExclusive(true);
  auto *allBtn = new QPushButton(tr("Все"));
  allBtn->setObjectName(QStringLiteral("filterBtn"));
  allBtn->setCheckable(true);
  allBtn->setChecked(true);
  filterRow->addWidget(allBtn, 1);
  auto *recentBtn = new QPushButton(tr("Недавние"));
  recentBtn->setObjectName(QStringLiteral("filterBtn"));
  recentBtn->setCheckable(true);
  filterRow->addWidget(recentBtn, 1);
  auto *externalBtn = new QPushButton(tr("Внешние"));
  externalBtn->setObjectName(QStringLiteral("filterBtn"));
  externalBtn->setCheckable(true);
  filterRow->addWidget(externalBtn, 1);
  m_filterGroup->addButton(allBtn, static_cast<int>(ContactSortMode::All));
  m_filterGroup->addButton(recentBtn, static_cast<int>(ContactSortMode::Recent));
  m_filterGroup->addButton(externalBtn, static_cast<int>(ContactSortMode::External));
  toolbarLayout->addLayout(filterRow);
  updateFilterButtonStyles();

  m_searchEdit = new QLineEdit;
  m_searchEdit->setObjectName(QStringLiteral("searchEdit"));
  m_searchEdit->setPlaceholderText(tr("Введите номер или имя контакта"));
  toolbarLayout->addWidget(m_searchEdit);
  contactsLayout->addWidget(m_contactsToolbar);

  m_contactsList = new QListWidget;
  m_contactsList->setObjectName(QStringLiteral("contactList"));
  m_contactsList->setFrameShape(QFrame::NoFrame);
  m_contactsList->setSpacing(0);
  applyDimList(m_contactsList, false, 255);
  contactsLayout->addWidget(m_contactsList, 1);
  m_tabBar->addTab(tr("Контакты"));
  m_tabStack->addWidget(contactsPage);

  m_dialPage = new QWidget;
  m_dialPage->setObjectName(QStringLiteral("dialPage"));
  m_dialPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *dialLayout = new QVBoxLayout(m_dialPage);
  dialLayout->setContentsMargins(16, 24, 16, 16);
  dialLayout->addWidget(new QLabel(tr("Набрать номер или внутренний код:")));
  m_dialInput = new QLineEdit;
  m_dialInput->setObjectName(QStringLiteral("dialEdit"));
  m_dialInput->setPlaceholderText(tr("702, ivan или +7..."));
  m_dialInput->setAlignment(Qt::AlignCenter);
  dialLayout->addWidget(m_dialInput);

  m_dialKeypad = new DialKeypadWidget(m_dialPage);
  m_dialKeypad->setLineEdit(m_dialInput);
  dialLayout->addWidget(m_dialKeypad);

  m_dialCallBtn = new QPushButton(tr("Позвонить"));
  m_dialCallBtn->setObjectName(QStringLiteral("dialCallBtn"));
  dialLayout->addWidget(m_dialCallBtn);
  dialLayout->addStretch(1);
  m_tabBar->addTab(tr("Набрать вручную"));
  m_tabStack->addWidget(m_dialPage);

  auto *historyPage = new QWidget;
  historyPage->setObjectName(QStringLiteral("historyPage"));
  m_historyPage = historyPage;
  auto *historyLayout = new QVBoxLayout(historyPage);
  historyLayout->setContentsMargins(8, 8, 8, 8);
  historyLayout->setSpacing(8);

  m_historyToolbar = new QWidget(historyPage);
  m_historyToolbar->setObjectName(QStringLiteral("historyToolbar"));
  auto *historyToolbarLayout = new QVBoxLayout(m_historyToolbar);
  historyToolbarLayout->setContentsMargins(6, 6, 6, 6);
  historyToolbarLayout->setSpacing(8);

  auto *periodRow = new QHBoxLayout;
  periodRow->addWidget(new QLabel(tr("Показать за:")));
  m_historyPeriodBtn = new QPushButton;
  m_historyPeriodBtn->setObjectName(QStringLiteral("linkButton"));
  m_historyPeriodBtn->setFlat(true);
  m_historyPeriodBtn->setCursor(Qt::PointingHandCursor);
  applyLinkButtonStyle(m_historyPeriodBtn);
  periodRow->addWidget(m_historyPeriodBtn);
  periodRow->addStretch();
  historyToolbarLayout->addLayout(periodRow);

  auto *historyFilterRow = new QHBoxLayout;
  m_historyDirGroup = new QButtonGroup(this);
  const struct {
    const char *label;
    HistoryDir mode;
  } dirButtons[] = {
      {QT_TR_NOOP("Все"), HistoryDir::All},
      {QT_TR_NOOP("Входящие"), HistoryDir::Incoming},
      {QT_TR_NOOP("Без ответа"), HistoryDir::Missed},
      {QT_TR_NOOP("Исходящие"), HistoryDir::Outgoing},
  };
  for (const auto &def : dirButtons) {
    auto *btn = new QPushButton(tr(def.label));
    btn->setObjectName(QStringLiteral("filterBtn"));
    btn->setCheckable(true);
    btn->setChecked(def.mode == HistoryDir::All);
    m_historyDirGroup->addButton(btn, static_cast<int>(def.mode));
    historyFilterRow->addWidget(btn, 1);
  }
  historyToolbarLayout->addLayout(historyFilterRow);

  m_historySearchEdit = new QLineEdit;
  m_historySearchEdit->setObjectName(QStringLiteral("searchEdit"));
  m_historySearchEdit->setPlaceholderText(tr("Поиск в истории звонков"));
  m_historySearchEdit->setClearButtonEnabled(true);
  historyToolbarLayout->addWidget(m_historySearchEdit);
  historyLayout->addWidget(m_historyToolbar);

  m_historyList = new QListWidget;
  m_historyList->setObjectName(QStringLiteral("historyList"));
  m_historyList->setFrameShape(QFrame::NoFrame);
  m_historyList->setSpacing(0);
  m_historyList->setMouseTracking(true);
  m_historyList->viewport()->setMouseTracking(true);
  applyHistoryListPalette(m_historyList, palette());
  historyLayout->addWidget(m_historyList, 1);

  m_historyScopeBar = new QWidget(historyPage);
  m_historyScopeBar->setObjectName(QStringLiteral("historyScopeBar"));
  auto *historyScopeLayout = new QHBoxLayout(m_historyScopeBar);
  historyScopeLayout->setContentsMargins(2, 4, 2, 4);
  historyScopeLayout->setSpacing(4);
  m_historyScopeGroup = new QButtonGroup(this);
  const struct {
    const char *label;
    const char *tooltip;
    HistoryScope mode;
  } scopeButtons[] = {
      {QT_TR_NOOP("Мои"), QT_TR_NOOP("Мои звонки"), HistoryScope::Mine},
      {QT_TR_NOOP("Компания"), QT_TR_NOOP("Звонки компании"), HistoryScope::Company},
      {QT_TR_NOOP("Внутренние"), QT_TR_NOOP("Внутренние звонки"), HistoryScope::Internal},
  };
  for (const auto &def : scopeButtons) {
    auto *btn = new QPushButton(tr(def.label));
    btn->setObjectName(QStringLiteral("filterBtn"));
    btn->setCheckable(true);
    btn->setChecked(def.mode == HistoryScope::Mine);
    btn->setToolTip(tr(def.tooltip));
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QFont scopeFont = btn->font();
    scopeFont.setPixelSize(11);
    btn->setFont(scopeFont);
    m_historyScopeGroup->addButton(btn, static_cast<int>(def.mode));
    historyScopeLayout->addWidget(btn, 1);
  }
  historyLayout->addWidget(m_historyScopeBar);

  m_tabBar->addTab(tr("История"));
  m_tabStack->addWidget(historyPage);

  auto *footer = new QWidget;
  footer->setObjectName(QStringLiteral("mainFooter"));
  m_mainFooter = footer;
  auto *footerLayout = new QHBoxLayout(footer);
  auto *addBtn = new QPushButton(tr("Добавить"));
  addBtn->setObjectName(QStringLiteral("footerBtn"));
  m_addMenu = new QMenu(addBtn);
  m_addMenu->addAction(tr("Контакт"), this, &MainWindow::onAddContact);
  m_addMenu->addAction(tr("Импорт"), this, &MainWindow::onImportContacts);
  addBtn->setMenu(m_addMenu);
  auto *confBtn = new QPushButton(tr("Конференция"));
  confBtn->setObjectName(QStringLiteral("footerBtn"));
  auto *viewBtn = new QPushButton(tr("Вид"));
  viewBtn->setObjectName(QStringLiteral("footerBtn"));
  m_viewMenu = new QMenu(viewBtn);
  m_viewChatAction = m_viewMenu->addAction(tr("Кнопка сообщений"));
  m_viewChatAction->setCheckable(true);
  m_viewCallAction = m_viewMenu->addAction(tr("Кнопка звонка"));
  m_viewCallAction->setCheckable(true);
  m_viewVideoAction = m_viewMenu->addAction(tr("Кнопка видеозвонка"));
  m_viewVideoAction->setCheckable(true);
  connect(m_addMenu, &QMenu::aboutToShow, this, [this]() {
    applyDimmedMenu(m_addMenu, true);
  });
  connect(m_viewMenu, &QMenu::aboutToShow, this, [this]() {
    applyDimmedMenu(m_viewMenu, true);
    if (m_viewChatAction) {
      m_viewChatAction->setChecked(m_client->appSettings().showChatButtons());
    }
    if (m_viewCallAction) {
      m_viewCallAction->setChecked(m_client->appSettings().showCallButtons());
    }
    if (m_viewVideoAction) {
      m_viewVideoAction->setChecked(m_client->appSettings().showVideoButtons());
      m_viewVideoAction->setVisible(serverVideoUiAvailable());
    }
  });
  connect(m_viewChatAction, &QAction::triggered, this, [this](bool checked) {
    m_client->appSettings().setShowChatButtons(checked);
    m_client->saveSettings();
    applyContactViewSettings();
  });
  connect(m_viewCallAction, &QAction::triggered, this, [this](bool checked) {
    m_client->appSettings().setShowCallButtons(checked);
    m_client->saveSettings();
    applyContactViewSettings();
  });
  connect(m_viewVideoAction, &QAction::triggered, this, [this](bool checked) {
    m_client->appSettings().setShowVideoButtons(checked);
    m_client->saveSettings();
    applyContactViewSettings();
  });
  viewBtn->setMenu(m_viewMenu);
  footerLayout->addWidget(addBtn);
  footerLayout->addWidget(confBtn);
  footerLayout->addWidget(viewBtn);
  root->addWidget(footer);

  connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettings);
  connect(helpBtn, &QPushButton::clicked, this, &MainWindow::onHelp);
  connect(confBtn, &QPushButton::clicked, this, &MainWindow::onConference);
  connect(m_dialCallBtn, &QPushButton::clicked, this, &MainWindow::onDial);
  connect(m_dialInput, &QLineEdit::returnPressed, this, &MainWindow::onDial);
  connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);
  connect(m_contactsList, &QListWidget::currentItemChanged, this, [this]() { onContactSelected(); });
  connect(m_presenceSelector, &PresenceSelector::currentIndexChanged, this, &MainWindow::onPresenceChanged);
  connect(m_filterGroup, &QButtonGroup::idClicked, this, &MainWindow::onFilterChanged);
  connect(m_historyDirGroup, &QButtonGroup::idClicked, this, &MainWindow::onHistoryDirChanged);
  connect(m_historyScopeGroup, &QButtonGroup::idClicked, this, &MainWindow::onHistoryScopeChanged);
  connect(m_historyPeriodBtn, &QPushButton::clicked, this, &MainWindow::onHistoryPeriodClicked);
  connect(m_historySearchEdit, &QLineEdit::textChanged, this, &MainWindow::onHistorySearchChanged);
  // Do not setCurrentItem on itemEntered — QListWidget scrolls to the current row and
  // "rips" the list to the bottom while the cursor moves down. Hover chrome is in HistoryRowWidget.
  connect(m_historyList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *, QListWidgetItem *) {
    onHistorySelected();
  });
  connect(m_headerAvatar, &ProfileAvatarWidget::settingsChanged, this, &MainWindow::onProfileAvatarChanged);
  connect(&m_client->appSettings(), &itl::AppSettings::settingsChanged, this, &MainWindow::refreshWallpaper);

  connect(m_callWindow, &CallWindow::hangupRequested, this, &MainWindow::onHangup);
  connect(m_callWindow, &CallWindow::answerRequested, this, &MainWindow::onAnswer);
  connect(m_callWindow, &CallWindow::holdRequested, this, &MainWindow::onHold);
  connect(m_callWindow, &CallWindow::dtmfRequested, this, &MainWindow::onCallDtmf);
  connect(m_callWindow, &CallWindow::transferRequested, this, &MainWindow::onTransfer);
  connect(m_callWindow, &CallWindow::notesChanged, this, &MainWindow::onCallNotesChanged);
  connect(m_callWindow, &CallWindow::videoSendingRequested, this, [this](bool enabled) {
    if (!m_activeLeg.isEmpty()) {
      m_calls->sendVideo(m_activeLeg, enabled);
    }
  });
  connect(m_callWindow, &CallWindow::screenSharingRequested, this, [this](bool enabled) {
    if (!m_activeLeg.isEmpty()) {
      m_calls->setScreenSharing(m_activeLeg, enabled);
    }
  });
  connect(m_callWindow, &CallWindow::videoBlurRequested, this, [this](bool enabled) {
    m_calls->setVideoBlur(enabled);
  });

  connect(m_client, &itl::CommunicatorClient::statusMessage, this, &MainWindow::onStatusMessage);
  connect(m_client, &itl::CommunicatorClient::contactUpdated, this, &MainWindow::onContactUpdated);
  connect(m_client, &itl::CommunicatorClient::addressBookChanged, this, &MainWindow::onAddressBookChanged);
  connect(m_client->addressBook(), &itl::AddressBookManager::deleteFailed, this,
          [this](const QString &, const QString &reason) {
            const QString message = reason.isEmpty()
                ? tr("Не удалось удалить контакт на сервере.")
                : tr("Не удалось удалить контакт на сервере: %1").arg(reason);
            QMessageBox::warning(this, tr("Удалить контакт"), message);
          });
  connect(m_client, &itl::CommunicatorClient::chatMessage, this, &MainWindow::onIncomingChatMessage);
  connect(m_client->chat(), &itl::ChatManager::unreadChanged, this, [this](const QString &) {
    updateUnreadIndicators();
  });
  connect(m_client->chat(), &itl::ChatManager::peerColorReceived, this, [this](const QString &peer, const QString &color) {
    applyPeerColorForPeer(peer, color);
  });
  connect(m_client->chat(), &itl::ChatManager::peerAvatarReceived, this,
          [this](const QString &peer, const QPixmap &avatar) {
            applyPeerAvatarForPeer(peer, avatar);
          });
  connect(m_client->chat(), &itl::ChatManager::oscPeerDiscovered, this, [this](const QString &peer) {
    ContactRowWidget *row = rowWidgetForPeer(peer);
    if (!row) {
      return;
    }
    row->setOscPeerStyle(true);
    for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
      if (isSamePeer(it.key(), peer)) {
        row->updatePresence(it.value().presence);
        break;
      }
    }
    row->startOscDiscoveryWave();
  });
  connect(m_client->chat(), &itl::ChatManager::demoPeerRenameRequested, this,
          [this](const QString &peer, const QString &newName) {
            if (!m_demoMode || newName.trimmed().isEmpty()) {
              return;
            }
            QString key = peer;
            if (!m_contacts.contains(key)) {
              for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
                if (isSamePeer(it.key(), peer)) {
                  key = it.key();
                  break;
                }
              }
            }
            if (!m_contacts.contains(key) || m_contacts.value(key).isSelf) {
              return;
            }
            m_contacts[key].name = newName.trimmed();
            rebuildContactList();
            rebuildHistoryList();
            if (m_chatDialog && m_chatDialog->isOpenForPeer(key)) {
              m_chatDialog->updatePeerDisplayName(m_contacts[key].name);
            }
            if (m_callWindow && m_activeLeg == m_demoCallLeg && m_callTracking.contains(m_demoCallLeg)
                && isSamePeer(m_callTracking.value(m_demoCallLeg).peer, key)) {
              m_callWindow->setAvatarLetter(m_contacts[key].name);
            }
          });
  connect(m_client->chat(), &itl::ChatManager::historyLoaded, this, [this](const QString &) {
    refreshAllContactPeerColors();
    refreshAllContactPeerAvatars();
  });
  connect(m_client, &itl::CommunicatorClient::callEvent, this, &MainWindow::onCallEvent);
  connect(m_client, &itl::CommunicatorClient::serverVideoEnabledChanged, this, [this](bool) {
    applyContactViewSettings();
  });
  connect(m_client->api(), &itl::WsApiClient::domainContactsLoaded, this, &MainWindow::onContactsLoaded);
  connect(m_client->api(), &itl::WsApiClient::historyLoaded, this, &MainWindow::onServerHistoryLoaded);
  connect(m_calls, &itl::CallManager::callStateChanged, this, &MainWindow::onCallStateChanged);
  connect(m_calls, &itl::CallManager::callRecordingFinished, this, [this](const QString &path) {
    onStatusMessage(tr("Запись сохранена: %1").arg(path));
  });
  connect(m_calls, &itl::CallManager::remoteAudioStarted, this, [this](const QString &leg) {
    markCallConnected(leg);
    m_callWindow->beginConversationTimer();
  });
  connect(m_calls, &itl::CallManager::remoteAudioLevel, this, [this](float level) {
    m_callWindow->updateRemoteAudioLevel(level);
  });
  connect(m_calls, &itl::CallManager::remoteVideoFrame,
          this, [this](const QString &leg, const QImage &frame) {
            if (leg == m_activeLeg || leg == m_activeIncomingLeg) {
              m_callWindow->setRemoteVideoFrame(frame);
            }
          });
  connect(m_calls, &itl::CallManager::localVideoFrame,
          m_callWindow, &CallWindow::setLocalVideoFrame);
  connect(m_calls, &itl::CallManager::videoSendingChanged,
          this, [this](const QString &leg, bool sending) {
            if (leg == m_activeLeg || leg == m_activeIncomingLeg) {
              m_callWindow->setVideoSending(sending);
            }
          });
  connect(m_calls, &itl::CallManager::screenSharingChanged,
          this, [this](const QString &leg, bool sharing) {
            if (leg == m_activeLeg || leg == m_activeIncomingLeg) {
              m_callWindow->setScreenSharing(sharing);
            }
          });

  m_client->loadSettings();
  m_messageNotify->applySettings(&m_client->appSettings());
  mergeCustomContacts();
  updateSelfHeader();
  updateHistoryPeriodLabel();
  updateHistoryButtonStyles();
  updateDialCallButtonStyle();
  rebuildHistoryList();
  setOnlineUi(false);
  setupDragDrop();
  itl::setPopupChromeUiOpacity(100);
  refreshWallpaper();
  QTimer::singleShot(0, this, &MainWindow::startSession);
}

void MainWindow::showEvent(QShowEvent *event)
{
  QMainWindow::showEvent(event);
  if (windowState() & (Qt::WindowFullScreen | Qt::WindowMaximized)) {
    setWindowState(Qt::WindowNoState);
  }
  resize(390, 646);
  // First layout pass: overlays are 0×0 in the constructor. Re-apply without changing the
  // 100% opaque / ≤99% translucent rule (see refreshWallpaper).
  QTimer::singleShot(0, this, [this]() { refreshWallpaper(); });
}

void MainWindow::changeEvent(QEvent *event)
{
  if (event->type() == QEvent::WindowStateChange) {
    if (windowState() & (Qt::WindowFullScreen | Qt::WindowMaximized)) {
      setWindowState(Qt::WindowNoState);
      event->accept();
      return;
    }
  }
  QMainWindow::changeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
  QMainWindow::resizeEvent(event);
  if (m_wallpaperBg) {
    m_wallpaperBg->setGeometry(rect());
    m_wallpaperBg->lower();
    if (m_menuBar) {
      m_menuBar->raise();
    }
    if (QWidget *central = centralWidget()) {
      central->raise();
    }
  }
  for (QWidget *host : {static_cast<QWidget *>(m_menuBar), static_cast<QWidget *>(m_mainHeader),
                        static_cast<QWidget *>(m_mainFooter), static_cast<QWidget *>(m_tabStrip),
                        static_cast<QWidget *>(m_contactsPage), static_cast<QWidget *>(m_historyPage),
                        static_cast<QWidget *>(m_dialPage)}) {
    syncDimOverlayGeometry(host);
  }
}

void MainWindow::refreshWallpaper()
{
  if (!m_wallpaperBg) {
    return;
  }

  const QString path = m_client->appSettings().appWallpaperPath();
  QPixmap pixmap;
  if (!path.isEmpty() && QFile::exists(path)) {
    pixmap.load(path);
  }

  const bool hasWallpaper = !pixmap.isNull();
  const int opacity = m_client->appSettings().appWallpaperOpacity();
  // Hard rule: 100% = opaque native UI; 99% and below = translucent panels over wallpaper.
  const bool translucentUi = hasWallpaper && opacity < 100;

  if (hasWallpaper) {
    m_wallpaperBg->setPixmap(pixmap);
    m_wallpaperBg->setGeometry(rect());
    m_wallpaperBg->setVisible(translucentUi);
    m_wallpaperBg->lower();
    if (m_menuBar) {
      m_menuBar->raise();
    }
    if (QWidget *central = centralWidget()) {
      central->raise();
    }
  } else {
    m_wallpaperBg->clear();
    m_wallpaperBg->hide();
  }

  if (QWidget *central = centralWidget()) {
    // Transparent fill without stylesheets (parent QSS restyles all children).
    central->setStyleSheet({});
    central->setAttribute(Qt::WA_StyledBackground, false);
    if (translucentUi) {
      central->setAutoFillBackground(false);
      central->setPalette(QApplication::palette());
    } else {
      clearPanelStyle(central);
      central->setAutoFillBackground(true);
      central->setPalette(QApplication::palette());
    }
  }

  setStyleSheet({});
  setAttribute(Qt::WA_StyledBackground, false);
  if (translucentUi) {
    setAutoFillBackground(false);
    setPalette(QApplication::palette());
  } else {
    setAutoFillBackground(true);
    setPalette(QApplication::palette());
  }

  m_wallpaperActive = translucentUi;
  // Popups stay a bit more opaque than the main UI panels.
  itl::setPopupChromeUiOpacity(translucentUi ? opacity : 100);
  applyWallpaperOverlay(translucentUi);
  if (!translucentUi) {
    refreshTheme();
  }
}

void MainWindow::applyWallpaperOverlay(bool enabled)
{
  const int alpha = wallpaperAlphaFromOpacity(m_client->appSettings().appWallpaperOpacity());
  const int listAlpha = wallpaperAlphaFromOpacity(m_client->appSettings().appWallpaperListOpacity());
  const int chromeAlpha = enabled ? alpha : 255;
  const int listChromeAlpha = enabled ? listAlpha : 255;

  applyDimPanel(m_menuBar, enabled, alpha);
  if (m_menuBar) {
    if (QWidget *corner = m_menuBar->cornerWidget(Qt::TopLeftCorner)) {
      if (enabled) {
        corner->setStyleSheet({});
        corner->setAutoFillBackground(false);
        corner->setPalette(QApplication::palette());
      } else {
        clearPanelStyle(corner);
      }
    }
  }

  applyDimPanel(m_mainHeader, enabled, alpha);
  applyDimPanel(m_mainFooter, enabled, alpha);
  applyFooterButtonTransparency(m_mainFooter, enabled, alpha);
  applyDimTabs(m_tabStrip, m_tabBar, m_tabStack, m_tabBaseLine, enabled, alpha);
  // Page dim stays tied to interface opacity (toolbars/labels sit on it).
  // Lists paint their own Window fill with list opacity on top of that.
  applyDimPanel(m_contactsPage, enabled, alpha);
  applyDimPanel(m_historyPage, enabled, alpha);
  applyDimPanel(m_dialPage, enabled, alpha);
  if (!enabled) {
    // Solid Window fill on pages. Never WA_OpaquePaintEvent — rows are transparent chrome
    // and that flag skips erase → dial/history ghost under contacts at 100% opacity.
    for (QWidget *page : {m_contactsPage, m_historyPage, m_dialPage}) {
      if (!page) {
        continue;
      }
      page->setAttribute(Qt::WA_OpaquePaintEvent, false);
      page->setAutoFillBackground(true);
      page->setPalette(QApplication::palette());
    }
  }

  for (QWidget *bar : {m_contactsToolbar, m_historyToolbar, m_historyScopeBar}) {
    if (!bar) {
      continue;
    }
    if (enabled) {
      bar->setStyleSheet({});
      bar->setAutoFillBackground(false);
      bar->setAttribute(Qt::WA_OpaquePaintEvent, false);
      bar->setAttribute(Qt::WA_TranslucentBackground, false);
      bar->setAttribute(Qt::WA_NoSystemBackground, false);
      bar->setPalette(QApplication::palette());
    } else {
      clearPanelStyle(bar);
    }
  }

  applySearchEditTransparency(m_searchEdit, enabled, alpha);
  applySearchEditTransparency(m_historySearchEdit, enabled, alpha);

  if (m_headerName) {
    if (enabled) {
      m_headerName->setStyleSheet({});
      m_headerName->setAutoFillBackground(false);
      m_headerName->setPalette(QApplication::palette());
    } else {
      clearPanelStyle(m_headerName);
    }
  }
  if (m_presenceSelector) {
    if (enabled) {
      m_presenceSelector->setStyleSheet({});
      m_presenceSelector->setAutoFillBackground(false);
      m_presenceSelector->setPalette(QApplication::palette());
    } else {
      clearPanelStyle(m_presenceSelector);
    }
    // Keep the system QComboBox look at every opacity — do not restyle for wallpaper.
    m_presenceSelector->setOpaquePopup(false);
  }

  // Footer menus stay opaque (native chrome).
  applyDimmedMenu(m_addMenu, true);
  applyDimmedMenu(m_viewMenu, true);

  applyDimList(m_contactsList, enabled, listAlpha);
  applyDimList(m_historyList, enabled, listAlpha);

  for (QWidget *page : {m_dialPage, m_historyPage, m_contactsPage}) {
    if (!page) {
      continue;
    }
    for (QLabel *label : page->findChildren<QLabel *>()) {
      if (qobject_cast<ContactRowWidget *>(label->parentWidget()) ||
          qobject_cast<HistoryRowWidget *>(label->parentWidget())) {
        continue;
      }
      if (enabled) {
        label->setStyleSheet({});
        label->setAutoFillBackground(false);
        label->setPalette(QApplication::palette());
      } else {
        clearPanelStyle(label);
      }
    }
  }

  if (m_dialKeypad) {
    if (enabled) {
      m_dialKeypad->setStyleSheet({});
      m_dialKeypad->setAutoFillBackground(false);
      m_dialKeypad->setPalette(QApplication::palette());
    } else {
      clearPanelStyle(m_dialKeypad);
    }
    m_dialKeypad->setChromeAlpha(chromeAlpha);
    m_dialKeypad->refreshAppearance();
  }

  if (m_dialInput) {
    // Native line edit at every opacity (same as search fields).
    m_dialInput->setStyleSheet({});
    m_dialInput->setAttribute(Qt::WA_StyledBackground, false);
    m_dialInput->setPalette(QApplication::palette());
    if (QStyle *appStyle = QApplication::style()) {
      m_dialInput->setStyle(appStyle);
    }
    m_dialInput->style()->unpolish(m_dialInput);
    m_dialInput->style()->polish(m_dialInput);
  }

  // Keep row widgets on the app palette so they do not inherit panel alpha.
  if (m_contactsList) {
    for (int i = 0; i < m_contactsList->count(); ++i) {
      if (auto *row = qobject_cast<ContactRowWidget *>(m_contactsList->itemWidget(m_contactsList->item(i)))) {
        row->setPalette(QApplication::palette());
        row->refreshAppearance();
      }
    }
  }
  if (m_historyList) {
    for (int i = 0; i < m_historyList->count(); ++i) {
      if (auto *row = qobject_cast<HistoryRowWidget *>(m_historyList->itemWidget(m_historyList->item(i)))) {
        row->setPalette(QApplication::palette());
        row->setChromeAlpha(listChromeAlpha);
        row->refreshAppearance();
      }
    }
  }

  for (QPushButton *linkBtn : findChildren<QPushButton *>(QStringLiteral("linkButton"))) {
    applyLinkButtonStyle(linkBtn);
  }

  updateFilterButtonStyles();
  updateHistoryButtonStyles();
  updateDialCallButtonStyle();

  itl::applyNativeScrollBars(m_contactsList);
  itl::applyNativeScrollBars(m_historyList);

  // Page geometry may still be empty during the first opacity change — sync overlays next tick.
  QTimer::singleShot(0, this, [this]() {
    for (QWidget *host : {static_cast<QWidget *>(m_menuBar), static_cast<QWidget *>(m_mainHeader),
                          static_cast<QWidget *>(m_mainFooter), static_cast<QWidget *>(m_tabStrip),
                          static_cast<QWidget *>(m_contactsPage), static_cast<QWidget *>(m_historyPage),
                          static_cast<QWidget *>(m_dialPage)}) {
      syncDimOverlayGeometry(host);
    }
    update();
  });
}

void MainWindow::refreshTheme()
{
  itl::setPopupChromeUiOpacity(m_wallpaperActive ? m_client->appSettings().appWallpaperOpacity() : 100);
  if (m_historyList) {
    if (!m_wallpaperActive) {
      applyHistoryListPalette(m_historyList, palette());
    }
    for (int i = 0; i < m_historyList->count(); ++i) {
      auto *item = m_historyList->item(i);
      if (auto *row = qobject_cast<HistoryRowWidget *>(m_historyList->itemWidget(item))) {
        row->refreshAppearance();
        row->setSelected(item == m_historyList->currentItem());
      }
    }
    m_historyList->update();
    if (QWidget *viewport = m_historyList->viewport()) {
      viewport->update();
    }
  }

  if (m_contactsList && !m_wallpaperActive) {
    applyDimList(m_contactsList, false, 255);
    if (QWidget *viewport = m_contactsList->viewport()) {
      viewport->update();
    }
  }
  if (m_historyList && !m_wallpaperActive) {
    applyDimList(m_historyList, false, 255);
    if (QWidget *viewport = m_historyList->viewport()) {
      viewport->update();
    }
  }

  if (m_wallpaperActive) {
    applyWallpaperOverlay(true);
  }

  if (m_headerName && !m_wallpaperActive) {
    m_headerName->setStyleSheet({});
    m_headerName->setPalette(QApplication::palette(m_headerName));
    m_headerName->update();
  }

  if (m_presenceSelector) {
    m_presenceSelector->refreshAppearance();
  }

  if (m_headerAvatar) {
    m_headerAvatar->refreshAppearance();
  }

  if (m_callWindow) {
    m_callWindow->refreshAppearance();
  }

  if (m_chatDialog && m_chatDialog->isVisible()) {
    m_chatDialog->refreshAppearance();
  }

  for (ContactRowWidget *row : findChildren<ContactRowWidget *>()) {
    row->refreshAppearance();
  }

  if (m_dialKeypad) {
    m_dialKeypad->refreshAppearance();
  }

  updateDialCallButtonStyle();
  updateFilterButtonStyles();
  updateHistoryButtonStyles();
  for (QPushButton *linkBtn : findChildren<QPushButton *>(QStringLiteral("linkButton"))) {
    applyLinkButtonStyle(linkBtn);
  }
  update();
}

void MainWindow::setOnlineUi(bool online)
{
  m_online = online;
  m_dialInput->setEnabled(online);
  m_dialCallBtn->setEnabled(online);
  if (m_dialKeypad) {
    m_dialKeypad->setEnabled(online);
  }
  m_searchEdit->setEnabled(online);
  m_presenceSelector->setEnabled(online);
  m_contactsList->setEnabled(online);
  if (online) {
    m_presenceSelector->setManualInCallAllowed(usesOpenSourcePresence());
    m_presenceSelector->setCurrentStatus(QStringLiteral("online"));
    refreshServerHistory();
  } else {
    m_callPresenceActive = false;
    m_presenceBeforeCall.clear();
    m_presenceSelector->setManualInCallAllowed(false);
    m_presenceSelector->setInCall(false);
    m_presenceSelector->setCurrentStatus(QStringLiteral("offline"));
    m_serverHistory.clear();
    m_companyHistory.clear();
    m_internalHistory.clear();
    m_historyRequestId = -1;
    m_companyHistoryRequestId = -1;
    m_internalHistoryRequestId = -1;
    m_historyLoading = false;
    m_companyHistoryLoading = false;
    m_internalHistoryLoading = false;
  }
}

QString MainWindow::selectedPeer() const
{
  if (m_contactsList->currentItem()) {
    return m_contactsList->currentItem()->data(Qt::UserRole).toString();
  }
  return resolvePeer(m_dialInput->text().trimmed());
}

QString MainWindow::resolvePeer(QString input) const
{
  input = input.trimmed();
  if (input.isEmpty()) {
    return {};
  }

  const QString domain = m_client->credentials().domain;

  if (input.contains(QLatin1Char('@'))) {
    const int at = input.indexOf(QLatin1Char('@'));
    const QString normalizedLocal = itl::AddressBookManager::normalizePhone(input.left(at));
    if (isDialableNumber(normalizedLocal)) {
      return normalizedLocal;
    }
    return input;
  }

  const QString normalized = itl::AddressBookManager::normalizePhone(input);
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    if (it.key().startsWith(input + QLatin1Char('@')) || it.key().startsWith(normalized + QLatin1Char('@'))
        || it.value().ext == input || it.value().ext == normalized || it.value().login == input
        || it.value().login == normalized || it.value().phone == input || it.value().phone == normalized
        || (!it.value().phone.isEmpty()
            && itl::AddressBookManager::normalizePhone(it.value().phone) == normalized)) {
      return it.key();
    }
  }

  if (isDialableNumber(normalized)) {
    return normalized;
  }
  return input + QLatin1Char('@') + domain;
}

QString MainWindow::displayNameForPeer(const QString &peer) const
{
  const QString name = m_contacts.value(peer).name;
  return name.isEmpty() ? peer.section(QLatin1Char('@'), 0, 0) : name;
}

QString MainWindow::detailForPeer(const QString &peer) const
{
  const QString ext = m_contacts.value(peer).ext;
  return ext.isEmpty() ? peer : tr("%1 (Внутренний номер)").arg(ext);
}

QString MainWindow::recordingNameForPeer(const QString &peer, const QString &fallbackDisplayName) const
{
  const QString resolved = resolvePeer(peer);
  const ContactEntry entry = m_contacts.value(resolved.isEmpty() ? peer : resolved);

  if (!entry.name.isEmpty()) {
    return entry.name;
  }
  if (!fallbackDisplayName.isEmpty() && !fallbackDisplayName.contains(QLatin1Char('@'))) {
    return fallbackDisplayName;
  }

  if (!entry.ext.isEmpty()) {
    return entry.ext;
  }
  if (!entry.phone.isEmpty()) {
    return entry.phone;
  }
  if (!entry.personalPhone.isEmpty()) {
    return entry.personalPhone;
  }

  const QString target = resolved.isEmpty() ? peer : resolved;
  if (!target.contains(QLatin1Char('@'))) {
    return target;
  }
  return target.section(QLatin1Char('@'), 0, 0);
}

bool MainWindow::matchesFilterMode(const QString &peer) const
{
  if (m_sortMode != ContactSortMode::External) {
    return true;
  }

  const ContactEntry entry = m_contacts.value(peer);
  if (entry.isSelf || !entry.ext.isEmpty()) {
    return false;
  }

  if (countDigits(entry.phone) > 5) {
    return true;
  }

  return !peer.contains(QLatin1Char('@')) && countDigits(peer) > 5;
}

bool MainWindow::matchesSearch(const QString &peer) const
{
  const QString q = m_searchEdit->text().trimmed().toLower();
  if (q.isEmpty()) {
    return true;
  }
  const ContactEntry entry = m_contacts.value(peer);
  return peer.toLower().contains(q) || entry.name.toLower().contains(q) || entry.ext.contains(q)
      || entry.phone.contains(q);
}

bool MainWindow::isSamePeer(const QString &a, const QString &b) const
{
  if (a.isEmpty() || b.isEmpty()) {
    return a == b;
  }
  if (a == b) {
    return true;
  }
  const QString ra = resolvePeer(a);
  const QString rb = resolvePeer(b);
  if (!ra.isEmpty() && ra.compare(rb, Qt::CaseInsensitive) == 0) {
    return true;
  }
  return false;
}

ContactRowWidget *MainWindow::rowWidgetForPeer(const QString &peer) const
{
  if (QListWidgetItem *item = m_contactItems.value(peer)) {
    if (auto *row = qobject_cast<ContactRowWidget *>(m_contactsList->itemWidget(item))) {
      return row;
    }
  }
  for (auto it = m_contactItems.cbegin(); it != m_contactItems.cend(); ++it) {
    if (isSamePeer(it.key(), peer)) {
      return qobject_cast<ContactRowWidget *>(m_contactsList->itemWidget(it.value()));
    }
  }
  return nullptr;
}

void MainWindow::rebuildContactList()
{
  m_contactsList->clear();
  m_contactItems.clear();
  QStringList others;
  QString selfPeer;
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    if (it.value().isSelf) {
      selfPeer = it.key();
    } else if (matchesSearch(it.key()) && matchesFilterMode(it.key())) {
      others.append(it.key());
    }
  }
  std::sort(others.begin(), others.end(), [this](const QString &a, const QString &b) {
    if (m_sortMode == ContactSortMode::Recent) {
      const qint64 ta = m_client->appSettings().recentCallTime(a);
      const qint64 tb = m_client->appSettings().recentCallTime(b);
      if (ta != tb) {
        return ta > tb;
      }
    }
    return displayNameForPeer(a).localeAwareCompare(displayNameForPeer(b)) < 0;
  });
  for (const QString &peer : others) {
    addOrUpdateContactRow(peer);
  }
  if (!selfPeer.isEmpty() && matchesSearch(selfPeer) && matchesFilterMode(selfPeer)) {
    addOrUpdateContactRow(selfPeer);
  }
  if (m_contactsList) {
    if (m_wallpaperActive) {
      applyDimList(m_contactsList, true,
                   wallpaperAlphaFromOpacity(m_client->appSettings().appWallpaperListOpacity()));
    } else {
      applyDimList(m_contactsList, false, 255);
    }
    syncDimOverlayGeometry(m_contactsPage);
  }
}

void MainWindow::addOrUpdateContactRow(const QString &peer)
{
  const ContactEntry entry = m_contacts.value(peer);
  auto *row = new ContactRowWidget(peer, entry.name, entry.ext, entry.phone, entry.presence, entry.isSelf,
                                  entry.isCustom);
  if (!entry.isSelf) {
    row->setCallNumbers(callNumbersForPeer(peer));
  } else {
    row->setPhones(entry.phone, entry.personalPhone);
  }
  row->setChatButtonVisible(m_client->appSettings().showChatButtons());
  row->setCallButtonVisible(m_client->appSettings().showCallButtons());
  row->setVideoCallSupported(serverVideoUiAvailable());
  row->setVideoButtonVisible(m_client->appSettings().showVideoButtons());
  row->setUnreadBlink(m_client->appSettings().showChatButtons() && m_client->chat()->hasUnread(peer));
  const QString peerColor = entry.isSelf
      ? m_client->appSettings().profileAvatarColor()
      : m_client->chat()->peerColor(peer);
  if (!peerColor.isEmpty()) {
    row->setPeerColor(peerColor);
  }
  QPixmap peerAvatar;
  if (entry.isSelf) {
    const QString avatarPath = m_client->appSettings().profileAvatarPath();
    if (!avatarPath.isEmpty()) {
      peerAvatar.load(avatarPath);
    }
  } else {
    peerAvatar = m_client->chat()->peerAvatar(peer);
  }
  if (!peerAvatar.isNull()) {
    row->setPeerAvatar(peerAvatar);
  }
  if (!entry.isSelf && m_client->chat()->isOscPeer(peer)) {
    row->setOscPeerStyle(true);
  }
  auto *item = new QListWidgetItem;
  item->setData(Qt::UserRole, peer);
  // Fixed slot height matching the row widget — prevents empty gap under the last
  // contact without collapsing item heights (which broke scrolling).
  const int rowHeight = (entry.ext.isEmpty() && entry.phone.isEmpty()) ? 48 : 56;
  row->setFixedHeight(rowHeight);
  item->setSizeHint(QSize(0, rowHeight));
  m_contactsList->addItem(item);
  m_contactsList->setItemWidget(item, row);
  m_contactItems.insert(peer, item);
  connect(row, &ContactRowWidget::callRequested, this, &MainWindow::onCallFromRow);
  connect(row, &ContactRowWidget::videoCallRequested, this, &MainWindow::onVideoCallFromRow);
  connect(row, &ContactRowWidget::callNumberRequested, this, [this](const QString &number) {
    const QString target = resolvePeer(number);
    if (!target.isEmpty()) {
      onCallFromRow(target);
    }
  });
  connect(row, &ContactRowWidget::chatRequested, this, &MainWindow::onChatFromRow);
  connect(row, &ContactRowWidget::notesRequested, this, &MainWindow::onNotesFromRow);
  connect(row, &ContactRowWidget::deleteRequested, this, &MainWindow::onDeleteContactFromRow);
  connect(row, &ContactRowWidget::exportRequested, this, &MainWindow::onExportContactFromRow);
  connect(row, &ContactRowWidget::copyNumberRequested, this, [](const QString &number) {
    QApplication::clipboard()->setText(number);
  });
}

QVector<ContactRowWidget::CallNumber> MainWindow::callNumbersForPeer(const QString &peer) const
{
  const ContactEntry entry = m_contacts.value(peer);
  QVector<ContactRowWidget::CallNumber> numbers;
  if (!entry.ext.isEmpty()) {
    numbers.append({tr("Внутренний номер"), entry.ext});
  }
  if (!entry.phone.isEmpty()) {
    numbers.append({tr("Мобильный номер"), entry.phone});
  }
  if (!entry.personalPhone.isEmpty() && entry.personalPhone != entry.phone) {
    numbers.append({tr("Личный мобильный"), entry.personalPhone});
  }
  return numbers;
}

void MainWindow::updateSelfHeader()
{
  if (!m_selfName.isEmpty()) {
    m_headerName->setText(m_selfName);
    m_headerAvatar->setLetter(m_selfName.left(1).toUpper());
  } else {
    const QString login = m_client->credentials().login.section(QLatin1Char('@'), 0, 0);
    m_headerName->setText(login);
    m_headerAvatar->setLetter(login.left(1).toUpper());
  }
  m_headerAvatar->refreshFromSettings();
}

void MainWindow::enterCallPresence()
{
  if (m_callPresenceActive) {
    return;
  }

  m_callPresenceActive = true;
  if (m_presenceSelector && !m_demoMode) {
    m_presenceBeforeCall = m_presenceSelector->currentStatus();
    if (m_presenceBeforeCall == QStringLiteral("in-call")) {
      m_presenceBeforeCall = QStringLiteral("online");
    }
  } else {
    m_presenceBeforeCall = QStringLiteral("online");
  }

  if (m_presenceSelector) {
    const QSignalBlocker blocker(m_presenceSelector);
    m_presenceSelector->setInCall(true);
  }
  if (!m_selfPeer.isEmpty()) {
    m_contacts[m_selfPeer].presence = QStringLiteral("in-call");
  }
  if (m_online && !m_demoMode && usesOpenSourcePresence()) {
    m_client->api()->setOwnPresence(QStringLiteral("in-call"), true);
  }
  // Megafon PBX sets voice:"in-call" automatically; SetPresence(in-call) is rejected (971).
}

void MainWindow::leaveCallPresence()
{
  if (!m_callPresenceActive) {
    return;
  }
  if (m_calls && m_calls->hasActiveCalls()) {
    return;
  }
  if (m_demoMode && !m_demoCallLeg.isEmpty()) {
    return;
  }

  m_callPresenceActive = false;
  const QString restore =
      m_presenceBeforeCall.isEmpty() || m_presenceBeforeCall == QStringLiteral("in-call")
          ? QStringLiteral("online")
          : m_presenceBeforeCall;
  m_presenceBeforeCall.clear();

  if (m_presenceSelector) {
    const QSignalBlocker blocker(m_presenceSelector);
    m_presenceSelector->setInCall(false);
    m_presenceSelector->setCurrentStatus(restore);
  }
  if (!m_selfPeer.isEmpty()) {
    m_contacts[m_selfPeer].presence = restore;
  }
  if (m_online && !m_demoMode) {
    m_client->api()->setOwnPresence(restore, true);
  }
}

bool MainWindow::usesOpenSourcePresence() const
{
  if (!m_client) {
    return false;
  }
  return m_client->credentials().partner.trimmed().compare(QStringLiteral("opensource"), Qt::CaseInsensitive) == 0;
}

void MainWindow::onFilterChanged(int id)
{
  m_sortMode = static_cast<ContactSortMode>(id);
  updateFilterButtonStyles();
  rebuildContactList();
}

void MainWindow::updateDialCallButtonStyle()
{
  applyNativePushButton(m_dialCallBtn);
}

void MainWindow::updateFilterButtonStyles()
{
  if (!m_filterGroup) {
    return;
  }

  for (QAbstractButton *button : m_filterGroup->buttons()) {
    auto *filterBtn = qobject_cast<QPushButton *>(button);
    if (!filterBtn) {
      continue;
    }
    // Drive by current mode — more reliable than isChecked() after theme/wallpaper restyles.
    const bool checked = m_filterGroup->id(filterBtn) == static_cast<int>(m_sortMode);
    if (filterBtn->isChecked() != checked) {
      QSignalBlocker blocker(m_filterGroup);
      filterBtn->setChecked(checked);
    }
    applyCheckableFilterButtonStyle(filterBtn, checked);
  }
}

void MainWindow::onProfileAvatarChanged()
{
  m_client->saveSettings();
  m_headerAvatar->refreshFromSettings();
  if (ContactRowWidget *selfRow = rowWidgetForPeer(m_selfPeer)) {
    selfRow->setPeerColor(m_client->appSettings().profileAvatarColor());
    QPixmap avatar;
    const QString avatarPath = m_client->appSettings().profileAvatarPath();
    if (!avatarPath.isEmpty()) {
      avatar.load(avatarPath);
    }
    selfRow->setPeerAvatar(avatar);
  }
  syncSelfOscShareProfile();
  if (m_online) {
    refreshColorAdvertisementPeers();
    m_client->chat()->sendColorAdvertisement(m_client->appSettings().profileAvatarColor());
  }
}

void MainWindow::loadCallNotes(const QString &peer)
{
  m_callWindow->setNotesText(m_client->appSettings().noteForPeer(peer));
}

void MainWindow::recordCallForPeer(const QString &peer)
{
  const QString resolved = resolvePeer(peer);
  if (resolved.isEmpty()) {
    return;
  }
  m_client->appSettings().recordRecentCall(resolved);
  if (m_sortMode == ContactSortMode::Recent) {
    rebuildContactList();
  }
}

QString MainWindow::formatHistoryDuration(int seconds)
{
  if (seconds <= 0) {
    return QStringLiteral("—");
  }
  return QStringLiteral("%1:%2")
      .arg(seconds / 60)
      .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString MainWindow::formatHistoryTime(qint64 ms)
{
  if (ms <= 0) {
    return {};
  }
  return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("dd.MM.yy HH:mm"));
}

QString MainWindow::formatHistoryWhen(qint64 ms)
{
  if (ms <= 0) {
    return {};
  }
  const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
  const QDate today = QDate::currentDate();
  const QString time = dt.toString(QStringLiteral("HH:mm"));
  if (dt.date() == today) {
    return tr("сегодня, %1").arg(time);
  }
  if (dt.date() == today.addDays(-1)) {
    return tr("вчера, %1").arg(time);
  }
  return dt.toString(QStringLiteral("dd.MM.yy, HH:mm"));
}

void MainWindow::onHistoryDirChanged(int id)
{
  m_historyDir = static_cast<HistoryDir>(id);
  updateHistoryButtonStyles();
  refreshServerHistory();
  rebuildHistoryList();
}

void MainWindow::onHistoryScopeChanged(int id)
{
  m_historyScope = static_cast<HistoryScope>(id);
  updateHistoryButtonStyles();
  refreshServerHistory();
  rebuildHistoryList();
}

void MainWindow::onHistorySearchChanged(const QString &text)
{
  m_historySearch = text.trimmed();
  refreshServerHistory();
  rebuildHistoryList();
}

void MainWindow::applyLinkButtonStyle(QPushButton *button) const
{
  if (!button) {
    return;
  }
  QPalette pal = QApplication::palette(button);
  pal.setColor(QPalette::Button, Qt::transparent);
  pal.setColor(QPalette::ButtonText, pal.color(QPalette::Link));
  button->setPalette(pal);
  button->setStyleSheet({});
  button->style()->unpolish(button);
  button->style()->polish(button);
}

void MainWindow::onHistoryPeriodClicked()
{
  QMenu menu(this);
  itl::applyPopupMenuStyle(&menu);
  const struct {
    const char *label;
    HistoryPeriod period;
  } items[] = {
      {QT_TR_NOOP("сегодня"), HistoryPeriod::Today},
      {QT_TR_NOOP("текущую неделю"), HistoryPeriod::Week},
      {QT_TR_NOOP("текущий месяц"), HistoryPeriod::Month},
      {QT_TR_NOOP("всё время"), HistoryPeriod::AllTime},
  };
  for (const auto &item : items) {
    QAction *action = menu.addAction(tr(item.label));
    QFont font = action->font();
    font.setBold(item.period == m_historyPeriod);
    action->setFont(font);
    const HistoryPeriod period = item.period;
    connect(action, &QAction::triggered, this, [this, period]() {
      m_historyPeriod = period;
      updateHistoryPeriodLabel();
      refreshServerHistory();
      rebuildHistoryList();
    });
  }
  menu.exec(m_historyPeriodBtn->mapToGlobal(QPoint(0, m_historyPeriodBtn->height())));
}

void MainWindow::updateHistoryPeriodLabel()
{
  if (!m_historyPeriodBtn) {
    return;
  }
  QString text;
  switch (m_historyPeriod) {
  case HistoryPeriod::Today:
    text = tr("сегодня");
    break;
  case HistoryPeriod::Week:
    text = tr("текущую неделю");
    break;
  case HistoryPeriod::Month:
    text = tr("текущий месяц");
    break;
  case HistoryPeriod::AllTime:
    text = tr("всё время");
    break;
  }
  m_historyPeriodBtn->setText(text);
  applyLinkButtonStyle(m_historyPeriodBtn);
}

QJsonObject MainWindow::buildHistoryRequest(HistoryScope scope) const
{
  QJsonObject request{
      {QString::fromUtf8(itl::kEmptyKey), QStringLiteral("gethistory")},
      {QStringLiteral("CallType"), QStringLiteral("all")},
      {QStringLiteral("splitout"), 1},
      {QStringLiteral("Limit"), 100},
  };

  switch (m_historyDir) {
  case HistoryDir::Incoming:
    request.insert(QStringLiteral("CallType"), QStringLiteral("in"));
    break;
  case HistoryDir::Outgoing:
    request.insert(QStringLiteral("CallType"), QStringLiteral("out"));
    break;
  case HistoryDir::Missed:
    request.insert(QStringLiteral("CallType"), QStringLiteral("missed"));
    break;
  case HistoryDir::All:
    break;
  }

  switch (scope) {
  case HistoryScope::Mine:
    // Только звонки текущего пользователя.
    request.insert(QStringLiteral("owner"), QStringLiteral("my"));
    break;
  case HistoryScope::Company:
    // Все звонки сотрудников компании.
    break;
  case HistoryScope::Internal:
    // Внутренние звонки — отдельный запрос, как в официальном клиенте.
    request.insert(QStringLiteral("inner"), true);
    request.insert(QStringLiteral("owner"), QStringLiteral("my"));
    break;
  }

  const QDateTime now = QDateTime::currentDateTimeUtc();
  QDateTime start = now;
  switch (m_historyPeriod) {
  case HistoryPeriod::Today:
    start = QDateTime(QDate(now.date()), QTime(0, 0), Qt::UTC);
    break;
  case HistoryPeriod::Week:
    start = QDateTime(now.date().addDays(-(now.date().dayOfWeek() - 1)), QTime(0, 0), Qt::UTC);
    break;
  case HistoryPeriod::Month:
    start = QDateTime(QDate(now.date().year(), now.date().month(), 1), QTime(0, 0), Qt::UTC);
    break;
  case HistoryPeriod::AllTime:
    start = QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC);
    break;
  }
  request.insert(QStringLiteral("start"), start.toString(Qt::ISODateWithMs));
  request.insert(QStringLiteral("end"), now.toString(Qt::ISODateWithMs));

  if (!m_historySearch.isEmpty()) {
    request.insert(QStringLiteral("search"), QJsonObject{{QStringLiteral("query"), m_historySearch}});
  }

  return request;
}

void MainWindow::refreshServerHistory()
{
  if (m_demoMode || !m_online || m_client->api()->appState() != itl::AppState::Online) {
    return;
  }

  if (m_historyScope == HistoryScope::Company && !m_companyHistory.isEmpty()) {
    m_historyLoading = false;
    rebuildHistoryList();
    return;
  }

  if (m_historyScope == HistoryScope::Internal && !m_internalHistory.isEmpty()) {
    m_historyLoading = false;
    rebuildHistoryList();
    return;
  }

  m_historyRequestScope = m_historyScope;
  const int requestId = m_client->api()->getHistory(buildHistoryRequest(m_historyRequestScope));
  if (requestId >= 0) {
    m_historyRequestId = requestId;
    m_historyLoading = true;
    rebuildHistoryList();
  } else {
    m_historyLoading = false;
  }
}

void MainWindow::prefetchCompanyHistory()
{
  if (m_demoMode || !m_online || m_client->api()->appState() != itl::AppState::Online) {
    return;
  }
  if (!m_companyHistory.isEmpty() || m_companyHistoryLoading) {
    return;
  }

  const int requestId = m_client->api()->getHistory(buildHistoryRequest(HistoryScope::Company));
  if (requestId >= 0) {
    m_companyHistoryRequestId = requestId;
    m_companyHistoryLoading = true;
  }
}

void MainWindow::prefetchInternalHistory()
{
  if (m_demoMode || !m_online || m_client->api()->appState() != itl::AppState::Online) {
    return;
  }
  if (!m_internalHistory.isEmpty() || m_internalHistoryLoading) {
    return;
  }

  const int requestId = m_client->api()->getHistory(buildHistoryRequest(HistoryScope::Internal));
  if (requestId >= 0) {
    m_internalHistoryRequestId = requestId;
    m_internalHistoryLoading = true;
  }
}

void MainWindow::onServerHistoryLoaded(int requestId, const QJsonObject &response)
{
  enum class Bucket { None, Mine, Company, Internal };
  Bucket bucket = Bucket::None;
  if (requestId == m_companyHistoryRequestId) {
    bucket = Bucket::Company;
  } else if (requestId == m_internalHistoryRequestId) {
    bucket = Bucket::Internal;
  } else if (requestId == m_historyRequestId) {
    if (m_historyRequestScope == HistoryScope::Company) {
      bucket = Bucket::Company;
    } else if (m_historyRequestScope == HistoryScope::Internal) {
      bucket = Bucket::Internal;
    } else {
      bucket = Bucket::Mine;
    }
  }
  if (bucket == Bucket::None) {
    return;
  }

  const QJsonObject inner = response.value(QString::fromUtf8(itl::kEmptyKey)).toObject();
  const QJsonObject payload = inner.value(QStringLiteral("response")).toObject();

  auto finishLoading = [&](Bucket target) {
    switch (target) {
    case Bucket::Company:
      m_companyHistoryLoading = false;
      m_companyHistoryRequestId = -1;
      break;
    case Bucket::Internal:
      m_internalHistoryLoading = false;
      m_internalHistoryRequestId = -1;
      break;
    case Bucket::Mine:
      m_historyLoading = false;
      m_historyRequestId = -1;
      break;
    default:
      break;
    }
  };

  if (payload.contains(QStringLiteral("error"))) {
    qCWarning(lcHistory) << "gethistory error:" << payload.value(QStringLiteral("error")).toString()
                         << "bucket" << static_cast<int>(bucket);
    if (bucket == Bucket::Company) {
      m_companyHistory.clear();
    } else if (bucket == Bucket::Internal) {
      m_internalHistory.clear();
    } else {
      m_serverHistory.clear();
    }
    finishLoading(bucket);
    rebuildHistoryList();
    return;
  }

  const QJsonObject result = payload.value(QStringLiteral("result")).toObject();
  itl::CallHistoryParseContext context;
  context.domain = m_client->credentials().domain;
  context.selfPeer = m_selfPeer;
  context.selfLogin = m_client->credentials().login.section(QLatin1Char('@'), 0, 0);
  const QList<itl::CallHistoryEntry> parsed =
      itl::parseServerCallHistory(result.value(QStringLiteral("Calls")), context);

  if (bucket == Bucket::Company) {
    m_companyHistory = parsed;
    qCInfo(lcHistory) << "Company history loaded:" << parsed.size() << "entries";
  } else if (bucket == Bucket::Internal) {
    m_internalHistory = parsed;
    qCInfo(lcHistory) << "Internal history loaded:" << parsed.size() << "entries";
  } else {
    m_serverHistory = parsed;
    qCInfo(lcHistory) << "Mine history loaded:" << parsed.size() << "entries";
  }

  finishLoading(bucket);
  rebuildHistoryList();
}

QList<itl::CallHistoryEntry> MainWindow::currentHistoryEntries() const
{
  if (m_demoMode) {
    return m_demoCallHistory;
  }
  if (m_historyScope == HistoryScope::Company) {
    return m_companyHistory;
  }
  if (m_historyScope == HistoryScope::Internal) {
    if (!m_internalHistory.isEmpty()) {
      return m_internalHistory;
    }
    // Запасной вариант: отбор коротких номеров из истории компании.
    return m_companyHistory;
  }
  if (m_online && m_client->api()->appState() == itl::AppState::Online && !m_serverHistory.isEmpty()) {
    return m_serverHistory;
  }
  if (m_historyScope == HistoryScope::Mine) {
    const QList<itl::CallHistoryEntry> local = m_client->appSettings().callHistory();
    if (!local.isEmpty()) {
      return local;
    }
  }
  return m_serverHistory;
}

bool MainWindow::historyEntryMatches(const itl::CallHistoryEntry &entry) const
{
  const bool incoming = entry.direction == QStringLiteral("incoming");
  switch (m_historyDir) {
  case HistoryDir::All:
    break;
  case HistoryDir::Incoming:
    if (!incoming) {
      return false;
    }
    break;
  case HistoryDir::Outgoing:
    if (incoming) {
      return false;
    }
    break;
  case HistoryDir::Missed:
    if (!(incoming && !entry.answered)) {
      return false;
    }
    break;
  }

  const QString domain = m_client->credentials().domain;
  switch (m_historyScope) {
  case HistoryScope::Mine:
    break;
  case HistoryScope::Company:
    break;
  case HistoryScope::Internal:
    // Данные уже с сервера (inner:true); дополнительно отсеиваем не-короткие номера.
    if (!entry.isInnerCall && !itl::historyEntryIsInternal(entry, domain)) {
      return false;
    }
    break;
  }

  const QDateTime now = QDateTime::currentDateTime();
  const QDateTime started = QDateTime::fromMSecsSinceEpoch(entry.startedAtMs);
  switch (m_historyPeriod) {
  case HistoryPeriod::Today:
    if (started.date() != now.date()) {
      return false;
    }
    break;
  case HistoryPeriod::Week: {
    QDate weekStart = now.date().addDays(-(now.date().dayOfWeek() - 1));
    if (started.date() < weekStart) {
      return false;
    }
    break;
  }
  case HistoryPeriod::Month:
    if (started.date().year() != now.date().year()
        || started.date().month() != now.date().month()) {
      return false;
    }
    break;
  case HistoryPeriod::AllTime:
    break;
  }

  if (!m_historySearch.isEmpty()) {
    const QString name = entry.displayName.isEmpty() ? entry.peer : entry.displayName;
    if (!name.contains(m_historySearch, Qt::CaseInsensitive)
        && !entry.peer.contains(m_historySearch, Qt::CaseInsensitive)) {
      return false;
    }
  }

  return true;
}

void MainWindow::rebuildHistoryList()
{
  m_historyList->clear();
  const QList<itl::CallHistoryEntry> history = currentHistoryEntries();

  if ((m_historyLoading || m_companyHistoryLoading || m_internalHistoryLoading) && history.isEmpty()
      && m_online && !m_demoMode) {
    auto *placeholder = new QListWidgetItem(tr("Загрузка истории..."));
    placeholder->setFlags(Qt::NoItemFlags);
    m_historyList->addItem(placeholder);
    return;
  }

  int shown = 0;
  for (const itl::CallHistoryEntry &entry : history) {
    if (!historyEntryMatches(entry)) {
      continue;
    }
    ++shown;

    const bool incoming = entry.direction == QStringLiteral("incoming");
    const bool missed = incoming && !entry.answered;
    const QString arrow = incoming ? QStringLiteral("↙") : QStringLiteral("↗");
    QString arrowColor;
    if (missed) {
      arrowColor = QStringLiteral("#c0392b");
    } else if (incoming) {
      arrowColor = QStringLiteral("#27ae60");
    } else {
      arrowColor = QStringLiteral("#2b7bd6");
    }

    // Время ожидания ответа (гудки).
    qint64 waitMs = 0;
    if (entry.answered && entry.connectedAtMs > 0) {
      waitMs = entry.connectedAtMs - entry.startedAtMs;
    } else if (entry.endedAtMs > 0) {
      waitMs = entry.endedAtMs - entry.startedAtMs;
    }
    const int waitSec = waitMs > 0 ? static_cast<int>((waitMs + 500) / 1000) : 0;

    QString status;
    if (entry.result == QStringLiteral("transferred") && !entry.transferTo.isEmpty()) {
      status = tr("переведён на %1").arg(entry.transferTo);
    } else if (entry.answered && entry.durationSec > 0) {
      status = tr("%1 сек.").arg(entry.durationSec);
    } else if (missed) {
      status = tr("пропущенный");
    } else if (!incoming && !entry.answered) {
      status = tr("не отвечено");
    }

    const QString name = entry.displayName.isEmpty() ? entry.peer : entry.displayName;

    // Layout like contacts: title on top, number/details below in Link color.
    const QString firstLine = name;

    QStringList detailParts;
    if (!entry.displayName.isEmpty() && entry.displayName != entry.peer && !entry.peer.isEmpty()) {
      detailParts.append(entry.peer);
    } else if (m_historyScope == HistoryScope::Company && !entry.employeeInfo.isEmpty()) {
      detailParts.append(entry.employeeInfo);
    }
    if (waitSec > 0) {
      detailParts.append(tr("ждал: %1 сек.").arg(waitSec));
    }
    if (!status.isEmpty()) {
      detailParts.append(status);
    }
    const QString secondLine = detailParts.join(QStringLiteral("; "));

    auto *rowWidget = new HistoryRowWidget(entry.peer, name, firstLine, secondLine,
                                           formatHistoryWhen(entry.startedAtMs), arrow, arrowColor, missed);
    rowWidget->setChromeAlpha(m_wallpaperActive
                                  ? wallpaperAlphaFromOpacity(m_client->appSettings().appWallpaperListOpacity())
                                  : 255);
    connect(rowWidget, &HistoryRowWidget::callRequested, this, &MainWindow::onCallFromRow);
    connect(rowWidget, &HistoryRowWidget::chatRequested, this, &MainWindow::onChatFromRow);
    connect(rowWidget, &HistoryRowWidget::notesRequested, this, [this](const QString &peer) {
      for (int i = 0; i < m_historyList->count(); ++i) {
        auto *listItem = m_historyList->item(i);
        if (listItem && listItem->data(Qt::UserRole).toString() == peer) {
          onHistoryItemActivated(listItem);
          return;
        }
      }
    });

    auto *item = new QListWidgetItem;
    item->setData(Qt::UserRole, entry.peer);
    item->setData(Qt::UserRole + 1, name);
    const int rowHeight = secondLine.isEmpty() ? 48 : 56;
    rowWidget->setFixedHeight(rowHeight);
    item->setSizeHint(QSize(0, rowHeight));
    m_historyList->addItem(item);
    m_historyList->setItemWidget(item, rowWidget);
  }

  if (shown == 0) {
    auto *placeholder = new QListWidgetItem(history.isEmpty()
                                                ? tr("История звонков пуста")
                                                : tr("Нет звонков по выбранному фильтру"));
    placeholder->setFlags(Qt::NoItemFlags);
    m_historyList->addItem(placeholder);
  }
  if (m_historyList) {
    const int alpha = wallpaperAlphaFromOpacity(m_client->appSettings().appWallpaperListOpacity());
    if (m_wallpaperActive) {
      applyDimList(m_historyList, true, alpha);
      syncDimOverlayGeometry(m_historyPage);
      for (int i = 0; i < m_historyList->count(); ++i) {
        if (auto *row = qobject_cast<HistoryRowWidget *>(m_historyList->itemWidget(m_historyList->item(i)))) {
          row->setChromeAlpha(alpha);
          row->refreshAppearance();
        }
      }
    } else {
      applyDimList(m_historyList, false, 255);
      syncDimOverlayGeometry(m_historyPage);
      for (int i = 0; i < m_historyList->count(); ++i) {
        if (auto *row = qobject_cast<HistoryRowWidget *>(m_historyList->itemWidget(m_historyList->item(i)))) {
          row->setChromeAlpha(255);
          row->refreshAppearance();
        }
      }
    }
  }
}

void MainWindow::runHistorySelfTest()
{
  auto countForScope = [this](HistoryScope scope) {
    const HistoryScope saved = m_historyScope;
    m_historyScope = scope;
    int shown = 0;
    for (const itl::CallHistoryEntry &entry : currentHistoryEntries()) {
      if (historyEntryMatches(entry)) {
        ++shown;
      }
    }
    m_historyScope = saved;
    return shown;
  };

  const int mine = countForScope(HistoryScope::Mine);
  const int company = countForScope(HistoryScope::Company);
  const int internal = countForScope(HistoryScope::Internal);

  qCInfo(lcHistory) << "SELFTEST demo=" << m_demoMode << "mine=" << mine << "company=" << company
                    << "internal=" << internal << "demoEntries=" << m_demoCallHistory.size();

  const bool ok = m_demoMode && mine == 4 && company == 4 && internal == 1;
  if (!ok) {
    qCCritical(lcHistory) << "SELFTEST FAILED";
    QApplication::exit(1);
    return;
  }

  qCInfo(lcHistory) << "SELFTEST OK";
  QApplication::exit(0);
}

void MainWindow::updateHistoryButtonStyles()
{
  for (QButtonGroup *group : {m_historyDirGroup, m_historyScopeGroup}) {
    if (!group) {
      continue;
    }
    const int checkedId = group->checkedId();
    for (QAbstractButton *button : group->buttons()) {
      auto *btn = qobject_cast<QPushButton *>(button);
      if (!btn) {
        continue;
      }
      const bool checked = group->id(btn) == checkedId;
      applyCheckableFilterButtonStyle(btn, checked);
    }
  }
}

void MainWindow::beginCallTracking(const QString &leg, const QString &peer, const QString &displayName, bool incoming)
{
  if (leg.isEmpty() || peer.isEmpty()) {
    return;
  }
  CallTracking tracking;
  tracking.peer = peer;
  tracking.displayName = displayName.isEmpty() ? displayNameForPeer(peer) : displayName;
  tracking.incoming = incoming;
  tracking.startedAtMs = QDateTime::currentMSecsSinceEpoch();
  m_callTracking.insert(leg, tracking);
  m_calls->setRecordingName(leg, recordingNameForPeer(peer, displayName));
}

void MainWindow::markCallConnected(const QString &leg)
{
  if (!m_callTracking.contains(leg) || m_callTracking[leg].connectedAtMs > 0) {
    return;
  }
  m_callTracking[leg].connectedAtMs = QDateTime::currentMSecsSinceEpoch();
}

void MainWindow::resumeExternalMediaIfIdle()
{
  if (m_demoMode) {
    if (m_demoCallLeg.isEmpty()) {
      m_calls->resumeExternalMedia();
    }
    return;
  }
  if (m_calls->hasActiveCalls() || !m_activeLeg.isEmpty() || !m_activeIncomingLeg.isEmpty()) {
    return;
  }
  m_calls->resumeExternalMedia();
}

void MainWindow::finalizeCallHistory(const QString &leg, const QString &state, const QString &transferTo)
{
  if (!m_callTracking.contains(leg)) {
    return;
  }

  const CallTracking tracking = m_callTracking.take(leg);
  itl::CallHistoryEntry entry;
  entry.peer = tracking.peer;
  entry.displayName = tracking.displayName;
  entry.direction = tracking.incoming ? QStringLiteral("incoming") : QStringLiteral("outgoing");
  entry.startedAtMs = tracking.startedAtMs;
  entry.connectedAtMs = tracking.connectedAtMs;
  entry.endedAtMs = QDateTime::currentMSecsSinceEpoch();
  entry.answered = tracking.connectedAtMs > 0;
  entry.durationSec = entry.answered ? static_cast<int>((entry.endedAtMs - entry.connectedAtMs) / 1000) : 0;
  entry.result = state;

  if (state == QStringLiteral("transferred")) {
    entry.result = QStringLiteral("transferred");
    entry.transferTo = transferTo;
    if (entry.transferTo.isEmpty()) {
      entry.transferTo = tr("контакт");
    }
  } else if (!entry.answered && tracking.incoming) {
    entry.result = QStringLiteral("missed");
  } else if (!entry.answered) {
    entry.result = QStringLiteral("unanswered");
  }

  if (m_demoMode) {
    m_demoCallHistory.prepend(entry);
    while (m_demoCallHistory.size() > 50) {
      m_demoCallHistory.removeLast();
    }
    rebuildHistoryList();
    return;
  }

  m_client->appSettings().addCallHistoryEntry(entry);
  recordCallForPeer(tracking.peer);
  rebuildHistoryList();
}

void MainWindow::onCallNotesChanged(const QString &peer, const QString &text)
{
  m_client->appSettings().setNoteForPeer(peer, text);
}

void MainWindow::onNotesFromRow(const QString &peer)
{
  const bool duringCall = !m_activeLeg.isEmpty();
  const bool activeCallPeer = duringCall && isSamePeer(peer, m_callWindow->peer());

  NotePopupDialog dlg(peer, displayNameForPeer(peer), &m_client->appSettings(), this);
  dlg.setShowCallAction(true);
  dlg.setDuringCall(activeCallPeer);
  const int result = dlg.exec();
  if (result == NotePopupDialog::CallResult) {
    onCallFromRow(peer);
    return;
  }
  if (activeCallPeer) {
    loadCallNotes(peer);
    m_callWindow->setNotesVisible(true);
  }
}

void MainWindow::onHistoryItemActivated(QListWidgetItem *item)
{
  if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
    return;
  }

  const QString peer = item->data(Qt::UserRole).toString();
  if (peer.isEmpty()) {
    return;
  }

  QString displayName = displayNameForPeer(peer);
  if (displayName == peer.section(QLatin1Char('@'), 0, 0) || displayName == peer) {
    // Prefer the name stored with the history entry when contact book has no RealName.
    for (const itl::CallHistoryEntry &entry : currentHistoryEntries()) {
      if (entry.peer == peer && !entry.displayName.isEmpty()) {
        displayName = entry.displayName;
        break;
      }
    }
  }

  NotePopupDialog dlg(peer, displayName, &m_client->appSettings(), this);
  dlg.setShowCallAction(true);
  const bool duringCall = !m_activeLeg.isEmpty();
  const bool activeCallPeer = duringCall && isSamePeer(peer, m_callWindow->peer());
  dlg.setDuringCall(activeCallPeer);
  const int result = dlg.exec();
  if (result == NotePopupDialog::CallResult) {
    onCallFromRow(peer);
    return;
  }
  if (activeCallPeer) {
    loadCallNotes(peer);
    m_callWindow->setNotesVisible(true);
  }
}

void MainWindow::onDeleteContactFromRow(const QString &peer)
{
  const ContactEntry entry = m_contacts.value(peer);
  if (!entry.isCustom || entry.isSelf) {
    return;
  }

  const QString name = entry.name.isEmpty() ? peer.section(QLatin1Char('@'), 0, 0) : entry.name;
  if (QMessageBox::question(this, tr("Удалить контакт"),
                            tr("Удалить контакт «%1»?").arg(name))
      != QMessageBox::Yes) {
    return;
  }

  if (useServerContacts()) {
    if (m_client->addressBook()->deleteContactByPeer(peer) < 0) {
      QMessageBox::warning(this, tr("Удалить контакт"), tr("Не удалось удалить контакт на сервере."));
      return;
    }
    return;
  }

  m_client->appSettings().removeCustomContact(peer);
  m_client->saveSettings();
  m_contacts.remove(peer);
  rebuildContactList();
}

void MainWindow::onExportContactFromRow(const QString &peer)
{
  const ContactEntry entry = m_contacts.value(peer);
  const QString displayName = entry.name.isEmpty() ? peer.section(QLatin1Char('@'), 0, 0) : entry.name;
  QString defaultBase = displayName.trimmed();
  defaultBase.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));

  const QString path = QFileDialog::getSaveFileName(
      this, tr("Экспорт контакта"), defaultBase,
      tr("vCard (*.vcf);;CSV (*.csv)"));
  if (path.isEmpty()) {
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Экспорт"), tr("Не удалось создать файл."));
    return;
  }

  QTextStream out(&file);
  if (path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
    out << displayName << QLatin1Char(',') << entry.phone << QLatin1Char(',') << entry.ext << QLatin1Char('\n');
  } else {
    out << QStringLiteral("BEGIN:VCARD\n");
    out << QStringLiteral("VERSION:3.0\n");
    out << QStringLiteral("FN:") << displayName << QLatin1Char('\n');
    if (!entry.ext.isEmpty()) {
      out << QStringLiteral("TEL;TYPE=WORK:") << entry.ext << QLatin1Char('\n');
    }
    if (!entry.phone.isEmpty()) {
      out << QStringLiteral("TEL;TYPE=CELL:") << entry.phone << QLatin1Char('\n');
    }
    if (!entry.personalPhone.isEmpty()) {
      out << QStringLiteral("TEL;TYPE=HOME:") << entry.personalPhone << QLatin1Char('\n');
    }
    out << QStringLiteral("END:VCARD\n");
  }

  file.close();
  onStatusMessage(tr("Контакт экспортирован: %1").arg(path));
}

void MainWindow::startSession()
{
  m_client->loadSettings();

  if (qEnvironmentVariableIsSet("OSC_SELFTEST")) {
    itl::LoginCredentials demoCred;
    demoCred.login = QStringLiteral("demo");
    demoCred.password = QStringLiteral("demo");
    m_client->setCredentials(demoCred);
    beginSessionWithCurrentCredentials();
    QTimer::singleShot(300, this, &MainWindow::runHistorySelfTest);
    return;
  }

  if (m_client->rememberMe()) {
    const itl::LoginCredentials cred = m_client->credentials();
    const bool hasAccount = !cred.login.trimmed().isEmpty() && !cred.password.isEmpty()
        && !itl::DemoData::isDemoCredentials(cred.login, cred.password);
    if (hasAccount) {
      beginSessionWithCurrentCredentials();
      return;
    }
  }

  onLogin();
}

void MainWindow::beginSessionWithCurrentCredentials()
{
  if (m_demoMode) {
    exitDemoInterface();
  }

  m_contacts.clear();
  m_contactItems.clear();
  m_contactsList->clear();

  const itl::LoginCredentials cred = m_client->credentials();
  if (itl::DemoData::isDemoCredentials(cred.login, cred.password)) {
    itl::LoginCredentials demoCred = cred;
    demoCred.login = QStringLiteral("demo@") + itl::DemoData::demoDomain();
    demoCred.password = QStringLiteral("demo");
    demoCred.domain = itl::DemoData::demoDomain();
    demoCred.authDomain.clear();
    demoCred.serverPort = 0;
    demoCred.ignoreInsecureTls = false;
    m_client->setCredentials(demoCred);
    m_client->enterDemoMode();
    enterDemoInterface();
    return;
  }

  m_client->login();
}

void MainWindow::onLogin()
{
  if (m_demoMode) {
    exitDemoInterface();
  }

  LoginDialog dlg(m_client, this);
  m_client->loadSettings();
  dlg.loadFromClient();
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  beginSessionWithCurrentCredentials();
}

void MainWindow::enterDemoInterface()
{
  m_demoMode = true;
  m_demoCallHistory = itl::DemoData::callHistory();
  m_contacts.clear();

  for (const itl::DemoData::DemoContact &contact : itl::DemoData::contacts()) {
    ContactEntry entry;
    entry.name = contact.name;
    entry.ext = contact.ext;
    entry.phone = contact.phone;
    entry.presence = contact.presence;
    entry.login = contact.peer.section(QLatin1Char('@'), 0, 0);
    entry.isSelf = contact.isSelf;
    if (contact.isSelf) {
      m_selfPeer = contact.peer;
      m_selfName = contact.name;
    }
    m_contacts.insert(contact.peer, entry);
  }

  itl::DemoData::seedChatMessages(m_client->chat());
  m_client->chat()->seedDemoOscPeers({});
  scheduleDemoOscDiscovery();
  refreshColorAdvertisementPeers();
  m_client->chat()->sendColorAdvertisement(m_client->appSettings().profileAvatarColor());
  updateSelfHeader();
  rebuildContactList();
  refreshAllContactPeerAvatars();
  rebuildHistoryList();
  updateUnreadIndicators();
  setOnlineUi(true);
}

void MainWindow::scheduleDemoOscDiscovery()
{
  if (!m_demoOscDiscoverTimer) {
    m_demoOscDiscoverTimer = new QTimer(this);
    m_demoOscDiscoverTimer->setSingleShot(true);
    connect(m_demoOscDiscoverTimer, &QTimer::timeout, this, &MainWindow::discoverDemoOscAdmin);
  }
  m_demoOscDiscoverTimer->start(5000);
}

void MainWindow::discoverDemoOscAdmin()
{
  if (!m_demoMode) {
    return;
  }
  const QString admin = itl::DemoData::adminPeer();
  m_client->chat()->discoverOscPeer(admin);
  m_client->chat()->demoIncomingFileShare(
      admin, QStringLiteral("Hello world.txt"),
      QByteArray("Hello world\n"));

  const QString wallpaperPath = m_client->appSettings().appWallpaperPath();
  if (!wallpaperPath.isEmpty()) {
    const QPixmap wallpaper(wallpaperPath);
    if (!wallpaper.isNull()) {
      m_client->chat()->demoIncomingThemeShare(
          admin, wallpaper, m_client->appSettings().appWallpaperOpacity(),
          m_client->appSettings().appWallpaperListOpacity());
    }
  }
}

void MainWindow::exitDemoInterface()
{
  stopDemoCallSimulation();
  if (m_demoOscDiscoverTimer) {
    m_demoOscDiscoverTimer->stop();
  }
  m_demoMode = false;
  m_demoCallHistory.clear();
  m_contacts.clear();
  m_contactItems.clear();
  m_contactsList->clear();
  m_selfPeer.clear();
  m_selfName.clear();
  m_client->leaveDemoMode();
  m_callWindow->closeCall();
  updateSelfHeader();
  rebuildHistoryList();
  setOnlineUi(false);
}

void MainWindow::stopDemoCallSimulation()
{
  if (!m_demoCallLeg.isEmpty()) {
    m_calls->resumeExternalMedia();
  }
  if (m_demoVoiceTimer) {
    m_demoVoiceTimer->stop();
  }
  m_demoVoiceActive = false;
  if (m_callWindow && m_callWindow->isVisible()) {
    m_callWindow->setRemoteSpeakingIndicator(false);
  }
  m_demoCallLeg.clear();
  leaveCallPresence();
}

void MainWindow::startDemoVoiceSimulation()
{
  if (!m_demoVoiceTimer) {
    m_demoVoiceTimer = new QTimer(this);
    connect(m_demoVoiceTimer, &QTimer::timeout, this, [this]() {
      if (!m_demoMode || m_activeLeg != m_demoCallLeg) {
        m_demoVoiceTimer->stop();
        return;
      }
      m_demoVoiceActive = !m_demoVoiceActive;
      m_callWindow->setRemoteSpeakingIndicator(m_demoVoiceActive);
      m_demoVoiceTimer->start(150 + QRandomGenerator::global()->bounded(650));
    });
  }
  m_demoVoiceActive = false;
  m_demoVoiceTimer->start(400);
}

void MainWindow::startDemoCallSimulation(const QString &peer, const QString &displayName, const QString &detail)
{
  stopDemoCallSimulation();
  m_demoCallLeg = QStringLiteral("demo-call");
  m_activeLeg = m_demoCallLeg;
  m_activeIncomingLeg.clear();
  loadCallNotes(peer);
  m_callWindow->showOutgoing(peer, displayName, detail);
  m_callWindow->setAvatarColor(m_client->chat()->peerColor(peer));
  m_callWindow->setAvatarPixmap(m_client->chat()->peerAvatar(peer));
  m_calls->pauseExternalMedia();
  beginCallTracking(m_demoCallLeg, peer, displayName, false);
  enterCallPresence();

  QTimer::singleShot(1200, this, [this, displayName]() {
    if (!m_demoMode || m_activeLeg != m_demoCallLeg) {
      return;
    }
    m_callWindow->updateState(QStringLiteral("ringing"), {});
    QTimer::singleShot(1800, this, [this, displayName]() {
      if (!m_demoMode || m_activeLeg != m_demoCallLeg) {
        return;
      }
      m_callWindow->updateState(QStringLiteral("connected"), displayName);
      markCallConnected(m_demoCallLeg);
      m_callWindow->beginConversationTimer();
      startDemoVoiceSimulation();
    });
  });
}

void MainWindow::onLogout()
{
  if (m_demoMode) {
    exitDemoInterface();
    return;
  }
  m_client->logout();
  setOnlineUi(false);
  m_callWindow->closeCall();
}

void MainWindow::onSettings()
{
  QHash<QString, QString> sharePeers;
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    if (it.value().isSelf) {
      continue;
    }
    if (!m_client->chat()->isOscPeer(it.key())) {
      continue;
    }
    const QString name = it.value().name.isEmpty() ? displayNameForPeer(it.key()) : it.value().name;
    sharePeers.insert(it.key(), name);
  }

  SettingsDialog dlg(m_client, m_calls, m_selfName, sharePeers, m_selfPeer, this);
  const int result = dlg.exec();
  m_messageNotify->applySettings(&m_client->appSettings());
  if (result == QDialog::Accepted) {
    const QString newName = dlg.displayName();
    if (!newName.isEmpty() && newName != m_selfName) {
      m_selfName = newName;
      m_headerName->setText(m_selfName);
      m_headerAvatar->setLetter(m_selfName.left(1).toUpper());
    }
    m_headerAvatar->refreshFromSettings();
    if (m_online) {
      syncSelfOscShareProfile();
      refreshColorAdvertisementPeers();
      m_client->chat()->sendColorAdvertisement(m_client->appSettings().profileAvatarColor());
    }
    refreshWallpaper();
  } else if (result == 3) {
    onLogin();
  }
}

void MainWindow::applyContactViewSettings()
{
  const bool showChat = m_client->appSettings().showChatButtons();
  const bool showCall = m_client->appSettings().showCallButtons();
  const bool showVideo = m_client->appSettings().showVideoButtons();
  const bool videoSupported = serverVideoUiAvailable();
  for (ContactRowWidget *row : findChildren<ContactRowWidget *>()) {
    row->setChatButtonVisible(showChat);
    row->setCallButtonVisible(showCall);
    row->setVideoCallSupported(videoSupported);
    row->setVideoButtonVisible(showVideo);
    if (!showChat) {
      row->setUnreadBlink(false);
    }
  }
  if (showChat) {
    updateUnreadIndicators();
  }
}

bool MainWindow::serverVideoUiAvailable() const
{
  return m_online && !m_demoMode && m_client->serverVideoEnabled();
}

bool MainWindow::shouldNotifyForChatMessage(const QString &peer) const
{
  if (QApplication::activeModalWidget()) {
    return false;
  }
  if (!m_client->appSettings().showChatButtons()) {
    return false;
  }
  if (m_chatDialog && m_chatDialog->isOpenForPeer(peer)) {
    return false;
  }
  return true;
}

void MainWindow::updateUnreadIndicators()
{
  if (!m_client->appSettings().showChatButtons()) {
    for (ContactRowWidget *row : findChildren<ContactRowWidget *>()) {
      row->setUnreadBlink(false);
    }
    return;
  }

  for (auto it = m_contactItems.cbegin(); it != m_contactItems.cend(); ++it) {
    ContactRowWidget *row = rowWidgetForPeer(it.key());
    if (!row) {
      continue;
    }
    row->setUnreadBlink(m_client->chat()->hasUnread(it.key()));
  }
}

void MainWindow::onIncomingChatMessage(const QString &peer, const QString &text, bool incoming,
                                     const QDateTime &timestamp)
{
  Q_UNUSED(text)
  Q_UNUSED(timestamp)

  if (!incoming) {
    return;
  }

  if (m_chatDialog && m_chatDialog->isOpenForPeer(peer)) {
    m_client->chat()->markPeerRead(peer);
    updateUnreadIndicators();
    return;
  }

  updateUnreadIndicators();

  if (shouldNotifyForChatMessage(peer)) {
    QTimer::singleShot(0, this, [this, peer]() {
      if (shouldNotifyForChatMessage(peer)) {
        m_messageNotify->play();
      }
    });
  }
}

void MainWindow::setupDragDrop()
{
  setAcceptDrops(true);
  qApp->installEventFilter(this);
}

void MainWindow::registerDropTarget(QWidget *widget)
{
  Q_UNUSED(widget)
}

bool MainWindow::isTelUri(const QString &text) const
{
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty() || trimmed.contains(QLatin1Char('\n'))) {
    return false;
  }
  if (trimmed.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive)) {
    return true;
  }
  const QUrl url(trimmed);
  return url.scheme() == QStringLiteral("tel");
}

bool MainWindow::shouldInterceptTelPaste(QObject *focusWidget) const
{
  if (QApplication::activeModalWidget()) {
    return false;
  }

  QWidget *widget = qobject_cast<QWidget *>(focusWidget);
  if (!widget) {
    widget = QApplication::focusWidget();
  }

  for (QWidget *current = widget; current; current = current->parentWidget()) {
    if (current == m_chatDialog) {
      return false;
    }
    if (qobject_cast<QDialog *>(current) && current != this) {
      return false;
    }
  }
  return true;
}

void MainWindow::handleIncomingTelUri(const QString &uri)
{
  applyTelUriToDial(uri);
  show();
  raise();
  activateWindow();
}

void MainWindow::applyTelUriToDial(const QString &raw)
{
  QString value = raw.trimmed();
  if (value.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive)) {
    value = value.mid(4);
  }

  const QUrl asUrl(raw.trimmed());
  if (asUrl.scheme() == QStringLiteral("tel")) {
    value = asUrl.path();
    if (value.isEmpty()) {
      value = asUrl.toString(QUrl::RemoveScheme).trimmed();
    }
  }

  value = QUrl::fromPercentEncoding(value.trimmed().toUtf8());
  const int semicolon = value.indexOf(QLatin1Char(';'));
  if (semicolon >= 0) {
    value = value.left(semicolon);
  }
  value = itl::AddressBookManager::normalizePhone(value.trimmed());
  if (value.isEmpty() || !m_dialInput) {
    return;
  }

  if (m_tabStack && m_tabBar && m_dialPage) {
    const int index = m_tabStack->indexOf(m_dialPage);
    if (index >= 0) {
      m_tabStack->setCurrentIndex(index);
      m_tabBar->setCurrentIndex(index);
    }
  }
  m_dialInput->setText(value);
  m_dialInput->setFocus();
  m_dialInput->selectAll();
}

int MainWindow::addImportedContact(const QString &name, const QString &phone, const QString &ext)
{
  const QString cleanPhone = phone.trimmed();
  const QString cleanExt = ext.trimmed();
  QString cleanName = name.trimmed();
  if (cleanPhone.isEmpty() && cleanExt.isEmpty()) {
    return 0;
  }
  if (cleanName.isEmpty()) {
    cleanName = cleanPhone.isEmpty() ? cleanExt : cleanPhone;
  }

  const QString domain = m_client->credentials().domain;
  const QString handle = !cleanExt.isEmpty() ? cleanExt : cleanPhone;
  itl::CustomContact contact;
  contact.peer = handle.contains(QLatin1Char('@')) ? handle
                                                 : handle + QLatin1Char('@') + domain;
  contact.name = cleanName;
  contact.phone = cleanPhone;
  contact.ext = cleanExt;

  if (useServerContacts()) {
    return m_client->addressBook()->createContact(contact) >= 0 ? 1 : 0;
  }

  m_client->appSettings().addCustomContact(contact);

  ContactEntry entry;
  entry.name = contact.name;
  entry.ext = contact.ext;
  entry.phone = contact.phone;
  entry.login = contact.peer.section(QLatin1Char('@'), 0, 0);
  entry.isCustom = true;
  m_contacts.insert(contact.peer, entry);
  return 1;
}

int MainWindow::importContactsFromText(const QString &text, bool isVcard, bool notify, bool fromDrop)
{
  QByteArray data = text.toUtf8();
  QBuffer buffer(&data);
  if (!buffer.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return 0;
  }

  QTextStream in(&buffer);
  int imported = 0;
  QString vcardName;
  QString vcardTel;

  while (!in.atEnd()) {
    const QString line = in.readLine().trimmed();
    if (line.isEmpty()) {
      continue;
    }

    if (isVcard) {
      if (line.startsWith(QStringLiteral("BEGIN:VCARD"), Qt::CaseInsensitive)) {
        vcardName.clear();
        vcardTel.clear();
      } else if (line.startsWith(QStringLiteral("FN:"), Qt::CaseInsensitive)) {
        vcardName = line.mid(3).trimmed();
      } else if (line.startsWith(QStringLiteral("TEL"), Qt::CaseInsensitive)) {
        vcardTel = line.section(QLatin1Char(':'), 1).trimmed();
      } else if (line.startsWith(QStringLiteral("END:VCARD"), Qt::CaseInsensitive)) {
        imported += addImportedContact(vcardName, vcardTel, {});
      }
      continue;
    }

    const QStringList parts = line.split(QLatin1Char(','));
    imported += addImportedContact(parts.value(0), parts.value(1), parts.value(2));
  }

    if (imported > 0) {
    if (!useServerContacts()) {
      m_client->saveSettings();
      rebuildContactList();
    }
    if (notify) {
      const QString title = fromDrop ? tr("Контакт") : tr("Импорт");
      const QString message = fromDrop ? tr("Добавлено контактов: %1").arg(imported)
                                     : tr("Импортировано контактов: %1").arg(imported);
      QMessageBox::information(this, title, message);
    } else {
      onStatusMessage(tr("Добавлено контактов: %1").arg(imported));
    }
  } else if (notify) {
    QMessageBox::warning(this, tr("Импорт"),
                         tr("В файле не найдено контактов.\n\nФормат CSV: имя,номер,внутренний"));
  }

  return imported;
}

int MainWindow::importContactsFromPath(const QString &path, bool notify, bool fromDrop)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (notify) {
      QMessageBox::warning(this, tr("Импорт"), tr("Не удалось открыть файл."));
    }
    return 0;
  }

  const bool isVcard = path.endsWith(QStringLiteral(".vcf"), Qt::CaseInsensitive);
  const int imported = importContactsFromText(QString::fromUtf8(file.readAll()), isVcard, notify, fromDrop);
  file.close();
  return imported;
}

bool MainWindow::canAcceptDrag(const QMimeData *mimeData) const
{
  if (!mimeData || !m_online) {
    return false;
  }

  if (mimeData->hasFormat(QStringLiteral("text/x-vcard"))) {
    return true;
  }

  if (mimeData->hasUrls()) {
    for (const QUrl &url : mimeData->urls()) {
      if (url.scheme() == QStringLiteral("tel")) {
        return true;
      }
      if (url.isLocalFile()) {
        const QString path = url.toLocalFile().toLower();
        if (path.endsWith(QStringLiteral(".vcf")) || path.endsWith(QStringLiteral(".csv"))
            || path.endsWith(QStringLiteral(".txt"))) {
          return true;
        }
      }
    }
  }

  const QString text = mimeData->text().trimmed();
  if (text.isEmpty()) {
    return false;
  }
  if (isTelUri(text)) {
    return true;
  }
  if (text.contains(QStringLiteral("BEGIN:VCARD"), Qt::CaseInsensitive)) {
    return true;
  }
  if (text.contains(QLatin1Char(','))) {
    return true;
  }

  if (mimeData->hasFormat(QStringLiteral("text/uri-list"))) {
    const QString uriList = QString::fromUtf8(mimeData->data(QStringLiteral("text/uri-list")));
    for (const QString &line : uriList.split(QLatin1Char('\n'))) {
      const QString trimmed = line.trimmed();
      if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
        continue;
      }
      if (trimmed.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive)
          || QUrl(trimmed).scheme() == QStringLiteral("tel")) {
        return true;
      }
    }
  }

  return false;
}

bool MainWindow::handleDroppedMimeData(const QMimeData *mimeData, bool notify)
{
  if (!mimeData || !m_online) {
    return false;
  }

  auto firstUri = [&]() -> QString {
    if (mimeData->hasFormat(QStringLiteral("text/uri-list"))) {
      const QString uriList = QString::fromUtf8(mimeData->data(QStringLiteral("text/uri-list")));
      for (const QString &line : uriList.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith(QLatin1Char('#'))) {
          return trimmed;
        }
      }
    }
    if (mimeData->hasUrls() && !mimeData->urls().isEmpty()) {
      return mimeData->urls().first().toString();
    }
    return {};
  };

  const QString uri = firstUri();
  if (!uri.isEmpty()) {
    const QUrl url(uri);
    if (url.scheme() == QStringLiteral("tel") || uri.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive)) {
      applyTelUriToDial(uri);
      onStatusMessage(tr("Номер вставлен в «Набрать вручную»"));
      return true;
    }
    if (url.isLocalFile()) {
      const QString path = url.toLocalFile();
      const QString lower = path.toLower();
      if (lower.endsWith(QStringLiteral(".vcf")) || lower.endsWith(QStringLiteral(".csv"))
          || lower.endsWith(QStringLiteral(".txt"))) {
        return importContactsFromPath(path, notify, true) > 0;
      }
    }
  }

  if (mimeData->hasUrls()) {
    for (const QUrl &url : mimeData->urls()) {
      if (!url.isLocalFile()) {
        continue;
      }
      const QString path = url.toLocalFile();
      const QString lower = path.toLower();
      if (lower.endsWith(QStringLiteral(".vcf")) || lower.endsWith(QStringLiteral(".csv"))
          || lower.endsWith(QStringLiteral(".txt"))) {
        return importContactsFromPath(path, notify, true) > 0;
      }
    }
  }

  QString text;
  if (mimeData->hasFormat(QStringLiteral("text/x-vcard"))) {
    text = QString::fromUtf8(mimeData->data(QStringLiteral("text/x-vcard")));
  } else {
    text = mimeData->text();
  }

  const QString trimmed = text.trimmed();
  if (isTelUri(trimmed)) {
    applyTelUriToDial(trimmed);
    onStatusMessage(tr("Номер вставлен в «Набрать вручную»"));
    return true;
  }

  if (trimmed.contains(QStringLiteral("BEGIN:VCARD"), Qt::CaseInsensitive)) {
    return importContactsFromText(trimmed, true, notify, true) > 0;
  }

  if (trimmed.contains(QLatin1Char(','))) {
    return importContactsFromText(trimmed, false, notify, true) > 0;
  }

  return false;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  if (canAcceptDrag(event->mimeData())) {
    event->acceptProposedAction();
    return;
  }
  QMainWindow::dragEnterEvent(event);
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
  if (canAcceptDrag(event->mimeData())) {
    event->acceptProposedAction();
    return;
  }
  QMainWindow::dragMoveEvent(event);
}

void MainWindow::dropEvent(QDropEvent *event)
{
  if (handleDroppedMimeData(event->mimeData(), true)) {
    event->acceptProposedAction();
    return;
  }
  QMainWindow::dropEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
  if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
    auto *dragEvent = static_cast<QDragEnterEvent *>(event);
    if (canAcceptDrag(dragEvent->mimeData())) {
      dragEvent->acceptProposedAction();
      return true;
    }
  }

  if (event->type() == QEvent::Drop) {
    auto *dropEvent = static_cast<QDropEvent *>(event);
    if (handleDroppedMimeData(dropEvent->mimeData(), true)) {
      dropEvent->acceptProposedAction();
      return true;
    }
  }

  if (event->type() == QEvent::KeyPress && shouldInterceptTelPaste(QApplication::focusWidget())) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->matches(QKeySequence::Paste)) {
      const QString clipboardText = QGuiApplication::clipboard()->text().trimmed();
      if (isTelUri(clipboardText)) {
        applyTelUriToDial(clipboardText);
        return true;
      }
    }
  }

  return QObject::eventFilter(watched, event);
}

void MainWindow::onAddContact()
{
  if (!m_online) {
    return;
  }

  AddContactDialog dlg(m_client->credentials().domain, this);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  const itl::CustomContact contact = dlg.contact();
  if (useServerContacts()) {
    if (m_client->addressBook()->createContact(contact) < 0) {
      QMessageBox::warning(this, tr("Контакт"), tr("Не удалось отправить контакт на сервер."));
      return;
    }
    onStatusMessage(tr("Контакт «%1» отправлен на сервер").arg(contact.name));
    return;
  }

  m_client->appSettings().addCustomContact(contact);
  m_client->saveSettings();

  ContactEntry entry;
  entry.name = contact.name;
  entry.ext = contact.ext;
  entry.phone = contact.phone;
  entry.login = contact.peer.section(QLatin1Char('@'), 0, 0);
  m_contacts.insert(contact.peer, entry);
  rebuildContactList();
}

void MainWindow::onImportContacts()
{
  if (!m_online) {
    return;
  }

  const QString path = QFileDialog::getOpenFileName(
      this, tr("Импорт контактов"), QString(),
      tr("Контакты (*.csv *.vcf *.txt);;Все файлы (*)"));
  if (path.isEmpty()) {
    return;
  }

  importContactsFromPath(path, true);
}

void MainWindow::onConference()
{
  if (!m_online) {
    return;
  }

  QHash<QString, QString> peerNames;
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    if (it.value().isSelf) {
      continue;
    }
    const QString name = it.value().name.isEmpty() ? displayNameForPeer(it.key()) : it.value().name;
    peerNames.insert(it.key(), name);
  }

  ConferenceDialog dlg(peerNames, m_selfPeer, this);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  if (m_demoMode) {
    const QList<itl::ConferenceParticipant> participants = dlg.participants();
    const QString subject = dlg.subject();
    startDemoCallSimulation(subject.isEmpty() ? tr("Конференция") : subject, subject.isEmpty() ? tr("Конференция") : subject,
                            tr("%1 участников").arg(participants.size()));
    return;
  }

  const QList<itl::ConferenceParticipant> participants = dlg.participants();
  const QString subject = dlg.subject();
  m_activeLeg = m_calls->startConferenceCall(subject, participants);
  if (m_activeLeg.isEmpty()) {
    return;
  }
  m_callWindow->showOutgoing(subject.isEmpty() ? tr("Конференция") : subject, tr("Конференция"),
                             tr("%1 участников").arg(participants.size()));
  m_callWindow->setAvatarColor({});
}
void MainWindow::onHelp()
{
  HelpDialog dlg(this);
  dlg.exec();
}

void MainWindow::onDial()
{
  const QString peer = resolvePeer(m_dialInput->text());
  if (!peer.isEmpty()) {
    onCallFromRow(peer);
  }
}

void MainWindow::onCallFromRow(const QString &peer)
{
  if (!m_online) {
    return;
  }

  if (m_demoMode) {
    startDemoCallSimulation(peer, displayNameForPeer(peer), detailForPeer(peer));
    return;
  }

  m_activeLeg = m_calls->startOutgoingCall(peer);
  m_activeIncomingLeg.clear();
  loadCallNotes(peer);
  m_callWindow->showOutgoing(peer, displayNameForPeer(peer), detailForPeer(peer));
  m_callWindow->setAvatarColor(m_client->chat()->peerColor(peer));
  m_callWindow->setAvatarPixmap(m_client->chat()->peerAvatar(peer));
}

void MainWindow::onVideoCallFromRow(const QString &peer)
{
  if (!m_online || m_demoMode || !m_client->serverVideoEnabled()) {
    return;
  }

  m_activeLeg = m_calls->startOutgoingCall(peer, true);
  m_activeIncomingLeg.clear();
  loadCallNotes(peer);
  m_callWindow->showOutgoing(peer, displayNameForPeer(peer), detailForPeer(peer));
  m_callWindow->setVideoCall(true);
  const itl::CallSession *session = m_calls->call(m_activeLeg);
  m_callWindow->setVideoSending(session && session->sendVideo);
  m_callWindow->setAvatarColor(m_client->chat()->peerColor(peer));
  m_callWindow->setAvatarPixmap(m_client->chat()->peerAvatar(peer));
}

void MainWindow::onChatFromRow(const QString &peer)
{
  m_chatDialog->openForPeer(peer, displayNameForPeer(peer), m_selfName);
  updateUnreadIndicators();
}

void MainWindow::onHangup()
{
  if (m_demoMode) {
    const QString leg = m_activeLeg;
    stopDemoCallSimulation();
    m_activeLeg.clear();
    m_activeIncomingLeg.clear();
    m_onHold = false;
    if (!leg.isEmpty()) {
      finalizeCallHistory(leg, QStringLiteral("ended"));
    }
    m_callWindow->closeCall();
    return;
  }

  m_calls->hangupAll();
  m_activeLeg.clear();
  m_activeIncomingLeg.clear();
  m_onHold = false;
  m_callWindow->closeCall();
}

void MainWindow::onAnswer()
{
  if (!m_activeIncomingLeg.isEmpty()) {
    m_calls->acceptIncomingCall(m_activeIncomingLeg);
    m_activeLeg = m_activeIncomingLeg;
  }
}

void MainWindow::onCallDtmf(const QString &digit)
{
  if (digit.isEmpty() || m_activeLeg.isEmpty() || m_demoMode) {
    return;
  }
  m_calls->sendDtmf(m_activeLeg, digit.at(0));
}

void MainWindow::onHold()
{
  if (m_activeLeg.isEmpty()) {
    return;
  }

  if (m_demoMode) {
    m_onHold = !m_onHold;
    m_callWindow->updateState(m_onHold ? QStringLiteral("hold") : QStringLiteral("resumed"), {});
    return;
  }

  m_onHold = !m_onHold;
  m_calls->setHold(m_activeLeg, m_onHold);
}

void MainWindow::onTransfer()
{
  if (m_activeLeg.isEmpty() || !m_online) {
    return;
  }

  QHash<QString, QString> peerNames;
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    if (it.value().isSelf) {
      continue;
    }
    const QString name = it.value().name.isEmpty() ? displayNameForPeer(it.key()) : it.value().name;
    peerNames.insert(it.key(), name);
  }

  const QString excludePeer = m_callWindow ? m_callWindow->peer() : QString();
  TransferDialog dlg(peerNames, m_selfPeer, excludePeer, this);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  const QString peer = dlg.selectedPeer();
  if (peer.isEmpty()) {
    return;
  }

  const QString transferName = dlg.selectedDisplayName().isEmpty()
                                   ? displayNameForPeer(peer)
                                   : dlg.selectedDisplayName();
  const QString leg = m_activeLeg;

  if (m_demoMode) {
    stopDemoCallSimulation();
    finalizeCallHistory(leg, QStringLiteral("transferred"), transferName);
    m_activeLeg.clear();
    m_activeIncomingLeg.clear();
    m_onHold = false;
    m_callWindow->closeCall();
    return;
  }

  m_calls->blindTransfer(leg, peer);
}

void MainWindow::onPresenceChanged(int index)
{
  if (!m_online || index < 0 || m_demoMode || m_callPresenceActive) {
    return;
  }

  const QString status = m_presenceSelector->currentStatus();
  if (status == QStringLiteral("in-call") && !usesOpenSourcePresence()) {
    // Megafon PBX rejects SetPresence(status:"in-call") with error 971.
    if (!m_selfPeer.isEmpty()) {
      m_contacts[m_selfPeer].presence = QStringLiteral("in-call");
    }
    return;
  }

  if (!m_selfPeer.isEmpty()) {
    m_contacts[m_selfPeer].presence = status;
  }
  m_client->api()->setOwnPresence(status);
}

void MainWindow::onSearchChanged(const QString &) { rebuildContactList(); }

void MainWindow::onContactSelected()
{
  for (int i = 0; i < m_contactsList->count(); ++i) {
    auto *item = m_contactsList->item(i);
    if (auto *row = qobject_cast<ContactRowWidget *>(m_contactsList->itemWidget(item))) {
      row->setSelected(item == m_contactsList->currentItem());
    }
  }
}

void MainWindow::onHistorySelected()
{
  for (int i = 0; i < m_historyList->count(); ++i) {
    auto *item = m_historyList->item(i);
    if (auto *row = qobject_cast<HistoryRowWidget *>(m_historyList->itemWidget(item))) {
      row->setSelected(item == m_historyList->currentItem());
    }
  }
}

void MainWindow::onStatusMessage(const QString &message)
{
  if (m_demoMode) {
    return;
  }
  if (message.contains(tr("В сети"))) {
    setOnlineUi(true);
  }
}

void MainWindow::onContactUpdated(const QString &peer, const QString &name, const QString &presence)
{
  ContactEntry &entry = m_contacts[peer];
  if (!name.isEmpty()) {
    entry.name = name;
  }
  if (!presence.isEmpty()) {
    entry.presence = presence;
  }
  if (auto *row = rowWidgetForPeer(peer)) {
    if (!name.isEmpty()) {
      row->updateName(entry.name);
    }
    if (!presence.isEmpty()) {
      row->updatePresence(presence);
    }
  }
}

void MainWindow::onContactsLoaded(const QJsonObject &contacts)
{
  const QJsonObject accList = contacts.value(QStringLiteral("accList")).toObject();
  if (accList.isEmpty()) {
    return;
  }

  const QString domain = m_client->credentials().domain;
  const QString selfLogin = m_client->credentials().login.section(QLatin1Char('@'), 0, 0);

  for (auto it = accList.begin(); it != accList.end(); ++it) {
    if (it.key() == QStringLiteral("pbx")) {
      continue;
    }

    const QJsonObject acc = it.value().toObject();
    ContactEntry entry;
    entry.login = it.key();
    entry.name = acc.value(QStringLiteral("RealName")).toString();
    const QJsonArray ext = acc.value(QStringLiteral("ext")).toArray();
    if (!ext.isEmpty()) {
      entry.ext = ext.first().toString();
    }
    entry.phone = acc.value(QStringLiteral("mobile")).toString();
    if (entry.phone.isEmpty()) {
      const QJsonArray tn = acc.value(QStringLiteral("tn")).toArray();
      if (!tn.isEmpty()) {
        entry.phone = tn.first().toString();
      }
    }
    entry.personalPhone = acc.value(QStringLiteral("sim")).toString();
    if (entry.phone.isEmpty()) {
      entry.phone = entry.personalPhone;
      entry.personalPhone.clear();
    }
    entry.isSelf = (entry.login == selfLogin);
    const QString peer = entry.login + QLatin1Char('@') + domain;
    if (entry.isSelf) {
      m_selfPeer = peer;
      m_selfName = entry.name;
    }
    m_contacts.insert(peer, entry);
  }

  updateSelfHeader();
  mergeCustomContacts();
  rebuildContactList();
  refreshAllContactPeerColors();
  refreshAllContactPeerAvatars();
  refreshServerHistory();
  prefetchCompanyHistory();
  prefetchInternalHistory();

  refreshColorAdvertisementPeers();
  m_client->chat()->sendColorAdvertisement(m_client->appSettings().profileAvatarColor());
}

void MainWindow::onAddressBookChanged()
{
  mergeCustomContacts();
  rebuildContactList();
  refreshColorAdvertisementPeers();
}

bool MainWindow::useServerContacts() const
{
  return m_online && !m_demoMode;
}

void MainWindow::refreshColorAdvertisementPeers()
{
  QStringList peers;
  const QString domain = m_client->credentials().domain;
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    if (it.value().isSelf) {
      continue;
    }
    const QString &peer = it.key();
    const int at = peer.indexOf(QLatin1Char('@'));
    if (at <= 0) {
      continue;
    }
    if (peer.mid(at + 1).compare(domain, Qt::CaseInsensitive) != 0) {
      continue;
    }
    peers.append(peer);
  }
  syncSelfOscShareProfile();
  m_client->chat()->setOpenpingCandidates(peers);
  m_client->chat()->sendOpenpingBroadcast();
}

void MainWindow::syncSelfOscShareProfile()
{
  const QString color = m_client->appSettings().profileAvatarColor();
  QPixmap photo;
  const QString path = m_client->appSettings().profileAvatarPath();
  if (!path.isEmpty()) {
    photo.load(path);
  }
  m_client->chat()->setSelfShareProfile(color, photo);
}

void MainWindow::applyPeerColorForPeer(const QString &peer, const QString &color)
{
  if (color.isEmpty()) {
    return;
  }

  int matched = 0;
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    if (!contactMatchesSender(it.key(), it.value(), peer)) {
      continue;
    }
    ++matched;
    if (ContactRowWidget *row = rowWidgetForPeer(it.key())) {
      row->setPeerColor(color);
    }
  }

  if (matched == 0) {
    qCWarning(lcHistory) << "Color advertisement stored for" << peer << "but no contact matched (color" << color << ')';
  }

  if (m_callWindow && !m_callWindow->peer().isEmpty() && isSamePeer(m_callWindow->peer(), peer)) {
    m_callWindow->setAvatarColor(color);
  }
}

void MainWindow::applyPeerAvatarForPeer(const QString &peer, const QPixmap &avatar)
{
  if (avatar.isNull()) {
    return;
  }

  int matched = 0;
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    if (!contactMatchesSender(it.key(), it.value(), peer)) {
      continue;
    }
    ++matched;
    if (ContactRowWidget *row = rowWidgetForPeer(it.key())) {
      row->setPeerAvatar(avatar);
    }
  }

  if (matched == 0) {
    qCWarning(lcHistory) << "Avatar share stored for" << peer << "but no contact matched";
  }

  if (m_callWindow && !m_callWindow->peer().isEmpty() && isSamePeer(m_callWindow->peer(), peer)) {
    m_callWindow->setAvatarPixmap(avatar);
  }
}

bool MainWindow::contactMatchesSender(const QString &contactPeer, const ContactEntry &entry,
                                      const QString &sender) const
{
  if (isSamePeer(contactPeer, sender)) {
    return true;
  }

  const QString senderLogin = sender.section(QLatin1Char('@'), 0, 0).toLower();
  if (senderLogin.isEmpty()) {
    return false;
  }
  if (entry.login.compare(senderLogin, Qt::CaseInsensitive) == 0) {
    return true;
  }
  return contactPeer.section(QLatin1Char('@'), 0, 0).compare(senderLogin, Qt::CaseInsensitive) == 0;
}

void MainWindow::refreshAllContactPeerColors()
{
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    const QString color = m_client->chat()->peerColor(it.key());
    if (!color.isEmpty()) {
      applyPeerColorForPeer(it.key(), color);
    }
  }
}

void MainWindow::refreshAllContactPeerAvatars()
{
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    const QPixmap avatar = m_client->chat()->peerAvatar(it.key());
    if (!avatar.isNull()) {
      applyPeerAvatarForPeer(it.key(), avatar);
    }
  }
}

void MainWindow::mergeCustomContacts()
{
  for (auto it = m_contacts.begin(); it != m_contacts.end();) {
    if (it->isCustom && !it->isSelf) {
      it = m_contacts.erase(it);
    } else {
      ++it;
    }
  }

  const QList<itl::CustomContact> personal = useServerContacts()
      ? m_client->addressBook()->contacts()
      : m_client->appSettings().customContacts();

  for (const itl::CustomContact &contact : personal) {
    if (contact.peer.isEmpty()) {
      continue;
    }
    ContactEntry entry = m_contacts.value(contact.peer);
    entry.name = contact.name;
    entry.ext = contact.ext;
    entry.phone = contact.phone;
    entry.login = contact.peer.section(QLatin1Char('@'), 0, 0);
    entry.isCustom = true;
    m_contacts.insert(contact.peer, entry);
  }
}

void MainWindow::onCallEvent(const QString &leg, const QString &what, const QJsonObject &payload)
{
  m_calls->handleServerCallEvent(leg, what, payload);
  if (what == QStringLiteral("incomingCall")) {
    m_activeIncomingLeg = leg;
  }
}

void MainWindow::onCallStateChanged(const QString &leg, const QString &state, const QString &detail)
{
  if (state == QStringLiteral("incoming")) {
    enterCallPresence();
    m_activeIncomingLeg = leg;
    QString incomingPeer;
    if (itl::CallSession *session = m_calls->call(leg)) {
      incomingPeer = session->peer;
      loadCallNotes(incomingPeer);
    }
    beginCallTracking(leg, incomingPeer, detail, true);
    const QString incomingName = detail.isEmpty() ? displayNameForPeer(incomingPeer) : detail;
    m_callWindow->showIncoming(incomingPeer, incomingName, detailForPeer(incomingPeer));
    if (itl::CallSession *session = m_calls->call(leg)) {
      m_callWindow->setVideoCall(session->videoCall);
      m_callWindow->setVideoSending(session->videoCall);
    }
    m_callWindow->setAvatarColor(m_client->chat()->peerColor(incomingPeer));
    m_callWindow->setAvatarPixmap(m_client->chat()->peerAvatar(incomingPeer));
    return;
  }
  if (state == QStringLiteral("connecting") || state == QStringLiteral("dialing")
      || state == QStringLiteral("ringing")) {
    enterCallPresence();
    if (state != QStringLiteral("connecting") && state != QStringLiteral("dialing")) {
      m_activeLeg = leg;
    }
    if (state == QStringLiteral("connecting") || state == QStringLiteral("dialing")) {
      const QString peer = m_callWindow->peer().isEmpty() ? detail : m_callWindow->peer();
      beginCallTracking(leg, peer, displayNameForPeer(peer), false);
    }
    m_callWindow->updateState(state, detail);
    return;
  }
  if (state == QStringLiteral("connected")) {
    m_activeLeg = leg;
    m_activeIncomingLeg.clear();
    // Conversation duration / history "answered" start when remote audio arrives.
    m_callWindow->updateState(state, detail.isEmpty() ? displayNameForPeer(m_callWindow->peer()) : detail);
    return;
  }
  if (state == QStringLiteral("accepting") || state == QStringLiteral("media")
      || state == QStringLiteral("hold") || state == QStringLiteral("resumed")) {
    m_activeLeg = leg;
    m_callWindow->updateState(state, detail);
    return;
  }
  if (state == QStringLiteral("transferred")) {
    const QString transferName = detail.isEmpty() ? tr("контакт") : displayNameForPeer(detail);
    finalizeCallHistory(leg, QStringLiteral("transferred"),
                        transferName.isEmpty() ? detail : transferName);
    if (m_activeLeg == leg || m_activeIncomingLeg == leg || m_activeLeg.isEmpty()) {
      m_activeLeg.clear();
      m_activeIncomingLeg.clear();
      m_onHold = false;
    }
    if (m_demoMode) {
      stopDemoCallSimulation();
    } else {
      resumeExternalMediaIfIdle();
      leaveCallPresence();
    }
    m_callWindow->closeCall();
    return;
  }
  if (state == QStringLiteral("ended") || state == QStringLiteral("rejected")
      || state == QStringLiteral("error")) {
    finalizeCallHistory(leg, state);
    if (m_activeLeg == leg || m_activeIncomingLeg == leg) {
      m_activeLeg.clear();
      m_activeIncomingLeg.clear();
      m_onHold = false;
    }
    if (m_demoMode) {
      stopDemoCallSimulation();
    } else {
      resumeExternalMediaIfIdle();
      leaveCallPresence();
    }
    m_callWindow->updateState(state, detail);
  }
}
