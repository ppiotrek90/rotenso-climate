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

# The C++ class (and so the declared type of CONF_ID) must be fixed at
# schema-definition time - it can't be swapped later via an extra
# new_Pvariable() argument (that argument is a constructor argument, not a
# type override). So "vane: vertical" and "vane: horizontal" resolve to two
# separate schemas, each declaring the right class for CONF_ID.
VERTICAL_SCHEMA = select.select_schema(RotensoVerticalVaneSelect).extend(
    {
        cv.GenerateID(CONF_ROTENSO_ID): cv.use_id(RotensoClimate),
        cv.Optional(CONF_VANE, default="vertical"): cv.one_of(
            "vertical", lower=True
        ),
    }
)

HORIZONTAL_SCHEMA = select.select_schema(RotensoHorizontalVaneSelect).extend(
    {
        cv.GenerateID(CONF_ROTENSO_ID): cv.use_id(RotensoClimate),
        cv.Required(CONF_VANE): cv.one_of("horizontal", lower=True),
    }
)


def _rotenso_vane_schema(value):
    if not isinstance(value, dict):
        raise cv.Invalid("value must be a dictionary")
    vane = str(value.get(CONF_VANE, "vertical")).lower()
    if vane == "horizontal":
        return HORIZONTAL_SCHEMA(value)
    return VERTICAL_SCHEMA(value)


CONFIG_SCHEMA = _rotenso_vane_schema


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ROTENSO_ID])

    var = cg.new_Pvariable(config[CONF_ID])

    if config[CONF_VANE] == "horizontal":
        await select.register_select(
            var, config, options=HORIZONTAL_VANE_OPTIONS
        )
        cg.add(var.set_parent(parent))
        cg.add(parent.set_horizontal_vane_select(var))
    else:
        await select.register_select(var, config, options=VERTICAL_VANE_OPTIONS)
        cg.add(var.set_parent(parent))
        cg.add(parent.set_vertical_vane_select(var))