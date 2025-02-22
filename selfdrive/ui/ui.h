#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <optional>

#include <QObject>
#include <QTimer>
#include <QColor>
#include <QTransform>
#include "nanovg.h"

#include "cereal/messaging/messaging.h"
#include "common/transformations/orientation.hpp"
#include "selfdrive/camerad/cameras/camera_common.h"
#include "selfdrive/common/mat.h"
#include "selfdrive/common/modeldata.h"
#include "selfdrive/common/params.h"
#include "selfdrive/common/util.h"
#include "selfdrive/common/visionimg.h"

#define COLOR_BLACK nvgRGBA(0, 0, 0, 255)
#define COLOR_BLACK_ALPHA(x) nvgRGBA(0, 0, 0, x)
#define COLOR_WHITE nvgRGBA(255, 255, 255, 255)
#define COLOR_WHITE_ALPHA(x) nvgRGBA(255, 255, 255, x)
#define COLOR_RED nvgRGBA(255, 0, 0, 255)
#define COLOR_RED_ALPHA(x) nvgRGBA(255, 0, 0, x)
#define COLOR_YELLOW nvgRGBA(255, 255, 0, 255)
#define COLOR_YELLOW_ALPHA(x) nvgRGBA(255, 255, 0, x)
#define COLOR_ENGAGED nvgRGBA(23, 134, 68, 255)
#define COLOR_ENGAGED_ALPHA(x) nvgRGBA(23, 134, 68, x)
#define COLOR_WARNING nvgRGBA(218, 111, 37, 255)
#define COLOR_WARNING_ALPHA(x) nvgRGBA(218, 111, 37, x)
#define COLOR_ENGAGEABLE nvgRGBA(23, 51, 73, 255)
#define COLOR_ENGAGEABLE_ALPHA(x) nvgRGBA(23, 51, 73, x)
#define COLOR_LIME nvgRGBA(120, 255, 120, 255)
#define COLOR_LIME_ALPHA(x) nvgRGBA(120, 255, 120, x)

const int bdr_s = 10;
const int header_h = 420;
const int footer_h = 280;

const int UI_FREQ = 20;   // Hz
typedef cereal::CarControl::HUDControl::AudibleAlert AudibleAlert;

// TODO: this is also hardcoded in common/transformations/camera.py
// TODO: choose based on frame input size
const float y_offset = Hardware::EON() ? 0.0 : 150.0;
const float ZOOM = Hardware::EON() ? 2138.5 : 2912.8;

typedef struct Rect {
  int x, y, w, h;
  int centerX() const { return x + w / 2; }
  int centerY() const { return y + h / 2; }
  int right() const { return x + w; }
  int bottom() const { return y + h; }
  bool ptInRect(int px, int py) const {
    return px >= x && px < (x + w) && py >= y && py < (y + h);
  }
} Rect;

struct Alert {
  QString text1;
  QString text2;
  QString type;
  cereal::ControlsState::AlertSize size;
  AudibleAlert sound;
  bool equal(const Alert &a2) {
    return text1 == a2.text1 && text2 == a2.text2 && type == a2.type && sound == a2.sound;
  }

  static Alert get(const SubMaster &sm, uint64_t started_frame) {
    if (sm.updated("controlsState")) {
      const cereal::ControlsState::Reader &cs = sm["controlsState"].getControlsState();
      return {cs.getAlertText1().cStr(), cs.getAlertText2().cStr(),
              cs.getAlertType().cStr(), cs.getAlertSize(),
              cs.getAlertSound()};
    } else if ((sm.frame - started_frame) > 5 * UI_FREQ) {
      const int CONTROLS_TIMEOUT = 5;
      // Handle controls timeout
      if (sm.rcv_frame("controlsState") < started_frame) {
        // car is started, but controlsState hasn't been seen at all
        return {"오픈파일럿을 사용할수없습니다", "프로세스가 준비중입니다",
                "프로세스가 준비중입니다", cereal::ControlsState::AlertSize::MID,
                                      AudibleAlert::NONE};
      } else if ((nanos_since_boot() - sm.rcv_time("controlsState")) / 1e9 > CONTROLS_TIMEOUT) {
        // car is started, but controls is lagging or died
        return {"핸들을 잡아주세요", "프로세스가 응답하지않습니다",
                "프로세스가 응답하지않습니다", cereal::ControlsState::AlertSize::FULL,
                AudibleAlert::WARNING_IMMEDIATE};
      }
    }
    return {};
  }
};

