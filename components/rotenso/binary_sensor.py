import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_PROBLEM

from . import climate as rotenso_climate

CONF_ROTENSO_ID = "rotenso_id"
CONF_ERROR = "error"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ROTENSO_ID): cv.use_id(rotenso_climate.RotensoClimate),
        cv.Optional(CONF_ERROR): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ROTENSO_ID])

    if CONF_ERROR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_ERROR])
        cg.add(parent.set_error_binary_sensor(sens))