import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import select
from esphome.const import CONF_ID

from . import climate as rotenso_climate


CONF_ROTENSO_ID = "rotenso_id"
CONF_VERTICAL_VANE = "vertical_vane"

rotenso_ns = cg.esphome_ns.namespace("rotenso")
RotensoVaneSelect = rotenso_ns.class_(
    "RotensoVaneSelect", select.Select, cg.Component
)

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
]

VANE_SELECT_SCHEMA = select.select_schema(RotensoVaneSelect)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ROTENSO_ID): cv.use_id(rotenso_climate.RotensoClimate),
        cv.Required(CONF_VERTICAL_VANE): VANE_SELECT_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ROTENSO_ID])
    select_config = config[CONF_VERTICAL_VANE]

    var = cg.new_Pvariable(select_config[CONF_ID])
    await cg.register_component(var, select_config)
    await select.register_select(var, select_config, options=VANE_OPTIONS)

    cg.add(var.set_parent(parent))
    cg.add(parent.set_vertical_vane_select(var))
