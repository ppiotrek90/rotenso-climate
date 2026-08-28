DEPENDENCIES = ["climate"]

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID

from .climate import RotensoClimate

CONF_ROTENSO_ID = "rotenso_id"
CONF_VANE = "vane"

rotenso_ns = cg.esphome_ns.namespace("rotenso")
RotensoVerticalVaneSelect = rotenso_ns.class_(
    "RotensoVerticalVaneSelect", select.Select
)
RotensoHorizontalVaneSelect = rotenso_ns.class_(
    "RotensoHorizontalVaneSelect", select.Select
)

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

HORIZONTAL_VANE_OPTIONS = [
    "Off",
    "Left",
    "Mid-left",
    "Mid",
    "Mid-right",
    "Right",
    "Move Full",
    "Move Left",
    "Move Mid",
    "Move Right",
    "Unknown",
]

CONFIG_SCHEMA = select.select_schema(RotensoVerticalVaneSelect).extend(
    {
        cv.GenerateID(CONF_ROTENSO_ID): cv.use_id(RotensoClimate),
        cv.Optional(CONF_VANE, default="vertical"): cv.one_of(
            "vertical", "horizontal", lower=True
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ROTENSO_ID])

    if config[CONF_VANE] == "horizontal":
        var = cg.new_Pvariable(config[CONF_ID], RotensoHorizontalVaneSelect)
        await select.register_select(
            var, config, options=HORIZONTAL_VANE_OPTIONS
        )
        cg.add(var.set_parent(parent))
        cg.add(parent.set_horizontal_vane_select(var))
    else:
        var = cg.new_Pvariable(config[CONF_ID], RotensoVerticalVaneSelect)
        await select.register_select(var, config, options=VERTICAL_VANE_OPTIONS)
        cg.add(var.set_parent(parent))
        cg.add(parent.set_vertical_vane_select(var))