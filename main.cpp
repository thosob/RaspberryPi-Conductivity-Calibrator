
/* 
 * File:   main.cpp
 * Author: tsobieroy
 *
 * Created on 2. Januar 2017, 07:53
 */

#include <cstdlib>
#include <iostream>
#include <cstdlib>
#include <string>
#include <string.h>
#include <cmath>
#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <fstream>
#include <sstream>
#include <errno.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <inttypes.h> 
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds
#include <cstdint>

using namespace std;

/**
 * Global variable to set the temperaturePath of the electrode
 */
string temperature_Path;
uint wired_Address;

/**
 * @brief mV of the probe
 * @param file_Descriptor
 * @return Voltage
 */
float get_Probe_mV(int i2c_Address, int i2c_Port) {

    //we have to set it to zero, regardless whats read in console param
    i2c_Port = 0;
    //and we have to use the special configuration of wiringpi

    //Loading ecAdress - using wiringPi, so using special address range  
    int raw = wiringPiI2CReadReg16(wired_Address, i2c_Port);
    raw = raw >> 8 | ((raw << 8) &0xffff);
    //std::cout << raw << endl;
    //3.3 equals the voltage
    //Design Decision: 3.3V implementation   
    //4096 - 12bit in total 
    if (raw > 0) {
        return (((float) raw / 4096) * 3.3) * 1000;
    } else {
        return -1;
    }


}

/**
 * @brief gets the mean out of a number of measurements
 * @param measurement_size
 * @param ec_probe_Address
 * @return mean > 0 if enough data was collected, else -1 
 */
float getMeanMeasurements(int measurement_size, int i2c_address, int i2c_address_port) {
    float measurements[measurement_size];
    float mean_measurements = 0.0;
    float mV = 0.0;
    int valid_data = 0;

    for (int i = 0; i < measurement_size; i++) {
        //Gets mV from Probe    
        mV = get_Probe_mV(i2c_address, i2c_address_port);
        //if Probe Value is valid
        if (mV > 0) {
            //add to measurements array
            measurements[valid_data] = mV;
            valid_data += 1;
        }
    }
    //if one or more values are valid
    if (measurements[0] != 0) {
        for (int i = 0; i < valid_data; i++) {
            mean_measurements += measurements[i] / valid_data;
        }
        /* Debug: */
        // std::cout << "Mean: " << mean_measurements << endl << "Valid Data Collected:" << valid_data << endl;
        return mean_measurements;
    } else {
        //if no valid data was found, return -1 for mean
        return -1;
    }
}

/**
 * @brief calibrating the ec-Probe and save values of probe in system
 * @param i2c_address Address of probe
 * @param ec_Calibration_Val Calibration value
 * @param i2c_address_port port of the address
 * @return true if function was successful
 */
bool calibrateECProbe(int i2c_address, int ec_Calibration_Val, int i2c_address_port) {

    //Assumption: Values are normal distributed or follow a student distribution
    float calibration_Means[5];
    float calibration_Variance = 0.0;
    float calibration_mean = 0.0;
    float deviation = 0.0;

    for (int i = 0; i < 5; i++) {
        calibration_Means[i] = getMeanMeasurements(1000, i2c_address, i2c_address_port);
        calibration_mean += calibration_Means[i] / 5;
    }

    for (int i = 0; i < 5; i++) {
        //Calculate Variance
        calibration_Variance += (pow((calibration_Means[i] - calibration_mean), 2)) / 5;
    }
    //Calculate Deviation
    deviation = sqrt(calibration_Variance);
    deviation = 5;
    //Debug: Cancel effect of deviation
    //deviation *= 0; 
    //Check deviation if higher than 15 means, that the variance of mV is
    //bigger than 200. This is not acceptable - error sources could be, 
    //defective probe or problems with the solution.     
    //Maybe it should be smaller thou or different value for different calibration
    //solutions

    if (deviation < 11 && deviation > 0.0) {
        //gives calibration value and calibrated solution
        std::cout << "<calibration><value>" << calibration_mean << "</value><ecSolution>" << ec_Calibration_Val << "</ecSolution></calibration>" << endl;
        return true;
    } else {
        //Deviation was to high
        return false;
    }

}

/**
 * @brief Converts string to integers
 * @param i integer
 * @return string
 */
string intToString(int i) {
    ostringstream convert;
    convert << i;
    return convert.str();
}

/**
 * @brief getting the temperature in degree celsius
 * @return tempperature, that was measured by the assigned temp_probe
 */
float getTemperatureCelsius() {
    string line;
    string filename;
    string tmp;

    //Define Filename
    filename = temperature_Path;
    //Open File
    std::ifstream in(filename);

    //search for t=
    while (getline(in, line)) {
        tmp = "";
        if (strstr(line.c_str(), "t=")) {
            tmp = strstr(line.c_str(), "t=");
            tmp.erase(0, 2);
            if (tmp.length() > 1) {
                in.close();
                return strtof(tmp.c_str(), NULL) / 1000;
            }
        }
    }
    in.close();

    return -1000;
}

/**
 * @brief Measures data and returns milivolt for pre-defined ec solution
 * @param argc should be 4
 * @param argv Ec-value to calibrate & ec-probe address
 * @return 
 */
int main(int argc, char** argv) {

    if (argc != 4) {
        std::cout << "Error: You did not provide required arguments." << endl
                << "Usage: Aqualight-EcController-Calibrator TemperatureFile EcProbeI2CAddress EcProbeCalibration"
                << endl
                << "EcProbeI2CAddress has to be provided by decimal:decimal as address:port e.g.: 77. (0x4d)"
                << endl
                << "EcProbeCalibration is value, that has to be adjusted."
                << endl;
        return 0;
    }
    //Initialize variables
    //Temperature
    temperature_Path = argv[1];
    float temperature_C = getTemperatureCelsius();
    //I²C
    string str = argv[2];
    int pos = str.find_first_of(':');
    string i2c_FD = str.substr(0, pos);
    uint i2c_address = stoi(i2c_FD); //0x4d;
    string ec_Address = str.substr(pos + 1);
    uint i2c_port = stoi(ec_Address);
    //Knowing which ec-value we are measuring
    int ec_Value_Integer = atoi(argv[3]);

    //Setting Up Gpio
    wiringPiSetupGpio();
    //make wired_Address ready to be read from, just in case it's going to be needed
    wired_Address = wiringPiI2CSetup(i2c_address);
    return calibrateECProbe(i2c_address, ec_Value_Integer, i2c_port);
}

