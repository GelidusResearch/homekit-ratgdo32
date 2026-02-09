/****************************************************************************
 * RATGDO HomeKit for ESP32
 * https://ratcloud.llc
 * https://github.com/PaulWieland/ratgdo
 *
 * Copyright (c) 2024-25 David A Kerr... https://github.com/dkerr64/
 * All Rights Reserved.
 * Licensed under terms of the GPL-3.0 License.
 *
 */
#ifdef RATGDO32_DISCO
// C/C++ language includes
#include <bitset>

// Arduino includes
#include <Wire.h>
#include <vl53l1x_class.h>
#include "Ticker.h"

// RATGDO project includes
#include "ratgdo.h"
#include "led.h"
#include "vehicle.h"
#include "homekit.h"
#include "config.h"

// Logger tag
static const char *TAG = "ratgdo-vehicle";
bool vehicle_setup_done = false;
bool vehicle_setup_error = false;

VL53L1X distanceSensor(&Wire, SENSOR_SHUTDOWN_PIN);

static const int32_t MIN_DISTANCE = 25; // ignore bugs crawling on the distance sensor

int16_t vehicleDistance = 0;
int16_t vehicleThresholdDistance = 1000; // set by user
char vehicleStatus[16] = "Away";         // or Arriving or Departing or Parked
bool vehicleStatusChange = false;

static bool vehicleDetected = false;
static bool vehicleArriving = false;
static bool vehicleDeparting = false;
_millis_t lastVehicleChangeAt = 0;
static _millis_t presence_timer = 0; // to be set by door open action
static _millis_t vehicle_motion_timer = 0;

static constexpr uint32_t PRESENCE_DETECT_DURATION = (5 * 60 * 1000); // how long to calculate presence after door state change
// increasing these values increases reliability but also increases detection time
static constexpr uint32_t PRESENCE_DETECTION_ON_THRESHOLD = 5; // Minimum percentage of valid samples required to detect vehicle
static constexpr uint32_t PRESENCE_DETECTION_OFF_DEBOUNCE = 2; // The number of consecutive iterations that must be 0 before clearing vehicle detected state
static constexpr int32_t VEHICLE_AVERAGE_OVER = 50;            // vehicle distance measure is averaged over last X samples
static std::bitset<256> distanceInRange;                       // the length of this bitset determines how many out of range readings are required for presence detection to change states
void calculatePresence(int32_t distance);

void setup_vehicle()
{
    uint8_t status = 0;

    if (vehicle_setup_done || vehicle_setup_error)
        return;

    ESP_LOGI(TAG, "=== Setup VL53L1X time-of-flight sensor ===");

#ifdef GRGDO1_V1
    // GRGDO1 v1: Serial pins conflict with I2C pins, redirect to Serial1
    Serial1.begin(115200);
    Serial.end();
    Serial = Serial1;
#endif

    if (!Wire.begin(SENSOR_SDA_PIN, SENSOR_SCL_PIN))
    {
        ESP_LOGE(TAG, "VL53L1X I2C pin setup failed");
        vehicle_setup_error = true;
#ifdef GRGDO1_V1
        // Restore Serial to UART0 for Improv
        Serial1.end();
        Serial0.begin(115200);
        Serial = Serial0;
#endif
        return;
    }

    // Check if sensor is present at I2C address 0x29
    Wire.beginTransmission(0x29);
    if (Wire.endTransmission() != 0)
    {
        ESP_LOGE(TAG, "VL53L1X ToF not detected at address 0x29");
        Wire.end();
        vehicle_setup_error = true;
#ifdef GRGDO1_V1
        // Restore Serial to UART0 for Improv
        Serial1.end();
        Serial0.begin(115200);
        Serial = Serial0;
#endif
        return;
    }
    ESP_LOGI(TAG, "VL53L1X ToF detected at address 0x29");

    // Initialize sensor
    distanceSensor.begin();

    // Power off sensor first
    distanceSensor.VL53L1X_Off();
    delay(10); // Wait for sensor to power down

    // Power on sensor
    distanceSensor.VL53L1X_On();
    delay(10); // Wait for sensor to power up

    // Initialize sensor with default I2C address (0x29 in 7-bit, which is 0x52 in 8-bit)
    status = distanceSensor.InitSensor(0x29 << 1);
    if (status != 0)
    {
        ESP_LOGE(TAG, "VL53L1X failed to initialize error: %d", status);
        Wire.end();
        vehicle_setup_error = true;
#ifdef GRGDO1_V1
        // Restore Serial to UART0 for Improv
        Serial1.end();
        Serial0.begin(115200);
        Serial = Serial0;
#endif
        return;
    }

    status = distanceSensor.VL53L1X_SetDistanceMode(2); // 2 = Long distance mode (up to 4m)
    if (status != 0)
    {
        ESP_LOGE(TAG, "VL53L1X_SetDistanceMode error: %d", status);
        vehicle_setup_error = true;
        return;
    }
    status = distanceSensor.VL53L1X_SetTimingBudgetInMs(100); // Set timing budget to 100ms
    if (status != 0)
    {
        ESP_LOGE(TAG, "VL53L1X_SetTimingBudgetInMs error: %d", status);
        vehicle_setup_error = true;
        return;
    }
    status = distanceSensor.VL53L1X_SetInterMeasurementInMs(100); // Set inter-measurement period to 100ms
    if (status != 0)
    {
        ESP_LOGE(TAG, "VL53L1X_SetInterMeasurementInMs error: %d", status);
        vehicle_setup_error = true;
        return;
    }
    status = distanceSensor.VL53L1X_StartRanging();
    if (status != 0)
    {
        ESP_LOGE(TAG, "VL53L1X_StartRanging error: %d", status);
        vehicle_setup_error = true;
        return;
    }

    garage_door.has_distance_sensor = true;
    nvRam->write(nvram_has_distance, 1);
    vehicleThresholdDistance = userConfig->getVehicleThreshold() * 10; // convert centimeters to millimeters
    enable_service_homekit_vehicle(userConfig->getVehicleHomeKit());
    vehicle_setup_done = true;
}

