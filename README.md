# IoT-Soil-Monitoring-Device
Arduino Code for IoT Soil Monitoring Device
- - - -
**SensorTest** is used for obtain data from sensors:
* AHT10 for detecting temperature & humidity
* Light intensity sensor: Light dependent resistor
* pH Sensor
* Soil Moisture Sensor

Also it used as controlling water pump for water spraying.
- - - -
**wifi_to_cloud_test_1** is used to receive data from **SensorTest** and pass the data to AWS IoT.

Also it used as receiving message from AWS IoT and then pass to **SensorTest** for controlling Arduino.
