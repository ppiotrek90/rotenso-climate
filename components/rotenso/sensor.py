import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    UNIT_CELSIUS,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    ICON_THERMOMETER,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from . import climate as rotenso_climate

CONF_ROTENSO_ID = "rotenso_id"
CONF_COIL_TEMPERATURE = "coil_temperature"
CONF_ROOM_TEMPERATURE = "room_temperature"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ROTENSO_ID): cv.use_id(rotenso_climate.RotensoClimate),
        cv.Optional(CONF_COIL_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_THERMOMETER,
            accuracy_decimals=1,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        # Same value as the current temperature
        cv.Optional(CONF_ROOM_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_THERMOMETER,
            accuracy_decimals=1,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ROTENSO_ID])

    if CONF_COIL_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_COIL_TEMPERATURE])
        cg.add(parent.set_coil_temperature_sensor(sens))

    if CONF_ROOM_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_ROOM_TEMPERATURE])
        cg.add(parent.set_room_temperature_sensor(sens))