void vehicle_loop()
{
    if (!vehicle_setup_done)
        return;

    uint8_t dataReady = 0;
    uint8_t status = 0;
    if ((status = distanceSensor.VL53L1X_CheckForDataReady(&dataReady)) == 0 && dataReady > 0)
    {
        uint16_t distance_mm = 0;
        uint8_t rangeStatus = 0;

        status = distanceSensor.VL53L1X_GetRangeStatus(&rangeStatus);
        if (status == 0)
        {
            status = distanceSensor.VL53L1X_GetDistance(&distance_mm);
            if (status == 0)
            {
                int16_t distance = -1;

                // VL53L1X range status codes:
                // 0: Valid range
                // 1: Sigma fail (low confidence)
                // 2: Signal fail
                // 4: Out of bounds (phase)
                // 7: Wraparound
                switch (rangeStatus)
                {
                case 0: // Valid range
                    distance = distance_mm;
                    break;
                case 1: // Sigma fail
                    ESP_LOGW(TAG, "Vehicle distance sensor sigma fail. Sensor may be pointing at glass, try repositioning: %dmm", distance_mm);
                    distance = distance_mm; // Use data but with caution
                    break;
                case 2: // Signal fail
                    ESP_LOGV(TAG, "Vehicle distance signal fail: %dmm", distance_mm);
                    distance = MAX_DISTANCE; // Assume no object
                    break;
                case 4: // Out of bounds
                    ESP_LOGV(TAG, "Vehicle distance out of bounds: %dmm", distance_mm);
                    distance = MAX_DISTANCE; // Assume no object
                    break;
                case 7: // Wraparound
                    ESP_LOGV(TAG, "Vehicle distance wrap target fail: %dmm", distance_mm);
                    break;
                default:
                    ESP_LOGE(TAG, "Unhandled VL53L1X Range Status: %d, Range: %dmm", rangeStatus, distance_mm);
                    break;
                }

                if (distance >= 0)
                {
                    calculatePresence(distance);
                }
            }
            else
            {
                ESP_LOGE(TAG, "VL53L1X_GetDistance reports error: %d", status);
            }
        }
        else
        {
            ESP_LOGE(TAG, "VL53L1X_GetRangeStatus reports error: %d", status);
        }

        // Clear the interrupt
        status = distanceSensor.VL53L1X_ClearInterrupt();
        if (status != 0)
        {
            ESP_LOGV(TAG, "VL53L1X_ClearInterrupt reports error: %d", status);
        }
    }
    else
    {
        if (status != 0)
            ESP_LOGE(TAG, "VL53L1X_CheckForDataReady reports error: %d", status);
    }

    _millis_t current_millis = _millis();
    // Vehicle Arriving Clear Timer
    if (vehicleArriving && (current_millis - vehicle_motion_timer) > MOTION_TIMER_DURATION)
    {
        vehicleArriving = false;
        strlcpy(vehicleStatus, vehicleDetected ? "Parked" : "Away", sizeof(vehicleStatus));
        vehicleStatusChange = true;
        ESP_LOGI(TAG, "Vehicle %s at %s", vehicleStatus, timeString());
        notify_homekit_vehicle_arriving(false);
    }
    // Vehicle Departing Clear Timer
    if (vehicleDeparting && (current_millis - vehicle_motion_timer) > MOTION_TIMER_DURATION)
    {
        vehicleDeparting = false;
        strlcpy(vehicleStatus, vehicleDetected ? "Parked" : "Away", sizeof(vehicleStatus));
        vehicleStatusChange = true;
        ESP_LOGI(TAG, "Vehicle %s at %s", vehicleStatus, timeString());
        notify_homekit_vehicle_departing(false);
    }
}

