import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import climate, uart

from esphome.const import CONF_ID, CONF_UART_ID

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "binary_sensor", "select"]

rotenso_ns = cg.esphome_ns.namespace("rotenso")
RotensoClimate = rotenso_ns.class_(
    "RotensoClimate", climate.Climate, cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = (
    climate.climate_schema(RotensoClimate)
    .extend(
        {
            cv.GenerateID(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)
