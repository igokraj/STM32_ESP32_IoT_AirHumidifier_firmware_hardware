#include "main.hpp"
#include "stm32f4xx_hal.h"
#include "main.h"
#include "i2c.h"
#include "humiditySensor.hpp"

enum class SystemStatus_t {
    Waiting,
    Running,
    EmptyContainer,
    Error
};


class Humidifier {

public:
    Humidifier(float desiredHum) : desiredHum_(desiredHum) {}

    // Getters
    SystemStatus_t getSystemStatus() const {
        return systemStatus_;
    }

    // Setters
    void setCurrentHum(float currentHum) {
        currentHum_ = currentHum;
    }

    void setDesiredHum(float desiredHum) {
        desiredHum_ = desiredHum;
    }

    void setContainerEmpty(bool containerEmpty) {
        containerEmpty_ = containerEmpty;
    }

    void setSystemFailed(bool systemFailed) {
        systemFailed_ = systemFailed;
    }

    void setSensorFailed(bool sensorFailed) {
        sensorFailed_ = sensorFailed;
    }

    
    void updateStatus() {

        if (systemFailed_ || sensorFailed_) {
            systemStatus_ = SystemStatus_t::Error;
            return;
        }

        if (containerEmpty_) {
            systemStatus_ = SystemStatus_t::EmptyContainer;
            return;
        }

        if (currentHum_ < desiredHum_) {
            systemStatus_ = SystemStatus_t::Running;
        }
        else if (currentHum_ >= desiredHum_) {
            systemStatus_ = SystemStatus_t::Waiting;
        }
    }

private:
    float currentHum_ = 0.0f;   // measured humidity [%]
    float desiredHum_;          // setpoint humidity [%]

    bool containerEmpty_ = false;
    bool systemFailed_ = false;
    bool sensorFailed_ = false;

    // Set inicialization status for: Waiting
    SystemStatus_t systemStatus_ = SystemStatus_t::Waiting;
};


void app_main() {

    Humidifier AirHumidifier(50.0f); // Set inicialization humidity as 50%
    HumiditySensor HTU21D(&hi2c1, 0x40); 

    while (true) {

        // read sensors before updateStatus()
        AirHumidifier.setCurrentHum(HTU21D.readHumidity());

        AirHumidifier.updateStatus();

    }
}