typedef enum UIStatus {
  STATUS_DISENGAGED,
  STATUS_ENGAGED,
  STATUS_WARNING,
  STATUS_ALERT,
} UIStatus;

const QColor bg_colors [] = {
  [STATUS_DISENGAGED] =  QColor(0x17, 0x33, 0x49, 0xc8),
  [STATUS_ENGAGED] = QColor(0x17, 0x86, 0x44, 0x01),
  [STATUS_WARNING] = QColor(0xDA, 0x6F, 0x25, 0x01),
  [STATUS_ALERT] = QColor(0xC9, 0x22, 0x31, 0xf1),
};

typedef struct {
  float x, y;
} vertex_data;

typedef struct {
  vertex_data v[TRAJECTORY_SIZE * 2];
  int cnt;
} line_vertices_data;

typedef struct UIScene {

  mat3 view_from_calib;
  bool world_objects_visible;

  // ui add
  float cpuTempAvg;
  int lateralControlSelect;
  float output_scale;
  bool leftBlinker, rightBlinker;
  int blinkingrate;

  // gps
  int satelliteCount;
  float gpsAccuracy;

  cereal::PandaState::PandaType pandaType;

  // modelV2
  float lane_line_probs[4];
  float road_edge_stds[2];
  line_vertices_data track_vertices;
  line_vertices_data lane_line_vertices[4];
  line_vertices_data road_edge_vertices[2];

  bool dm_active, engageable;

  // lead
  vertex_data lead_vertices_radar[2];
  vertex_data lead_vertices[2];

  float light_sensor, accel_sensor, gyro_sensor;
  bool started, ignition, is_metric, longitudinal_control, end_to_end;
  uint64_t started_frame;

  // neokii dev UI
  cereal::CarControl::Reader car_control;
  cereal::DeviceState::Reader device_state;
  cereal::CarState::Reader car_state;
  cereal::ControlsState::Reader controls_state;
  cereal::CarParams::Reader car_params;
  cereal::GpsLocationData::Reader gps_ext;
  cereal::LiveParametersData::Reader live_params;

} UIScene;

typedef struct UIState {
  int fb_w = 0, fb_h = 0;
  NVGcontext *vg;

  // images
  std::map<std::string, int> images;

  std::unique_ptr<SubMaster> sm;

  UIStatus status;
  UIScene scene = {};

  bool awake;
  bool has_prime = false;

  QTransform car_space_transform;
  bool wide_camera;

  float running_time;

  //
  int lock_on_anim_index;

} UIState;


class QUIState : public QObject {
  Q_OBJECT

public:
  QUIState(QObject* parent = 0);

  // TODO: get rid of this, only use signal
  inline static UIState ui_state = {0};

signals:
  void uiUpdate(const UIState &s);
  void offroadTransition(bool offroad);

private slots:
  void update();

private:
  QTimer *timer;
  bool started_prev = true;
};


// device management class

class Device : public QObject {
  Q_OBJECT

public:
  Device(QObject *parent = 0);

private:
  // auto brightness
  const float accel_samples = 5*UI_FREQ;

  bool awake = false;
  int awake_timeout = 0;
  float accel_prev = 0;
  float gyro_prev = 0;
  int last_brightness = 0;
  FirstOrderFilter brightness_filter;

  QTimer *timer;

  void updateBrightness(const UIState &s);
  void updateWakefulness(const UIState &s);

signals:
  void displayPowerChanged(bool on);

public slots:
  void setAwake(bool on, bool reset);
  void update(const UIState &s);
};
