import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import uart, climate, sensor
from esphome.components.climate import ClimateMode

from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
)

CODEOWNERS = ["Pablo"]
DEPENDENCIES = ["climate", "uart"]
AUTO_LOAD = ["sensor"]

rotenso_ns = cg.esphome_ns.namespace("rotenso")

RotensoClimate = rotenso_ns.class_(
    "RotensoClimate",
    climate.Climate,
    cg.Component,
    uart.UARTDevice,
)

ALLOWED_CLIMATE_MODES = {
    "HEAT_COOL": ClimateMode.CLIMATE_MODE_HEAT_COOL,
    "COOL": ClimateMode.CLIMATE_MODE_COOL,
    "HEAT": ClimateMode.CLIMATE_MODE_HEAT,
    "DRY": ClimateMode.CLIMATE_MODE_DRY,
    "FAN_ONLY": ClimateMode.CLIMATE_MODE_FAN_ONLY,
}

validate_modes = cv.enum(ALLOWED_CLIMATE_MODES, upper=True)


CONFIG_SCHEMA = climate.climate_schema(RotensoClimate).extend(
    {
        cv.GenerateID(CONF_UART_ID): cv.use_id(uart.UARTComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)