void setArriveDepart(bool vehiclePresent)
{
    static bool lastVehiclePresent = false;
    static bool doneOnce = false;

    if (doneOnce && vehiclePresent == lastVehiclePresent)
        return;

    // Only continue if the vehicle presence state has changed, or we don't know previous state.
    lastVehiclePresent = vehiclePresent;
    doneOnce = true;
    if (vehiclePresent)
    {
        if (!vehicleArriving)
        {
            vehicleArriving = true;
            vehicleDeparting = false;
            vehicle_motion_timer = lastVehicleChangeAt;
            strlcpy(vehicleStatus, "Arriving", sizeof(vehicleStatus));
            if (userConfig->getAssistDuration() > 0)
                laser.flash(userConfig->getAssistDuration() * 1000);
            vehicleStatusChange = true;
            ESP_LOGI(TAG, "Vehicle %s at %s", vehicleStatus, timeString());
            notify_homekit_vehicle_arriving(true);
        }
    }
    else
    {
        if (!vehicleDeparting)
        {
            vehicleArriving = false;
            vehicleDeparting = true;
            vehicle_motion_timer = lastVehicleChangeAt;
            strlcpy(vehicleStatus, "Departing", sizeof(vehicleStatus));
            vehicleStatusChange = true;
            ESP_LOGI(TAG, "Vehicle %s at %s", vehicleStatus, timeString());
            notify_homekit_vehicle_departing(true);
        }
    }
}

void calculatePresence(int32_t distance)
{
    if (distance < MIN_DISTANCE)
        return;

    // Test for change in vehicle presence
    bool priorVehicleDetected = vehicleDetected;

    distanceInRange <<= 1;
    distanceInRange.set(0, distance <= vehicleThresholdDistance);
    uint32_t percent = distanceInRange.count() * 100 / distanceInRange.size();
    static uint32_t last_percent = UINT32_MAX;
    static uint32_t off_counter = 0;

    if (percent >= PRESENCE_DETECTION_ON_THRESHOLD)
    {
        vehicleDetected = true;
    }
    else if (percent == 0 && vehicleDetected)
    {
        off_counter++;
        ESP_LOGV(TAG, "Vehicle distance off_counter: %d", off_counter);
        if (off_counter / distanceInRange.size() >= PRESENCE_DETECTION_OFF_DEBOUNCE)
        {
            off_counter = 0;
            vehicleDetected = false;
        }
    }

    if (percent != last_percent)
    {
        last_percent = percent;
        off_counter = 0;
        ESP_LOGV(TAG, "Vehicle distance in-range: %d%%", percent);
    }

    // calculate average over sample size to smooth out changes
    static double average = 0;
    static int32_t count = 0;
    if (count < VEHICLE_AVERAGE_OVER)
        count++;
    // must use double float math, integer math does not work
    average = average + (static_cast<double>(distance) - average) / count;
    // convert from millimeters to centimeters
    vehicleDistance = static_cast<int16_t>(std::round(average / 10));
    static int16_t lastDistance = 0;
    static uint32_t lastChange = 0;
    lastChange++;
    if (vehicleDistance != lastDistance)
    {

        ESP_LOGD(TAG, "Vehicle distance: %dcm (average over %d samples, last changed %d samples ago), current measured: %dcm", vehicleDistance, count, lastChange, distance / 10);
        lastDistance = vehicleDistance;
        lastChange = 0;
    }

    if (vehicleDetected != priorVehicleDetected)
    {
        // if change occurs with arrival/departure window then record motion,
        // presence timer is set when door opens or closes.
        lastVehicleChangeAt = _millis();
        if (presence_timer && ((lastVehicleChangeAt - presence_timer) < PRESENCE_DETECT_DURATION))
        {
            presence_timer = 0;
            setArriveDepart(vehicleDetected);
        }
        else
        {
            strlcpy(vehicleStatus, vehicleDetected ? "Parked" : "Away", sizeof(vehicleStatus));
            ESP_LOGI(TAG, "Vehicle %s at %s", vehicleStatus, timeString());
        }
        vehicleStatusChange = true;
        notify_homekit_vehicle_occupancy(vehicleDetected);
    }
}

// if notified of door opening, set timeout during which we check for arriving/departing vehicle (looking forward)
void doorOpening()
{
    if (!vehicle_setup_done)
        return;

    presence_timer = _millis();
}

// if notified of door closing, check for arrived/departed vehicle within time window (looking back)
void doorClosing()
{
    if (!vehicle_setup_done)
        return;

    presence_timer = _millis();
    // On door closing, the vehicle status change could already have happened...
    if (lastVehicleChangeAt && ((presence_timer - lastVehicleChangeAt) < PRESENCE_DETECT_DURATION))
    {
        setArriveDepart(vehicleDetected);
    }
}
#endif // RATGDO32_DISCO
