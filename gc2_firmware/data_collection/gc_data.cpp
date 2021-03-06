#include "gc_data.h"
#include "common.h"

GcData::GcData(GcClient &gc_client) : m_gc_client(gc_client),
p_battery_charge(0), m_last_report_battery_time(-REPORT_BATTERY_INTERVAL), m_report_status_battery(false),
m_simulation_mode(false), m_emg_beep(false){
}

void GcData::init() {
  //  set pin modes
  pinMode(EMG_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON1_PIN, INPUT);
  pinMode(BUTTON2_PIN, INPUT);

  // initialize various sensors

  if(USE_IMU) {
    // IMU setup
    m_imu.settings.device.commInterface = IMU_MODE_I2C;
    m_imu.settings.device.mAddress = LSM9DS1_M;
    m_imu.settings.device.agAddress = LSM9DS1_AG;

    if (!m_imu.begin())
    {
      DEBUG_LOG("Failed to communicate with LSM9DS1.");
    }
  }

  // setup battery gauge
  lipo.begin();
  lipo.quickStart();

  read_battery_charge();
}

void GcData::read_battery_charge() {
    float battery_charge = lipo.getSOC();
    battery_charge = min(battery_charge, 100.0);
    p_battery_charge = battery_charge;
    m_gc_client.battery_charge(battery_charge);
}

float GcData::get_gyro_max() {
  if(! USE_IMU) {
    return 0.0;
  }
  // retrieve the highest gyro rate on 3 axes
  m_imu.readGyro();
  float gyro_x = m_imu.calcGyro(m_imu.gx);
  float gyro_y = m_imu.calcGyro(m_imu.gy);
  float gyro_z = m_imu.calcGyro(m_imu.gz);

  return max(max(gyro_x, gyro_y), gyro_z);
}

void GcData::get_accel(float *accel_values) {
  if(! USE_IMU) {
    accel_values[0] = 0.0;
    accel_values[1] = 0.0;
    accel_values[2] = 0.0;
  } else {
    m_imu.readAccel();
    accel_values[0] = m_imu.calcAccel(m_imu.ax);
    accel_values[1] = m_imu.calcAccel(m_imu.ay);
    accel_values[2] = m_imu.calcAccel(m_imu.az);
  }
}

uint16_t GcData::read_emg() {
  if(m_simulation_mode) {
    unsigned long milliseconds = millis();
    unsigned long seconds = milliseconds / 1000;
    uint16_t minRand;
    uint16_t maxRand;
    if(seconds % 60 == 0 || seconds % 61 == 0 || seconds % 62 == 0 || seconds % 63 == 0
       || seconds % 64 == 0 || seconds % 65 == 0
    ) {
      // every N seconds
      minRand = 350;
      maxRand = 600;
    }  else {
      minRand = 85;
      maxRand = 125;
    }
    return rand() % (maxRand-minRand+1) + minRand;
  } else {
    return analogRead(A0);
  }
}

void GcData::emg_beep(uint16_t emg_value) {
  if(! m_emg_beep) {
    return;
  }

  if(emg_value > 1000) {
    tone(BUZZER_PIN, 2000, 80);
  } else if(emg_value > 500) {
    tone(BUZZER_PIN, 1200, 80);
  } else if(emg_value > 300) {
    tone(BUZZER_PIN, 800, 80);
  }

}

void GcData::report_battery_charge() {
  DEBUG_LOG("Reporting battery charge");
  read_battery_charge();
  m_last_report_battery_time = millis();
  m_gc_client.report_battery_charge();
}

bool GcData::need_report_battery_charge() {
  if(m_report_status_battery) {
    // status / battery report request
    m_report_status_battery = false;
    return true;
  }
  if (millis() - m_last_report_battery_time > REPORT_BATTERY_INTERVAL) {
    return true;
  }
  return false;
}

void GcData::queue_status_battery_charge() {
  m_report_status_battery = true;
}

void GcData::collect_data(bool upload_requested) {

  uint16_t emg_value = read_emg();
  emg_beep(emg_value);
  float gyro_max = get_gyro_max();
  float accel_values[3];
  get_accel(accel_values);

  float accel_x = accel_values[0];
  float accel_y = accel_values[1];
  float accel_z = accel_values[2];

  m_gc_client.add_datapoint(emg_value, gyro_max, accel_x, accel_y, accel_z);

  if(need_report_battery_charge()){
    report_battery_charge();
  }

  if(m_gc_client.need_upload() || upload_requested){
    DEBUG_LOG("need to upload batch");

    // turn on WiFi
    if(MANAGE_WIFI) {
      DEBUG_LOG("enabling wifi");
      WiFi.on();
    }
    DEBUG_LOG("wait for wifi to be available");
    // wait for wifi to be available
    waitFor(WiFi.ready, WIFI_MAX_WAIT);
    DEBUG_LOG("wifi available");

    m_gc_client.upload_batch();

    // turn off wifi
    if(MANAGE_WIFI) {
        DEBUG_LOG("disabling wifi");
        WiFi.off();
    }

  }
}
