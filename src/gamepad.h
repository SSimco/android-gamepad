#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <QAccelerometer>
#include <QByteArray>
#include <QGyroscope>
#include <QTimer>
#include <QWidget>

class Gamepad : public QWidget {
 private:
  enum class InputType : char;
  enum class Axis : unsigned short;
  enum class Button : unsigned short;

  std::function<void(QByteArray const&)> m_sendData;
  QAccelerometer m_accelerometer;
  QGyroscope m_gyroscope;
  QTimer m_timer;
  int const m_timerInterval = 500;
  bool m_doSendGyro = false;

  void sendButtonData(Button code, int val);
  void sendAxisData(Axis code, int maxAbsAxisVal, qreal val);
  void sendGyroData();

 public:
  Gamepad(QWidget* parent = nullptr,
          std::function<void(QByteArray const&)> const& sendData = {});

  void enableGyro();
  void disableGyro();
};

#endif  // GAMEPAD_H
