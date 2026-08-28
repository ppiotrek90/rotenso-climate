DEPENDENCIES = ["climate"]

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID

from .climate import RotensoClimate

CONF_ROTENSO_ID = "rotenso_id"

rotenso_ns = cg.esphome_ns.namespace("rotenso")
RotensoVerticalVaneSelect = rotenso_ns.class_("RotensoVerticalVaneSelect", select.Select)

VERTICAL_VANE_OPTIONS = [
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

CONFIG_SCHEMA = select.select_schema(RotensoVerticalVaneSelect).extend(
    {
        cv.GenerateID(CONF_ROTENSO_ID): cv.use_id(RotensoClimate),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ROTENSO_ID])

    var = cg.new_Pvariable(config[CONF_ID])
    await select.register_select(var, config, options=VERTICAL_VANE_OPTIONS)

    cg.add(var.set_parent(parent))
    cg.add(parent.set_vertical_vane_select(var))