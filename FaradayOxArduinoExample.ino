#include "faradaic_registers.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
	bool is_initialized;
	uint32_t id;
	uint8_t reg_map_ver_minor;
	uint8_t reg_map_ver_major;
} sensor_context_t;

#define PRINT_BUFFER_SIZE 128

static uint8_t rx_buffer[REGISTER_MAP_SIZE + 1]; // To fit in the whole register map + '\n'
char print_buffer[PRINT_BUFFER_SIZE + 1]; // To fit \0 at the end for sprintf
static sensor_context_t sensor = {
	.is_initialized = false,
	.id = 0,
	.reg_map_ver_minor = 0,
	.reg_map_ver_major = 0
};

static void sensor_get_metadata(sensor_context_t *sensor);
static void sensor_measurement(sensor_context_t *sensor);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial1.begin(9600);

  snprintf(print_buffer, PRINT_BUFFER_SIZE, "\r\nFaraday-Ox Example v%u.%u\r\n", APP_VER_MAJOR, APP_VER_MINOR);
  Serial.print(print_buffer);
  sensor_get_metadata(&sensor);
	if (sensor.is_initialized)
	{
		sensor_measurement(&sensor);
	}
}

void loop() {
}

static void sensor_get_metadata(sensor_context_t *sensor)
{
	uint8_t cmd[CMD_SIZE] = { 0 };

	// 1. Read the whole Register Map
	// Command size if 4 bytes.
	// Read 0x60 bytes starting from address 0x00

	cmd[0] = CMD_READ;
	cmd[1] = 0x00;
	cmd[2] = REGISTER_MAP_SIZE;
	cmd[3] = CMD_NEWLINE;

  Serial1.write(cmd, CMD_SIZE);
  size_t num = Serial1.readBytes(rx_buffer, REGISTER_MAP_SIZE + 1);
	if (num != REGISTER_MAP_SIZE + 1)
	{
		return;
	}

	// 2. Get Register Map Version
	sensor->reg_map_ver_minor = rx_buffer[REG_MAP_VER_LSB];
	sensor->reg_map_ver_major = rx_buffer[REG_MAP_VER_MSB];

	// 3. Get Module ID
	sensor->id = *(uint32_t *)&rx_buffer[REG_FRONTEND_ID_LLSB];

	snprintf(print_buffer, PRINT_BUFFER_SIZE, "Module ID: %lu, Register Ver: %u.%u\r\n", sensor->id,
			sensor->reg_map_ver_major, sensor->reg_map_ver_minor);
	
	Serial.print(print_buffer);

	sensor->is_initialized = true;
	
	return;
}


static void sensor_measurement(sensor_context_t *sensor)
{
	(void)sensor;

	// 1. Start the measurement
	uint8_t cmd[4] = { 0 };
	cmd[0] = CMD_WRITE;
	cmd[1] = REG_CONTROL;
	cmd[2] = REG_CONTROL_START_MEASUREMENT;
	cmd[3] = CMD_NEWLINE;

  Serial1.write(cmd, CMD_SIZE);
  size_t num = Serial1.readBytes(rx_buffer, CMD_WRITE_ACK_SIZE);
	if (num != CMD_WRITE_ACK_SIZE)
	{
		return;
	}

	delay(500); // Wait 500ms for the measurement to finish

	memset(rx_buffer, 0, REGISTER_MAP_SIZE + 1);
	cmd[0] = CMD_READ;
	cmd[1] = 0x00;
	cmd[2] = REGISTER_MAP_SIZE;
	cmd[3] = CMD_NEWLINE;

  Serial1.write(cmd, CMD_SIZE);
  num = Serial1.readBytes(rx_buffer, REGISTER_MAP_SIZE + 1);
	if (num != REGISTER_MAP_SIZE + 1)
	{
		return;
	}

	// 2. Check that Status register indicates that measurement was finished successfully
	if (rx_buffer[REG_STATUS] & REG_STATUS_MEASUREMENT_IN_PROGRESS)
	{
		Serial.print("Measurement is still in progress\r\n");
		return;
	}

	if ((rx_buffer[REG_STATUS] & REG_STATUS_MEASUREMENT_FINISHED)
			&& (rx_buffer[REG_STATUS] & REG_STATUS_MEASUREMENT_ERROR))
	{
		Serial.print("Sensor error\r\n");
		return;
	}

	if ((rx_buffer[REG_STATUS] & REG_STATUS_MEASUREMENT_FINISHED)
			&& (rx_buffer[REG_STATUS] & REG_STATUS_SHT4X_ERROR))
	{
		Serial.print("SHT4x sensor error\r\n");
		return;
	}

	if (rx_buffer[REG_STATUS] == REG_STATUS_MEASUREMENT_FINISHED)
	{
		float concentration = *(float *)&rx_buffer[REG_CONCENTRATION];
		float temperature = *(float *)&rx_buffer[REG_TEMPERATURE];
		float humidity = *(float *)&rx_buffer[REG_HUMIDITY];

		Serial.print("Concentration: ");
		Serial.print(concentration);
		Serial.print(" Temperature: ");
		Serial.print(temperature);
		Serial.print(" Humidity: ");
		Serial.print(humidity);
		Serial.print("\r\n");
	}
}

