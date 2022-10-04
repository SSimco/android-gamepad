#include "gamepad.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QScreen>

#include "dpadbutton.h"
#include "gamepadellipsebutton.h"
#include "gamepadrectbutton.h"
#include "joystick.h"
#include "util.h"

int const absAxisValue = 32767;

enum class Gamepad::InputType : char { key = 0x01, gyro = 0x02, axis = 0x03 };

enum class Gamepad::Axis : unsigned short {
  ABS_X = 0x00,
  ABS_Y = 0x01,
  ABS_Z = 0x02,
  ABS_RX = 0x03,
  ABS_RY = 0x04,
  ABS_RZ = 0x05
};

enum class Gamepad::Button : unsigned short {
  BTN_A = 0x130,
  BTN_B = 0x131,
  BTN_X = 0x133,
  BTN_Y = 0x134,
  BTN_TL = 0x136,
  BTN_TR = 0x137,
  BTN_TL2 = 0x138,
  BTN_TR2 = 0x139,
  BTN_SELECT = 0x13a,
  BTN_START = 0x13b,
  BTN_DPAD_UP = 0x220,
  BTN_DPAD_DOWN = 0x221,
  BTN_DPAD_LEFT = 0x222,
  BTN_DPAD_RIGHT = 0x223
};

Gamepad::Gamepad(QWidget* parent,
                 std::function<void(QByteArray const&)> const& sendData)
    : QWidget(parent),
      m_sendData(sendData),
      m_accelerometer(this),
      m_gyroscope(this),
      m_timer(this) {
  const QSize screenSize = qApp->screens().first()->size();
  const qreal ratio = 0.06;
  const qreal marginRatio = 0.5;
  int const controllWidth = screenSize.width() * ratio;
  int const controllHeight = screenSize.height() * ratio;
  int const margin = controllWidth * marginRatio;

  QHBoxLayout* mainLayout = new QHBoxLayout(this);

  auto addButtonsToLayout = [&](auto const& buttons, QVBoxLayout* layout) {
    for (auto&& button : buttons) {
      QString buttonText;
      Button buttonCode;
      std::tie(buttonText, buttonCode) = button;
      GamepadEllipseButton* ellpiseButton = new GamepadEllipseButton(
          this, buttonText,
          [=, this](bool val) { sendButtonData(buttonCode, val); },
          controllWidth, controllWidth);
      layout->addWidget(ellpiseButton);
    }
  };

  {
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setContentsMargins(margin * 2, 0, 0, 0);
    mainLayout->addLayout(leftLayout);
    leftLayout->setAlignment(Qt::AlignTop);
    std::initializer_list<std::tuple<QString, Button>> leftTriggers{
        {"L", Button::BTN_TL}, {"L2", Button::BTN_TL2}};
    addButtonsToLayout(leftTriggers, leftLayout);
  }

  QVBoxLayout* middleLayout = new QVBoxLayout();
  mainLayout->addLayout(middleLayout);
  QHBoxLayout* controlsLayout = new QHBoxLayout();
  middleLayout->addLayout(controlsLayout);

  {
    QGridLayout* dpadLayout = new QGridLayout();

    controlsLayout->addLayout(dpadLayout);
    std::initializer_list<
        std::tuple<std::tuple<int, int>, Button, DPadButton::Direction>>
        dpadButtons = {
            {{2, 1}, Button::BTN_DPAD_DOWN, DPadButton::Direction::Down},
            {{1, 2}, Button::BTN_DPAD_RIGHT, DPadButton::Direction::Right},
            {{1, 0}, Button::BTN_DPAD_LEFT, DPadButton::Direction::Left},
            {{0, 1}, Button::BTN_DPAD_UP, DPadButton::Direction::Up}};
    for (auto&& button : dpadButtons) {
      std::tuple<int, int> gridPos;
      int row;
      int col;
      Button buttonCode;
      DPadButton::Direction dir;
      std::tie(gridPos, buttonCode, dir) = button;
      std::tie(row, col) = gridPos;
      DPadButton* dpadButton = new DPadButton(
          this, [=, this](bool val) { sendButtonData(buttonCode, val); }, dir,
          controllWidth);
      dpadLayout->addWidget(dpadButton, row, col);
    }
  }

  {
    QHBoxLayout* rectButtonsLayout = new QHBoxLayout();
    controlsLayout->addLayout(rectButtonsLayout);
    std::initializer_list<std::tuple<QString, Button>> rectButtons = {
        {"Select", Button::BTN_SELECT}, {"Start", Button::BTN_START}};
    for (auto&& button : rectButtons) {
      QString buttonText;
      Button buttonCode;
      std::tie(buttonText, buttonCode) = button;
      GamepadRectButton* rectButton = new GamepadRectButton(
          this, buttonText,
          [=, this](bool val) { sendButtonData(buttonCode, val); },
          controllWidth, controllHeight);
      rectButtonsLayout->addWidget(rectButton, 0, Qt::AlignCenter);
    }
  }

  {
    QGridLayout* rightButtonsLayout = new QGridLayout();

    controlsLayout->addLayout(rightButtonsLayout);
    std::initializer_list<std::tuple<std::tuple<int, int>, QString, Button>>
        rightButtons = {{{2, 1}, "A", Button::BTN_A},
                        {{1, 2}, "B", Button::BTN_B},
                        {{1, 0}, "X", Button::BTN_X},
                        {{0, 1}, "Y", Button::BTN_Y}};
    for (auto&& button : rightButtons) {
      std::tuple<int, int> gridPos;
      int row;
      int col;
      QString buttonText;
      Button buttonCode;
      std::tie(gridPos, buttonText, buttonCode) = button;
      std::tie(row, col) = gridPos;
      GamepadEllipseButton* ellipseButton = new GamepadEllipseButton(
          this, buttonText,
          [=, this](bool val) { sendButtonData(buttonCode, val); },
          controllWidth, controllWidth);
      rightButtonsLayout->addWidget(ellipseButton, row, col);
    }
  }

  {
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(margin, margin * 0.25, margin,
                                     margin * 0.25);
    middleLayout->addLayout(bottomLayout);
    std::vector<std::tuple<Axis, Axis, Qt::AlignmentFlag>> joysticksAxes = {
        {Axis::ABS_X, Axis::ABS_Y, Qt::AlignLeft},
        {Axis::ABS_RX, Axis::ABS_RY, Qt::AlignRight}};

    for (auto&& joystickAxes : joysticksAxes) {
      Axis xAxis, yAxis;
      Qt::AlignmentFlag alignFlag;
      std::tie(xAxis, yAxis, alignFlag) = joystickAxes;
      Joystick* joystick = new Joystick(
          this,
          [=, this](qreal x, qreal y) {
            sendAxisData(xAxis, absAxisValue, x);
            sendAxisData(yAxis, absAxisValue, y);
          },
          controllWidth * 2);
      bottomLayout->addWidget(joystick, 0, alignFlag);
    }
  }

  {
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(0, 0, margin * 2, 0);
    mainLayout->addLayout(rightLayout);
    rightLayout->setAlignment(Qt::AlignTop);
    std::initializer_list<std::tuple<QString, Button>> leftTriggers{
        {"R", Button::BTN_TR}, {"R2", Button::BTN_TR2}};
    addButtonsToLayout(leftTriggers, rightLayout);
  }

  setMinimumSize(minimumSizeHint());

  m_timer.start(m_timerInterval);
  m_timer.callOnTimeout(this, [=, this]() { sendGyroData(); });
}
void Gamepad::enableGyro() {
  m_accelerometer.start();
  m_gyroscope.start();
  m_doSendGyro = true;
}
void Gamepad::disableGyro() {
  m_doSendGyro = false;
  m_accelerometer.stop();
  m_gyroscope.stop();
  QByteArray byteArray;
  append(byteArray,
         std::make_tuple(qToUnderlying(InputType::gyro), 0, 0, 0, 0, 0, 0));
  m_sendData(byteArray);
}
void Gamepad::sendGyroData() {
  if (m_doSendGyro) {
    auto accelReading = m_accelerometer.reading();
    auto gyroReading = m_gyroscope.reading();
    if (accelReading && gyroReading) {
      QByteArray byteArray;
      int accelX = accelReading->x();
      int accelY = accelReading->y();
      int accelZ = accelReading->z();
      int gyroX = gyroReading->x();
      int gyroY = gyroReading->y();
      int gyroZ = gyroReading->z();

      append(byteArray, std::make_tuple(qToUnderlying(InputType::gyro), accelX,
                                        accelY, accelZ, gyroX, gyroY, gyroZ));
      m_sendData(byteArray);
    }
  }
}
void Gamepad::sendAxisData(Axis code, int maxAbsAxisVal, qreal val) {
  int axisVal = maxAbsAxisVal * val;
  QByteArray byteArray;
  append(byteArray, std::make_tuple(qToUnderlying(InputType::axis),
                                    qToUnderlying(code), axisVal));
  m_sendData(byteArray);
}
void Gamepad::sendButtonData(Button code, int val) {
  QByteArray byteArray;
  append(byteArray, std::make_tuple(qToUnderlying(InputType::key),
                                    qToUnderlying(code), val));
  m_sendData(byteArray);
}
