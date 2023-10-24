#include <events/mbed_events.h>
#include "mbed.h"
#include "ble/BLE.h"
#include "ble/services/HealthThermometerService.h"
#include "pretty_printer.h"

AnalogIn   ain(P0_4);
DigitalOut led1(LED2, 1);
DigitalOut out1(P0_11, 1);

const static char DEVICE_NAME[] = "TensitionSensor"

static events::EventQueue event_queue(/* event count */ 16 * EVENTS_EVENT_SIZE);

class ThermometerDemo : ble::Gap::EventHandler {
public:
    ThermometerDemo(BLE &ble, events::EventQueue &event_queue) :
        _ble(ble),
        _event_queue(event_queue),
        _sensor_event_id(0),
        _tensitionsens_uuid(GattService::UUID_HEALTH_tensitionr_service),
        _current_tensition(0),
        _tensitionr_service(NULL),
        _adv_data_builder(_adv_buffer) { }

    void start() {
        _ble.gap().setEventHandler(this);

        _ble.init(this, &ThermometerDemo::on_init_complete);

        _event_queue.call_every(1000, this, &ThermometerDemo::blink);

        _event_queue.dispatch_forever();
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
        _tensitionr_service = new HealthThermometerService(_ble, _current_tensition, HealthThermometerService::LOCATION_EAR);

        start_advertising();
    }

    void start_advertising() {
        /* Create advertising parameters and payload */

        ble::AdvertisingParameters adv_parameters(
            ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
            ble::adv_interval_t(ble::millisecond_t(1000))
        );

        _adv_data_builder.setFlags();
        _adv_data_builder.setLocalServiceList(mbed::make_Span(&_tensitionsens_uuid, 1));
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
       out1=true;
	for(int i =0;i>=50;i++)
	{
	_current_tensition =(_current_tensition +ain.read()*100)/i;
	ThisThread::sleep_for(25ms);
	}
         printf("%f \n",_current_tensition);
        _tensitionr_service->updateTemperature((_current_tensition+_current_tensition1+_current_tensition2)/3);
        out1=false;
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
            _sensor_event_id = _event_queue.call_every(1000, this, &ThermometerDemo::update_sensor_value);
        }
    }

private:
    BLE &_ble;
    events::EventQueue &_event_queue;

    int _sensor_event_id;

    UUID _tensitionsens_uuid;

    float _current_tensition;
    HealthThermometerService *_tensitionr_service;

    uint8_t _adv_buffer[ble::LEGACY_ADVERTISING_MAX_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;
};

/** Schedule processing of events from the BLE middleware in the event queue. */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context) {
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}*/
int main()
{
    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(schedule_ble_events);

    ThermometerDemo demo(ble, event_queue);
    demo.start();

    return 0;
}
