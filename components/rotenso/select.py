import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import select

from . import climate as rotenso_climate

CONF_ROTENSO_ID = "rotenso_id"
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

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ROTENSO_ID): cv.use_id(rotenso_climate.RotensoClimate),
        cv.Required(CONF_VERTICAL_VANE): select.select_schema(
            rotenso_climate.RotensoVaneSelect, icon="mdi:arrow-up-down"
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ROTENSO_ID])
    vane_select = cg.new_Pvariable(config[CONF_VERTICAL_VANE][CONF_ID])
    await cg.register_component(vane_select, config[CONF_VERTICAL_VANE])
    await select.register_select(
        vane_select, config[CONF_VERTICAL_VANE], options=VANE_OPTIONS
    )
    cg.add(vane_select.set_parent(parent))
    cg.add(parent.set_vertical_vane_select(vane_select))
