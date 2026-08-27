import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, select, sensor, uart
from esphome.components.climate import ClimateMode, ClimatePreset
from esphome.const import CONF_ID, CONF_UART_ID

DEPENDENCIES = ["climate", "uart"]
AUTO_LOAD = ["sensor", "select"]

rotenso_ns = cg.esphome_ns.namespace("rotenso")
RotensoClimate = rotenso_ns.class_(
    "RotensoClimate", climate.Climate, cg.Component, uart.UARTDevice
)
RotensoVaneSelect = rotenso_ns.class_(
    "RotensoVaneSelect", select.Select, cg.Component
)

CONF_VERTICAL_VANE = "vertical_vane"

VANE_OPTIONS = [
    "Off",
    "Top",
    "Upper",
    "Mid",
    "Lower",
    "Bottom",
    "Move Full",
    "Move Upper",
    "Move Lower",
    "Unknown",
]

CONFIG_SCHEMA = climate.climate_schema(RotensoClimate).extend(
    {
        cv.GenerateID(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_VERTICAL_VANE): select.select_schema(
            RotensoVaneSelect, icon="mdi:arrow-up-down"
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)

    if CONF_VERTICAL_VANE in config:
        vane_select = cg.new_Pvariable(
            config[CONF_VERTICAL_VANE][CONF_ID]
        )
        await cg.register_component(vane_select, config[CONF_VERTICAL_VANE])
        await select.register_select(
            vane_select,
            config[CONF_VERTICAL_VANE],
            options=VANE_OPTIONS,
        )
        cg.add(vane_select.set_parent(var))
        cg.add(var.set_vertical_vane_select(vane_select))
