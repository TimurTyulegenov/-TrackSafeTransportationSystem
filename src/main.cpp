/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <events/mbed_events.h>
#include "mbed.h"
#include "ble/BLE.h"
#include "ble/services/HealthThermometerService.h"
#include "pretty_printer.h"
#include <Hx711.h>

DigitalOut led1(LED1, 1);
Hx711 s1(P0_11,P0_12);

UUID user_uuid("00001101-0000-1000-8000-00805F9B34FB");
GattCharacteristic characteristic(
        user_uuid,
        /* valuePtr */ NULL,
        /* len */ 1,
        /* maxLen */ 20,
        /* props */ GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE
        );

const static char DEVICE_NAME[] = "Trak";

static events::EventQueue event_queue(/* event count */ 16 * EVENTS_EVENT_SIZE);

class Track : ble::Gap::EventHandler {
public:
    Track(BLE &ble, events::EventQueue &event_queue) :
        _ble(ble),
        _event_queue(event_queue),
        _sensor_event_id(0),
        _thermometer_uuid(GattService::UUID_HEALTH_THERMOMETER_SERVICE),
        _user_uuid(user_uuid),
        _current_temperature(39.6f),
        _thermometer_service(NULL),
        _adv_data_builder(_adv_buffer) { }

    void start() {

        offset = (int)s1.read();
        s1.set_offset(offset);
        s1.set_scale(calibration_factor);

        _ble.gap().setEventHandler(this);

        _ble.init(this, &Track::on_init_complete);

        _event_queue.call_every(500, this, &Track::blink);

        _event_queue.dispatch_forever();

        printf("BLE START. \n");
    }

private:
    /** Callback triggered when the ble initialization process has finished */
    void on_init_complete(BLE::InitializationCompleteCallbackContext *params) {
        if (params->error != BLE_ERROR_NONE) {
            print_error(params->error, "Ble initialization failed.");
            return;
        }

        print_mac_address();

        /* Setup primary service. */
        _thermometer_service = new HealthThermometerService(_ble, _current_temperature, HealthThermometerService::LOCATION_EAR);

        start_advertising();
    }

    void start_advertising() {
        /* Create advertising parameters and payload */

        ble::AdvertisingParameters adv_parameters(
            ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
            ble::adv_interval_t(ble::millisecond_t(1000))
        );

        _adv_data_builder.setFlags();
        UUID h[2]={_thermometer_uuid,_user_uuid};
        //_adv_data_builder.setLocalServiceList(mbed::make_Span(h, 2));
        _adv_data_builder.setAppearance(ble::adv_data_appearance_t::THERMOMETER_EAR);
        _adv_data_builder.setName(DEVICE_NAME);

        /* Setup advertising */

        ble_error_t error = _ble.gap().setAdvertisingParameters(
            ble::LEGACY_ADVERTISING_HANDLE,
            adv_parameters
        );

        if (error) {
            print_error(error, "_ble.gap().setAdvertisingParameters() failed");
            return;
        }

        error = _ble.gap().setAdvertisingPayload(
            ble::LEGACY_ADVERTISING_HANDLE,
            _adv_data_builder.getAdvertisingData()
        );

        if (error) {
            print_error(error, "_ble.gap().setAdvertisingPayload() failed");
            return;
        }

        /* Start advertising */

        error = _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);

        if (error) {
            print_error(error, "_ble.gap().startAdvertising() failed");
            return;
        }
    }

    void update_sensor_value() {
        units_raw = s1.readRaw();
        units = s1.read();
        if (units < 0)
        {
            units = 0.00;
        }
        printf("Actual reading: %f grams. \n",units);
        _thermometer_service->updateTemperature(units);
    }

    void blink(void) {
        led1 = !led1;
    }

private:
    /* Event handler */

    virtual void onDisconnectionComplete(const ble::DisconnectionCompleteEvent&) {
        _event_queue.cancel(_sensor_event_id);
        _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
    }

    virtual void onConnectionComplete(const ble::ConnectionCompleteEvent &event) {
        if (event.getStatus() == BLE_ERROR_NONE) {
            _sensor_event_id = _event_queue.call_every(1000, this, &Track::update_sensor_value);
        }
    }

private:
    BLE &_ble;
    events::EventQueue &_event_queue;

    int _sensor_event_id;

    UUID _thermometer_uuid;
    UUID _user_uuid;

    float _current_temperature;
    HealthThermometerService *_thermometer_service;

    uint8_t _adv_buffer[ble::LEGACY_ADVERTISING_MAX_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;

    float   calibration_factor = -140.35f; // this calibration factor is adjusted according to my load cel
    int32_t units_raw;
    float   units;
    float   ounces;
    int     offset = 0;

};

/** Schedule processing of events from the BLE middleware in the event queue. */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context) {
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

int main()
{
    printf("Start program. \n");
    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(schedule_ble_events);

    Track demo(ble, event_queue);
    demo.start();

    return 0;
}
