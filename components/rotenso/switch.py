DEPENDENCIES = ["climate"]

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from .climate import RotensoClimate

CONF_ROTENSO_ID = "rotenso_id"
CONF_FUNCTION = "function"

rotenso_ns = cg.esphome_ns.namespace("rotenso")
RotensoAntiMildewSwitch = rotenso_ns.class_(
    "RotensoAntiMildewSwitch", switch.Switch
)
RotensoBuzzerSwitch = rotenso_ns.class_(
    "RotensoBuzzerSwitch", switch.Switch
)
RotensoDisplaySwitch = rotenso_ns.class_(
    "RotensoDisplaySwitch", switch.Switch
)

# The C++ class (and so the declared type of CONF_ID) must be fixed at
# schema-definition time - see select.py for the full explanation of why
# this can't be done via an extra new_Pvariable() argument instead.
ANTI_MILDEW_SCHEMA = switch.switch_schema(RotensoAntiMildewSwitch).extend(
    {
        cv.GenerateID(CONF_ROTENSO_ID): cv.use_id(RotensoClimate),
        cv.Optional(CONF_FUNCTION, default="anti_mildew"): cv.one_of(
            "anti_mildew", lower=True
        ),
    }
)

BUZZER_SCHEMA = switch.switch_schema(RotensoBuzzerSwitch).extend(
    {
        cv.GenerateID(CONF_ROTENSO_ID): cv.use_id(RotensoClimate),
        cv.Required(CONF_FUNCTION): cv.one_of("buzzer", lower=True),
    }
)

DISPLAY_SCHEMA = switch.switch_schema(RotensoDisplaySwitch).extend(
    {
        cv.GenerateID(CONF_ROTENSO_ID): cv.use_id(RotensoClimate),
        cv.Required(CONF_FUNCTION): cv.one_of("display", lower=True),
    }
)


def _rotenso_switch_schema(value):
    if not isinstance(value, dict):
        raise cv.Invalid("value must be a dictionary")
    function = str(value.get(CONF_FUNCTION, "anti_mildew")).lower()
    if function == "buzzer":
        return BUZZER_SCHEMA(value)
    if function == "display":
        return DISPLAY_SCHEMA(value)
    return ANTI_MILDEW_SCHEMA(value)


CONFIG_SCHEMA = _rotenso_switch_schema


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ROTENSO_ID])

    var = cg.new_Pvariable(config[CONF_ID])
    await switch.register_switch(var, config)
    cg.add(var.set_parent(parent))

    function = config[CONF_FUNCTION]
    if function == "buzzer":
        cg.add(parent.set_buzzer_switch(var))
    elif function == "display":
        cg.add(parent.set_display_switch(var))
    else:
        cg.add(parent.set_anti_mildew_switch(var))